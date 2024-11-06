#include "requests.h"
#include "../tools.h"
#include "../protocol.h"
#include "../replies/errors.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>

bool us_viscaserver_handle_command_pantilt_drive(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(9)

	VISCASERVER_PACKET_ASSERT(syntax, INRANGE(vs_req_pkt_buf[4], 0x00, 0x18))
	VISCASERVER_PACKET_ASSERT(syntax, INRANGE(vs_req_pkt_buf[5], 0x00, 0x18))
	int pan_speed = (vs_req_pkt_buf[4] + 1);
	int tilt_speed = (vs_req_pkt_buf[5] + 1);

	VISCASERVER_PACKET_ASSERT(syntax, ONEOF3(vs_req_pkt_buf[6], VISCA_VALUE_PANTILT_DRIVE_HL, VISCA_VALUE_PANTILT_DRIVE_HR, VISCA_VALUE_PANTILT_DRIVE_HS))
	VISCASERVER_PACKET_ASSERT(syntax, ONEOF3(vs_req_pkt_buf[7], VISCA_VALUE_PANTILT_DRIVE_HL, VISCA_VALUE_PANTILT_DRIVE_HR, VISCA_VALUE_PANTILT_DRIVE_HS))
	int pan_direction = vs_req_pkt_buf[6] == VISCA_VALUE_PANTILT_DRIVE_HL ? -1 : (vs_req_pkt_buf[6] == VISCA_VALUE_PANTILT_DRIVE_HR ? 1 : 0);
	int tilt_direction = vs_req_pkt_buf[7] == VISCA_VALUE_PANTILT_DRIVE_VD ? -1 : (vs_req_pkt_buf[7] == VISCA_VALUE_PANTILT_DRIVE_VU ? 1 : 0);

	int pan_rel = pan_speed * pan_direction;
	int tilt_rel = tilt_speed * tilt_direction;

	struct v4l2_control v4l2ctrl_pan;
	struct v4l2_control v4l2ctrl_tilt;
	v4l2ctrl_pan.id = V4L2_CID_PAN_ABSOLUTE;
	v4l2ctrl_tilt.id = V4L2_CID_TILT_ABSOLUTE;

	VISCASERVER_IOCTL_ASSERT(notexecutable, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctrl_pan)
	VISCASERVER_IOCTL_ASSERT(notexecutable, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctrl_tilt)

	v4l2ctrl_pan.value = clamp(v4l2ctrl_pan.value + 3600 * pan_rel, -468000, 468000);
	v4l2ctrl_tilt.value = clamp(v4l2ctrl_tilt.value + 3600 * tilt_rel, -324000, 324000);

	_LOG_VERBOSE("Manual pan-tilt drive pan%+d tilt%+d pan=%d, tilt=%d", pan_rel, tilt_rel, v4l2ctrl_pan.value, v4l2ctrl_tilt.value);

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

	_LOG_VERBOSE("Moving to position to pan=%d tilt=%d", pan_degrees, tilt_degrees);

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

	_LOG_VERBOSE("Homing position to pan=0 tilt=0");

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

	_LOG_VERBOSE("Reseting position to pan=0 tilt=0");

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
