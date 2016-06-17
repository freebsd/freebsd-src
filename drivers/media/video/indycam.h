#ifndef _indycam_h_
#define _indycam_h_

/* I2C address for the Guinness Camera */
#define INDYCAM_ADDR			0x56

/* Camera version */
#define CAMERA_VERSION_INDY		0x10	/* v1.0 */
#define CAMERA_VERSION_MOOSE		0x12	/* v1.2 */
#define INDYCAM_VERSION_MAJOR(x)	(((x) & 0xf0) >> 4)
#define INDYCAM_VERSION_MINOR(x)	((x) & 0x0f)

/* Register bus addresses */
#define INDYCAM_CONTROL			0x00
#define INDYCAM_SHUTTER			0x01
#define INDYCAM_GAIN			0x02
#define INDYCAM_BRIGHTNESS		0x03
#define INDYCAM_RED_BALANCE		0x04
#define INDYCAM_BLUE_BALANCE		0x05
#define INDYCAM_RED_SATURATION		0x06
#define INDYCAM_BLUE_SATURATION		0x07
#define INDYCAM_GAMMA			0x08
#define INDYCAM_VERSION			0x0e
#define INDYCAM_RESET			0x0f
#define INDYCAM_LED			0x46
#define INDYCAM_ORIENTATION		0x47
#define INDYCAM_BUTTON			0x48

/* Field definitions of registers */
#define INDYCAM_CONTROL_AGCENA		(1<<0)
#define INDYCAM_CONTROL_AWBCTL		(1<<1)
						/* 2-3 are reserved */
#define INDYCAM_CONTROL_EVNFLD		(1<<4)

#define INDYCAM_SHUTTER_10000		0x02	/* 1/10000 second */
#define INDYCAM_SHUTTER_4000		0x04	/* 1/4000 second */
#define INDYCAM_SHUTTER_2000		0x08	/* 1/2000 second */
#define INDYCAM_SHUTTER_1000		0x10	/* 1/1000 second */
#define INDYCAM_SHUTTER_500		0x20	/* 1/500 second */
#define INDYCAM_SHUTTER_250		0x3f	/* 1/250 second */
#define INDYCAM_SHUTTER_125		0x7e	/* 1/125 second */
#define INDYCAM_SHUTTER_100		0x9e	/* 1/100 second */
#define INDYCAM_SHUTTER_60		0x00	/* 1/60 second */

#define INDYCAM_BUTTON_RELEASED		(1<<4)
#define INDYCAM_LED_ACTIVE		(1<<5)
#define INDYCAM_BOTTOM_TO_TOP		(1<<6)

#endif
