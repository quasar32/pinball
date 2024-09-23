#include <glad/gl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <cglm/cglm.h>
#include "sim.h"

#define N_CIRCLE 256
#define N_RECT 4 
#define MAX_LOG 256

static GLuint vao[3];
static GLuint vbo[3];
static GLuint prog;
static GLint model_loc;

static struct vec2 rect[N_RECT] = {
	{1.0F, 1.0F},
	{0.0F, 1.0F},
	{0.0F, 0.0F},
	{1.0F, 0.0F}
};

#define BORDER_SIZE (N_BORDER * sizeof(struct vec2))
#define CIRCLE_SIZE (N_CIRCLE * sizeof(struct vec2))
#define RECT_SIZE (N_RECT * sizeof(struct vec2))

void die(const char *fmt, ...) {
	va_list ap;	
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

static void bind_data(int i, int sz, const void *data) {
	glBindVertexArray(vao[i]);
	glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, NULL);
	glEnableVertexAttribArray(0);
	glBufferData(GL_ARRAY_BUFFER, sz, data, GL_STATIC_DRAW);
}

static void init_circle(void) {
	struct vec2 *circle, *v;
	float theta = 0.0F;

	glCreateVertexArrays(3, vao);
	glCreateBuffers(3, vbo);
	bind_data(0, BORDER_SIZE, border);
	circle = malloc(CIRCLE_SIZE);
	if (!circle)
		die("malloc: out of memory\n");
	for (v = circle; v < circle + N_CIRCLE; v++) {
		v->x = cosf(theta);
		v->y = sinf(theta);
		theta += 2.0F * M_PI / (N_CIRCLE - 1); 
	}
	bind_data(1, CIRCLE_SIZE, circle);
	free(circle);
	bind_data(2, RECT_SIZE, rect);
}

static const char vs_src[] = 
	"#version 330 core\n"
	"layout(location = 0) in vec2 pos;"
	"uniform mat4 model;"
	"void main() {"
		"gl_Position = model * vec4(pos, 0.0F, 1.0F);"
	"}";

static const char fs_src[] = 
	"#version 330 core\n"
	"out vec4 color;"
	"void main() {"
		"color = vec4(1.0F, 1.0F, 1.0F, 1.0F);"
	"}";

#define N_GLSL 2

struct shader_desc {
	GLenum type;
	const char *src;
};

static struct shader_desc descs[N_GLSL] = {
	{GL_VERTEX_SHADER, vs_src},
	{GL_FRAGMENT_SHADER, fs_src},
};

static void init_prog(void) {
	GLuint shaders[N_GLSL];
	char log[MAX_LOG];
	int success, i;
	for (i = 0; i < N_GLSL; i++) {
		shaders[i] = glCreateShader(descs[i].type);
		glShaderSource(shaders[i], 1, &descs[i].src, NULL);
		glCompileShader(shaders[i]);
		glGetShaderiv(shaders[i], GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(shaders[i], MAX_LOG, NULL, log);
			die("shader: %s\n", log);
		}
	}
	prog = glCreateProgram();
	for (i = 0; i < N_GLSL; i++)
		glAttachShader(prog, shaders[i]);
	glLinkProgram(prog);
	for (i = 0; i < N_GLSL; i++) {
		glDetachShader(prog, shaders[i]);
		glDeleteShader(shaders[i]);
	}
	glGetProgramiv(prog, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(prog, MAX_LOG, NULL, log);
		die("program: %s\n", log);
	}
	model_loc = glGetUniformLocation(prog, "model");
}

void init_draw(void) {
	init_circle();
	init_prog();
}

static void vec3_xyz(float x, float y, float z, vec3 v) {
	v[0] = x;
	v[1] = y;
	v[2] = z;
}

void draw(int negate) {
	int i;
	mat4 m0, m1, m2;
	vec3 v;
	struct ball *b;
	struct obstacle *o;
	struct flipper *f;

	glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(prog);
	glBindVertexArray(vao[0]);
	vec3_xyz(-1.0F, -1.0F, 0.0F, v);
	glm_translate_make(m0, v);
	vec3_xyz(2.0F, 2.0F / 1.7F, 1.0F, v);
	glm_scale(m0, v);
	glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float *) m0);
	glDrawArrays(GL_LINE_LOOP, 0, N_BORDER);
	glBindVertexArray(vao[1]);
	for (i = 0; i < N_BALLS; i++) {
		b = &balls[i];
		glm_mat4_copy(m0, m1);
		vec3_xyz(b->pos.x, b->pos.y, 0.0F, v);
		glm_translate(m1, v);
		vec3_xyz(b->radius, b->radius, 1.0F, v);
		glm_scale(m1, v);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float *) m1);
		glDrawArrays(GL_TRIANGLE_FAN, 0, N_CIRCLE);
	}
	for (i = 0; i < N_OBSTACLES; i++) {
		o = &obstacles[i];
		glm_mat4_copy(m0, m1);
		vec3_xyz(o->pos.x, o->pos.y, 0.0F, v);
		glm_translate(m1, v);
		vec3_xyz(o->radius, o->radius, 1.0F, v);
		glm_scale(m1, v);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float *) m1);
		glDrawArrays(GL_TRIANGLE_FAN, 0, N_CIRCLE);
	}
	for (i = 0; i < N_FLIPPERS; i++) {
		glBindVertexArray(vao[1]);
		f = &flippers[i];
		glm_mat4_copy(m0, m1);
		vec3_xyz(f->pos.x, f->pos.y, 0.0F, v);
		glm_translate(m1, v);
		glm_rotate_z(m1, -f->rest_rad - f->sign * f->rot, m1);
		glm_mat4_copy(m1, m2);
		vec3_xyz(f->radius, f->radius, 1.0F, v);
		glm_scale(m2, v);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float *) m2);
		glDrawArrays(GL_TRIANGLE_FAN, 0, N_CIRCLE);
		glm_mat4_copy(m1, m2);
		vec3_xyz(f->length, 0.0F, 0.0F, v); 
		glm_translate(m2, v);
		vec3_xyz(f->radius, f->radius, 1.0F, v);
		glm_scale(m2, v);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float *) m2);
		glDrawArrays(GL_TRIANGLE_FAN, 0, N_CIRCLE);
		glBindVertexArray(vao[2]);
		glm_mat4_copy(m1, m2);
		vec3_xyz(0.0F, -f->radius, 0.0F, v); 
		glm_translate(m2, v);
		vec3_xyz(f->length, f->radius * 2.0F, 1.0F, v);
		glm_scale(m2, v);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float *) m2);
		glDrawArrays(GL_TRIANGLE_FAN, 0, N_RECT);
	}
}
