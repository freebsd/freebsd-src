/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc/gen/termios.c,v 1.9.2.1 2000/03/18 23:13:25 jasone Exp $
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)termios.c	8.2 (Berkeley) 2/21/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <errno.h>
#include <termios.h>
#include <unistd.h>

int
tcgetattr(fd, t)
	int fd;
	struct termios *t;
{

	return (ioctl(fd, TIOCGETA, t));
}

int
tcsetattr(fd, opt, t)
	int fd, opt;
	const struct termios *t;
{
	struct termios localterm;

	if (opt & TCSASOFT) {
		localterm = *t;
		localterm.c_cflag |= CIGNORE;
		t = &localterm;
	}
	switch (opt & ~TCSASOFT) {
	case TCSANOW:
		return (ioctl(fd, TIOCSETA, t));
	case TCSADRAIN:
		return (ioctl(fd, TIOCSETAW, t));
	case TCSAFLUSH:
		return (ioctl(fd, TIOCSETAF, t));
	default:
		errno = EINVAL;
		return (-1);
	}
}

int
#if __STDC__
tcsetpgrp(int fd, pid_t pgrp)
#else
tcsetpgrp(fd, pgrp)
	int fd;
	pid_t pgrp;
#endif
{
	int s;

	s = pgrp;
	return (ioctl(fd, TIOCSPGRP, &s));
}

pid_t
tcgetpgrp(fd)
	int fd;
{
	int s;

	if (ioctl(fd, TIOCGPGRP, &s) < 0)
		return ((pid_t)-1);

	return ((pid_t)s);
}

speed_t
cfgetospeed(t)
	const struct termios *t;
{

	return (t->c_ospeed);
}

speed_t
cfgetispeed(t)
	const struct termios *t;
{

	return (t->c_ispeed);
}

int
cfsetospeed(t, speed)
	struct termios *t;
	speed_t speed;
{

	t->c_ospeed = speed;
	return (0);
}

int
cfsetispeed(t, speed)
	struct termios *t;
	speed_t speed;
{

	t->c_ispeed = speed;
	return (0);
}

int
cfsetspeed(t, speed)
	struct termios *t;
	speed_t speed;
{

	t->c_ispeed = t->c_ospeed = speed;
	return (0);
}

/*
 * Make a pre-existing termios structure into "raw" mode: character-at-a-time
 * mode with no characters interpreted, 8-bit data path.
 */
void
cfmakeraw(t)
	struct termios *t;
{

	t->c_iflag &= ~(IMAXBEL|IXOFF|INPCK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IGNPAR);
	t->c_iflag |= IGNBRK;
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON|ISIG|IEXTEN|NOFLSH|TOSTOP|PENDIN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8|CREAD;
	t->c_cc[VMIN] = 1;
	t->c_cc[VTIME] = 0;
}

int
tcsendbreak(fd, len)
	int fd, len;
{
	struct timeval sleepytime;

	sleepytime.tv_sec = 0;
	sleepytime.tv_usec = 400000;
	if (ioctl(fd, TIOCSBRK, 0) == -1)
		return (-1);
	(void)select(0, 0, 0, 0, &sleepytime);
	if (ioctl(fd, TIOCCBRK, 0) == -1)
		return (-1);
	return (0);
}

int
__tcdrain(fd)
	int fd;
{
	return (ioctl(fd, TIOCDRAIN, 0));
}

#ifndef _THREAD_SAFE
__weak_reference(__tcdrain, tcdrain);
#endif

int
tcflush(fd, which)
	int fd, which;
{
	int com;

	switch (which) {
	case TCIFLUSH:
		com = FREAD;
		break;
	case TCOFLUSH:
		com = FWRITE;
		break;
	case TCIOFLUSH:
		com = FREAD | FWRITE;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	return (ioctl(fd, TIOCFLUSH, &com));
}

int
tcflow(fd, action)
	int fd, action;
{
	struct termios term;
	u_char c;

	switch (action) {
	case TCOOFF:
		return (ioctl(fd, TIOCSTOP, 0));
	case TCOON:
		return (ioctl(fd, TIOCSTART, 0));
	case TCION:
	case TCIOFF:
		if (tcgetattr(fd, &term) == -1)
			return (-1);
		c = term.c_cc[action == TCIOFF ? VSTOP : VSTART];
		if (c != _POSIX_VDISABLE && _write(fd, &c, sizeof(c)) == -1)
			return (-1);
		return (0);
	default:
		errno = EINVAL;
		return (-1);
	}
	/* NOTREACHED */
}
