/*
 *  User level driver support for input subsystem
 *
 * Heavily based on evdev.c by Vojtech Pavlik
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Author: Aristeu Sergio Rozanski Filho <aris@cathedrallabs.org>
 * 
 * Changes/Revisions:
 *	0.1	20/06/2002
 *		- first public version
 */

#ifndef __UINPUT_H_
#define __UINPUT_H_

#ifdef __KERNEL__
#define UINPUT_MINOR		223
#define UINPUT_NAME		"uinput"
#define UINPUT_BUFFER_SIZE	16

/* state flags => bit index for {set|clear|test}_bit ops */
#define UIST_CREATED		0

struct uinput_device {
	struct input_dev	*dev;
	unsigned long		state;
	wait_queue_head_t	waitq;
	unsigned char		ready,
				head,
				tail;
	struct input_event	buff[UINPUT_BUFFER_SIZE];
};
#endif	/* __KERNEL__ */

/* ioctl */
#define UINPUT_IOCTL_BASE	'U'
#define UI_DEV_CREATE		_IO(UINPUT_IOCTL_BASE, 1)
#define UI_DEV_DESTROY		_IO(UINPUT_IOCTL_BASE, 2)
#define UI_SET_EVBIT		_IOW(UINPUT_IOCTL_BASE, 100, int)
#define UI_SET_KEYBIT		_IOW(UINPUT_IOCTL_BASE, 101, int)
#define UI_SET_RELBIT		_IOW(UINPUT_IOCTL_BASE, 102, int)
#define UI_SET_ABSBIT		_IOW(UINPUT_IOCTL_BASE, 103, int)
#define UI_SET_MSCBIT		_IOW(UINPUT_IOCTL_BASE, 104, int)
#define UI_SET_LEDBIT		_IOW(UINPUT_IOCTL_BASE, 105, int)
#define UI_SET_SNDBIT		_IOW(UINPUT_IOCTL_BASE, 106, int)
#define UI_SET_FFBIT		_IOW(UINPUT_IOCTL_BASE, 107, int)

#ifndef NBITS
#define NBITS(x) ((((x)-1)/(sizeof(long)*8))+1)
#endif	/* NBITS */

#define UINPUT_MAX_NAME_SIZE	80
struct uinput_user_dev {
	char name[UINPUT_MAX_NAME_SIZE];
	unsigned short idbus;
	unsigned short idvendor;
	unsigned short idproduct;
	unsigned short idversion;
	int ff_effects_max;
	int absmax[ABS_MAX + 1];
	int absmin[ABS_MAX + 1];
	int absfuzz[ABS_MAX + 1];
	int absflat[ABS_MAX + 1];
};
#endif	/* __UINPUT_H_ */
