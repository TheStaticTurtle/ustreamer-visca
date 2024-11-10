#include "drm.h"

#include <drm/drm_fourcc.h>
#include <drm/drm_mode.h>
#include <libavcodec/codec_id.h>
#include <libavutil/frame.h>
#include <stdbool.h>
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
static uint32_t drm_find_crtc(us_drm_t* state, drmModeRes *res, drmModeConnector *connector, uint32_t *taken_crtcs) {
	us_drm_runtime_t* const run = state->run;

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
static bool drm_find_connector(us_drm_t* state, drmModeRes* res, drmModeConnector** connector, drmModeModeInfo** mode_ptr, uint32_t* crtc_id) {
	us_drm_runtime_t* const run = state->run;

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

		_LOG_DEBUG("Connector has %d modes:", (*connector)->count_modes);
		for(int i = 0; i < (*connector)->count_modes; i++) {
			_LOG_DEBUG(
				"  - %dx%d%s %dmHz", 
				(*connector)->modes[i].hdisplay, 
				(*connector)->modes[i].hdisplay, 
				((*connector)->modes[i].flags & DRM_MODE_FLAG_INTERLACE) ? "i" : "",
				drm_mode_refresh_rate(&(*connector)->modes[i])
			);
		}

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


us_drm_t *us_drm_init() {
	us_drm_runtime_t *run;
	US_CALLOC(run, 1);

	atomic_init(&run->stop, false);

	run->ft = us_frametext_init();
	
	us_drm_t *drm;
	US_CALLOC(drm, 1);
	drm->run = run;
	drm->vsync_timeout = 5;
	
	drm->card_path = "/dev/dri/card1";

	drm->requested_width = 1920;
	drm->requested_height = 1080;
	drm->requested_rate = 30000;

	return drm;
}
void us_drm_destroy(us_drm_t *drm) {
	us_drm_runtime_t* const run = drm->run;

	drmModeCrtc* crtc = run->old_crtc;
	if (crtc) {
		drmModeSetCrtc(run->fd, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &run->connector_id, 1, &crtc->mode);
		drmModeFreeCrtc(crtc);
	}

	for(int i=0; i < DRM_BUFFER_COUNT; i++) {
		drm_destroy_fb(run->fd, &run->buffers[i]);
	}

	close(run->fd);

	free(run);
	free(drm);
}


static void page_flip_handler(int drm_fd, unsigned sequence, unsigned tv_sec, unsigned tv_usec, void *data) {
	us_drm_t *drm = data;
	us_drm_runtime_t* const run = drm->run;

	run->has_vsync = true;

	_LOG_TRACE("Got VSync");
}
int us_drm_wait_vsync(us_drm_t *drm) {
	us_drm_runtime_t *const run = drm->run;

	assert(run->fd >= 0);
	
	if (run->has_vsync) {
		return 0;
	}

	struct timeval timeout = {.tv_sec = drm->vsync_timeout};
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(run->fd, &fds);

	_LOG_TRACE("Calling select() for VSync ...");
	const int result = select(run->fd + 1, &fds, NULL, NULL, &timeout);
	if (result < 0) {
		_LOG_PERROR("us_drm_wait_vsync: Can't select(%d) device for VSync", run->fd);
		return -1;
	} else if (result == 0) {
		_LOG_ERROR("Device timeout while waiting VSync");
		return -1;
	}
	
	drmEventContext ctx = {
		.version = DRM_EVENT_CONTEXT_VERSION,
		.page_flip_handler = page_flip_handler,
	};
	_LOG_TRACE("Handling DRM event (maybe VSync) ...");
	if (drmHandleEvent(run->fd, &ctx) < 0) {
		_LOG_PERROR("Can't handle DRM event");
		return -1;
	}

	return 0;
}
int us_drm_write_buffer(us_drm_t *drm, unsigned char* rgba_data, size_t len) {
	us_drm_runtime_t *const run = drm->run;

	if(len != run->buffers[run->write_buffer].size) {
		_LOG_ERROR("us_drm_write_buffer: Buffer len & Data length are different");
		return -1;
	}

	memcpy(run->buffers[run->write_buffer].data, rgba_data, len);
	
	run->has_vsync = false;
	
	if (drmModePageFlip(run->fd, run->crtc_id, run->buffers[run->write_buffer].id, DRM_MODE_PAGE_FLIP_EVENT, drm) < 0) {
		_LOG_PERROR("us_drm_write_buffer: Failed to execute page flip: ");
	}

	run->write_buffer++;
	if(run->write_buffer >= DRM_BUFFER_COUNT) { run->write_buffer = 0; }

	return true;
}
int us_drm_vsynced_buffer_write(us_drm_t *drm, unsigned char* rgba_data, size_t len) {
	int ret = us_drm_wait_vsync(drm);
	if(ret < 0) { return ret; }

	return us_drm_write_buffer(drm, rgba_data, len);
}


void us_drm_setup(us_drm_t *drm) {
	us_drm_runtime_t *const run = drm->run;

    run->fd = open(drm->card_path, O_RDWR | O_NONBLOCK);
	
	drmModeRes *res = drmModeGetResources(run->fd);
	if(!res) {
		_LOG_PERROR("Failed to get drmMode ressources!")
		exit(1);
	}

	_LOG_INFO("Found %d connectors", res->count_connectors)

	drmModeConnector* connector;
	drmModeModeInfo* mode;
	uint32_t tmp_crtc_id;
	if(!drm_find_connector(drm, res, &connector, &mode, &tmp_crtc_id)) {
		_LOG_PERROR("Failed to find a connector!")
		exit(1);
	}

	run->crtc_id = tmp_crtc_id;
	run->connector_id = connector->connector_id;


	us_frametext_draw(run->ft, "< / >", mode->hdisplay, mode->vdisplay);


	// Save the previous CRTC configuration
	run->old_crtc = drmModeGetCrtc(run->fd, run->crtc_id);


	for(int i=0; i < DRM_BUFFER_COUNT; i++) {
		if (!drm_create_fb(run->fd, mode->hdisplay, mode->vdisplay, &run->buffers[i])) {
			exit(1);
		}
		_LOG_DEBUG("Created frambuffer with ID %"PRIu32"", run->buffers[i].id);

		// Pre-Write the "blank" disconnected text
		fast_unpack(run->buffers[i].data, run->ft->frame->data, run->buffers[i].height*run->buffers[i].width);

		// Perform the modeset
		int ret = drmModeSetCrtc(run->fd, run->crtc_id, run->buffers[i].id, 0, 0, &run->connector_id, 1, mode);
		if (ret < 0) {
			_LOG_PERROR("drmModeSetCrtc");
		}
	}

	if (drmModePageFlip(run->fd, run->crtc_id, run->buffers[0].id, DRM_MODE_PAGE_FLIP_EVENT, drm) < 0) {
		_LOG_PERROR("Failed to execute page flip");
	}

	drmModeFreeConnector(connector);
	drmModeFreeResources(res);

	_LOG_INFO("DRM Initialized");
}
