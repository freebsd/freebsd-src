/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1998, 1999 Ralf Baechle (ralf@gnu.org)
 */
#ifndef _ASM_POLL_H
#define _ASM_POLL_H

#define POLLIN		0x0001
#define POLLPRI		0x0002
#define POLLOUT		0x0004

#define POLLERR		0x0008
#define POLLHUP		0x0010
#define POLLNVAL	0x0020

#define POLLRDNORM	0x0040
#define POLLRDBAND	0x0080
#define POLLWRNORM	POLLOUT
#define POLLWRBAND	0x0100

/* XXX This one seems to be more-or-less nonstandard.  */
#define POLLMSG		0x0400

struct pollfd {
	int fd;
	short events;
	short revents;
};

#endif /* _ASM_POLL_H */
