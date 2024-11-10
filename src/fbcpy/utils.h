#pragma once

#include <stdint.h>

#include <xf86drmMode.h>

void us_set_libav_log_callback();

void fast_unpack(unsigned char* rgba, const unsigned char* rgb, const int count);

static const char *drm_conn_str(uint32_t conn_type);
static int drm_mode_refresh_rate(drmModeModeInfo *mode);