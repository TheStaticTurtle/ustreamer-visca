#include "errors.h"
#include "../tools.h"

#include "../protocol.h"
#include "../net.h"

bool us_viscaserver_reply_error(us_viscaserver_s *viscaserver, char error_code) {
	vs_res_pkt_buf_len = 0;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x90;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_RESPONSE_ERROR;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = error_code;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_TERMINATOR;
	return us_viscaserver_send_reply_buffer(viscaserver); 
}
bool us_viscaserver_reply_error_syntax(us_viscaserver_s *viscaserver) { return us_viscaserver_reply_error(viscaserver, VISCA_ERROR_SYNTAX); } 
bool us_viscaserver_reply_error_bufferfull(us_viscaserver_s *viscaserver) { return us_viscaserver_reply_error(viscaserver, VISCA_ERROR_CMD_BUFFER_FULL); } 
bool us_viscaserver_reply_error_canceled(us_viscaserver_s *viscaserver) { return us_viscaserver_reply_error(viscaserver, VISCA_ERROR_CMD_CANCELLED); } 
bool us_viscaserver_reply_error_nosock(us_viscaserver_s *viscaserver) { return us_viscaserver_reply_error(viscaserver, VISCA_ERROR_NO_SOCKET); } 
bool us_viscaserver_reply_error_notexecutable(us_viscaserver_s *viscaserver) { return us_viscaserver_reply_error(viscaserver, VISCA_ERROR_CMD_NOT_EXECUTABLE); } 

bool us_viscaserver_reply_ack(us_viscaserver_s *viscaserver) {
	vs_res_pkt_buf_len = 0;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x90;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_RESPONSE_ACK;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_TERMINATOR;
	return us_viscaserver_send_reply_buffer(viscaserver); 
}
bool us_viscaserver_reply_completion(us_viscaserver_s *viscaserver) {
	vs_res_pkt_buf_len = 0;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x90;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_RESPONSE_COMPLETED;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_TERMINATOR;
	return us_viscaserver_send_reply_buffer(viscaserver); 
}
