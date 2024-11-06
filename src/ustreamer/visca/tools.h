#pragma once

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)



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
