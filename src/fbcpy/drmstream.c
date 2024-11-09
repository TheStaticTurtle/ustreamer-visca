#include "drmstream.h"

#include <drm/drm_fourcc.h>
#include <drm/drm_mode.h>
#include <libavcodec/codec_id.h>
#include <libavutil/frame.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
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

#include "capturesink.h"
#include "utils.h"

#define _LOG_ERROR(x_msg, ...)		US_LOG_ERROR("DRM: " x_msg, ##__VA_ARGS__)
#define _LOG_PERROR(x_msg, ...)		US_LOG_PERROR("DRM: " x_msg, ##__VA_ARGS__)
#define _LOG_WARN(x_msg, ...)		US_LOG_WARN("DRM: " x_msg, ##__VA_ARGS__)
#define _LOG_INFO(x_msg, ...)		US_LOG_INFO("DRM: " x_msg, ##__VA_ARGS__)
#define _LOG_VERBOSE(x_msg, ...)	US_LOG_VERBOSE("DRM: " x_msg, ##__VA_ARGS__)
#define _LOG_DEBUG(x_msg, ...)		US_LOG_DEBUG("DRM: " x_msg, ##__VA_ARGS__)
#define _LOG_TRACE(x_msg, ...)		US_LOG_TRACE("DRM: " x_msg, ##__VA_ARGS__)


// Get the human-readable string from a DRM connector type. This is compatible with Weston's connector naming.
static const char *drm_conn_str(uint32_t conn_type) {
	switch (conn_type) {
		case DRM_MODE_CONNECTOR_Unknown:     return "Unknown";
		case DRM_MODE_CONNECTOR_VGA:         return "VGA";
		case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
		case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
		case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
		case DRM_MODE_CONNECTOR_Composite:   return "Composite";
		case DRM_MODE_CONNECTOR_SVIDEO:      return "SVIDEO";
		case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
		case DRM_MODE_CONNECTOR_Component:   return "Component";
		case DRM_MODE_CONNECTOR_9PinDIN:     return "DIN";
		case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
		case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
		case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
		case DRM_MODE_CONNECTOR_TV:          return "TV";
		case DRM_MODE_CONNECTOR_eDP:         return "eDP";
		case DRM_MODE_CONNECTOR_VIRTUAL:     return "Virtual";
		case DRM_MODE_CONNECTOR_DSI:         return "DSI";
		default:                             return "Unknown";
	}
}
static int drm_mode_refresh_rate(drmModeModeInfo *mode) {
	int res = (mode->clock * 1000000LL / mode->htotal + mode->vtotal / 2) / mode->vtotal;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE) { res *= 2; }

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN) { res /= 2; }

	if (mode->vscan > 1) { res /= mode->vscan; }

	return res;
}
static void drm_print_modes(drmModeConnector* connector) {
	_LOG_DEBUG("Connector has %d modes:", connector->count_modes);
	for(int i = 0; i < connector->count_modes; i++) {
		_LOG_DEBUG(
			"  - %dx%d%s %dmHz", 
			connector->modes[i].hdisplay, 
			connector->modes[i].hdisplay, 
			(connector->modes[i].flags & DRM_MODE_FLAG_INTERLACE) ? "i" : "",
			drm_mode_refresh_rate(&connector->modes[i])
		);
	}
}
static drmModeModeInfo* drm_find_best_mode(drmModeConnector* connector, int req_width, int req_height, int req_rate, bool req_interlaced) {
	drmModeModeInfo* best_mode = NULL;

	for(int i = 0; i < connector->count_modes; i++) {
		bool interlaced = (connector->modes[i].flags & DRM_MODE_FLAG_INTERLACE) > 0;
		if(connector->modes[i].hdisplay == req_width && connector->modes[i].vdisplay == req_height && interlaced == req_interlaced) {
			best_mode = &connector->modes[i];

			if(drm_mode_refresh_rate(best_mode) == req_rate) {
				_LOG_INFO("Found exact mode match (%dx%d%s@%d)", connector->modes[i].hdisplay, connector->modes[i].vdisplay, interlaced?"i":"", req_rate)
				return best_mode;
			}
		}
	}
	if(best_mode == NULL) {
		best_mode = &connector->modes[0];
		bool interlaced = (best_mode->flags & DRM_MODE_FLAG_INTERLACE) > 0;
		_LOG_WARN("Didn't find a match for mode, using %dx%d%s@%d", best_mode->hdisplay, best_mode->vdisplay, interlaced?"i":"", drm_mode_refresh_rate(best_mode))
	}
	else {
		bool interlaced = (best_mode->flags & DRM_MODE_FLAG_INTERLACE) > 0;
		_LOG_WARN("Found inexact mode match (%dx%d%s@%d)", best_mode->hdisplay, best_mode->vdisplay, interlaced?"i":"", drm_mode_refresh_rate(best_mode))
	}

	return best_mode;
}


static bool drm_create_fb(int drm_fd, uint32_t width, uint32_t height, us_drm_dumb_framebuffer_t *fb) {
	int ret;

	struct drm_mode_create_dumb create = {
		.width = width,
		.height = height,
		.bpp = 32,
	};

	ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
	if (ret < 0) {
		_LOG_PERROR("drm_create_fb: DRM_IOCTL_MODE_CREATE_DUMB");
		return false;
	}

	fb->height = height;
	fb->width = width;
	fb->stride = create.pitch;
	fb->handle = create.handle;
	fb->size = create.size;

	uint32_t handles[4] = { fb->handle };
	uint32_t strides[4] = { fb->stride };
	uint32_t offsets[4] = { 0 };

	ret = drmModeAddFB2(drm_fd, width, height, DRM_FORMAT_XRGB8888, handles, strides, offsets, &fb->id, 0);
	if (ret < 0) {
		_LOG_PERROR("drm_create_fb: drmModeAddFB2");
		goto error_dumb;
	}

	struct drm_mode_map_dumb map = { .handle = fb->handle };
	ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
	if (ret < 0) {
		_LOG_PERROR("drm_create_fb: DRM_IOCTL_MODE_MAP_DUMB");
		goto error_fb;
	}

	fb->data = mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, map.offset);
	if (!fb->data) {
		_LOG_PERROR("drm_create_fb: mmap");
		goto error_fb;
	}

	memset(fb->data, 0x00, fb->size);

	return true;

error_fb:
	drmModeRmFB(drm_fd, fb->id);

error_dumb:
	;
	struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
	drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
	return false;
}
static void drm_destroy_fb(int drm_fd, us_drm_dumb_framebuffer_t *fb) {
	munmap(fb->data, fb->size);
	drmModeRmFB(drm_fd, fb->id);
	struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
	drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}


static uint32_t drm_find_crtc(us_drmstream_t* state, drmModeRes *res, drmModeConnector *connector, uint32_t *taken_crtcs) {
	us_drmstream_runtime_t* const run = state->run;

	for (int i = 0; i < connector->count_encoders; ++i) {
		drmModeEncoder *enc = drmModeGetEncoder(run->fd, connector->encoders[i]);
		if (!enc)
			continue;

		for (int i = 0; i < res->count_crtcs; ++i) {
			uint32_t bit = 1 << i;
			// Not compatible
			if ((enc->possible_crtcs & bit) == 0)
				continue;

			// Already taken
			if (*taken_crtcs & bit)
				continue;

			drmModeFreeEncoder(enc);
			*taken_crtcs |= bit;
			return res->crtcs[i];
		}

		drmModeFreeEncoder(enc);
	}

	return 0;
}
static bool drm_find_connector(us_drmstream_t* state, drmModeRes* res, drmModeConnector** connector, drmModeModeInfo** mode_ptr, uint32_t* crtc_id) {
	us_drmstream_runtime_t* const run = state->run;

	uint32_t taken_crtcs = 0;

	for (int i = 0; i < res->count_connectors; ++i) {
		*connector = drmModeGetConnector(run->fd, res->connectors[i]);
		if (!connector) { continue; }
		
		if((*connector)->connection != DRM_MODE_CONNECTED) {
			_LOG_WARN("Found display (%s) but it's disconnected", drm_conn_str((*connector)->connector_type))
			goto cleanup;
		}

		if ((*connector)->count_modes == 0) {
			_LOG_WARN("Found display (%s) it has no valid modes", drm_conn_str((*connector)->connector_type))
			goto cleanup;
		}

		*crtc_id = drm_find_crtc(state, res, *connector, &taken_crtcs);
		if (!crtc_id) {
			_LOG_WARN("Could not find CRTC for display %s", drm_conn_str((*connector)->connector_type))
			goto cleanup;
		}

		drm_print_modes(*connector);

		_LOG_INFO("Using display %s and with CRTC %"PRIu32"", drm_conn_str((*connector)->connector_type), *crtc_id)

		*mode_ptr = drm_find_best_mode(*connector, state->requested_width, state->requested_height, state->requested_rate, false);

		bool interlaced = ((*mode_ptr)->flags & DRM_MODE_FLAG_INTERLACE) > 0;
		_LOG_INFO("Using mode %"PRIu32"x%"PRIu32"@%"PRIu32" %s", (*mode_ptr)->hdisplay, (*mode_ptr)->vdisplay, drm_mode_refresh_rate(*mode_ptr), interlaced?"(interlaced)":"");

		return true;

	cleanup:
		drmModeFreeConnector(*connector);
	}
	
	return false;
}


us_drmstream_t *us_drmstream_init() {
	us_drmstream_runtime_t *run;
	US_CALLOC(run, 1);

	atomic_init(&run->stop, false);
	US_CALLOC(run->front, 1);
	US_CALLOC(run->back, 1);

	run->fps = us_fpsi_init("drmstream", false);
	run->ft = us_frametext_init();


	us_drmstream_t *drmstream;
	US_CALLOC(drmstream, 1);
	drmstream->run = run;
	drmstream->ring_capacity = 6;
	
	drmstream->card_path = "/dev/dri/card1";

	drmstream->requested_width = 1920;
	drmstream->requested_height = 1080;
	drmstream->requested_rate = 30000;

	return drmstream;
}

void us_drmstream_destroy(us_drmstream_t *drmstream) {
	us_drmstream_runtime_t* const run = drmstream->run;

	us_fpsi_destroy(drmstream->run->fps);

	drm_destroy_fb(run->fd, run->front);
	drm_destroy_fb(run->fd, run->back);
	
	drmModeCrtc* crtc = run->old_crtc;
	if (crtc) {
		drmModeSetCrtc(run->fd, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &run->connector_id, 1, &crtc->mode);
		drmModeFreeCrtc(crtc);
	}

	close(run->fd);

	free(run->front);
	free(run->back);

	US_RING_DELETE_WITH_ITEMS(run->frame_ring, free);

	free(run);
	free(drmstream);
}


void drm_do_pageflip(us_drmstream_t* state, void* userdata) {
	us_drmstream_runtime_t* const run = state->run;

	if (drmModePageFlip(run->fd, run->crtc_id, run->front->id, DRM_MODE_PAGE_FLIP_EVENT, userdata) < 0) {
		_LOG_PERROR("drmModePageFlip");
	}
}


static void page_flip_handler(int drm_fd, unsigned sequence, unsigned tv_sec, unsigned tv_usec, void *data) {
	(void)sequence;
	(void)tv_sec;
	(void)tv_usec;

	us_drmstream_t *drmstream = data;
	us_drmstream_runtime_t* const run = drmstream->run;

	// Try to get a frame from the ringbuffer for 1sec
	int idx = us_ring_consumer_acquire(run->frame_ring, 1);
	if(idx == -1) {
		// If 1sec elapsed, consider that something got disconencted
		_LOG_WARN("Ring buffer underun - Is the sink connected ?")

		// Unpach the RGB24 error message to both framebuffers (don't care about glitches)
		fast_unpack(run->front->data, run->ft->frame->data, run->back->width * run->back->height);
		fast_unpack(run->back->data, run->ft->frame->data, run->back->width * run->back->height);

	} else {
		// Got a frame in less than 1sec, copy it to the backbuffer
		memcpy(run->back->data, run->frame_ring->items[idx], run->back->size);
		// Release the frame
		us_ring_consumer_release(run->frame_ring, idx);

		//Update the FPS counter
		us_fpsi_update(run->fps, true, NULL);
	}

	// Continue pageflips
	drm_do_pageflip(drmstream, drmstream);

	// Swap the buffers
	us_drm_dumb_framebuffer_t* old_back = run->back;
	us_drm_dumb_framebuffer_t* old_front = run->front;
	run->front = old_back;
	run->back = old_front;
}


bool us_drmstream_produce_buffer(us_drmstream_t *drmstream, unsigned char* rgba_data, size_t len) {
	us_drmstream_runtime_t *const run = drmstream->run;
	
	int idx = us_ring_producer_acquire(run->frame_ring, 1);
	if(idx == -1) {
		_LOG_WARN("Couldn't write frame, is drm too slow?")
		return false;
	}

	memcpy(run->frame_ring->items[idx], rgba_data, US_MIN(len, run->back->size));

	us_ring_producer_release(run->frame_ring, idx);

	return true;
}


void us_drmstream_loop(us_drmstream_t *drmstream) {
	us_drmstream_runtime_t *const run = drmstream->run;

    run->fd = open(drmstream->card_path, O_RDWR | O_NONBLOCK);
	
	drmModeRes *res = drmModeGetResources(run->fd);
	if(!res) {
		_LOG_PERROR("Failed to get drmMode ressources!")
		exit(1);
	}

	_LOG_INFO("Found %d connectors", res->count_connectors)

	drmModeConnector* connector;
	drmModeModeInfo* mode;
	uint32_t tmp_crtc_id;
	if(!drm_find_connector(drmstream, res, &connector, &mode, &tmp_crtc_id)) {
		_LOG_PERROR("Failed to find a connector!")
		exit(1);
	}

	run->crtc_id = tmp_crtc_id;
	run->connector_id = connector->connector_id;

	if (!drm_create_fb(run->fd, mode->hdisplay, mode->vdisplay, run->front)) {
		exit(1);
	}
	_LOG_DEBUG("Created frambuffer with ID %"PRIu32"", run->front->id);

	if (!drm_create_fb(run->fd, mode->hdisplay, mode->vdisplay, run->back)) {
		exit(1);
	}
	_LOG_DEBUG("Created frambuffer with ID %"PRIu32"", run->back->id);

	// Save the previous CRTC configuration
	run->old_crtc = drmModeGetCrtc(run->fd, run->crtc_id);

	// Perform the modeset
	int ret = drmModeSetCrtc(run->fd, run->crtc_id, run->front->id, 0, 0, &run->connector_id, 1, mode);
	if (ret < 0) {
		_LOG_PERROR("drmModeSetCrtc");
	}

	drmModeFreeConnector(connector);
	drmModeFreeResources(res);

	_LOG_INFO("DRM Initialized");


	us_frametext_draw(run->ft, "< NO SIGNAL >", run->front->width, run->front->height);

	run->frame_ring = us_ring_init(drmstream->ring_capacity);

	for (size_t m_index = 0; m_index < run->frame_ring->capacity; ++m_index) {
		run->frame_ring->items[m_index] = malloc(run->front->size);
		us_ring_producer_release(run->frame_ring, us_ring_producer_acquire(run->frame_ring, 0));
	}
	
	drm_do_pageflip(drmstream, drmstream);


	drmEventContext context = {
		.version = DRM_EVENT_CONTEXT_VERSION,
		.page_flip_handler = page_flip_handler,
	};

	struct pollfd pollfd = {
		.fd = run->fd,
		.events = POLLIN,
	};

	while (!atomic_load(&run->stop)) {

		int ret = poll(&pollfd, 1, 5000);
		if (ret < 0 && errno != EAGAIN) {
			_LOG_PERROR("poll");
			break;
		}

		if (pollfd.revents & POLLIN) {
			if (drmHandleEvent(run->fd, &context) < 0) {
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



