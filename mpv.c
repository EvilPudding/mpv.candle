#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>
#include <utils/glutil.h>
#include <components/model.h>

#include "mpv.h"
#include "mpv/libmpv/client.h"
#include "mpv/libmpv/render_gl.h"

extern mesh_t *g_quad_mesh;

static uint32_t wakeup_on_mpv_render_update, wakeup_on_mpv_events;

static void *get_proc_address_mpv(void *fn_ctx, const char *name)
{
    return SDL_GL_GetProcAddress(name);
}

static void on_mpv_events(void *ctx)
{
    SDL_Event event = {.type = wakeup_on_mpv_events};
    SDL_PushEvent(&event);
}

static void on_mpv_render_update(void *ctx)
{
    SDL_Event event = {.type = wakeup_on_mpv_render_update};
    SDL_PushEvent(&event);
}

static int c_mpv_position_changed(c_mpv_t *self)
{
	c_node_t *node = c_node(self);
	c_node_update_model(node);
	drawable_set_transform(&self->draw, node->model);
	drawable_set_entity(&self->draw, node->unpack_inheritance);
	return CONTINUE;
}


static
void c_mpv__loadfile(c_mpv_t *self, const char *filename)
{
    const char *cmd[] = {"loadfile", filename, NULL};
    mpv_command_async(self->handle, 0, cmd);
}

void c_mpv_init_gl(c_mpv_t *self)
{
    self->handle = mpv_create();
    if (!self->handle)
	{
		exit(1);
	}

    if (mpv_initialize(self->handle) < 0)
	{
		exit(1);
	}

    mpv_request_log_messages(self->handle, "debug");

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &(mpv_opengl_init_params){
            .get_proc_address = get_proc_address_mpv,
        }},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &(int){1}},
        {0}
    };

    if (mpv_render_context_create((mpv_render_context**)&self->context,
                                  self->handle, params) < 0)
	{
		exit(1);
	}

    wakeup_on_mpv_render_update = SDL_RegisterEvents(1);
    wakeup_on_mpv_events = SDL_RegisterEvents(1);
    if (wakeup_on_mpv_render_update == (uint32_t)-1 ||
        wakeup_on_mpv_events == (uint32_t)-1)
	{
		exit(1);
	}

    mpv_set_wakeup_callback(self->handle, on_mpv_events, NULL);

    mpv_render_context_set_update_callback(self->context, on_mpv_render_update, NULL);

	if (self->queued[0])
	{
		c_mpv__loadfile(self, self->queued);
		self->queued[0] = '\0';
	}
}

c_mpv_t *c_mpv_new(char *filename)
{
	c_mpv_t *self = component_new("mpv");

	self->texture = texture_new_2D(800, 354, TEX_INTERPOLATE,
		buffer_new("color",	false, 4));

	drawable_init(&self->draw, ref("framebuffer"));
	drawable_add_group(&self->draw, ref("selectable"));
	drawable_set_texture(&self->draw, self->texture);
	drawable_set_vs(&self->draw, model_vs());
	drawable_set_mesh(&self->draw, g_quad_mesh);
	drawable_set_entity(&self->draw, c_entity(self));
	c_spatial_set_scale(c_spatial(self), vec3(2.0f * 16.0 / 9.0, 2.0f, 2.0f));

	if (filename[0])
	{
		c_mpv_loadfile(self, filename);
	}

	return self;
}

void c_mpv_loadfile(c_mpv_t *self, const char *filename)
{
	if (self->handle)
	{
		c_mpv__loadfile(self, filename);
	}
	else
	{
		strncpy(self->queued, filename, sizeof(self->queued) - 1);
	}
}

static void c_mpv_destroy(c_mpv_t *self)
{
    mpv_render_context_free(self->context);
    mpv_detach_destroy(self->handle);
	drawable_set_mesh(&self->draw, NULL);
}

int c_mpv_draw(c_mpv_t *self)
{
	if (!self->handle)
	{
		c_mpv_init_gl(self);
	}
	if (!self->redraw)
	{
		return CONTINUE;
	}
	self->redraw = false;

	texture_target(self->texture, NULL, 0);

	mpv_render_param params[] = {
		{MPV_RENDER_PARAM_OPENGL_FBO, &(mpv_opengl_fbo){
			.fbo = self->texture->frame_buffer[0],
			.w = self->texture->width,
			.h = self->texture->height,
		}},
		{MPV_RENDER_PARAM_FLIP_Y, &(int){1}},
		{0}
	};
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	mpv_render_context_render(self->context, params);
	return CONTINUE;
}

int32_t c_mpv_event(c_mpv_t *self, const SDL_Event *event)
{
	if (!self->handle)
	{
		return CONTINUE;
	}
	switch (event->type) {
		case SDL_WINDOWEVENT:
			if (event->window.event == SDL_WINDOWEVENT_EXPOSED)
			{
				self->redraw = true;
			}
			break;
		case SDL_KEYDOWN:
			if (event->key.keysym.sym == SDLK_SPACE) {
				const char *cmd_pause[] = {"cycle", "pause", NULL};
				mpv_command_async(self->handle, 0, cmd_pause);
			}
			if (event->key.keysym.sym == SDLK_s) {
				const char *cmd_scr[] = {"screenshot-to-file",
					"screenshot.png",
					"window",
					NULL};
				printf("attempting to save screenshot to %s\n", cmd_scr[1]);
				mpv_command_async(self->handle, 0, cmd_scr);
			}
			break;
		default:	
			if (event->type == wakeup_on_mpv_render_update)
			{
				uint64_t flags = mpv_render_context_update(self->context);
				if (flags & MPV_RENDER_UPDATE_FRAME)
				{
					self->redraw = true;
				}
				return STOP;
			}
			if (event->type == wakeup_on_mpv_events)
			{
				while (1) {
					mpv_event *mp_event = mpv_wait_event(self->handle, 0);
					if (mp_event->event_id == MPV_EVENT_NONE)
						break;
					if (mp_event->event_id == MPV_EVENT_LOG_MESSAGE) {
						mpv_event_log_message *msg = mp_event->data;
						if (strstr(msg->text, "DR image"))
							printf("log: %s", msg->text);
						continue;
					}
					printf("event: %s\n", mpv_event_name(mp_event->event_id));
				}
				return STOP;
			}
	}
	return CONTINUE;
}

REG()
{
	ct_t *ct = ct_new("mpv", sizeof(c_mpv_t), c_mpv_init, c_mpv_destroy,
	                  1, ref("node"));

	ct_listener(ct, WORLD, 11, sig("world_draw"), c_mpv_draw);
	ct_listener(ct, WORLD, -1, sig("event_handle"), c_mpv_event);

	ct_listener(ct, ENTITY, 0, ref("node_changed"), c_mpv_position_changed);
}

