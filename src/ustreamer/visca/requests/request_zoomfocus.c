#include "requests.h"
#include "../tools.h"
#include "../protocol.h"
#include "../replies/errors.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>

bool us_viscaserver_handle_command_cam1_zoom(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(6)

	int zoom_speed = 1;
	int zoom_direction = 0;

	if(ONEOF3(vs_req_pkt_buf[4], VISCA_VALUE_CAM1_ZOOM_WIDE, VISCA_VALUE_CAM1_ZOOM_TELE, VISCA_VALUE_CAM1_ZOOM_STOP)) {
		zoom_speed = 1;
		zoom_direction = vs_req_pkt_buf[4] == VISCA_VALUE_CAM1_ZOOM_WIDE ? -1 : (vs_req_pkt_buf[4] == VISCA_VALUE_CAM1_ZOOM_TELE ? 1 : 0);
	} else if(ONEOF3(vs_req_pkt_buf[4] >> 4, VISCA_VALUE_CAM1_ZOOM_WIDE, VISCA_VALUE_CAM1_ZOOM_TELE, VISCA_VALUE_CAM1_ZOOM_STOP)) {
		VISCASERVER_PACKET_ASSERT(syntax, INRANGE(vs_req_pkt_buf[4] & 0x0F, 0, 7))
		zoom_speed = (vs_req_pkt_buf[4] & 0x0F) + 1;
		zoom_direction = (vs_req_pkt_buf[4] >> 4) == VISCA_VALUE_CAM1_ZOOM_WIDE ? -1 : ((vs_req_pkt_buf[4] >> 4) == VISCA_VALUE_CAM1_ZOOM_TELE ? 1 : 0);
	} else {
		VISCASERVER_PACKET_ASSERT(syntax, false);
	}

	int zoom_rel = zoom_speed * zoom_direction;

	struct v4l2_control v4l2ctrl_zoom;
	v4l2ctrl_zoom.id = V4L2_CID_ZOOM_ABSOLUTE;

	VISCASERVER_IOCTL_ASSERT(notexecutable, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctrl_zoom)

	// TODO: CHECK Against actual min/max
	v4l2ctrl_zoom.value = clamp(v4l2ctrl_zoom.value + zoom_rel, 0, 100);
	_LOG_VERBOSE("Manual zoom drive %+d abs=%d", zoom_rel, v4l2ctrl_zoom.value);

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_zoom);

	return true;
}
bool us_viscaserver_handle_command_cam1_zoomdirect(us_viscaserver_s *viscaserver) {
	if(vs_req_pkt_buf_len == 13) {
		// Assume that a 13 bytes packet with the same IDs additionaly contains the focus value
		return us_viscaserver_handle_command_cam1_zoomfocusdirect(viscaserver);
	}

	VISCASERVER_PACKET_ASSERT_SIZE(9)
	
	VISCASERVER_PACKET_ASSERT(syntax, IS_VISCA_QUAD_I16(vs_req_pkt_buf, 4))
	int16_t zoom = VISCA_4BYTES_TO_INT16(vs_req_pkt_buf, 4);

	_LOG_VERBOSE("Zooming to zoom=%d", zoom);

	struct v4l2_control v4l2ctrl_zoom;
	v4l2ctrl_zoom.id = V4L2_CID_ZOOM_ABSOLUTE;
	v4l2ctrl_zoom.value = zoom;
	
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_zoom);

	return true; 
}
bool us_viscaserver_handle_command_cam1_focus(us_viscaserver_s *viscaserver) {
	VISCASERVER_PACKET_ASSERT_SIZE(6)

	int focus_speed = 1;
	int focus_direction = 0;

	if(ONEOF3(vs_req_pkt_buf[4], VISCA_VALUE_CAM1_FOCUS_WIDE, VISCA_VALUE_CAM1_FOCUS_TELE, VISCA_VALUE_CAM1_FOCUS_STOP)) {
		focus_speed = 1;
		focus_direction = vs_req_pkt_buf[4] == VISCA_VALUE_CAM1_FOCUS_WIDE ? -1 : (vs_req_pkt_buf[4] == VISCA_VALUE_CAM1_FOCUS_TELE ? 1 : 0);
	} else if(ONEOF3(vs_req_pkt_buf[4] >> 4, VISCA_VALUE_CAM1_FOCUS_WIDE, VISCA_VALUE_CAM1_FOCUS_TELE, VISCA_VALUE_CAM1_FOCUS_STOP)) {
		VISCASERVER_PACKET_ASSERT(syntax, INRANGE(vs_req_pkt_buf[4] & 0x0F, 0, 7))
		focus_speed = (vs_req_pkt_buf[4] & 0x0F) + 1;
		focus_direction = (vs_req_pkt_buf[4] >> 4) == VISCA_VALUE_CAM1_FOCUS_WIDE ? -1 : ((vs_req_pkt_buf[4] >> 4) == VISCA_VALUE_CAM1_FOCUS_TELE ? 1 : 0);
	} else {
		VISCASERVER_PACKET_ASSERT(syntax, false);
	}

	int focus_rel = focus_speed * focus_direction;

	struct v4l2_control v4l2ctrl_focus;
	v4l2ctrl_focus.id = V4L2_CID_FOCUS_ABSOLUTE;

	VISCASERVER_IOCTL_ASSERT(notexecutable, vs_v4l2_dev_fd, VIDIOC_G_CTRL, v4l2ctrl_focus)

	v4l2ctrl_focus.value = clamp(v4l2ctrl_focus.value + focus_rel, 0, 100);
	_LOG_VERBOSE("Manual zoom drive %+d abs=%d", focus_rel, v4l2ctrl_focus.value);

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_focus);

	return true;
}
bool us_viscaserver_handle_command_cam1_focusdirect(us_viscaserver_s *viscaserver) {
	VISCASERVER_PACKET_ASSERT_SIZE(9)
	
	VISCASERVER_PACKET_ASSERT(syntax, IS_VISCA_QUAD_I16(vs_req_pkt_buf, 4))
	int16_t focus = VISCA_4BYTES_TO_INT16(vs_req_pkt_buf, 4);

	_LOG_VERBOSE("Focusing to focus=%d", focus);

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

	_LOG_VERBOSE("Autofocus enabled=%d", autofocus_enabled);

	struct v4l2_control v4l2ctl_focus_auto;
	v4l2ctl_focus_auto.id = V4L2_CID_FOCUS_AUTO;
	v4l2ctl_focus_auto.value = autofocus_enabled;

	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctl_focus_auto);

	return true;
}
bool us_viscaserver_handle_command_cam1_zoomfocusdirect(us_viscaserver_s *viscaserver) {
	VISCASERVER_PACKET_ASSERT_SIZE(13)
	
	VISCASERVER_PACKET_ASSERT(syntax, IS_VISCA_QUAD_I16(vs_req_pkt_buf, 4))
	VISCASERVER_PACKET_ASSERT(syntax, IS_VISCA_QUAD_I16(vs_req_pkt_buf, 8))

	int16_t zoom = VISCA_4BYTES_TO_INT16(vs_req_pkt_buf, 4);
	int16_t focus = VISCA_4BYTES_TO_INT16(vs_req_pkt_buf, 8);

	_LOG_VERBOSE("Zooming and focusing to zoom=%d focus=%d", zoom, focus);

	struct v4l2_control v4l2ctrl_zoom;
	struct v4l2_control v4l2ctrl_focus;
	v4l2ctrl_zoom.id = V4L2_CID_ZOOM_ABSOLUTE;
	v4l2ctrl_focus.id = V4L2_CID_FOCUS_ABSOLUTE;

	v4l2ctrl_zoom.value = zoom;
	v4l2ctrl_focus.value = focus;
	
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_zoom);
	VISCASERVER_IOCTL_ASSERT(canceled, vs_v4l2_dev_fd, VIDIOC_S_CTRL, v4l2ctrl_focus);

	return true; 
}
