#include "inqueries.h"
#include "errors.h"
#include "../tools.h"
#include "../protocol.h"
#include "../net.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>


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
		
	_LOG_VERBOSE("Responding to pan-tilt position inquery pan=%d tilt=%d", pan_degrees, tilt_degrees);

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