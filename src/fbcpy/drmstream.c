#include "drmstream.h"

#include <libavcodec/codec_id.h>
#include <libavutil/frame.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
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
#include "../libs/frame.h"
#include "../libs/memsink.h"
#include "drm.h"

#include "capturesink.h"

#define _LOG_ERROR(x_msg, ...)		US_LOG_ERROR("DRMSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_PERROR(x_msg, ...)		US_LOG_PERROR("DRMSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_WARN(x_msg, ...)		US_LOG_WARN("DRMSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_INFO(x_msg, ...)		US_LOG_INFO("DRMSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_VERBOSE(x_msg, ...)	US_LOG_VERBOSE("DRMSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_DEBUG(x_msg, ...)		US_LOG_DEBUG("DRMSTREAM: " x_msg, ##__VA_ARGS__)
#define _LOG_TRACE(x_msg, ...)		US_LOG_TRACE("DRMSTREAM: " x_msg, ##__VA_ARGS__)


us_drmstream_t *us_drmstream_init() {

	us_drm_state_s* drm = drm_init();

	us_drmstream_runtime_t *run;
	US_CALLOC(run, 1);
	atomic_init(&run->stop, false);
	run->drm = drm;
	run->fps = us_fpsi_init("drmstream", false);

	us_drmstream_t *drmstream;
	US_CALLOC(drmstream, 1);
	drmstream->run = run;

	return drmstream;
}

void us_drmstream_destroy(us_drmstream_t *drmstream) {
	us_fpsi_destroy(drmstream->run->fps);

	drm_destroy(drmstream->run->drm);

	free(drmstream->run);
	free(drmstream);
}




static void page_flip_handler(int drm_fd, unsigned sequence, unsigned tv_sec, unsigned tv_usec, void *data) {
	(void)sequence;
	(void)tv_sec;
	(void)tv_usec;

	us_drmstream_t *drmstream = data;
	us_drm_state_s* state = drmstream->run->drm;
	us_fpsi_s* fpsi = drmstream->run->fps;

	us_fpsi_update(fpsi, true, NULL);

	// Get the backbuffer
	struct dumb_framebuffer *fb = state->back;

	// Get the input frame
	AVFrame* frame = drmstream->run->capture->run->frame_out_rgb_front;

	// Make sure the input frame is valid 
	if(!frame) {
		_LOG_ERROR("Frame from capture is null")
		goto bypass_frame_copy;
	}

	// Make sure the input frame matches framebuffer 
	if(frame->width != fb->width || frame->height != fb->height || frame->linesize[0] != fb->stride) {
		_LOG_ERROR("Frame from capture does not match framebuffer output in=[w=%d h=%d s=%d] out=[w=%d h=%d s=%d]", frame->width, frame->height, frame->linesize[0], fb->width, fb->height, fb->stride)
		goto bypass_frame_copy;
	}

	// Copy the frame data to the backbuffer
	memcpy(fb->data, frame->data[0], fb->height * fb->stride);

bypass_frame_copy:
	// Continue pageflips
	drm_do_pageflip(state, drmstream);

	// Comment these two lines out to remove double buffering
	state->back = state->front;
	state->front = fb;
}




void us_drmstream_loop(us_drmstream_t *drmstream) {
	us_drmstream_runtime_t *const run = drmstream->run;

	drm_setup(run->drm);

	_LOG_INFO("Waiting for capture start");
	while(run->capture->run->frame_out_rgb_back == NULL && !atomic_load(&run->stop)) {
		usleep(250000);
	}
	if (atomic_load(&run->stop)) { goto exit_early; }

	drm_do_pageflip(run->drm, drmstream);


	drmEventContext context = {
		.version = DRM_EVENT_CONTEXT_VERSION,
		.page_flip_handler = page_flip_handler,
	};

	struct pollfd pollfd = {
		.fd = run->drm->fd,
		.events = POLLIN,
	};

	while (!atomic_load(&run->stop)) {

		int ret = poll(&pollfd, 1, 5000);
		if (ret < 0 && errno != EAGAIN) {
			_LOG_PERROR("poll");
			break;
		}

		if (pollfd.revents & POLLIN) {
			if (drmHandleEvent(run->drm->fd, &context) < 0) {
				_LOG_PERROR("drmHandleEvent");
				break;
			}
		}

	}

exit_early:
	_LOG_INFO("Finished");

	if (!atomic_load(&run->stop)) {
		US_SEP_INFO('=');
	}
}

void us_drmstream_loop_break(us_drmstream_t *stream) {
	atomic_store(&stream->run->stop, true);
}



