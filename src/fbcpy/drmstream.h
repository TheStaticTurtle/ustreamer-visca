#pragma once

#include <stdatomic.h>

#include <pthread.h>
#include <stddef.h>

#include "../libs/types.h"
#include "../libs/queue.h"
#include "../libs/ring.h"
#include "../libs/fpsi.h"
#include "../libs/frametext.h"

#include "drm.h"

struct us_capturesink_s;

typedef struct us_drmstream_runtime_s {
	atomic_bool					stop;

	us_drm_state_s*				drm;
	us_fpsi_s* 					fps;
	struct us_capturesink_s*	capture;


	us_frametext_s* 			ft;
	us_ring_s*					frame_ring;

} us_drmstream_runtime_t ;

typedef struct us_drmstream_s {
	char*						framebuffer_path;
	char*						sink_raw_name;

	int							ring_capacity;

	us_drmstream_runtime_t		*run;
} us_drmstream_t;

bool us_drmstream_produce_buffer(us_drmstream_t *drmstream, unsigned char* rgba_data, size_t len);

us_drmstream_t *us_drmstream_init();
void us_drmstream_destroy(us_drmstream_t *drmstream);

void us_drmstream_loop(us_drmstream_t *drmstream);
void us_drmstream_loop_break(us_drmstream_t *drmstream);
