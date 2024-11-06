#include "inqueries.h"
#include "errors.h"
#include "../tools.h"
#include "../protocol.h"
#include "../net.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>


bool us_viscaserver_handle_inquery_zoom_pos(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(5)

	struct v4l2_control v4l2ctl_zoom;
	v4l2ctl_zoom.id = V4L2_CID_ZOOM_ABSOLUTE;

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctl_zoom);
	
	int16_t zoom = v4l2ctl_zoom.value;

	_LOG_VERBOSE("Responding to zoom position inquery zoom=%d", zoom);
	
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

	_LOG_VERBOSE("Responding to autofocus enabled inquery enabled=%d", autofocus_enabled);

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

	_LOG_VERBOSE("Responding to focus position inquery focus=%d", focus);
	
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