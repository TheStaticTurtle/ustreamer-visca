#include "handle.h"
#include "tools.h"

#include "protocol.h"

#include "replies/errors.h"

#include "requests/requests.h"
#include "replies/inqueries.h"


bool us_viscaserver_handle_commands(us_viscaserver_s *viscaserver) {
	VISCASERVER_PACKET_ASSERT_MINSIZE(3)
	
	if(vs_req_pkt_buf[2] == VISCA_CATEGORY_CAMERA1) {
		VISCASERVER_PACKET_ASSERT_MINSIZE(4)

		//if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_POWER) 							{}
		if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_ZOOM)							{ return us_viscaserver_handle_command_cam1_zoom(viscaserver); }
		else if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_ZOOM_DIRECT) 				{ return us_viscaserver_handle_command_cam1_zoomdirect(viscaserver); }
		else if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_FOCUS) 						{ return us_viscaserver_handle_command_cam1_focus(viscaserver); }
		else if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_FOCUS_DIRECT)				{ return us_viscaserver_handle_command_cam1_focusdirect(viscaserver); }
		else if(vs_req_pkt_buf[3] == VISCA_CONTROL_CAM1_FOCUS_AUTO) 				{ return us_viscaserver_handle_command_cam1_focusauto(viscaserver); }
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
bool us_viscaserver_handle_ptzoptics_commands(us_viscaserver_s *viscaserver) {
	VISCASERVER_PACKET_ASSERT_MINSIZE(3)
	
	if(vs_req_pkt_buf[2] == VISCA_CATEGORY_PTZOPTICS) {
		VISCASERVER_PACKET_ASSERT_MINSIZE(4)

		if(vs_req_pkt_buf[3] == VISCA_CONTROL_TALLY) 								{ return us_viscaserver_handle_command_tallylight_set(viscaserver); }
	}

	_LOG_ERROR("Could find the handler for the following packet: ");
	for (int i = 0; i < vs_req_pkt_buf_len; ++i) printf(" %02x", vs_req_pkt_buf[i]);
	printf("\n");
	return false;
}


bool us_viscaserver_handle_inqueries(us_viscaserver_s *viscaserver) {
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
