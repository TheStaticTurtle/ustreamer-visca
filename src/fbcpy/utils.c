

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