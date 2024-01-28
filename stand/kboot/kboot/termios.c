/*
 * Copyright (c) 2005-2020 Rich Felker, et al.
 *
 * SPDX-License-Identifier: MIT
 *
 * Note: From the musl project, stripped down and repackaged with HOST_/host_ prepended
 */

#include <sys/types.h>
#include "termios.h"
#include "host_syscall.h"

int
host_tcgetattr(int fd, struct host_termios *tio)
{
	if (host_ioctl(fd, HOST_TCGETS, (uintptr_t)tio))
		return -1;
	return 0;
}

int
host_tcsetattr(int fd, int act, const struct host_termios *tio)
{
	if (act < 0 || act > 2) {
//		errno = EINVAL;	/* XXX ?? */
		return -1;
	}
	return host_ioctl(fd, HOST_TCSETS+act, (uintptr_t)tio);
}

void
host_cfmakeraw(struct host_termios *t)
{
	t->c_iflag &= ~(HOST_IGNBRK | HOST_BRKINT | HOST_PARMRK | HOST_ISTRIP |
	    HOST_INLCR | HOST_IGNCR | HOST_ICRNL | HOST_IXON);
	t->c_oflag &= ~HOST_OPOST;
	t->c_lflag &= ~(HOST_ECHO | HOST_ECHONL | HOST_ICANON | HOST_ISIG |
	    HOST_IEXTEN);
	t->c_cflag &= ~(HOST_CSIZE | HOST_PARENB);
	t->c_cflag |= HOST_CS8;
	t->c_cc[HOST_VMIN] = 1;
	t->c_cc[HOST_VTIME] = 0;
}

int host_cfsetospeed(struct host_termios *tio, host_speed_t speed)
{
	if (speed & ~HOST_CBAUD) {
//		errno = EINVAL; /* XXX ? */
		return -1;
	}
	tio->c_cflag &= ~HOST_CBAUD;
	tio->c_cflag |= speed;
	return 0;
}

int host_cfsetispeed(struct host_termios *tio, host_speed_t speed)
{
	return speed ? host_cfsetospeed(tio, speed) : 0;
}

int
host_cfsetspeed(struct host_termios *tio, host_speed_t speed)
{
	return host_cfsetospeed(tio, speed);	/* weak alias in musl */
}

