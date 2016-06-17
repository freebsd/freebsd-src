/*
 * Definitions for a Mitsumi CD-ROM interface
 *
 *	Copyright (C) 1992  Martin Harriss
 *
 *	martin@bdsi.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/* Increase this if you get lots of timeouts */
#define MCD_STATUS_DELAY	1000

/* number of times to retry a command before giving up */
#define MCD_RETRY_ATTEMPTS      10

/* port access macro */
#define MCDPORT(x)		(mcd_port + (x))

/* How many sectors to read at 1x when an error at 2x speed occurs. */
/* You can change this to anything from 2 to 32767, but 30 seems to */
/* work best for me.  I have found that when the drive has problems */
/* reading one sector, it will have troubles reading the next few.  */
#define SINGLE_HOLD_SECTORS 30	

#define MCMD_2X_READ 0xC1	/* Double Speed Read DON'T TOUCH! */

/* status bits */

#define MST_CMD_CHECK		0x01		/* command error */
#define MST_BUSY		0x02		/* now playing */
#define MST_READ_ERR		0x04		/* read error */
#define MST_DSK_TYPE		0x08
#define MST_SERVO_CHECK		0x10
#define MST_DSK_CHG		0x20		/* disk removed or changed */
#define MST_READY		0x40		/* disk in the drive */
#define MST_DOOR_OPEN		0x80		/* door is open */

/* flag bits */

#define MFL_DATA		0x02		/* data available */
#define MFL_STATUS		0x04		/* status available */

/* commands */

#define MCMD_GET_DISK_INFO	0x10		/* read info from disk */
#define MCMD_GET_Q_CHANNEL	0x20		/* read info from q channel */
#define MCMD_GET_STATUS		0x40
#define MCMD_SET_MODE		0x50
#define MCMD_SOFT_RESET		0x60
#define MCMD_STOP		0x70		/* stop play */
#define MCMD_CONFIG_DRIVE	0x90
#define MCMD_SET_VOLUME		0xAE		/* set audio level */
#define MCMD_PLAY_READ		0xC0		/* play or read data */
#define MCMD_GET_VERSION  	0xDC
#define MCMD_EJECT		0xF6            /* eject (FX drive) */

/* borrowed from hd.c */

#define MAX_TRACKS		104

struct msf {
	unsigned char	min;
	unsigned char	sec;
	unsigned char	frame;
};

struct mcd_Play_msf {
	struct msf	start;
	struct msf	end;
};

struct mcd_DiskInfo {
	unsigned char	first;
	unsigned char	last;
	struct msf	diskLength;
	struct msf	firstTrack;
};

struct mcd_Toc {
	unsigned char	ctrl_addr;
	unsigned char	track;
	unsigned char	pointIndex;
	struct msf	trackTime;
	struct msf	diskTime;
};

#define test1(x)
#define test2(x)
#define test3(x)
#define test4(x)
#define test5(x)

