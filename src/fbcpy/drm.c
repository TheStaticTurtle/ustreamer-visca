#include "drm.h"

#include "../libs/logging.h"

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#include <drm/drm_fourcc.h>

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


static uint32_t drm_find_crtc(us_drm_state_s* state, drmModeRes *res, drmModeConnector *connector, uint32_t *taken_crtcs) {
	for (int i = 0; i < connector->count_encoders; ++i) {
		drmModeEncoder *enc = drmModeGetEncoder(state->fd, connector->encoders[i]);
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

static bool drm_find_connector(us_drm_state_s* state, drmModeRes* res, drmModeConnector** connector, drmModeModeInfo** mode_ptr, uint32_t* crtc_id) {
	uint32_t taken_crtcs = 0;

	for (int i = 0; i < res->count_connectors; ++i) {
		*connector = drmModeGetConnector(state->fd, res->connectors[i]);
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


static bool drm_create_fb(int drm_fd, uint32_t width, uint32_t height, struct dumb_framebuffer *fb) {
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
static void drm_destroy_fb(int drm_fd, struct dumb_framebuffer *fb) {
	munmap(fb->data, fb->size);
	drmModeRmFB(drm_fd, fb->id);
	struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
	drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}



us_drm_state_s* drm_init() {

	us_drm_state_s *state;
	US_CALLOC(state, 1);

	state->card_path = "/dev/dri/card1";

	state->requested_width = 1920;
	state->requested_height = 1080;
	state->requested_rate = 30000;

	return state;
}

void drm_destroy(us_drm_state_s* state) {

	drm_destroy_fb(state->fd, &state->fb[0]);
	drm_destroy_fb(state->fd, &state->fb[1]);
	
	drmModeCrtc* crtc = state->old_crtc;
	if (crtc) {
		drmModeSetCrtc(state->fd, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &state->connector_id, 1, &crtc->mode);
		drmModeFreeCrtc(crtc);
	}

	close(state->fd);

	free(state);
}



void drm_do_pageflip(us_drm_state_s* state, void* userdata) {
	if (drmModePageFlip(state->fd, state->crtc_id, state->front->id, DRM_MODE_PAGE_FLIP_EVENT, userdata) < 0) {
		_LOG_PERROR("drmModePageFlip");
	}
}


void drm_setup(us_drm_state_s* state) {

    state->fd = open(state->card_path, O_RDWR | O_NONBLOCK);

	drmModeRes *res = drmModeGetResources(state->fd);
	if(!res) {
		_LOG_PERROR("Failed to get drmMode ressources!")
		exit(1);
	}


	_LOG_INFO("Found %d connectors", res->count_connectors)

	drmModeConnector* connector;
	drmModeModeInfo* mode;
	uint32_t tmp_crtc_id;
	if(!drm_find_connector(state, res, &connector, &mode, &tmp_crtc_id)) {
		_LOG_PERROR("Failed to find a connector!")
		exit(1);
	}
	
	state->crtc_id = tmp_crtc_id;
	state->connector_id = connector->connector_id;


	if (!drm_create_fb(state->fd, mode->hdisplay, mode->vdisplay, &state->fb[0])) {
		exit(1);
	}
	_LOG_DEBUG("Created frambuffer with ID %"PRIu32"", state->fb[0].id);

	if (!drm_create_fb(state->fd, mode->hdisplay, mode->vdisplay, &state->fb[1])) {
		drm_destroy_fb(state->fd, &state->fb[0]);
		exit(1);
	}
	_LOG_DEBUG("Created frambuffer with ID %"PRIu32"", state->fb[1].id);


	state->front = &state->fb[0];
	state->back = &state->fb[1];

	// Save the previous CRTC configuration
	state->old_crtc = drmModeGetCrtc(state->fd, state->crtc_id);

	// Perform the modeset
	int ret = drmModeSetCrtc(state->fd, state->crtc_id, state->fb->id, 0, 0, &state->connector_id, 1, mode);
	if (ret < 0) {
		_LOG_PERROR("drmModeSetCrtc");
	}


	drmModeFreeConnector(connector);
	drmModeFreeResources(res);

	_LOG_INFO("Initialized");
}