#pragma once


#define VISCASERVER_CAMERA_FD viscaserver->stream->cap->run->fd

#define v4l2_ioctl_try(fd, request, control, control_name, error_action)	if(ioctl(fd, request, &control) == -1) { \
																char* errno_msg = us_errno_to_string(errno); \
																_LOG_ERROR("ioctl %s request to %d failed: \"%s\"", control_name, fd, errno_msg);  \
																free(errno_msg); \
																error_action; \
															};											
