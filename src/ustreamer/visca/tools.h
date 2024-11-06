#pragma once

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#include "../../libs/logging.h"

#define _LOG_ERROR(x_msg, ...)		US_LOG_ERROR("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_PERROR(x_msg, ...)		US_LOG_PERROR("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_WARN(x_msg, ...)		US_LOG_WARN("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_INFO(x_msg, ...)		US_LOG_INFO("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_VERBOSE(x_msg, ...)	US_LOG_VERBOSE("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_DEBUG(x_msg, ...)		US_LOG_DEBUG("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_TRACE(x_msg, ...)		US_LOG_TRACE("VISCA: " x_msg, ##__VA_ARGS__)



#define vs_v4l2_dev_fd viscaserver->stream->cap->run->fd

#define vs_req_pkt_buf_len viscaserver->run->packet.buffer_len
#define vs_req_pkt_buf viscaserver->run->packet.buffer
#define vs_res_pkt_buf_len viscaserver->run->packet.reply_buffer_len
#define vs_res_pkt_buf viscaserver->run->packet.reply_buffer

inline int clamp(int d, int min, int max) {
  const int t = d < min ? min : d;
  return t > max ? max : t;
}

inline char* buffer_to_hex(char* buffer, int len) {
	char* str = (char*)malloc(3 * len);

    const char * hex = "0123456789ABCDEF";
    char* pin = buffer;
    char* pout = str;

	for (int i = 0; i < len; ++i) {
        *pout++ = hex[(*pin>>4)&0xF];
        *pout++ = hex[(*pin++)&0xF];
        *pout++ = ' ';
	}
    *pout++ = hex[(*pin>>4)&0xF];
    *pout++ = hex[(*pin)&0xF];
    *pout = 0;


	return str;
}


#define INRANGE(x, min, max) (x) >= (min) && (x) <= (max)

#define ONEOF2(x, a, b) (x) == (a) || (x) == (b)
#define ONEOF3(x, a, b, c) ONEOF2(x, a, b) || (x) == (c)
#define ONEOF4(x, a, b, c, d) ONEOF3(x, a, b, c) || (x) == (d)
#define ONEOF5(x, a, b, c, d, e) ONEOF4(x, a, b, c, d) || (x) == (e)
#define ONEOF6(x, a, b, c, d, e, f) ONEOF5(x, a, b, c, d, e) || (x) == (f)

#define IS_VISCA_QUAD_I16(buf, offset) (buf[offset] & 0xF0) == 0 && (buf[offset+1] & 0xF0) == 0 && (buf[offset+2] & 0xF0) == 0 && (buf[offset+3] & 0xF0) == 0


#define VISCASERVER_PACKET_ASSERT(error, check)	if(!(check)) { \
													_LOG_ERROR("Packet assertion failed (in %s:%d in %s)", __FILENAME__, __LINE__, __func__); \
													us_viscaserver_reply_error_##error(viscaserver); \
													return false; \
												}

#define VISCASERVER_PACKET_ASSERT_TERMINATOR() VISCASERVER_PACKET_ASSERT(syntax, vs_req_pkt_buf[vs_req_pkt_buf_len-1] == 0xFF);
#define VISCASERVER_PACKET_ASSERT_ADDRESS() VISCASERVER_PACKET_ASSERT(syntax, vs_req_pkt_buf[0] == 0x81);
#define VISCASERVER_PACKET_ASSERT_MINSIZE(excpected_size) VISCASERVER_PACKET_ASSERT(syntax, vs_req_pkt_buf_len >= excpected_size);
#define VISCASERVER_PACKET_ASSERT_SIZE(excpected_size) VISCASERVER_PACKET_ASSERT(syntax, vs_req_pkt_buf_len == excpected_size);


#define VISCASERVER_IOCTL_ASSERT(error, fd, request, control)	if(ioctl(fd, request, &control) == -1) { \
																	char* errno_msg = us_errno_to_string(errno); \
																	_LOG_ERROR("ioctl request to %d failed: \"%s\" (in %s:%d in %s)", fd, errno_msg, __FILENAME__, __LINE__, __func__); \
																	free(errno_msg); \
																	us_viscaserver_reply_error_##error(viscaserver); \
																	return false; \
																}

#define VISCA_4BYTES_TO_INT16(buffer, offset) (((buffer[offset] & 0x0F) << 0) | ((buffer[offset+1] & 0x0F) << 4) | ((buffer[offset+2] & 0x0F) << 8) | ((buffer[offset+3] & 0x0F) << 12))
