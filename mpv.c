#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <utils/glutil.h>
#include <components/model.h>
#include <third_party/glfw/include/GLFW/glfw3.h>

#include "mpv.h"
#include "mpv/libmpv/client.h"
#include "mpv/libmpv/render_gl.h"

extern mesh_t *g_quad_mesh;

static void *get_proc_address_mpv(void *fn_ctx, const char *name)
{
    return glfwGetProcAddress(name);
}

static void on_mpv_events(void *ctx)
{
	c_mpv_t *self = ctx;
	mpv_event *mp_event = mpv_wait_event(self->handle, 0);
	if (mp_event->event_id == MPV_EVENT_NONE)
		return;
	if (mp_event->event_id == MPV_EVENT_LOG_MESSAGE) {
		mpv_event_log_message *msg = mp_event->data;
		if (strstr(msg->text, "DR image"))
			printf("log: %s", msg->text);
		return;
	}
	printf("event: %s\n", mpv_event_name(mp_event->event_id));
}

static void on_mpv_render_update(void *ctx)
{
	c_mpv_t *self = ctx;
	printf("redraw\n");
	self->redraw = true;
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

    mpv_set_wakeup_callback(self->handle, on_mpv_events, self);

    mpv_render_context_set_update_callback(self->context, on_mpv_render_update, self);

	if (self->queued[0])
	{
		c_mpv__loadfile(self, self->queued);
		self->queued[0] = '\0';
	}
}

c_mpv_t *c_mpv_new(char *filename)
{
	c_mpv_t *self = component_new(ct_mpv);

	self->texture = texture_new_2D(800, 354, TEX_INTERPOLATE,
		1, buffer_new("color", false, 4));

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
	uint64_t flags;

	if (!self->handle)
	{
		c_mpv_init_gl(self);
	}
	if (!self->redraw)
	{
		return CONTINUE;
	}
	self->redraw = false;

	flags = mpv_render_context_update(self->context);
	if (!(flags & MPV_RENDER_UPDATE_FRAME))
	{
		return CONTINUE;
	}

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

			/* TODO controls */
		/* case SDL_KEYDOWN: */
			/* if (event->key.keysym.sym == SDLK_SPACE) { */
			/* 	const char *cmd_pause[] = {"cycle", "pause", NULL}; */
			/* 	mpv_command_async(self->handle, 0, cmd_pause); */
			/* } */
			/* if (event->key.keysym.sym == SDLK_s) { */
			/* 	const char *cmd_scr[] = {"screenshot-to-file", */
			/* 		"screenshot.png", */
			/* 		"window", */
			/* 		NULL}; */
			/* 	printf("attempting to save screenshot to %s\n", cmd_scr[1]); */
			/* 	mpv_command_async(self->handle, 0, cmd_scr); */
			/* } */
			/* break; */
			/* TODO handle logs */

void ct_mpv(ct_t *self)
{
	ct_init(self, "mpv", sizeof(c_mpv_t));
	ct_set_destroy(self, (destroy_cb)c_mpv_destroy);
	ct_add_dependency(self, ct_node);

	ct_add_listener(self, WORLD, 11, ref("world_draw"), c_mpv_draw);

	ct_add_listener(self, ENTITY, 0, ref("node_changed"), c_mpv_position_changed);
}

