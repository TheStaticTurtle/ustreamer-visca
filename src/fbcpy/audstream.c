#include "audstream.h"

#include <stdlib.h>
#include <stdatomic.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <pthread.h>

#include "../libs/types.h"
#include "../libs/errors.h"
#include "../libs/tools.h"
#include "../libs/threading.h"
#include "../libs/process.h"
#include "../libs/logging.h"
#include "../libs/memsink.h"

#define _LOG_ERROR(x_msg, ...)		US_LOG_ERROR("AUDSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_PERROR(x_msg, ...)		US_LOG_PERROR("AUDSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_WARN(x_msg, ...)		US_LOG_WARN("AUDSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_INFO(x_msg, ...)		US_LOG_INFO("AUDSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_VERBOSE(x_msg, ...)	US_LOG_VERBOSE("AUDSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_DEBUG(x_msg, ...)		US_LOG_DEBUG("AUDSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_TRACE(x_msg, ...)		US_LOG_TRACE("AUDSTREAM: " x_msg, ##__VA_ARGS__)


us_audstream_s *us_audstream_init() {

	us_audstream_runtime_s *run;
	US_CALLOC(run, 1);
	atomic_init(&run->stop, false);

	us_audstream_s *audstream;
	US_CALLOC(audstream, 1);
	audstream->run = run;

	return audstream;
}

void us_audstream_destroy(us_audstream_s *audstream) {
	free(audstream->run);
	free(audstream);
}

void us_audstream_loop(us_audstream_s *audstream) {
	us_audstream_runtime_s *const run = audstream->run;

	_LOG_INFO("Hello start");

	while (!atomic_load(&run->stop)) {
		usleep(500 * 1000);
	}

	_LOG_INFO("Hello end");
	
	if (!atomic_load(&run->stop)) {
		US_SEP_INFO('=');
	}
}

void us_audstream_loop_break(us_audstream_s *stream) {
	atomic_store(&stream->run->stop, true);
}