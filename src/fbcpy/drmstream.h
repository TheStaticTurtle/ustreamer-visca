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

struct us_capturesink_s;


typedef struct us_drm_dumb_framebuffer_s {
	uint32_t id;     // DRM object ID
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t handle; // driver-specific handle
	uint64_t size;   // size of mapping

	uint8_t *data;   // mmapped data we can write to
} us_drm_dumb_framebuffer_t;


typedef struct us_drmstream_runtime_s {
	int							fd;
	
	uint32_t 					connector_id;
	uint32_t 					crtc_id;
	drmModeCrtc* 				old_crtc;

	us_drm_dumb_framebuffer_t* 	front;
	us_drm_dumb_framebuffer_t* 	middle;
	us_drm_dumb_framebuffer_t* 	back;


	us_fpsi_s* 					fps;
	struct us_capturesink_s*	capture;
	
	us_frametext_s* 			ft;
	us_ring_s*					frame_ring;

	atomic_bool					stop;
} us_drmstream_runtime_t ;

typedef struct us_drmstream_s {
	char* 						card_path;
	char*					sink_raw_name;

	uint32_t 					requested_width;
	uint32_t 					requested_height;
	uint32_t 					requested_rate;

	int							ring_capacity;

	us_drmstream_runtime_t		*run;
} us_drmstream_t;

void drm_do_pageflip(us_drmstream_t* state, void* userdata);

bool us_drmstream_produce_buffer(us_drmstream_t *drmstream, unsigned char* rgba_data, size_t len);

us_drmstream_t *us_drmstream_init();
void us_drmstream_destroy(us_drmstream_t *drmstream);

void us_drmstream_loop(us_drmstream_t *drmstream);
void us_drmstream_loop_break(us_drmstream_t *drmstream);
