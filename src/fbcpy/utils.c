#include "utils.h"

#include "../libs/logging.h"
#include <libavutil/log.h>
#include <stdio.h>

#define _LOG_ERROR(x_msg, ...)		US_LOG_ERROR("AV: " x_msg, ##__VA_ARGS__)
#define _LOG_PERROR(x_msg, ...)		US_LOG_PERROR("AV: " x_msg, ##__VA_ARGS__)
#define _LOG_WARN(x_msg, ...)		US_LOG_WARN("AV: " x_msg, ##__VA_ARGS__)
#define _LOG_INFO(x_msg, ...)		US_LOG_INFO("AV: " x_msg, ##__VA_ARGS__)
#define _LOG_VERBOSE(x_msg, ...)	US_LOG_VERBOSE("AV: " x_msg, ##__VA_ARGS__)
#define _LOG_DEBUG(x_msg, ...)		US_LOG_DEBUG("AV: " x_msg, ##__VA_ARGS__)
#define _LOG_TRACE(x_msg, ...)		US_LOG_TRACE("AV: " x_msg, ##__VA_ARGS__)

static char lib_av_log_buffer[2048];

static void libav_log_callback(void * avcl, int level, const char * fmt, va_list va) {
    if (level > av_log_get_level()) {
        return;
    }

    memset(lib_av_log_buffer, 0, 2048);
    int n = vsnprintf(lib_av_log_buffer, 2048, fmt, va);
    if(n == 0) { return; }
    lib_av_log_buffer[n-1] = 0;

    switch (level) {
        case AV_LOG_PANIC:
        case AV_LOG_FATAL:
        case AV_LOG_ERROR:
            _LOG_ERROR("%s", lib_av_log_buffer);
            break;
        case AV_LOG_WARNING:
            _LOG_WARN("%s", lib_av_log_buffer);
            break;
        case AV_LOG_INFO:
            _LOG_INFO("%s", lib_av_log_buffer);
            break;
        case AV_LOG_VERBOSE:
            _LOG_VERBOSE("%s", lib_av_log_buffer);
            break;
        case AV_LOG_DEBUG:
            _LOG_DEBUG("%s", lib_av_log_buffer);
            break;
        case AV_LOG_TRACE:
            _LOG_TRACE("%s", lib_av_log_buffer);
            break;
    }
}

void us_set_libav_log_callback() {
    av_log_set_callback(libav_log_callback);
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