#include "drmstream.h"

#include <libavcodec/codec_id.h>
#include <libavutil/frame.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <string.h>
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

	run->ft = us_frametext_init();


	us_drmstream_t *drmstream;
	US_CALLOC(drmstream, 1);
	drmstream->run = run;
	drmstream->ring_capacity = 6;

	return drmstream;
}

void us_drmstream_destroy(us_drmstream_t *drmstream) {
	us_fpsi_destroy(drmstream->run->fps);

	US_RING_DELETE_WITH_ITEMS(drmstream->run->frame_ring, free);

	drm_destroy(drmstream->run->drm);

	free(drmstream->run);
	free(drmstream);
}


// See: https://stackoverflow.com/questions/7069090/convert-rgb-to-rgba-in-c
void fast_unpack(unsigned char* rgba, const unsigned char* rgb, const int count) {
    if(count==0)
        return;
    for(int i=count; --i; rgba+=4, rgb+=3) {
        *(uint32_t*)(void*)rgba = *(const uint32_t*)(const void*)rgb;
    }
    for(int j=0; j<3; ++j) {
        rgba[j] = rgb[j];
    }
}

static void page_flip_handler(int drm_fd, unsigned sequence, unsigned tv_sec, unsigned tv_usec, void *data) {
	(void)sequence;
	(void)tv_sec;
	(void)tv_usec;

	us_drmstream_t *drmstream = data;
	us_drmstream_runtime_t* run = drmstream->run;
	us_drm_state_s* state = run->drm;

	// Try to get a frame from the ringbuffer for 1sec
	int idx = us_ring_consumer_acquire(run->frame_ring, 1);
	if(idx == -1) {
		// If 1sec elapsed, consider that something got disconencted
		_LOG_WARN("Ring buffer underun - Is the sink connected ?")

		// Unpach the RGB24 error message to both framebuffers (don't care about glitches)
		fast_unpack(state->front->data, run->ft->frame->data, state->back->width * state->back->height);
		fast_unpack(state->back->data, run->ft->frame->data, state->back->width * state->back->height);

	} else {
		// Got a frame in less than 1sec, copy it to the backbuffer
		memcpy(state->back->data, run->frame_ring->items[idx], state->back->size);
		// Release the frame
		us_ring_consumer_release(run->frame_ring, idx);

		//Update the FPS counter
		us_fpsi_update(run->fps, true, NULL);
	}

	// Continue pageflips
	drm_do_pageflip(state, drmstream);

	// Swap the buffers
	dumb_framebuffer_t* tmp = state->back;
	state->back = state->front;
	state->front = tmp;
}


bool us_drmstream_produce_buffer(us_drmstream_t *drmstream, unsigned char* rgba_data, size_t len) {
	us_drmstream_runtime_t *const run = drmstream->run;
	
	int idx = us_ring_producer_acquire(run->frame_ring, 1);
	if(idx == -1) {
		_LOG_WARN("Couldn't write frame, is drm too slow?")
		return false;
	}

	memcpy(run->frame_ring->items[idx], rgba_data, US_MIN(len, run->drm->back->size));

	us_ring_producer_release(run->frame_ring, idx);

	return true;
}


void us_drmstream_loop(us_drmstream_t *drmstream) {
	us_drmstream_runtime_t *const run = drmstream->run;

	drm_setup(run->drm);
	
	us_frametext_draw(run->ft, "< NO SIGNAL >", 1920, 1080);

	run->frame_ring = us_ring_init(drmstream->ring_capacity);

	for (size_t m_index = 0; m_index < run->frame_ring->capacity; ++m_index) {
		run->frame_ring->items[m_index] = malloc(run->drm->front->size);
		us_ring_producer_release(run->frame_ring, us_ring_producer_acquire(run->frame_ring, 0));
	}
	
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
	
	_LOG_INFO("Finished");

	if (!atomic_load(&run->stop)) {
		US_SEP_INFO('=');
	}
}

void us_drmstream_loop_break(us_drmstream_t *stream) {
	atomic_store(&stream->run->stop, true);
}



