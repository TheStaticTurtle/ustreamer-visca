#pragma once

#define VISCA_COMMAND                    0x01
#define VISCA_INQUIRY                    0x09

#define VISCA_CATEGORY_INTERFACE           0x00
#define VISCA_CATEGORY_CAMERA1             0x04
#define   VISCA_CONTROL_CAM1_POWER           0x00
#define   VISCA_CONTROL_CAM1_ZOOM                0x07
#define     VISCA_VALUE_CAM1_ZOOM_STOP           0x00
#define     VISCA_VALUE_CAM1_ZOOM_TELE           0x02
#define     VISCA_VALUE_CAM1_ZOOM_WIDE           0x03
#define     VISCA_VALUE_CAM1_ZOOM_TELE_SPEED     0x20
#define     VISCA_VALUE_CAM1_ZOOM_WIDE_SPEED     0x30
#define   VISCA_CONTROL_CAM1_ZOOM_DIRECT         0x47
#define   VISCA_CONTROL_CAM1_FOCUS                0x08
#define     VISCA_VALUE_CAM1_FOCUS_STOP           0x00
#define     VISCA_VALUE_CAM1_FOCUS_TELE           0x02
#define     VISCA_VALUE_CAM1_FOCUS_WIDE           0x03
#define     VISCA_VALUE_CAM1_FOCUS_TELE_SPEED     0x20
#define     VISCA_VALUE_CAM1_FOCUS_WIDE_SPEED     0x30
#define   VISCA_CONTROL_CAM1_FOCUS_DIRECT         0x48
#define   VISCA_CONTROL_CAM1_FOCUS_AUTO           0x38
#define     VISCA_VALUE_CAM1_FOCUS_AUTO_ON        0x02
#define     VISCA_VALUE_CAM1_FOCUS_AUTO_OFF       0x03
#define     VISCA_VALUE_CAM1_FOCUS_AUTO_MAN       0x10
#define VISCA_CATEGORY_PANTILT             0x06
#define   VISCA_CONTROL_PANTILT_DRIVE        0x01
#define     VISCA_VALUE_PANTILT_DRIVE_HL     0x01
#define     VISCA_VALUE_PANTILT_DRIVE_HR     0x02
#define     VISCA_VALUE_PANTILT_DRIVE_HS     0x03
#define     VISCA_VALUE_PANTILT_DRIVE_VU     0x01
#define     VISCA_VALUE_PANTILT_DRIVE_VD     0x02
#define     VISCA_VALUE_PANTILT_DRIVE_VS     0x03
#define   VISCA_CONTROL_PANTILT_ABSOLUTE     0x02
#define   VISCA_CONTROL_PANTILT_RELATIVE     0x03
#define   VISCA_CONTROL_PANTILT_HOME         0x04
#define   VISCA_CONTROL_PANTILT_RESET        0x05
#define VISCA_CATEGORY_CAMERA2             0x07

#define VISCA_TERMINATOR                 0xFF

/***************/
/* ERROR CODES */
/***************/

/* specs errors: */
#define VISCA_ERROR_SYNTAX               0x02
#define VISCA_ERROR_CMD_BUFFER_FULL      0x03
#define VISCA_ERROR_CMD_CANCELLED        0x04
#define VISCA_ERROR_NO_SOCKET            0x05
#define VISCA_ERROR_CMD_NOT_EXECUTABLE   0x41

/* Generic definitions */
#define VISCA_ON                         0x02
#define VISCA_OFF                        0x03
#define VISCA_RESET                      0x00
#define VISCA_UP                         0x02
#define VISCA_DOWN                       0x03

/* response types */
#define VISCA_RESPONSE_CLEAR             0x40
#define VISCA_RESPONSE_ADDRESS           0x30
#define VISCA_RESPONSE_ACK               0x40
#define VISCA_RESPONSE_COMPLETED         0x50
#define VISCA_RESPONSE_ERROR             0x60