#include "viscaserver.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "../../libs/tools.h"
#include "../../libs/threading.h"
#include "../../libs/logging.h"
#include "../stream.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "protocol.h"
#include "tools.h"

#define _LOG_ERROR(x_msg, ...)	US_LOG_ERROR("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_PERROR(x_msg, ...)	US_LOG_PERROR("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_INFO(x_msg, ...)		US_LOG_INFO("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_VERBOSE(x_msg, ...)	US_LOG_VERBOSE("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_DEBUG(x_msg, ...)	US_LOG_DEBUG("VISCA: " x_msg, ##__VA_ARGS__)


#define vs_v4l2_dev_fd viscaserver->stream->cap->run->fd

#define vs_req_pkt_buf_len viscaserver->run->packet.buffer_len
#define vs_req_pkt_buf viscaserver->run->packet.buffer
#define vs_res_pkt_buf_len viscaserver->run->packet.reply_buffer_len
#define vs_res_pkt_buf viscaserver->run->packet.reply_buffer


bool us_viscaserver_wait_for_packet(us_viscaserver_s *viscaserver);


us_viscaserver_s *us_viscaserver_init(us_stream_s *stream) {
	us_viscaserver_s *viscaserver;
	US_CALLOC(viscaserver, 1);
	viscaserver->stream = stream;

	viscaserver->host = "127.0.0.1";
	viscaserver->port = 1259;

	us_viscaserver_runtime_s *run;
	US_CALLOC(run, 1);

	atomic_init(&run->stop, false);

	// socket address structure specific to IPv4
	memset(&run->sin, 0, sizeof(run->sin));
	run->sin.sin_family = AF_INET;
	run->sin.sin_addr.s_addr = htonl(INADDR_ANY);; //inet_addr(viscaserver->host); 
	run->sin.sin_port = htons(viscaserver->port);

	viscaserver->run = run;

	return viscaserver;
}
void us_viscaserver_destroy(us_viscaserver_s *viscaserver) {
	free(viscaserver->run);
	free(viscaserver);
}

void us_viscaserver_loop(us_viscaserver_s *viscaserver) {
	us_viscaserver_runtime_s *const run = viscaserver->run;

	run->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (run->socket_fd == -1) {
		_LOG_ERROR("socket failed")
		exit(1);
	}

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	if (setsockopt(run->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv)) < 0) {
		_LOG_ERROR("Failed to set socket timeout");
		exit(1);
	}

	int optval = 1;
	if (setsockopt(run->socket_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0) {
		_LOG_ERROR("Failed to set socket reuse");
		exit(1);
	}
		   
    if (bind(run->socket_fd, (struct sockaddr *)&run->sin, sizeof(run->sin)) != 0)  { 
		_LOG_ERROR("Bind failed")
		exit(1);
	}
	
	_LOG_INFO("Successfuly bound to %s:%d", viscaserver->host, viscaserver->port);
	_LOG_INFO("Running ...");


	while (!atomic_load(&run->stop)) {
		us_viscaserver_wait_for_packet(viscaserver);
	}

	_LOG_INFO("Finished");
}
void us_viscaserver_loop_break(us_viscaserver_s *viscaserver) {
	atomic_store(&viscaserver->run->stop, true);
}



bool us_viscaserver_send_reply_buffer(us_viscaserver_s *viscaserver) { 
	us_viscaserver_runtime_s *const run = viscaserver->run;
	us_viscaserver_packet *const packet = &run->packet;

	return sendto( 
		run->socket_fd, 
		vs_res_pkt_buf, 
		vs_res_pkt_buf_len,  
		0,
		(struct sockaddr *)&run->packet.peeraddr,
		run->packet.peeraddr_len
	) > 0; 
}

bool us_viscaserver_reply_error(us_viscaserver_s *viscaserver, char error_code) {
	vs_res_pkt_buf_len = 0;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x90;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x60;
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
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x40;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_TERMINATOR;
	return us_viscaserver_send_reply_buffer(viscaserver); 
}
bool us_viscaserver_reply_completion(us_viscaserver_s *viscaserver) {
	vs_res_pkt_buf_len = 0;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x90;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x50;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_TERMINATOR;
	return us_viscaserver_send_reply_buffer(viscaserver); 
}




bool us_viscaserver_handle_command(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_inquery(us_viscaserver_s *viscaserver);

bool us_viscaserver_wait_for_packet(us_viscaserver_s *viscaserver) {
	us_viscaserver_runtime_s *const run = viscaserver->run;

	memset(&run->packet, 0, sizeof(run->packet));

	run->packet.peeraddr_len = sizeof(struct sockaddr_storage);
    vs_req_pkt_buf_len = recvfrom(run->socket_fd, vs_req_pkt_buf, VISCASERVER_BUFFER_SIZE, 0, (struct sockaddr *) &run->packet.peeraddr, &run->packet.peeraddr_len);
    if (vs_req_pkt_buf_len < 1) { return false; }

	printf("Recevied data len=%d data=", vs_req_pkt_buf_len);
	for (int i = 0; i < vs_req_pkt_buf_len; ++i) printf("%02x ", vs_req_pkt_buf[i]);
	printf("\n");

	VISCASERVER_PACKET_ASSERT_MINSIZE(2)

	VISCASERVER_PACKET_ASSERT_ADDRESS()

	VISCASERVER_PACKET_ASSERT_TERMINATOR()

	if(vs_req_pkt_buf[1] == VISCA_COMMAND) 		{ return us_viscaserver_handle_command(viscaserver); }
	else if(vs_req_pkt_buf[1] == VISCA_INQUIRY)	{ return us_viscaserver_handle_inquery(viscaserver); }
	else { return false; }
}


bool us_viscaserver_handle_command_cam1_power(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_command_cam1_zoom(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_command_cam1_zoomdirect(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_command_cam1_focus(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_command_cam1_focusdirect(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_command_cam1_focusauto(us_viscaserver_s *viscaserver);

bool us_viscaserver_handle_command_pantilt_drive(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_command_pantilt_absolute(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_command_pantilt_relative(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_command_pantilt_home(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_command_pantilt_reset(us_viscaserver_s *viscaserver);

bool us_viscaserver_handle_command(us_viscaserver_s *viscaserver) {
	us_viscaserver_runtime_s *const run = viscaserver->run;

	VISCASERVER_PACKET_ASSERT_MINSIZE(3)
	
	if(vs_req_pkt_buf[2] == VISCA_CATEGORY_CAMERA1) {
		VISCASERVER_PACKET_ASSERT_MINSIZE(4)

		//if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_POWER) 							{}
		if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_ZOOM)								{ return us_viscaserver_handle_command_cam1_zoom(viscaserver); }
		else if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_ZOOM_DIRECT) 				{ return us_viscaserver_handle_command_cam1_zoomdirect(viscaserver); }
		else if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_FOCUS) 						{ return us_viscaserver_handle_command_cam1_focus(viscaserver); }
		else if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_FOCUS_DIRECT)				{ return us_viscaserver_handle_command_cam1_focusdirect(viscaserver); }
		else if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_FOCUS_AUTO) 					{ return us_viscaserver_handle_command_cam1_focusauto(viscaserver); }
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_FOCUS_LOCK) 		{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_WB) 				{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_WB_ONEPUSH)		{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_RGAIN) 			{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_RGAIN_DIRECT) 	{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_BGAIN) 			{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_BGAIN_DIRECT) 	{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_COLORTEMP) 		{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_COLORTEMP_DIRECT) {}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_AE) 				{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_IRIS) 			{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_IRIS_DIRECT)		{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_SHUTTER) 			{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_SHUTTER_DIRECT) 	{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_GAIN) 			{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_GAIN_LIMIT) 		{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_EXPCOMP_ONOFF) 	{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_EXPCOMP) 			{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_EXPCOMP_DIRECT) 	{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_BACKLIGHT) 		{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_FLICKER) 			{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_BRIGHTNESS)		{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_CONTRAST) 		{}
		// else if(run->rx_packet.buffer[3] == VISCA_CONTROL_CAM1_FLIP) 			{}
	}
	else if(vs_req_pkt_buf[2] == VISCA_CATEGORY_PANTILT) {
		VISCASERVER_PACKET_ASSERT_MINSIZE(4)

		if(vs_req_pkt_buf[3] == VISCA_CONTROL_PANTILT_DRIVE) 						{ return us_viscaserver_handle_command_pantilt_drive(viscaserver); }
		else if(vs_req_pkt_buf[3] == VISCA_CONTROL_PANTILT_ABSOLUTE) 				{ return us_viscaserver_handle_command_pantilt_absolute(viscaserver); }
		//else if(vs_req_pkt_buf[3] == VISCA_CONTROL_PANTILT_RELATIVE) 				{}
		else if(vs_req_pkt_buf[3] == VISCA_CONTROL_PANTILT_HOME) 					{ return us_viscaserver_handle_command_pantilt_home(viscaserver); }
		else if(vs_req_pkt_buf[3] == VISCA_CONTROL_PANTILT_RESET) 					{ return us_viscaserver_handle_command_pantilt_reset(viscaserver); }
	}

	_LOG_ERROR("Could find the handler for the following packet: ");
	for (int i = 0; i < vs_req_pkt_buf_len; ++i) printf(" %02x", vs_req_pkt_buf[i]);
	printf("\n");
	return false;
}

bool us_viscaserver_handle_inquery_zoom_pos(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_inquery_autofocus(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_inquery_focus_pos(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_inquery_pantilt_pos(us_viscaserver_s *viscaserver);

bool us_viscaserver_handle_inquery(us_viscaserver_s *viscaserver) {
	us_viscaserver_runtime_s *const run = viscaserver->run;

	VISCASERVER_PACKET_ASSERT_MINSIZE(3)

	if(vs_req_pkt_buf[2] == VISCA_CATEGORY_CAMERA1) {
		VISCASERVER_PACKET_ASSERT_MINSIZE(4)

		if(vs_req_pkt_buf[3] == 0x47) 						{ return us_viscaserver_handle_inquery_zoom_pos(viscaserver); }
		else if(vs_req_pkt_buf[3] == 0x38) 					{ return us_viscaserver_handle_inquery_autofocus(viscaserver); }
		else if(vs_req_pkt_buf[3] == 0x48) 					{ return us_viscaserver_handle_inquery_focus_pos(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x35) 			{ return us_viscaserver_handle_inquery_wb_mode(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x43) 			{ return us_viscaserver_handle_inquery_rgain(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x44) 			{ return us_viscaserver_handle_inquery_bgain(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x20) 			{ return us_viscaserver_handle_inquery_color_temp(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x39) 			{ return us_viscaserver_handle_inquery_ae_mode(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x4A) 			{ return us_viscaserver_handle_inquery_shutter_pos(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x4B) 			{ return us_viscaserver_handle_inquery_iris_pos(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x4D) 			{ return us_viscaserver_handle_inquery_bright_pos(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x3E) 			{ return us_viscaserver_handle_inquery_expcomp_mode(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x4E) 			{ return us_viscaserver_handle_inquery_expcomp_pos(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x33) 			{ return us_viscaserver_handle_inquery_backlight_mode(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x50) 			{ return us_viscaserver_handle_inquery_noise2d(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x53) 			{ return us_viscaserver_handle_inquery_noise2d_level(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x54) 			{ return us_viscaserver_handle_inquery_noise3d_level(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x55) 			{ return us_viscaserver_handle_inquery_flicker_mode(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0xA1) 			{ return us_viscaserver_handle_inquery_brightness(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0xA2) 			{ return us_viscaserver_handle_inquery_contrast(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0xA4) 			{ return us_viscaserver_handle_inquery_filp(viscaserver); }
	}
	else if(vs_req_pkt_buf[2] == VISCA_CATEGORY_PANTILT) {
		VISCASERVER_PACKET_ASSERT_MINSIZE(4)

		if(vs_req_pkt_buf[3] == 0x12) 						{ return us_viscaserver_handle_inquery_pantilt_pos(viscaserver); }
	}

	_LOG_ERROR("Could find the handler for the following packet: ");
	for (int i = 0; i < vs_req_pkt_buf_len; ++i) printf(" %02x", vs_req_pkt_buf[i]);
	printf("\n");
	return false;
}






bool us_viscaserver_handle_command_cam1_zoom(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(6)

	VISCASERVER_PACKET_ASSERT(syntax, INRANGE(vs_req_pkt_buf[4] >> 4, 0, 7))
	int zoom_speed = (vs_req_pkt_buf[4] >> 4) + 1;

	VISCASERVER_PACKET_ASSERT(syntax, ONEOF3(vs_req_pkt_buf[4] & 0x0F, VISCA_VALUE_CAM1_ZOOM_WIDE, VISCA_VALUE_CAM1_ZOOM_TELE, VISCA_VALUE_CAM1_ZOOM_STOP))
	int zoom_direction = (vs_req_pkt_buf[4] & 0x0F) == VISCA_VALUE_CAM1_ZOOM_WIDE ? -1 : ((vs_req_pkt_buf[4] & 0x0F) == VISCA_VALUE_CAM1_ZOOM_TELE ? 1 : 0);

	_LOG_DEBUG("Manual zoom drive zoom%+d", zoom_speed*zoom_direction);

	struct v4l2_control v4l2ctrl_zoom;
	v4l2ctrl_zoom.id = V4L2_CID_ZOOM_ABSOLUTE;

	VISCASERVER_IOCTL_ASSERT(notexecutable, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctrl_zoom)

	v4l2ctrl_zoom.value += zoom_speed * zoom_direction;

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_zoom);

	return true;
}
bool us_viscaserver_handle_command_cam1_zoomdirect(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(9)
	
	VISCASERVER_PACKET_ASSERT(syntax, IS_VISCA_QUAD_I16(vs_req_pkt_buf, 4))
	int16_t zoom = VISCA_4BYTES_TO_INT16(vs_req_pkt_buf, 4);

	_LOG_DEBUG("Zooming to zoom=%d", zoom);

	struct v4l2_control v4l2ctrl_zoom;
	v4l2ctrl_zoom.id = V4L2_CID_ZOOM_ABSOLUTE;
	v4l2ctrl_zoom.value = zoom;
	
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_zoom);

	return true; 
}
bool us_viscaserver_handle_command_cam1_focus(us_viscaserver_s *viscaserver) {
	VISCASERVER_PACKET_ASSERT_SIZE(6)

	VISCASERVER_PACKET_ASSERT(syntax, INRANGE(vs_req_pkt_buf[4] >> 4, 0, 7))
	int focus_speed = (vs_req_pkt_buf[4] >> 4) + 1;

	VISCASERVER_PACKET_ASSERT(syntax, ONEOF3(vs_req_pkt_buf[4] & 0x0F, VISCA_VALUE_CAM1_FOCUS_WIDE, VISCA_VALUE_CAM1_FOCUS_TELE, VISCA_VALUE_CAM1_FOCUS_STOP))
	int focus_direction = (vs_req_pkt_buf[4] & 0x0F) == VISCA_VALUE_CAM1_FOCUS_WIDE ? -1 : ((vs_req_pkt_buf[4] & 0x0F) == VISCA_VALUE_CAM1_FOCUS_TELE ? 1 : 0);

	_LOG_DEBUG("Manual focus drive focus%+d", focus_speed*focus_direction);

	struct v4l2_control v4l2ctrl_focus;
	v4l2ctrl_focus.id = V4L2_CID_FOCUS_ABSOLUTE;

	VISCASERVER_IOCTL_ASSERT(notexecutable, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctrl_focus)

	v4l2ctrl_focus.value += focus_speed * focus_direction;

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_focus);

	return true;
}
bool us_viscaserver_handle_command_cam1_focusdirect(us_viscaserver_s *viscaserver) {
	VISCASERVER_PACKET_ASSERT_SIZE(9)
	
	VISCASERVER_PACKET_ASSERT(syntax, IS_VISCA_QUAD_I16(vs_req_pkt_buf, 4))
	int16_t focus = VISCA_4BYTES_TO_INT16(vs_req_pkt_buf, 4);

	_LOG_DEBUG("Focusing to focus=%d", focus);

	struct v4l2_control v4l2ctrl_focus;
	v4l2ctrl_focus.id = V4L2_CID_FOCUS_ABSOLUTE;
	v4l2ctrl_focus.value = focus;
	
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_focus);

	return true; 
}
bool us_viscaserver_handle_command_cam1_focusauto(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(6)

	VISCASERVER_PACKET_ASSERT(syntax, ONEOF3(vs_req_pkt_buf[4], VISCA_VALUE_CAM1_FOCUS_AUTO_ON, VISCA_VALUE_CAM1_FOCUS_AUTO_OFF, VISCA_VALUE_CAM1_FOCUS_AUTO_MAN))

	int autofocus_enabled = vs_req_pkt_buf[4] == VISCA_VALUE_CAM1_FOCUS_AUTO_ON || vs_req_pkt_buf[4] == VISCA_VALUE_CAM1_FOCUS_AUTO_MAN ? 1 : 0;

	_LOG_DEBUG("Autofocus enabled=%d", autofocus_enabled);

	struct v4l2_control v4l2ctl_focus_auto;
	v4l2ctl_focus_auto.id = V4L2_CID_FOCUS_AUTO;
	v4l2ctl_focus_auto.value = autofocus_enabled;

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctl_focus_auto);

	return true;
}

bool us_viscaserver_handle_command_pantilt_drive(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(8)

	VISCASERVER_PACKET_ASSERT(syntax, INRANGE(vs_req_pkt_buf[4], 0x01, 0x18))
	VISCASERVER_PACKET_ASSERT(syntax, INRANGE(vs_req_pkt_buf[5], 0x01, 0x18))
	int pan_speed = vs_req_pkt_buf[4];
	int tilt_speed = vs_req_pkt_buf[5];

	VISCASERVER_PACKET_ASSERT(syntax, ONEOF3(vs_req_pkt_buf[6], VISCA_VALUE_PANTILT_DRIVE_HL, VISCA_VALUE_PANTILT_DRIVE_HR, VISCA_VALUE_PANTILT_DRIVE_HS))
	VISCASERVER_PACKET_ASSERT(syntax, ONEOF3(vs_req_pkt_buf[7], VISCA_VALUE_PANTILT_DRIVE_HL, VISCA_VALUE_PANTILT_DRIVE_HR, VISCA_VALUE_PANTILT_DRIVE_HS))
	int pan_direction = vs_req_pkt_buf[6] == VISCA_VALUE_PANTILT_DRIVE_HL ? -1 : (vs_req_pkt_buf[6] == VISCA_VALUE_PANTILT_DRIVE_HR ? 1 : 0);
	int tilt_direction = vs_req_pkt_buf[6] == VISCA_VALUE_PANTILT_DRIVE_VD ? -1 : (vs_req_pkt_buf[6] == VISCA_VALUE_PANTILT_DRIVE_VU ? 1 : 0);

	_LOG_DEBUG("Manual pan-tilt drive pan%+d tilt%+d", pan_speed*pan_direction, tilt_speed*tilt_direction);

	struct v4l2_control v4l2ctrl_pan;
	struct v4l2_control v4l2ctrl_tilt;
	v4l2ctrl_pan.id = V4L2_CID_PAN_ABSOLUTE;
	v4l2ctrl_tilt.id = V4L2_CID_TILT_ABSOLUTE;

	VISCASERVER_IOCTL_ASSERT(notexecutable, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctrl_pan)
	VISCASERVER_IOCTL_ASSERT(notexecutable, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctrl_tilt)

	v4l2ctrl_pan.value += 3600 * pan_speed * pan_direction;
	v4l2ctrl_tilt.value += 3600 * tilt_speed * tilt_direction;

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_pan);
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_tilt);

	return true;
}
bool us_viscaserver_handle_command_pantilt_absolute(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(15)

	VISCASERVER_PACKET_ASSERT(syntax, INRANGE(vs_req_pkt_buf[4], 0x01, 0x18))
	VISCASERVER_PACKET_ASSERT(syntax, INRANGE(vs_req_pkt_buf[5], 0x01, 0x18))
	__attribute__((unused)) int pan_speed = vs_req_pkt_buf[4];
	__attribute__((unused)) int tilt_speed = vs_req_pkt_buf[5];

	VISCASERVER_PACKET_ASSERT(syntax, IS_VISCA_QUAD_I16(vs_req_pkt_buf, 6))
	VISCASERVER_PACKET_ASSERT(syntax, IS_VISCA_QUAD_I16(vs_req_pkt_buf, 10))
	int16_t pan_degrees = VISCA_4BYTES_TO_INT16(vs_req_pkt_buf, 6);
	int16_t tilt_degrees = VISCA_4BYTES_TO_INT16(vs_req_pkt_buf, 10);

	_LOG_DEBUG("Moving to position to pan=%d tilt=%d", pan_degrees, tilt_degrees);

	struct v4l2_control v4l2ctrl_pan;
	struct v4l2_control v4l2ctrl_tilt;
	v4l2ctrl_pan.id = V4L2_CID_PAN_ABSOLUTE;
	v4l2ctrl_tilt.id = V4L2_CID_TILT_ABSOLUTE;

	v4l2ctrl_pan.value = 3600 * pan_degrees;
	v4l2ctrl_tilt.value = 3600 * tilt_degrees;
	
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_pan);
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_tilt);

	return true; 
}
bool us_viscaserver_handle_command_pantilt_home(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(5)

	_LOG_DEBUG("Homing position to pan=0 tilt=0");

	struct v4l2_control v4l2ctrl_pan;
	struct v4l2_control v4l2ctrl_tilt;
	v4l2ctrl_pan.id = V4L2_CID_PAN_ABSOLUTE;
	v4l2ctrl_tilt.id = V4L2_CID_TILT_ABSOLUTE;

	v4l2ctrl_pan.value = 0;
	v4l2ctrl_tilt.value = 0;
	
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_pan);
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_tilt);

	return true;
}
bool us_viscaserver_handle_command_pantilt_reset(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(5)

	_LOG_DEBUG("Reseting position to pan=0 tilt=0");

	struct v4l2_control v4l2ctrl_pan;
	struct v4l2_control v4l2ctrl_tilt;
	v4l2ctrl_pan.id = V4L2_CID_PAN_ABSOLUTE;
	v4l2ctrl_tilt.id = V4L2_CID_TILT_ABSOLUTE;

	v4l2ctrl_pan.value = 0;
	v4l2ctrl_tilt.value = 0;
	
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_pan);
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_tilt);

	return true;
}


bool us_viscaserver_handle_inquery_zoom_pos(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(5)

	struct v4l2_control v4l2ctl_zoom;
	v4l2ctl_zoom.id = V4L2_CID_ZOOM_ABSOLUTE;

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctl_zoom);
	
	int16_t zoom = v4l2ctl_zoom.value;

	_LOG_DEBUG("Responding to zoom position inquery zoom=%d", zoom);
	
	vs_res_pkt_buf_len = 0;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x90;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_RESPONSE_COMPLETED;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (zoom >> 0) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (zoom >> 4) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (zoom >> 8) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (zoom >> 12) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_TERMINATOR;

	return us_viscaserver_send_reply_buffer(viscaserver);
}
bool us_viscaserver_handle_inquery_autofocus(us_viscaserver_s *viscaserver) {
 	VISCASERVER_PACKET_ASSERT_SIZE(5)

	struct v4l2_control v4l2ctl_focus_auto;
	v4l2ctl_focus_auto.id = V4L2_CID_FOCUS_AUTO;

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctl_focus_auto);
	
	bool autofocus_enabled = v4l2ctl_focus_auto.value == 1;

	_LOG_DEBUG("Responding to autofocus enabled inquery enabled=%d", autofocus_enabled);

	vs_res_pkt_buf_len = 0;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x90;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_RESPONSE_COMPLETED;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = autofocus_enabled ? VISCA_ON: VISCA_OFF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_TERMINATOR;

	return us_viscaserver_send_reply_buffer(viscaserver);
}
bool us_viscaserver_handle_inquery_focus_pos(us_viscaserver_s *viscaserver) {
	VISCASERVER_PACKET_ASSERT_SIZE(5)

	struct v4l2_control v4l2ctl_focus;
	v4l2ctl_focus.id = V4L2_CID_FOCUS_ABSOLUTE;

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctl_focus);
	
	int16_t focus = v4l2ctl_focus.value;

	_LOG_DEBUG("Responding to focus position inquery focus=%d", focus);
	
	vs_res_pkt_buf_len = 0;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x90;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_RESPONSE_COMPLETED;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (focus >> 0) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (focus >> 4) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (focus >> 8) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (focus >> 12) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_TERMINATOR;

	return us_viscaserver_send_reply_buffer(viscaserver);
}
bool us_viscaserver_handle_inquery_pantilt_pos(us_viscaserver_s *viscaserver) {
	VISCASERVER_PACKET_ASSERT_SIZE(5)

	struct v4l2_control v4l2ctl_pan;
	struct v4l2_control v4l2ctl_tilt;
	v4l2ctl_pan.id = V4L2_CID_PAN_ABSOLUTE;
	v4l2ctl_tilt.id = V4L2_CID_TILT_ABSOLUTE;

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctl_pan);
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctl_tilt);
	
	int16_t pan_degrees = v4l2ctl_pan.value / 3600;
	int16_t tilt_degrees = v4l2ctl_tilt.value / 3600;
		
	_LOG_DEBUG("Responding to pan-tilt position inquery pan=%d tilt=%d", pan_degrees, tilt_degrees);

	vs_res_pkt_buf_len = 0;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = 0x90;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_RESPONSE_COMPLETED;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (pan_degrees >> 0) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (pan_degrees >> 4) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (pan_degrees >> 8) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (pan_degrees >> 12) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (tilt_degrees >> 0) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (tilt_degrees >> 4) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (tilt_degrees >> 8) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = (tilt_degrees >> 12) & 0xF;
	vs_res_pkt_buf[vs_res_pkt_buf_len++] = VISCA_TERMINATOR;

	return us_viscaserver_send_reply_buffer(viscaserver);
}