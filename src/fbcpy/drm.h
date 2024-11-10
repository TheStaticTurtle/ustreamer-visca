#pragma once

#include <stdatomic.h>

#include <pthread.h>
#include <stddef.h>

#include "../libs/types.h"
#include "../libs/queue.h"
#include "../libs/ring.h"
#include "../libs/fpsi.h"
#include "../libs/frametext.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#define DRM_BUFFER_COUNT 5

typedef struct us_drm_dumb_framebuffer_s {
	uint32_t id;     // DRM object ID
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t handle; // driver-specific handle
	uint64_t size;   // size of mapping

	uint8_t *data;   // mmapped data we can write to
} us_drm_dumb_framebuffer_t;


typedef struct us_drm_runtime_s {
	int							fd;
	
	uint32_t 					connector_id;
	uint32_t 					crtc_id;
	drmModeCrtc* 				old_crtc;

	us_drm_dumb_framebuffer_t 	buffers[DRM_BUFFER_COUNT];
	int write_buffer;

	bool has_vsync;
	
	us_frametext_s* 			ft;

	atomic_bool					stop;
} us_drm_runtime_t ;

typedef struct us_drm_s {
	char* 					card_path;

	uint32_t 				requested_width;
	uint32_t 				requested_height;
	uint32_t 				requested_rate;

	int						vsync_timeout;

	us_drm_runtime_t		*run;
} us_drm_t;

int us_drm_wait_vsync(us_drm_t *drm);
int us_drm_write_buffer(us_drm_t *drm, unsigned char* rgba_data, size_t len);
int us_drm_vsynced_buffer_write(us_drm_t *drm, unsigned char* rgba_data, size_t len);

us_drm_t *us_drm_init();
void us_drm_destroy(us_drm_t *drm);

void us_drm_setup(us_drm_t *drm);
