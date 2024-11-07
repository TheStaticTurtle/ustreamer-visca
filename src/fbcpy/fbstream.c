#include "fbstream.h"

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


#define _LOG_ERROR(x_msg, ...)		US_LOG_ERROR("FBSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_PERROR(x_msg, ...)		US_LOG_PERROR("FBSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_WARN(x_msg, ...)		US_LOG_WARN("FBSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_INFO(x_msg, ...)		US_LOG_INFO("FBSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_VERBOSE(x_msg, ...)	US_LOG_VERBOSE("FBSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_DEBUG(x_msg, ...)		US_LOG_DEBUG("FBSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_TRACE(x_msg, ...)		US_LOG_TRACE("FBSTREAM: " x_msg, ##__VA_ARGS__)


us_fbstream_s *us_fbstream_init() {

	us_fbstream_runtime_s *run;
	US_CALLOC(run, 1);
	atomic_init(&run->stop, false);

	us_fbstream_s *fbstream;
	US_CALLOC(fbstream, 1);
	fbstream->run = run;

	return fbstream;
}

void us_fbstream_destroy(us_fbstream_s *fbstream) {
	free(fbstream->run);
	free(fbstream);
}

void us_fbstream_loop(us_fbstream_s *fbstream) {
	us_fbstream_runtime_s *const run = fbstream->run;

	_LOG_INFO("Hello start");

	while (!atomic_load(&run->stop)) {
		usleep(500 * 1000);
	}

	_LOG_INFO("Hello end");

	if (!atomic_load(&run->stop)) {
		US_SEP_INFO('=');
	}
}

void us_fbstream_loop_break(us_fbstream_s *stream) {
	atomic_store(&stream->run->stop, true);
}