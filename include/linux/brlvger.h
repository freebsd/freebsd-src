/*
 *      Tieman Voyager braille display USB driver.
 *
 *      Copyright 2001-2002 Stephane Dalton <sdalton@videotron.ca>
 *                      and Stéphane Doyon  <s.doyon@videotron.ca>
 *            Maintained by Stéphane Doyon  <s.doyon@videotron.ca>.
 */
/*
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_BRLVGER_H
#define _LINUX_BRLVGER_H

/* Ioctl request codes */
#define BRLVGER_GET_INFO	0
#define BRLVGER_DISPLAY_ON	2
#define BRLVGER_DISPLAY_OFF	3
#define BRLVGER_BUZZ		4

/* Number of supported devices, and range of covered minors */
#define MAX_NR_BRLVGER_DEVS	4

/* Base minor for the char devices */
#define BRLVGER_MINOR		128

/* Size of some fields */
#define BRLVGER_HWVER_SIZE	2
#define BRLVGER_FWVER_SIZE	200 /* arbitrary, a long string */
#define BRLVGER_SERIAL_BIN_SIZE	8
#define BRLVGER_SERIAL_SIZE	((2*BRLVGER_SERIAL_BIN_SIZE)+1)

struct brlvger_info {
	__u8 driver_version[12];
	__u8 driver_banner[200];

	__u32 display_length;
	/* All other char[] fields are strings except this one.
	   Hardware version: first byte is major, second byte is minor. */
	__u8 hwver[BRLVGER_HWVER_SIZE];
	__u8 fwver[BRLVGER_FWVER_SIZE];
	__u8 serialnum[BRLVGER_SERIAL_SIZE];
};

#endif
