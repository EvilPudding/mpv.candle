#ifndef CAN_MPV_H
#define CAN_MPV_H

#include <ecs/ecm.h>
#include <utils/texture.h>

typedef struct c_mpv_t
{
	c_t super;
	void *handle;
	void *context;
	bool_t redraw;
	texture_t *texture;
	drawable_t draw;
	char queued[256];
} c_mpv_t;

void ct_mpv(ct_t *self);
DEF_CASTER(ct_mpv, c_mpv, c_mpv_t)

c_mpv_t *c_mpv_new(char *filename);
void c_mpv_loadfile(c_mpv_t *mpv, const char *filename);

#endif /* !CAN_MPV_H */
