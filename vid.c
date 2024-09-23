#include <SDL2/SDL.h>
#include <glad/gl.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include "draw.h"
#include "sim.h"

#define WIDTH 640
#define HEIGHT 480

static uint8_t pixels[WIDTH * HEIGHT * 3];
static const char path[] = "ball.mp4";

static void encode(AVCodecContext *cctx, AVFrame *frame,
	       AVPacket *pkt, AVFormatContext *fmtctx, AVStream *vid) {
	int ret;

	ret = avcodec_send_frame(cctx, frame);
	if (ret < 0)
		die("avcodec_send_frame: %s\n", av_err2str(ret));
	while (ret >= 0) {
		ret = avcodec_receive_packet(cctx, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
			return;
		if (ret < 0)
			die("avcodec_receive_packet: %s\n", ret);
		av_packet_rescale_ts(pkt, cctx->time_base, vid->time_base);
		av_write_frame(fmtctx, pkt);
		av_packet_unref(pkt);
	}
}	

static void render(void) {
	struct SwsContext *sws;
	const uint8_t *src;
	int src_stride;
	const AVOutputFormat *outfmt;
	AVFormatContext *fmtctx;
	int ret;
	const AVCodec *codec;
	AVStream *vid;
	AVCodecContext *cctx;
	AVPacket *pkt;
	AVFrame *frame;
	int i;

	sws = sws_getContext(WIDTH, HEIGHT, AV_PIX_FMT_RGB24,
		WIDTH, HEIGHT, AV_PIX_FMT_YUV420P, 0,
		NULL, NULL, NULL);
	if (!sws)
		die("sws_getContext\n");
	src = pixels + WIDTH * 3 * (HEIGHT - 1);
	src_stride = -WIDTH * 3;
	outfmt = av_guess_format(NULL, path, NULL);
	if (!outfmt)
		die("av_guess_format\n");
	ret = avformat_alloc_output_context2(&fmtctx, outfmt, NULL, path);
	if (ret < 0)
		die("avformat_free_context: %s\n", av_err2str(ret));
	codec = avcodec_find_encoder(outfmt->video_codec);
	if (!codec)
		die("avcodec_find_encoder\n");
	vid = avformat_new_stream(fmtctx, codec);
	if (!vid)
		die("avformat_new_stream\n");
	cctx = avcodec_alloc_context3(codec);
	if (!cctx)
		die("avcodec_alloc_context3\n");
	vid->codecpar->codec_id = outfmt->video_codec;
	vid->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	vid->codecpar->width = WIDTH;
	vid->codecpar->height = HEIGHT;
	vid->codecpar->format = AV_PIX_FMT_YUV420P;
	vid->codecpar->bit_rate = 200000;
        av_opt_set(cctx, "preset", "ultrafast", 0);
	avcodec_parameters_to_context(cctx, vid->codecpar);
	cctx->time_base.num = 1; 
	cctx->time_base.den = FPS; 
	cctx->framerate.num = FPS; 
	cctx->framerate.den = 1; 
	cctx->gop_size = FPS * 10;
	cctx->max_b_frames = 1;
	avcodec_parameters_from_context(vid->codecpar, cctx);
	ret = avcodec_open2(cctx, codec, NULL);
	if (ret < 0)
		die("avcodec_open2: %s\n", av_err2str(ret));
	ret = avio_open(&fmtctx->pb, path, AVIO_FLAG_WRITE);
	if (ret < 0)
		die("avio_open: %s\n", av_err2str(ret));
    	ret = avformat_write_header(fmtctx, NULL);
	if (ret < 0)
		die("avformat_write_header: %s\n", av_err2str(ret));
	pkt = av_packet_alloc();
	if (!pkt)
		die("av_packet_alloc\n");
	frame = av_frame_alloc();
	if (!frame)
		die("av_frame_alloc\n");
	frame->format = AV_PIX_FMT_YUV420P;	
	frame->width = WIDTH;
	frame->height = HEIGHT;
	ret = av_frame_get_buffer(frame, 0);
	if (ret < 0)
		die("av_frame_get_buffer: %s\n", av_err2str(ret));
	for (i = 0; i < FPS * 10; i++) {
		ret = av_frame_make_writable(frame);
		if (ret < 0)
			die("av_frame_make_writable: %s\n", av_err2str(ret));
		simulate();
		draw();
		glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, 
				GL_UNSIGNED_BYTE, pixels); 
		ret = sws_scale(sws, &src,
				&src_stride, 0, HEIGHT, 
				frame->data, frame->linesize);
		frame->pts = i;
		encode(cctx, frame, pkt, fmtctx, vid);
	}
	encode(cctx, NULL, pkt, fmtctx, vid);
	av_frame_free(&frame);
	av_packet_free(&pkt);
	av_write_trailer(fmtctx);
	avio_close(fmtctx->pb);
	avcodec_free_context(&cctx);
	avformat_free_context(fmtctx);
}

int main(int argc, char **argv) {
	SDL_Window *wnd;
	GLuint fbo;
	GLuint tex;
	if (SDL_Init(SDL_INIT_EVERYTHING))
		die("SDL_Init: %s\n", SDL_GetError());
	if (atexit(SDL_Quit))
		die("atexit: SDL_Quit\n");
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 
			SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	wnd = SDL_CreateWindow("Billiards", SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, 
			SDL_WINDOW_OPENGL);
	if (!wnd)
		die("SDL_CreateWindow: %s\n", SDL_GetError());
	if (!SDL_GL_CreateContext(wnd))
		die("SDL_GL_CreateContext: %s\n", SDL_GetError());
	gladLoadGL((GLADloadfunc) SDL_GL_GetProcAddress);
	init_draw();
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 
			0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
			GL_TEXTURE_2D, tex, 0);
	glGenerateMipmap(GL_TEXTURE_2D);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != 
			GL_FRAMEBUFFER_COMPLETE)
		die("glCheckFramebufferStatus: %u\n", glGetError());
	render();
	return 0;
}
