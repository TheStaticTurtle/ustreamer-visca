#pragma once

#include <stdatomic.h>

#include <pthread.h>

#include "../libs/types.h"
#include "../libs/queue.h"
#include "../libs/ring.h"
#include "../libs/fpsi.h"

#include "drm.h"

struct us_capturesink_s;

typedef struct us_drmstream_runtime_s {
	atomic_bool					stop;

	us_drm_state_s*				drm;
	us_fpsi_s* 					fps;
	struct us_capturesink_s*	capture;

} us_drmstream_runtime_t ;

typedef struct us_drmstream_s {
	char*						framebuffer_path;
	char*						sink_raw_name;

	us_drmstream_runtime_t		*run;
} us_drmstream_t;


us_drmstream_t *us_drmstream_init();
void us_drmstream_destroy(us_drmstream_t *drmstream);

void us_drmstream_loop(us_drmstream_t *drmstream);
void us_drmstream_loop_break(us_drmstream_t *drmstream);
