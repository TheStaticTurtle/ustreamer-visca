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

#include "tools.h"

#define _LOG_ERROR(x_msg, ...)	US_LOG_ERROR("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_PERROR(x_msg, ...)	US_LOG_PERROR("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_INFO(x_msg, ...)		US_LOG_INFO("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_VERBOSE(x_msg, ...)	US_LOG_VERBOSE("VISCA: " x_msg, ##__VA_ARGS__)
#define _LOG_DEBUG(x_msg, ...)	US_LOG_DEBUG("VISCA: " x_msg, ##__VA_ARGS__)

void us_viscaserver_wait_for_packet(us_viscaserver_s *viscaserver);


us_viscaserver_s *us_viscaserver_init(us_stream_s *stream) {
	us_viscaserver_s *viscaserver;
	US_CALLOC(viscaserver, 1);
	viscaserver->stream = stream;

	viscaserver->host = "127.0.0.1";
	viscaserver->port = 1259;
	viscaserver->camera_address = 0x01;

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



bool us_viscaserver_handle_command(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_inquery(us_viscaserver_s *viscaserver);

void us_viscaserver_wait_for_packet(us_viscaserver_s *viscaserver) {
	us_viscaserver_runtime_s *const run = viscaserver->run;

	memset(&run->packet, 0, sizeof(run->packet));

	run->packet.peeraddr_len = sizeof(struct sockaddr_storage);
    run->packet.buffer_len = recvfrom(run->socket_fd, run->packet.buffer, VISCASERVER_BUFFER_SIZE, 0, (struct sockaddr *) &run->packet.peeraddr, &run->packet.peeraddr_len);
    if (run->packet.buffer_len < 1) { return; }

	printf("Recevied data len=%d data=", run->packet.buffer_len);
	for (int i = 0; i < run->packet.buffer_len; ++i) printf("%02x ", run->packet.buffer[i]);
	printf("\n");

	if( run->packet.buffer_len < 4 ) {
		_LOG_ERROR("Received a packet with an invalid length");
		//TODO: us_viscaserver_reply_error_length(run->rx_packet);
		return;
	}

	if( run->packet.buffer[run->packet.buffer_len-1] != VISCA_TERMINATOR ) {
		_LOG_ERROR("Received a packet with an invalid terminator");
		//TODO: us_viscaserver_reply_error_syntax(run->rx_packet);
		return;
	}

	if(run->packet.buffer[0] != (0x80 | viscaserver->camera_address))  {
		_LOG_INFO("Received a packet for the invalid visca address, expcected %x got %x", (0x80 | viscaserver->camera_address), run->packet.buffer[0]);
		return;
	}

	if(run->packet.buffer[1] == VISCA_COMMAND) 		{ us_viscaserver_handle_command(viscaserver); }
	else if(run->packet.buffer[1] == VISCA_INQUIRY)	{ us_viscaserver_handle_inquery(viscaserver); }
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

	if(run->packet.buffer[2] == VISCA_CATEGORY_CAMERA1) {
		//if(run->packet.buffer[3] == VISCA_CONTROL_CAM1_POWER) 					{}
		if(run->packet.buffer[3] == VISCA_CONTROL_CAM1_ZOOM)						{ return us_viscaserver_handle_command_cam1_zoom(viscaserver); }
		else if(run->packet.buffer[3] == VISCA_CONTROL_CAM1_ZOOM_DIRECT) 			{ return us_viscaserver_handle_command_cam1_zoomdirect(viscaserver); }
		else if(run->packet.buffer[3] == VISCA_CONTROL_CAM1_FOCUS) 					{ return us_viscaserver_handle_command_cam1_focus(viscaserver); }
		else if(run->packet.buffer[3] == VISCA_CONTROL_CAM1_FOCUS_DIRECT)			{ return us_viscaserver_handle_command_cam1_focusdirect(viscaserver); }
		else if(run->packet.buffer[3] == VISCA_CONTROL_CAM1_FOCUS_AUTO) 			{ return us_viscaserver_handle_command_cam1_focusauto(viscaserver); }
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
	else if(run->packet.buffer[2] == VISCA_CATEGORY_PANTILT) {
		if(run->packet.buffer[3] == VISCA_CONTROL_PANTILT_DRIVE) 					{ return us_viscaserver_handle_command_pantilt_drive(viscaserver); }
		else if(run->packet.buffer[3] == VISCA_CONTROL_PANTILT_ABSOLUTE) 			{ return us_viscaserver_handle_command_pantilt_absolute(viscaserver); }
		//else if(run->packet.buffer[3] == VISCA_CONTROL_PANTILT_RELATIVE) 			{}
		else if(run->packet.buffer[3] == VISCA_CONTROL_PANTILT_HOME) 				{ return us_viscaserver_handle_command_pantilt_home(viscaserver); }
		else if(run->packet.buffer[3] == VISCA_CONTROL_PANTILT_RESET) 				{ return us_viscaserver_handle_command_pantilt_reset(viscaserver); }
	}

	_LOG_ERROR("Could find the handler for the following packet: ");
	for (int i = 0; i < run->packet.buffer_len; ++i) printf(" %02x", run->packet.buffer[i]);
	printf("\n");
	return false;
}


bool us_viscaserver_handle_inquery_zoom_pos(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_inquery_autofocus(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_inquery_focus_pos(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_inquery_pantilt_pos(us_viscaserver_s *viscaserver);

bool us_viscaserver_handle_inquery(us_viscaserver_s *viscaserver) {
	us_viscaserver_runtime_s *const run = viscaserver->run;

	if(run->packet.buffer[2] == VISCA_CATEGORY_CAMERA1) {
		if(run->packet.buffer[3] == 0x47) 						{ return us_viscaserver_handle_inquery_zoom_pos(viscaserver); }
		else if(run->packet.buffer[3] == 0x38) 					{ return us_viscaserver_handle_inquery_autofocus(viscaserver); }
		else if(run->packet.buffer[3] == 0x48) 					{ return us_viscaserver_handle_inquery_focus_pos(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x35) 				{ return us_viscaserver_handle_inquery_wb_mode(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x43) 				{ return us_viscaserver_handle_inquery_rgain(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x44) 				{ return us_viscaserver_handle_inquery_bgain(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x20) 				{ return us_viscaserver_handle_inquery_color_temp(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x39) 				{ return us_viscaserver_handle_inquery_ae_mode(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x4A) 				{ return us_viscaserver_handle_inquery_shutter_pos(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x4B) 				{ return us_viscaserver_handle_inquery_iris_pos(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x4D) 				{ return us_viscaserver_handle_inquery_bright_pos(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x3E) 				{ return us_viscaserver_handle_inquery_expcomp_mode(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x4E) 				{ return us_viscaserver_handle_inquery_expcomp_pos(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x33) 				{ return us_viscaserver_handle_inquery_backlight_mode(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x50) 				{ return us_viscaserver_handle_inquery_noise2d(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x53) 				{ return us_viscaserver_handle_inquery_noise2d_level(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x54) 				{ return us_viscaserver_handle_inquery_noise3d_level(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0x55) 				{ return us_viscaserver_handle_inquery_flicker_mode(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0xA1) 				{ return us_viscaserver_handle_inquery_brightness(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0xA2) 				{ return us_viscaserver_handle_inquery_contrast(viscaserver); }
		//else if(run->rx_packet.buffer[3] == 0xA4) 				{ return us_viscaserver_handle_inquery_filp(viscaserver); }
	}
	else if(run->packet.buffer[2] == VISCA_CATEGORY_PANTILT) {
		if(run->packet.buffer[3] == 0x12) 						{ return us_viscaserver_handle_inquery_pantilt_pos(viscaserver); }
	}

	_LOG_ERROR("Could find the handler for the following packet: ");
	for (int i = 0; i < run->packet.buffer_len; ++i) printf(" %02x", run->packet.buffer[i]);
	printf("\n");
	return false;
}

			


bool us_viscaserver_handle_command_cam1_zoom(us_viscaserver_s *viscaserver) { return false; }
bool us_viscaserver_handle_command_cam1_zoomdirect(us_viscaserver_s *viscaserver) { return false; }
bool us_viscaserver_handle_command_cam1_focus(us_viscaserver_s *viscaserver) { return false; }
bool us_viscaserver_handle_command_cam1_focusdirect(us_viscaserver_s *viscaserver) { return false; }
bool us_viscaserver_handle_command_cam1_focusauto(us_viscaserver_s *viscaserver) { return false; }

bool us_viscaserver_handle_command_pantilt_drive(us_viscaserver_s *viscaserver) { 
	us_viscaserver_runtime_s *const run = viscaserver->run;
	
	int vert_speed = 0;
	int horz_speed = 0;
	if(run->packet.buffer[6] == VISCA_VALUE_PANTILT_DRIVE_HL) { horz_speed = -1; }
	if(run->packet.buffer[6] == VISCA_VALUE_PANTILT_DRIVE_HR) { horz_speed =  1; }
	if(run->packet.buffer[7] == VISCA_VALUE_PANTILT_DRIVE_VD) { vert_speed = -1; }
	if(run->packet.buffer[7] == VISCA_VALUE_PANTILT_DRIVE_VU) { vert_speed =  1; }
	
	_LOG_DEBUG("Manual drive pan+=%d tilt+=%d", horz_speed, vert_speed);

	struct v4l2_control setctrl_pan;
	struct v4l2_control setctrl_tilt;
	setctrl_pan.id = V4L2_CID_PAN_ABSOLUTE;
	setctrl_tilt.id = V4L2_CID_TILT_ABSOLUTE;

	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_G_CTRL, setctrl_pan, "PAN_ABSOLUTE (command_pantilt_drive)", setctrl_pan.value = 0);
	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_G_CTRL, setctrl_tilt, "TILT_ABSOLUTE (command_pantilt_drive)", setctrl_tilt.value = 0);
	setctrl_pan.value += 3600 * horz_speed;
	setctrl_tilt.value += 3600 * vert_speed;
	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_S_CTRL, setctrl_pan, "PAN_ABSOLUTE (command_pantilt_drive)", );
	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_S_CTRL, setctrl_tilt, "TILT_ABSOLUTE (command_pantilt_drive)", );

	return true;
}
bool us_viscaserver_handle_command_pantilt_absolute(us_viscaserver_s *viscaserver) { 
	us_viscaserver_runtime_s *const run = viscaserver->run;

	int16_t pan_degrees = ((run->packet.buffer[6] & 0x0F) << 0) | ((run->packet.buffer[7] & 0x0F) << 4) | ((run->packet.buffer[8] & 0x0F) << 8) | ((run->packet.buffer[9] & 0x0F) << 12);
	int16_t tilt_degrees = ((run->packet.buffer[10] & 0x0F) << 0) | ((run->packet.buffer[11] & 0x0F) << 4) | ((run->packet.buffer[12] & 0x0F) << 8) | ((run->packet.buffer[13] & 0x0F) << 12);

	_LOG_DEBUG("Moving to position to pan=%d tilt=%d", pan_degrees, tilt_degrees);

	struct v4l2_control setctrl_pan;
	struct v4l2_control setctrl_tilt;
	setctrl_pan.id = V4L2_CID_PAN_ABSOLUTE;
	setctrl_tilt.id = V4L2_CID_TILT_ABSOLUTE;
	setctrl_pan.value = 3600 * pan_degrees;
	setctrl_tilt.value = 3600 * tilt_degrees;
	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_S_CTRL, setctrl_pan, "PAN_ABSOLUTE (command_pantilt_absolute)", );
	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_S_CTRL, setctrl_tilt, "TILT_ABSOLUTE (command_pantilt_absolute)", );

	return true; 
}
bool us_viscaserver_handle_command_pantilt_home(us_viscaserver_s *viscaserver ) { 
	__attribute__((unused)) us_viscaserver_runtime_s *const run = viscaserver->run;

	_LOG_DEBUG("Reseting position to pan=0 tilt=0");

	struct v4l2_control setctrl_pan;
	struct v4l2_control setctrl_tilt;
	setctrl_pan.id = V4L2_CID_PAN_ABSOLUTE;
	setctrl_tilt.id = V4L2_CID_TILT_ABSOLUTE;

	setctrl_pan.value = 0;
	setctrl_tilt.value = 0;
	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_S_CTRL, setctrl_pan, "PAN_ABSOLUTE (command_pantilt_home)", );
	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_S_CTRL, setctrl_tilt, "TILT_ABSOLUTE (command_pantilt_home)", );

	return true;
}
bool us_viscaserver_handle_command_pantilt_reset(us_viscaserver_s *viscaserver) { 
	__attribute__((unused)) us_viscaserver_runtime_s *const run = viscaserver->run;

	_LOG_DEBUG("Reseting position to pan=0 zoom=0");

	struct v4l2_control setctrl_pan;
	struct v4l2_control setctrl_tilt;
	setctrl_pan.id = V4L2_CID_PAN_ABSOLUTE;
	setctrl_tilt.id = V4L2_CID_TILT_ABSOLUTE;

	setctrl_pan.value = 0;
	setctrl_tilt.value = 0;
	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_S_CTRL, setctrl_pan, "PAN_ABSOLUTE (command_pantilt_reset)", );
	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_S_CTRL, setctrl_tilt, "TILT_ABSOLUTE (command_pantilt_reset)", );

	return true;
}


bool us_viscaserver_send_reply_buffer(us_viscaserver_s *viscaserver) { 
	us_viscaserver_runtime_s *const run = viscaserver->run;
	us_viscaserver_packet *const packet = &run->packet;

	return sendto( 
		run->socket_fd, 
		packet->reply_buffer, 
		packet->reply_buffer_len,  
		0,
		(struct sockaddr *)&run->packet.peeraddr,
		run->packet.peeraddr_len
	) > 0; 
}

bool us_viscaserver_handle_inquery_zoom_pos(us_viscaserver_s *viscaserver) { 
	us_viscaserver_runtime_s *const run = viscaserver->run;
	us_viscaserver_packet *const packet = &run->packet;

	int16_t position = 0;

	_LOG_DEBUG("Responding to zoom position inquery zoom=%d", position);
	
	packet->reply_buffer_len = 0;
	packet->reply_buffer[packet->reply_buffer_len++] = 0x90;
	packet->reply_buffer[packet->reply_buffer_len++] = VISCA_RESPONSE_COMPLETED;
	packet->reply_buffer[packet->reply_buffer_len++] = (position >> 0) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (position >> 4) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (position >> 8) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (position >> 12) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = VISCA_TERMINATOR;

	return us_viscaserver_send_reply_buffer(viscaserver);
}
bool us_viscaserver_handle_inquery_autofocus(us_viscaserver_s *viscaserver) { 
	us_viscaserver_runtime_s *const run = viscaserver->run;
	us_viscaserver_packet *const packet = &run->packet;

	bool autofocus_enabled = false;

	_LOG_DEBUG("Responding to autofocus enabled inquery enabled=%d", autofocus_enabled);

	packet->reply_buffer_len = 0;
	packet->reply_buffer[packet->reply_buffer_len++] = 0x90;
	packet->reply_buffer[packet->reply_buffer_len++] = VISCA_RESPONSE_COMPLETED;
	packet->reply_buffer[packet->reply_buffer_len++] = autofocus_enabled ? VISCA_ON: VISCA_OFF;
	packet->reply_buffer[packet->reply_buffer_len++] = VISCA_TERMINATOR;

	return us_viscaserver_send_reply_buffer(viscaserver);
}
bool us_viscaserver_handle_inquery_focus_pos(us_viscaserver_s *viscaserver) { 
	us_viscaserver_runtime_s *const run = viscaserver->run;
	us_viscaserver_packet *const packet = &run->packet;

	int16_t position = 0;

	_LOG_DEBUG("Responding to focus position inquery focus=%d", position);

	packet->reply_buffer_len = 0;
	packet->reply_buffer[packet->reply_buffer_len++] = 0x90;
	packet->reply_buffer[packet->reply_buffer_len++] = VISCA_RESPONSE_COMPLETED;
	packet->reply_buffer[packet->reply_buffer_len++] = (position >> 0) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (position >> 4) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (position >> 8) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (position >> 12) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = VISCA_TERMINATOR;

	return us_viscaserver_send_reply_buffer(viscaserver);
}
bool us_viscaserver_handle_inquery_pantilt_pos(us_viscaserver_s *viscaserver) { 
	us_viscaserver_runtime_s *const run = viscaserver->run;
	us_viscaserver_packet *const packet = &run->packet;

	struct v4l2_control setctrl_pan;
	struct v4l2_control setctrl_tilt;
	setctrl_pan.id = V4L2_CID_PAN_ABSOLUTE;
	setctrl_tilt.id = V4L2_CID_TILT_ABSOLUTE;
	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_G_CTRL, setctrl_pan, "PAN_ABSOLUTE (inquery_pantilt_pos)", setctrl_pan.value = 0);
	v4l2_ioctl_try(VISCASERVER_CAMERA_FD, VIDIOC_G_CTRL, setctrl_tilt, "TILT_ABSOLUTE (inquery_pantilt_pos)", setctrl_tilt.value = 0);

	int16_t pan_degrees = setctrl_pan.value / 3600;
	int16_t tilt_degrees = setctrl_tilt.value / 3600;
		
	_LOG_DEBUG("Responding to pan-tilt position inquery pan=%d tilt=%d", pan_degrees, tilt_degrees);

	packet->reply_buffer_len = 0;
	packet->reply_buffer[packet->reply_buffer_len++] = 0x90;
	packet->reply_buffer[packet->reply_buffer_len++] = VISCA_RESPONSE_COMPLETED;
	packet->reply_buffer[packet->reply_buffer_len++] = (pan_degrees >> 0) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (pan_degrees >> 4) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (pan_degrees >> 8) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (pan_degrees >> 12) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (tilt_degrees >> 0) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (tilt_degrees >> 4) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (tilt_degrees >> 8) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = (tilt_degrees >> 12) & 0xF;
	packet->reply_buffer[packet->reply_buffer_len++] = VISCA_TERMINATOR;

	return us_viscaserver_send_reply_buffer(viscaserver);
}