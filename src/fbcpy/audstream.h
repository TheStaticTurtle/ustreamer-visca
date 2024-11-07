#pragma once

#include <stdatomic.h>

#include <pthread.h>

#include "../libs/types.h"
#include "../libs/queue.h"
#include "../libs/ring.h"


typedef struct {
	atomic_bool			stop;
} us_audstream_runtime_s;

typedef struct {
	char*			dev_in_name;
	char*			dev_out_name;
	int				buffer_size;

	us_audstream_runtime_s	*run;
} us_audstream_s;


us_audstream_s *us_audstream_init();
void us_audstream_destroy(us_audstream_s *audstream);

void us_audstream_loop(us_audstream_s *audstream);
void us_audstream_loop_break(us_audstream_s *audstream);
