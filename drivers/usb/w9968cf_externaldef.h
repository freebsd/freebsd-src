/***************************************************************************
 * Various definitions for compatibility with OVCAMCHIP external module.   *
 * This file is part of the W996[87]CF driver for Linux.                   *
 *                                                                         *
 * The definitions have been taken from the OVCAMCHIP module written by    *
 * Mark McClelland.                                                        *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software             *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.               *
 ***************************************************************************/

#ifndef _W9968CF_EXTERNALDEF_H_
#define _W9968CF_EXTERNALDEF_H_

#include <linux/videodev.h>
#include <linux/i2c.h>
#include <asm/ioctl.h>
#include <asm/types.h>

#ifndef I2C_DRIVERID_OVCAMCHIP
#	define I2C_DRIVERID_OVCAMCHIP 0xf00f
#endif

/* Controls */
enum {
	OVCAMCHIP_CID_CONT,       /* Contrast */
	OVCAMCHIP_CID_BRIGHT,     /* Brightness */
	OVCAMCHIP_CID_SAT,        /* Saturation */
	OVCAMCHIP_CID_HUE,        /* Hue */
	OVCAMCHIP_CID_EXP,        /* Exposure */
	OVCAMCHIP_CID_FREQ,       /* Light frequency */
	OVCAMCHIP_CID_BANDFILT,   /* Banding filter */
	OVCAMCHIP_CID_AUTOBRIGHT, /* Auto brightness */
	OVCAMCHIP_CID_AUTOEXP,    /* Auto exposure */
	OVCAMCHIP_CID_BACKLIGHT,  /* Back light compensation */
	OVCAMCHIP_CID_MIRROR,     /* Mirror horizontally */
};

/* I2C addresses */
#define OV7xx0_SID   (0x42 >> 1)
#define OV6xx0_SID   (0xC0 >> 1)

/* Sensor types */
enum {
	CC_UNKNOWN,
	CC_OV76BE,
	CC_OV7610,
	CC_OV7620,
	CC_OV7620AE,
	CC_OV6620,
	CC_OV6630,
	CC_OV6630AE,
	CC_OV6630AF,
};

/* API */
struct ovcamchip_control {
	__u32 id;
	__s32 value;
};

struct ovcamchip_window {
	int x;
	int y;
	int width;
	int height;
	int format;
	int quarter;  /* Scale width and height down 2x */

	/* This stuff will be removed eventually */
	int clockdiv; /* Clock divisor setting */
};

/* Commands. 
   You must call OVCAMCHIP_CMD_INITIALIZE before any of other commands */
#define OVCAMCHIP_CMD_Q_SUBTYPE  _IOR  (0x88, 0x00, int)
#define OVCAMCHIP_CMD_INITIALIZE _IOW  (0x88, 0x01, int)
#define OVCAMCHIP_CMD_S_CTRL     _IOW  (0x88, 0x02, struct ovcamchip_control)
#define OVCAMCHIP_CMD_G_CTRL     _IOWR (0x88, 0x03, struct ovcamchip_control)
#define OVCAMCHIP_CMD_S_MODE     _IOW  (0x88, 0x04, struct ovcamchip_window)
#define OVCAMCHIP_MAX_CMD        _IO   (0x88, 0x3f)

#endif /* _W9968CF_EXTERNALDEF_H_ */
