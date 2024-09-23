#ifndef BALL_H
#define BALL_H

#define N_BALLS 2
#define N_BORDER 8
#define N_OBSTACLES 4
#define N_FLIPPERS 2
#define FPS 60
#define DT (1.0F / FPS)

struct vec2 {
	float x;
	float y;
};

struct ball {
	float radius;
	float mass;
	struct vec2 pos;
	struct vec2 vel;
	float restitution;
};

struct obstacle {
	float radius;
	struct vec2 pos;
	float push_vel;
};

struct flipper {
	float radius;
	struct vec2 pos;
	float length;
	float rest_rad;
	float max_rot;
	float sign;
	float wvel;
	float rot;
	float cur_wvel;
	float touch_id;
};

extern struct ball balls[];
extern struct vec2 border[];
extern struct obstacle obstacles[];
extern struct flipper flippers[];

void init_balls(void); 
void simulate(void);

#endif
