#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "sim.h"

struct ball balls[N_BALLS] = {
	{0.03F, M_PI * 0.03F * 0.03F, {0.92F, 0.5F}, {-0.2F, 3.5F}, 0.2F},
	{0.03F, M_PI * 0.03F * 0.03F, {0.08F, 0.5F}, {0.2F, 3.5F}, 0.2F},
};

struct vec2 border[N_BORDER] = {
	{0.74F, 0.25F},
	{0.98F, 0.4F},
	{0.98F, 1.68F},
	{0.02F, 1.68F},
	{0.02F, 0.4F},
	{0.26F, 0.25F},
	{0.26F, 0.02F},
	{0.74F, 0.02f}
};

struct obstacle obstacles[N_OBSTACLES] = {
	{0.1F, {0.25F, 0.6F}, 2.0F},
	{0.1F, {0.75F, 0.5F}, 2.0F},
	{0.12F, {0.7F, 1.0F}, 2.0F},
	{0.1F, {0.2F, 1.2F}, 2.0F},
};

struct flipper flippers[N_FLIPPERS] = {
	{0.03F, {0.26F, 0.22F}, 0.2F, -0.5F, 
	 1.0F, 1.0F, 10.0F, 0.0F, 0.0F, -1.0F},
	{0.03F, {0.74F, 0.22F}, 0.2F, M_PI + 0.5F, 
	 1.0F, -1.0F, 10.0F, 0.0F, 0.0F, -1.0F}
};

static struct vec2 gravity = {0.0F, -3.0F};

static void ball_ball(struct ball *a, struct ball *b) {
	float dx, dy, dv;
	float corr;
	float ma, mb;
	float v0a, v1a, v0b, v1b;
	float rest;

	rest = fminf(a->restitution, b->restitution);
	dx = b->pos.x - a->pos.x;
	dy = b->pos.y - a->pos.y;
	dv = sqrtf(dx * dx + dy * dy);
	if (dv == 0.0F || dv > a->radius + b->radius)
		return;
	dx /= dv;
	dy /= dv;
	corr = (a->radius + b->radius - dv) / 2.0F;
	a->pos.x -= dx * corr;
	a->pos.y -= dy * corr;
	b->pos.x += dx * corr;
	b->pos.y += dy * corr;
	v0a = a->vel.x * dx + a->vel.y * dy;
	v0b = b->vel.x * dx + b->vel.y * dy;
	ma = a->mass;
	mb = b->mass;
	v1a = (ma * v0a + mb * v0b - mb * (v0a - v0b) * rest) / (ma + mb);
	v1b = (ma * v0a + mb * v0b - ma * (v0b - v0a) * rest) / (ma + mb);
	a->vel.x += dx * (v1a - v0a);
	a->vel.y += dy * (v1a - v0a);
	b->vel.x += dx * (v1b - v0b);
	b->vel.y += dy * (v1b - v0b);
}

static struct vec2 perp(struct vec2 a) {
	struct vec2 res;

	res.x = -a.y;
	res.y = a.x;
	return res; 
}

static float dot(struct vec2 a, struct vec2 b) {
	return a.x * b.x + a.y * b.y;
}

static float clamp(float v, float l, float h) {
	return fmaxf(fminf(v, h), l);
}

static struct vec2 vec2_sub(struct vec2 a, struct vec2 b) { 
	struct vec2 res;

	res.x = a.x - b.x;
	res.y = a.y - b.y;
	return res;
}

static struct vec2 closest_pos(struct vec2 p, struct vec2 a, struct vec2 b) {
	struct vec2 ab;
	struct vec2 res;
	float t;

	ab = vec2_sub(b, a);
	t = dot(ab, ab);
	t = (dot(p, ab) - dot(a, ab)) / t;
	t = clamp(t, 0.0F, 1.0F);
	res.x = a.x + ab.x * t;
	res.y = a.y + ab.y * t;
	return res;
}

static void ball_border(struct ball *ball) {
	struct vec2 a, b, c, d, min_disp;
	struct vec2 ab, n;
	float scale, dist, min_dist;
	float v0, v1;
	int i;

	for (i = 0; i < N_BORDER; i++) {
		a = border[i];
		b = border[(i + 1) % N_BORDER];
		c = closest_pos(ball->pos, a, b);
		d = vec2_sub(ball->pos, c);
		dist = dot(d, d); 
		if (i == 0 || dist < min_dist) {
			min_dist = dist;
			min_disp = d;
			ab = vec2_sub(b, a);
			n = perp(ab);
		}
	}
	d = min_dist == 0.0F ? n : min_disp;
	dist = sqrtf(dot(d, d));
	d.x /= dist;
	d.y /= dist;
	if (dot(d, n) < 0.0F) 
		scale = -ball->radius - dist;
	else if (dist > ball->radius) 
		return;
	else 
		scale = ball->radius - dist;
	ball->pos.x += d.x * scale;
	ball->pos.y += d.y * scale;
	v0 = dot(ball->vel, d);
	v1 = fabsf(v0) * ball->restitution;
	ball->vel.x += d.x * (v1 - v0);
	ball->vel.y += d.y * (v1 - v0);
}

static void ball_obstacle(struct ball *a, struct obstacle *b) {
	struct vec2 v;
	float s, corr;

	v = vec2_sub(a->pos, b->pos);
	s = sqrtf(dot(v, v));
	if (s == 0.0F || s > a->radius + b->radius)
		return;
	v.x /= s;
	v.y /= s;
	corr = a->radius + b->radius - s;
	a->pos.x += v.x * corr;
	a->pos.y += v.y * corr;
	corr = b->push_vel - dot(a->vel, v);
	a->vel.x += v.x * corr;
	a->vel.y += v.y * corr;
}

static struct vec2 get_tip(struct flipper *a) {
	float rot;
	struct vec2 tip;

	rot = -(a->rest_rad + a->sign * a->rot);
	tip.x = a->pos.x + cosf(rot) * a->length;
	tip.y = a->pos.y + sinf(rot) * a->length; 
	return tip;
}

static void ball_flipper(struct ball *a, struct flipper *b) {
	struct vec2 pos, dir;
	float s, corr;

	pos = closest_pos(a->pos, b->pos, get_tip(b));
	dir = vec2_sub(a->pos, pos);
	s = sqrtf(dot(dir, dir));
	if (s == 0.0F || s > a->radius + b->radius)
		return;
	dir.x /= s;
	dir.y /= s;
	corr = a->radius + b->radius - s;
	a->pos.x += dir.x * corr;
	a->pos.y += dir.y * corr;
	pos.x = (pos.x + dir.x * b->radius - b->pos.x) * b->cur_wvel;
	pos.y = (pos.y + dir.y * b->radius - b->pos.y) * b->cur_wvel; 
	pos = perp(pos);
	s = dot(pos, dir) - dot(a->vel, dir);
	a->vel.x += dir.x * s;
	a->vel.y += dir.y * s;
}

void simulate(void) {
	int i, j;
	struct ball *b;
	struct flipper *f;
	float prev_rot;

	for (i = 0; i < N_FLIPPERS; i++) {
		f = &flippers[i];
		prev_rot = f->rot;
		f->rot = f->touch_id < 0 ? 
			fmaxf(f->rot - DT * f->wvel, 0.0F) :
			fminf(f->rot + DT * f->wvel, f->max_rot);
		f->cur_wvel = f->sign * (prev_rot - f->rot) / DT;
	}
	for (i = 0; i < N_BALLS; i++) {
		b = &balls[i];
		b->vel.x += gravity.x * DT;
		b->vel.y += gravity.y * DT;
		b->pos.x += b->vel.x * DT;
		b->pos.y += b->vel.y * DT;
		for (j = i + 1; j < N_BALLS; j++) 
			ball_ball(b, &balls[j]);
		for (j = 0; j < N_OBSTACLES; j++)
			ball_obstacle(b, &obstacles[j]);
		for (j = 0; j < N_FLIPPERS; j++)
			ball_flipper(b, &flippers[j]);
		ball_border(b);
	}
}
