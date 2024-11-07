#pragma once

#include <stdatomic.h>

#include <pthread.h>

#include "../libs/types.h"
#include "../libs/queue.h"
#include "../libs/ring.h"


typedef struct {
	atomic_bool			stop;
} us_fbstream_runtime_s;

typedef struct {
	char*			framebuffer_path;
	char*			sink_raw_name;

	us_fbstream_runtime_s	*run;
} us_fbstream_s;


us_fbstream_s *us_fbstream_init();
void us_fbstream_destroy(us_fbstream_s *fbstream);

void us_fbstream_loop(us_fbstream_s *fbstream);
void us_fbstream_loop_break(us_fbstream_s *fbstream);
