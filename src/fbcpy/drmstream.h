#pragma once

#include <stdatomic.h>

#include <pthread.h>

#include "../libs/types.h"
#include "../libs/queue.h"
#include "../libs/ring.h"
#include "../libs/fpsi.h"

#include "capturesink.h"

#include "drm.h"

typedef struct {
	atomic_bool			stop;

	us_drm_state_s*		drm;
	us_fpsi_s* 			fps;
	us_capturesink_s*	capture;

} us_drmstream_runtime_s;

typedef struct {
	char*			framebuffer_path;
	char*			sink_raw_name;

	us_drmstream_runtime_s	*run;
} us_drmstream_s;


us_drmstream_s *us_drmstream_init(us_capturesink_s* capture);
void us_drmstream_destroy(us_drmstream_s *drmstream);

void us_drmstream_loop(us_drmstream_s *drmstream);
void us_drmstream_loop_break(us_drmstream_s *drmstream);
