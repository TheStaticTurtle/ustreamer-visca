#pragma once


#include <xf86drm.h>
#include <xf86drmMode.h>

struct dumb_framebuffer {
	uint32_t id;     // DRM object ID
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t handle; // driver-specific handle
	uint64_t size;   // size of mapping

	uint8_t *data;   // mmapped data we can write to
};


typedef struct {
	char* 		card_path;
	int			fd;
	
	uint32_t requested_width;
	uint32_t requested_height;
	uint32_t requested_rate;

	uint32_t connector_id;
	uint32_t crtc_id;
	drmModeCrtc* old_crtc;

	struct dumb_framebuffer fb[2];
	struct dumb_framebuffer *front;
	struct dumb_framebuffer *back;
	
} us_drm_state_s;

us_drm_state_s* drm_init();

void drm_destroy(us_drm_state_s* state);

void drm_setup(us_drm_state_s* state);

void drm_do_pageflip(us_drm_state_s* state, void* userdata);