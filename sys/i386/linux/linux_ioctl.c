/*
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/linux/linux_ioctl.c,v 1.50.2.1 2000/07/07 01:09:51 obrien Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/cdio.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/linker_set.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <sys/sockio.h>
#include <sys/soundcard.h>
#include <sys/disklabel.h>

#include <machine/console.h>

#include <i386/linux/linux.h>
#include <i386/linux/linux_ioctl.h>
#include <i386/linux/linux_mib.h>
#include <i386/linux/linux_proto.h>
#include <i386/linux/linux_util.h>

static linux_ioctl_function_t linux_ioctl_cdrom;
static linux_ioctl_function_t linux_ioctl_console;
static linux_ioctl_function_t linux_ioctl_disk;
static linux_ioctl_function_t linux_ioctl_socket;
static linux_ioctl_function_t linux_ioctl_sound;
static linux_ioctl_function_t linux_ioctl_termio;

static struct linux_ioctl_handler cdrom_handler =
{ linux_ioctl_cdrom, LINUX_IOCTL_CDROM_MIN, LINUX_IOCTL_CDROM_MAX };
static struct linux_ioctl_handler console_handler =
{ linux_ioctl_console, LINUX_IOCTL_CONSOLE_MIN, LINUX_IOCTL_CONSOLE_MAX };
static struct linux_ioctl_handler disk_handler =
{ linux_ioctl_disk, LINUX_IOCTL_DISK_MIN, LINUX_IOCTL_DISK_MAX };
static struct linux_ioctl_handler socket_handler =
{ linux_ioctl_socket, LINUX_IOCTL_SOCKET_MIN, LINUX_IOCTL_SOCKET_MAX };
static struct linux_ioctl_handler sound_handler =
{ linux_ioctl_sound, LINUX_IOCTL_SOUND_MIN, LINUX_IOCTL_SOUND_MAX };
static struct linux_ioctl_handler termio_handler =
{ linux_ioctl_termio, LINUX_IOCTL_TERMIO_MIN, LINUX_IOCTL_TERMIO_MAX };

DATA_SET(linux_ioctl_handler_set, cdrom_handler);
DATA_SET(linux_ioctl_handler_set, console_handler);
DATA_SET(linux_ioctl_handler_set, disk_handler);
DATA_SET(linux_ioctl_handler_set, socket_handler);
DATA_SET(linux_ioctl_handler_set, sound_handler);
DATA_SET(linux_ioctl_handler_set, termio_handler);

struct handler_element 
{
	TAILQ_ENTRY(handler_element) list;
	int	(*func)(struct proc *, struct linux_ioctl_args *);
	int	low, high, span;
};

static TAILQ_HEAD(, handler_element) handlers =
	TAILQ_HEAD_INITIALIZER(handlers);

static int
linux_ioctl_disk(struct proc *p, struct linux_ioctl_args *args)
{
	struct file *fp = p->p_fd->fd_ofiles[args->fd];
	int error;
	struct disklabel dl;

	switch (args->cmd & 0xffff) {
	case LINUX_BLKGETSIZE:
		error = fo_ioctl(fp, DIOCGDINFO, (caddr_t)&dl, p);
		if (error)
			return (error);
		return copyout(&(dl.d_secperunit), (caddr_t)args->arg, sizeof(dl.d_secperunit));
		break;
	}
	return (ENOIOCTL);
}

/*
 * termio related ioctls
 */

struct linux_termio {
	unsigned short c_iflag;
	unsigned short c_oflag;
	unsigned short c_cflag;
	unsigned short c_lflag;
	unsigned char c_line;
	unsigned char c_cc[LINUX_NCC];
};

struct linux_termios {
	unsigned int c_iflag;
	unsigned int c_oflag;
	unsigned int c_cflag;
	unsigned int c_lflag;
	unsigned char c_line;
	unsigned char c_cc[LINUX_NCCS];
};

struct linux_winsize {
	unsigned short ws_row, ws_col;
	unsigned short ws_xpixel, ws_ypixel;
};

static struct speedtab sptab[] = {
	{ B0, LINUX_B0 }, { B50, LINUX_B50 },
	{ B75, LINUX_B75 }, { B110, LINUX_B110 },
	{ B134, LINUX_B134 }, { B150, LINUX_B150 },
	{ B200, LINUX_B200 }, { B300, LINUX_B300 },
	{ B600, LINUX_B600 }, { B1200, LINUX_B1200 },
	{ B1800, LINUX_B1800 }, { B2400, LINUX_B2400 },
	{ B4800, LINUX_B4800 }, { B9600, LINUX_B9600 },
	{ B19200, LINUX_B19200 }, { B38400, LINUX_B38400 },
	{ B57600, LINUX_B57600 }, { B115200, LINUX_B115200 },
	{-1, -1 }
};

struct linux_serial_struct {
	int	type;
	int	line;
	int	port;
	int	irq;
	int	flags;
	int	xmit_fifo_size;
	int	custom_divisor;
	int	baud_base;
	unsigned short close_delay;
	char	reserved_char[2];
	int	hub6;
	unsigned short closing_wait;
	unsigned short closing_wait2;
	int	reserved[4];
};

static int
linux_to_bsd_speed(int code, struct speedtab *table)
{
	for ( ; table->sp_code != -1; table++)
		if (table->sp_code == code)
			return (table->sp_speed);
	return -1;
}

static int
bsd_to_linux_speed(int speed, struct speedtab *table)
{
	for ( ; table->sp_speed != -1; table++)
		if (table->sp_speed == speed)
			return (table->sp_code);
	return -1;
}

static void
bsd_to_linux_termios(struct termios *bios, struct linux_termios *lios)
{
	int i;

#ifdef DEBUG
	printf("LINUX: BSD termios structure (input):\n");
	printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d\n",
	    bios->c_iflag, bios->c_oflag, bios->c_cflag, bios->c_lflag,
	    bios->c_ispeed, bios->c_ospeed);
	printf("c_cc ");
	for (i=0; i<NCCS; i++)
		printf("%02x ", bios->c_cc[i]);
	printf("\n");
#endif

	lios->c_iflag = 0;
	if (bios->c_iflag & IGNBRK)
		lios->c_iflag |= LINUX_IGNBRK;
	if (bios->c_iflag & BRKINT)
		lios->c_iflag |= LINUX_BRKINT;
	if (bios->c_iflag & IGNPAR)
		lios->c_iflag |= LINUX_IGNPAR;
	if (bios->c_iflag & PARMRK)
		lios->c_iflag |= LINUX_PARMRK;
	if (bios->c_iflag & INPCK)
		lios->c_iflag |= LINUX_INPCK;
	if (bios->c_iflag & ISTRIP)
		lios->c_iflag |= LINUX_ISTRIP;
	if (bios->c_iflag & INLCR)
		lios->c_iflag |= LINUX_INLCR;
	if (bios->c_iflag & IGNCR)
		lios->c_iflag |= LINUX_IGNCR;
	if (bios->c_iflag & ICRNL)
		lios->c_iflag |= LINUX_ICRNL;
	if (bios->c_iflag & IXON)
		lios->c_iflag |= LINUX_IXON;
	if (bios->c_iflag & IXANY)
		lios->c_iflag |= LINUX_IXANY;
	if (bios->c_iflag & IXOFF)
		lios->c_iflag |= LINUX_IXOFF;
	if (bios->c_iflag & IMAXBEL)
		lios->c_iflag |= LINUX_IMAXBEL;

	lios->c_oflag = 0;
	if (bios->c_oflag & OPOST)
		lios->c_oflag |= LINUX_OPOST;
	if (bios->c_oflag & ONLCR)
		lios->c_oflag |= LINUX_ONLCR;
	if (bios->c_oflag & OXTABS)
		lios->c_oflag |= LINUX_XTABS;

	lios->c_cflag = bsd_to_linux_speed(bios->c_ispeed, sptab);
	lios->c_cflag |= (bios->c_cflag & CSIZE) >> 4;
	if (bios->c_cflag & CSTOPB)
		lios->c_cflag |= LINUX_CSTOPB;
	if (bios->c_cflag & CREAD)
		lios->c_cflag |= LINUX_CREAD;
	if (bios->c_cflag & PARENB)
		lios->c_cflag |= LINUX_PARENB;
	if (bios->c_cflag & PARODD)
		lios->c_cflag |= LINUX_PARODD;
	if (bios->c_cflag & HUPCL)
		lios->c_cflag |= LINUX_HUPCL;
	if (bios->c_cflag & CLOCAL)
		lios->c_cflag |= LINUX_CLOCAL;
	if (bios->c_cflag & CRTSCTS)
		lios->c_cflag |= LINUX_CRTSCTS;

	lios->c_lflag = 0;
	if (bios->c_lflag & ISIG)
		lios->c_lflag |= LINUX_ISIG;
	if (bios->c_lflag & ICANON)
		lios->c_lflag |= LINUX_ICANON;
	if (bios->c_lflag & ECHO)
		lios->c_lflag |= LINUX_ECHO;
	if (bios->c_lflag & ECHOE)
		lios->c_lflag |= LINUX_ECHOE;
	if (bios->c_lflag & ECHOK)
		lios->c_lflag |= LINUX_ECHOK;
	if (bios->c_lflag & ECHONL)
		lios->c_lflag |= LINUX_ECHONL;
	if (bios->c_lflag & NOFLSH)
		lios->c_lflag |= LINUX_NOFLSH;
	if (bios->c_lflag & TOSTOP)
		lios->c_lflag |= LINUX_TOSTOP;
	if (bios->c_lflag & ECHOCTL)
		lios->c_lflag |= LINUX_ECHOCTL;
	if (bios->c_lflag & ECHOPRT)
		lios->c_lflag |= LINUX_ECHOPRT;
	if (bios->c_lflag & ECHOKE)
		lios->c_lflag |= LINUX_ECHOKE;
	if (bios->c_lflag & FLUSHO)
		lios->c_lflag |= LINUX_FLUSHO;
	if (bios->c_lflag & PENDIN)
		lios->c_lflag |= LINUX_PENDIN;
	if (bios->c_lflag & IEXTEN)
		lios->c_lflag |= LINUX_IEXTEN;

	for (i=0; i<LINUX_NCCS; i++)
		lios->c_cc[i] = LINUX_POSIX_VDISABLE;
	lios->c_cc[LINUX_VINTR] = bios->c_cc[VINTR];
	lios->c_cc[LINUX_VQUIT] = bios->c_cc[VQUIT];
	lios->c_cc[LINUX_VERASE] = bios->c_cc[VERASE];
	lios->c_cc[LINUX_VKILL] = bios->c_cc[VKILL];
	lios->c_cc[LINUX_VEOF] = bios->c_cc[VEOF];
	lios->c_cc[LINUX_VEOL] = bios->c_cc[VEOL];
	lios->c_cc[LINUX_VMIN] = bios->c_cc[VMIN];
	lios->c_cc[LINUX_VTIME] = bios->c_cc[VTIME];
	lios->c_cc[LINUX_VEOL2] = bios->c_cc[VEOL2];
	lios->c_cc[LINUX_VSUSP] = bios->c_cc[VSUSP];
	lios->c_cc[LINUX_VSTART] = bios->c_cc[VSTART];
	lios->c_cc[LINUX_VSTOP] = bios->c_cc[VSTOP];
	lios->c_cc[LINUX_VREPRINT] = bios->c_cc[VREPRINT];
	lios->c_cc[LINUX_VDISCARD] = bios->c_cc[VDISCARD];
	lios->c_cc[LINUX_VWERASE] = bios->c_cc[VWERASE];
	lios->c_cc[LINUX_VLNEXT] = bios->c_cc[VLNEXT];

	for (i=0; i<LINUX_NCCS; i++) {
		if (lios->c_cc[i] == _POSIX_VDISABLE)
			lios->c_cc[i] = LINUX_POSIX_VDISABLE;
	}
	lios->c_line = 0;

#ifdef DEBUG
	printf("LINUX: LINUX termios structure (output):\n");
	printf("i=%08x o=%08x c=%08x l=%08x line=%d\n", lios->c_iflag,
	    lios->c_oflag, lios->c_cflag, lios->c_lflag, (int)lios->c_line);
	printf("c_cc ");
	for (i=0; i<LINUX_NCCS; i++) 
		printf("%02x ", lios->c_cc[i]);
	printf("\n");
#endif
}

static void
linux_to_bsd_termios(struct linux_termios *lios, struct termios *bios)
{
	int i;

#ifdef DEBUG
	printf("LINUX: LINUX termios structure (input):\n");
	printf("i=%08x o=%08x c=%08x l=%08x line=%d\n", lios->c_iflag,
	    lios->c_oflag, lios->c_cflag, lios->c_lflag, (int)lios->c_line);
	printf("c_cc ");
	for (i=0; i<LINUX_NCCS; i++)
		printf("%02x ", lios->c_cc[i]);
	printf("\n");
#endif

	bios->c_iflag = 0;
	if (lios->c_iflag & LINUX_IGNBRK)
		bios->c_iflag |= IGNBRK;
	if (lios->c_iflag & LINUX_BRKINT)
		bios->c_iflag |= BRKINT;
	if (lios->c_iflag & LINUX_IGNPAR)
		bios->c_iflag |= IGNPAR;
	if (lios->c_iflag & LINUX_PARMRK)
		bios->c_iflag |= PARMRK;
	if (lios->c_iflag & LINUX_INPCK)
		bios->c_iflag |= INPCK;
	if (lios->c_iflag & LINUX_ISTRIP)
		bios->c_iflag |= ISTRIP;
	if (lios->c_iflag & LINUX_INLCR)
		bios->c_iflag |= INLCR;
	if (lios->c_iflag & LINUX_IGNCR)
		bios->c_iflag |= IGNCR;
	if (lios->c_iflag & LINUX_ICRNL)
		bios->c_iflag |= ICRNL;
	if (lios->c_iflag & LINUX_IXON)
		bios->c_iflag |= IXON;
	if (lios->c_iflag & LINUX_IXANY)
		bios->c_iflag |= IXANY;
	if (lios->c_iflag & LINUX_IXOFF)
		bios->c_iflag |= IXOFF;
	if (lios->c_iflag & LINUX_IMAXBEL)
		bios->c_iflag |= IMAXBEL;

	bios->c_oflag = 0;
	if (lios->c_oflag & LINUX_OPOST)
		bios->c_oflag |= OPOST;
	if (lios->c_oflag & LINUX_ONLCR)
		bios->c_oflag |= ONLCR;
	if (lios->c_oflag & LINUX_XTABS)
		bios->c_oflag |= OXTABS;

	bios->c_cflag = (lios->c_cflag & LINUX_CSIZE) << 4;
	if (lios->c_cflag & LINUX_CSTOPB)
		bios->c_cflag |= CSTOPB;
	if (lios->c_cflag & LINUX_CREAD)
		bios->c_cflag |= CREAD;
	if (lios->c_cflag & LINUX_PARENB)
		bios->c_cflag |= PARENB;
	if (lios->c_cflag & LINUX_PARODD)
		bios->c_cflag |= PARODD;
	if (lios->c_cflag & LINUX_HUPCL)
		bios->c_cflag |= HUPCL;
	if (lios->c_cflag & LINUX_CLOCAL)
		bios->c_cflag |= CLOCAL;
	if (lios->c_cflag & LINUX_CRTSCTS)
		bios->c_cflag |= CRTSCTS;

	bios->c_lflag = 0;
	if (lios->c_lflag & LINUX_ISIG)
		bios->c_lflag |= ISIG;
	if (lios->c_lflag & LINUX_ICANON)
		bios->c_lflag |= ICANON;
	if (lios->c_lflag & LINUX_ECHO)
		bios->c_lflag |= ECHO;
	if (lios->c_lflag & LINUX_ECHOE)
		bios->c_lflag |= ECHOE;
	if (lios->c_lflag & LINUX_ECHOK)
		bios->c_lflag |= ECHOK;
	if (lios->c_lflag & LINUX_ECHONL)
		bios->c_lflag |= ECHONL;
	if (lios->c_lflag & LINUX_NOFLSH)
		bios->c_lflag |= NOFLSH;
	if (lios->c_lflag & LINUX_TOSTOP)
		bios->c_lflag |= TOSTOP;
	if (lios->c_lflag & LINUX_ECHOCTL)
		bios->c_lflag |= ECHOCTL;
	if (lios->c_lflag & LINUX_ECHOPRT)
		bios->c_lflag |= ECHOPRT;
	if (lios->c_lflag & LINUX_ECHOKE)
		bios->c_lflag |= ECHOKE;
	if (lios->c_lflag & LINUX_FLUSHO)
		bios->c_lflag |= FLUSHO;
	if (lios->c_lflag & LINUX_PENDIN)
		bios->c_lflag |= PENDIN;
	if (lios->c_lflag & LINUX_IEXTEN)
		bios->c_lflag |= IEXTEN;

	for (i=0; i<NCCS; i++)
		bios->c_cc[i] = _POSIX_VDISABLE;
	bios->c_cc[VINTR] = lios->c_cc[LINUX_VINTR];
	bios->c_cc[VQUIT] = lios->c_cc[LINUX_VQUIT];
	bios->c_cc[VERASE] = lios->c_cc[LINUX_VERASE];
	bios->c_cc[VKILL] = lios->c_cc[LINUX_VKILL];
	bios->c_cc[VEOF] = lios->c_cc[LINUX_VEOF];
	bios->c_cc[VEOL] = lios->c_cc[LINUX_VEOL];
	bios->c_cc[VMIN] = lios->c_cc[LINUX_VMIN];
	bios->c_cc[VTIME] = lios->c_cc[LINUX_VTIME];
	bios->c_cc[VEOL2] = lios->c_cc[LINUX_VEOL2];
	bios->c_cc[VSUSP] = lios->c_cc[LINUX_VSUSP];
	bios->c_cc[VSTART] = lios->c_cc[LINUX_VSTART];
	bios->c_cc[VSTOP] = lios->c_cc[LINUX_VSTOP];
	bios->c_cc[VREPRINT] = lios->c_cc[LINUX_VREPRINT];
	bios->c_cc[VDISCARD] = lios->c_cc[LINUX_VDISCARD];
	bios->c_cc[VWERASE] = lios->c_cc[LINUX_VWERASE];
	bios->c_cc[VLNEXT] = lios->c_cc[LINUX_VLNEXT];

	for (i=0; i<NCCS; i++) {
		if (bios->c_cc[i] == LINUX_POSIX_VDISABLE)
			bios->c_cc[i] = _POSIX_VDISABLE;
	}

	bios->c_ispeed = bios->c_ospeed =
	    linux_to_bsd_speed(lios->c_cflag & LINUX_CBAUD, sptab);

#ifdef DEBUG
	printf("LINUX: BSD termios structure (output):\n");
	printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d\n",
	    bios->c_iflag, bios->c_oflag, bios->c_cflag, bios->c_lflag,
	    bios->c_ispeed, bios->c_ospeed);
	printf("c_cc ");
	for (i=0; i<NCCS; i++) 
		printf("%02x ", bios->c_cc[i]);
	printf("\n");
#endif
}

static void
bsd_to_linux_termio(struct termios *bios, struct linux_termio *lio)
{
	struct linux_termios lios;

	bsd_to_linux_termios(bios, &lios);
	lio->c_iflag = lios.c_iflag;
	lio->c_oflag = lios.c_oflag;
	lio->c_cflag = lios.c_cflag;
	lio->c_lflag = lios.c_lflag;
	lio->c_line  = lios.c_line;
	memcpy(lio->c_cc, lios.c_cc, LINUX_NCC);
}

static void
linux_to_bsd_termio(struct linux_termio *lio, struct termios *bios)
{
	struct linux_termios lios;
	int i;

	lios.c_iflag = lio->c_iflag;
	lios.c_oflag = lio->c_oflag;
	lios.c_cflag = lio->c_cflag;
	lios.c_lflag = lio->c_lflag;
	for (i=LINUX_NCC; i<LINUX_NCCS; i++)
		lios.c_cc[i] = LINUX_POSIX_VDISABLE;
	memcpy(lios.c_cc, lio->c_cc, LINUX_NCC);
	linux_to_bsd_termios(&lios, bios);
}

static int
linux_ioctl_termio(struct proc *p, struct linux_ioctl_args *args)
{
	struct termios bios;
	struct linux_termios lios;
	struct linux_termio lio;
	struct file *fp = p->p_fd->fd_ofiles[args->fd];
	int error;

	switch (args->cmd & 0xffff) {

	case LINUX_TCGETS:
		error = fo_ioctl(fp, TIOCGETA, (caddr_t)&bios, p);
		if (error)
			return (error);
		bsd_to_linux_termios(&bios, &lios);
		return copyout(&lios, (caddr_t)args->arg, sizeof(lios));

	case LINUX_TCSETS:
		error = copyin((caddr_t)args->arg, &lios, sizeof(lios));
		if (error)
			return (error);
		linux_to_bsd_termios(&lios, &bios);
		return (fo_ioctl(fp, TIOCSETA, (caddr_t)&bios, p));

	case LINUX_TCSETSW:
		error = copyin((caddr_t)args->arg, &lios, sizeof(lios));
		if (error)
			return (error);
		linux_to_bsd_termios(&lios, &bios);
		return (fo_ioctl(fp, TIOCSETAW, (caddr_t)&bios, p));

	case LINUX_TCSETSF:
		error = copyin((caddr_t)args->arg, &lios, sizeof(lios));
		if (error)
			return (error);
		linux_to_bsd_termios(&lios, &bios);
		return (fo_ioctl(fp, TIOCSETAF, (caddr_t)&bios, p));

	case LINUX_TCGETA:
		error = fo_ioctl(fp, TIOCGETA, (caddr_t)&bios, p);
		if (error)
			return (error);
		bsd_to_linux_termio(&bios, &lio);
		return (copyout(&lio, (caddr_t)args->arg, sizeof(lio)));

	case LINUX_TCSETA:
		error = copyin((caddr_t)args->arg, &lio, sizeof(lio));
		if (error)
			return (error);
		linux_to_bsd_termio(&lio, &bios);
		return (fo_ioctl(fp, TIOCSETA, (caddr_t)&bios, p));

	case LINUX_TCSETAW:
		error = copyin((caddr_t)args->arg, &lio, sizeof(lio));
		if (error)
			return (error);
		linux_to_bsd_termio(&lio, &bios);
		return (fo_ioctl(fp, TIOCSETAW, (caddr_t)&bios, p));

	case LINUX_TCSETAF:
		error = copyin((caddr_t)args->arg, &lio, sizeof(lio));
		if (error)
			return (error);
		linux_to_bsd_termio(&lio, &bios);
		return (fo_ioctl(fp, TIOCSETAF, (caddr_t)&bios, p));

	/* LINUX_TCSBRK */

	case LINUX_TCXONC: {
		switch (args->arg) {
		case LINUX_TCOOFF:
			args->cmd = TIOCSTOP;
			break;
		case LINUX_TCOON:
			args->cmd = TIOCSTART;
			break;
		case LINUX_TCIOFF:
		case LINUX_TCION: {
			int c;
			struct write_args wr;
			error = fo_ioctl(fp, TIOCGETA, (caddr_t)&bios, p);
			if (error)
				return (error);
			c = (args->arg == LINUX_TCIOFF) ? VSTOP : VSTART;
			c = bios.c_cc[c];
			if (c != _POSIX_VDISABLE) {
				wr.fd = args->fd;
				wr.buf = &c;
				wr.nbyte = sizeof(c);
				return (write(p, &wr));
			} else
				return (0);
		}
		default:
			return (EINVAL);
		}
		args->arg = 0;
		return (ioctl(p, (struct ioctl_args *)args));
	}

	case LINUX_TCFLSH: {
		args->cmd = TIOCFLUSH;
		switch (args->arg) {
		case LINUX_TCIFLUSH:
			args->arg = FREAD;
			break;
		case LINUX_TCOFLUSH:
			args->arg = FWRITE;
			break;
		case LINUX_TCIOFLUSH:
			args->arg = FREAD | FWRITE;
			break;
		default:
			return (EINVAL);
		}
		return (ioctl(p, (struct ioctl_args *)args));
	}

	case LINUX_TIOCEXCL:
		args->cmd = TIOCEXCL;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_TIOCNXCL:
		args->cmd = TIOCNXCL;
		return (ioctl(p, (struct ioctl_args *)args));

	/* LINUX_TIOCSCTTY */

	case LINUX_TIOCGPGRP:
		args->cmd = TIOCGPGRP;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_TIOCSPGRP:
		args->cmd = TIOCSPGRP;
		return (ioctl(p, (struct ioctl_args *)args));

	/* LINUX_TIOCOUTQ */
	/* LINUX_TIOCSTI */

	case LINUX_TIOCGWINSZ:
		args->cmd = TIOCGWINSZ;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_TIOCSWINSZ:
		args->cmd = TIOCSWINSZ;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_TIOCMGET:
		args->cmd = TIOCMGET;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_TIOCMBIS:
		args->cmd = TIOCMBIS;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_TIOCMBIC:
		args->cmd = TIOCMBIC;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_TIOCMSET:
		args->cmd = TIOCMSET;
		return (ioctl(p, (struct ioctl_args *)args));

	/* TIOCGSOFTCAR */
	/* TIOCSSOFTCAR */

	case LINUX_FIONREAD: /* LINUX_TIOCINQ */
		args->cmd = FIONREAD;
		return (ioctl(p, (struct ioctl_args *)args));

	/* LINUX_TIOCLINUX */

	case LINUX_TIOCCONS:
		args->cmd = TIOCCONS;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_TIOCGSERIAL: {
		struct linux_serial_struct lss;
		lss.type = LINUX_PORT_16550A;
		lss.flags = 0;
		lss.close_delay = 0;
		return copyout(&lss, (caddr_t)args->arg, sizeof(lss));
	}

	case LINUX_TIOCSSERIAL: {
		struct linux_serial_struct lss;
		error = copyin((caddr_t)args->arg, &lss, sizeof(lss));
		if (error)
			return (error);
		/* XXX - It really helps to have an implementation that
		 * does nothing. NOT!
		 */
		return (0);
	}

	/* LINUX_TIOCPKT */

	case LINUX_FIONBIO:
		args->cmd = FIONBIO;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_TIOCNOTTY:
		args->cmd = TIOCNOTTY;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_TIOCSETD: {
		int line;
		switch (args->arg) {
		case LINUX_N_TTY:
			line = TTYDISC;
			break;
		case LINUX_N_SLIP:
			line = SLIPDISC;
			break;
		case LINUX_N_PPP:
			line = PPPDISC;
			break;
		default:
			return (EINVAL);
		}
		return (fo_ioctl(fp, TIOCSETD, (caddr_t)&line, p));
	}

	case LINUX_TIOCGETD: {
		int linux_line;
		int bsd_line = TTYDISC;
		error = fo_ioctl(fp, TIOCGETD, (caddr_t)&bsd_line, p);
		if (error)
			return (error);
		switch (bsd_line) {
		case TTYDISC:
			linux_line = LINUX_N_TTY;
			break;
		case SLIPDISC:
			linux_line = LINUX_N_SLIP;
			break;
		case PPPDISC:
			linux_line = LINUX_N_PPP;
			break;
		default:
			return (EINVAL);
		}
		return (copyout(&linux_line, (caddr_t)args->arg, sizeof(int)));
	}

	/* LINUX_TCSBRKP */
	/* LINUX_TIOCTTYGSTRUCT */

	case LINUX_FIONCLEX:
		args->cmd = FIONCLEX;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_FIOCLEX:
		args->cmd = FIOCLEX;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_FIOASYNC:
		args->cmd = FIOASYNC;
		return (ioctl(p, (struct ioctl_args *)args));

	/* LINUX_TIOCSERCONFIG */
	/* LINUX_TIOCSERGWILD */
	/* LINUX_TIOCSERSWILD */
	/* LINUX_TIOCGLCKTRMIOS */
	/* LINUX_TIOCSLCKTRMIOS */

	}

	return (ENOIOCTL);
}

/*
 * CDROM related ioctls
 */

struct linux_cdrom_msf
{
	u_char	cdmsf_min0;
	u_char	cdmsf_sec0;
	u_char	cdmsf_frame0;
	u_char	cdmsf_min1;
	u_char	cdmsf_sec1;
	u_char	cdmsf_frame1;
};

struct linux_cdrom_tochdr
{
	u_char	cdth_trk0;
	u_char	cdth_trk1;
};

union linux_cdrom_addr
{
	struct {
		u_char	minute;
		u_char	second;
		u_char	frame;
	} msf;
	int	lba;
};

struct linux_cdrom_tocentry
{
	u_char	cdte_track;     
	u_char	cdte_adr:4;
	u_char	cdte_ctrl:4;
	u_char	cdte_format;    
	union linux_cdrom_addr cdte_addr;
	u_char	cdte_datamode;  
};

struct linux_cdrom_subchnl
{
	u_char	cdsc_format;
	u_char	cdsc_audiostatus;
	u_char	cdsc_adr:4;
	u_char	cdsc_ctrl:4;
	u_char	cdsc_trk;
	u_char	cdsc_ind;
	union linux_cdrom_addr cdsc_absaddr;
	union linux_cdrom_addr cdsc_reladdr;
};

static void
bsd_to_linux_msf_lba(u_char af, union msf_lba *bp, union linux_cdrom_addr *lp)
{
	if (af == CD_LBA_FORMAT)
		lp->lba = bp->lba;
	else {
		lp->msf.minute = bp->msf.minute;
		lp->msf.second = bp->msf.second;
		lp->msf.frame = bp->msf.frame;
	}
}

static void
set_linux_cdrom_addr(union linux_cdrom_addr *addr, int format, int lba)
{
	if (format == LINUX_CDROM_MSF) {
		addr->msf.frame = lba % 75;
		lba /= 75;
		lba += 2;
		addr->msf.second = lba % 60;
		addr->msf.minute = lba / 60;
	} else
		addr->lba = lba;
}

static int
linux_ioctl_cdrom(struct proc *p, struct linux_ioctl_args *args)
{
	struct file *fp = p->p_fd->fd_ofiles[args->fd];
	int error;

	switch (args->cmd & 0xffff) {

	case LINUX_CDROMPAUSE:
		args->cmd = CDIOCPAUSE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_CDROMRESUME:
		args->cmd = CDIOCRESUME;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_CDROMPLAYMSF:
		args->cmd = CDIOCPLAYMSF;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_CDROMPLAYTRKIND:
		args->cmd = CDIOCPLAYTRACKS;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_CDROMREADTOCHDR: {
		struct ioc_toc_header th;
		struct linux_cdrom_tochdr lth;
		error = fo_ioctl(fp, CDIOREADTOCHEADER, (caddr_t)&th, p);
		if (!error) {
			lth.cdth_trk0 = th.starting_track;
			lth.cdth_trk1 = th.ending_track;
			copyout(&lth, (caddr_t)args->arg, sizeof(lth));
		}
		return (error);
	}

	case LINUX_CDROMREADTOCENTRY: {
		struct linux_cdrom_tocentry lte, *ltep =
		    (struct linux_cdrom_tocentry *)args->arg;
		struct ioc_read_toc_single_entry irtse;
		irtse.address_format = ltep->cdte_format;
		irtse.track = ltep->cdte_track;
		error = fo_ioctl(fp, CDIOREADTOCENTRY, (caddr_t)&irtse, p);
		if (!error) {
			lte = *ltep;
			lte.cdte_ctrl = irtse.entry.control;
			lte.cdte_adr = irtse.entry.addr_type;
			bsd_to_linux_msf_lba(irtse.address_format,
			    &irtse.entry.addr, &lte.cdte_addr);
			copyout(&lte, (caddr_t)args->arg, sizeof(lte));
		}
		return (error);
	}

	case LINUX_CDROMSTOP:
		args->cmd = CDIOCSTOP;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_CDROMSTART:
		args->cmd = CDIOCSTART;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_CDROMEJECT:
		args->cmd = CDIOCEJECT;
		return (ioctl(p, (struct ioctl_args *)args));

	/* LINUX_CDROMVOLCTRL */

	case LINUX_CDROMSUBCHNL: {
		struct linux_cdrom_subchnl sc;
		struct ioc_read_subchannel bsdsc;
		struct cd_sub_channel_info *bsdinfo;
		caddr_t sg = stackgap_init();
		bsdinfo = (struct cd_sub_channel_info*)stackgap_alloc(&sg,
		    sizeof(struct cd_sub_channel_info));
		bsdsc.address_format = CD_LBA_FORMAT;
		bsdsc.data_format = CD_CURRENT_POSITION;
		bsdsc.track = 0;
		bsdsc.data_len = sizeof(struct cd_sub_channel_info);
		bsdsc.data = bsdinfo;
		error = fo_ioctl(fp, CDIOCREADSUBCHANNEL, (caddr_t)&bsdsc, p);
		if (error)
			return (error);
		error = copyin((caddr_t)args->arg, &sc,
		    sizeof(struct linux_cdrom_subchnl));
		if (error)
			return (error);
		sc.cdsc_audiostatus = bsdinfo->header.audio_status;
		sc.cdsc_adr = bsdinfo->what.position.addr_type;
		sc.cdsc_ctrl = bsdinfo->what.position.control;
		sc.cdsc_trk = bsdinfo->what.position.track_number;
		sc.cdsc_ind = bsdinfo->what.position.index_number;
		set_linux_cdrom_addr(&sc.cdsc_absaddr, sc.cdsc_format,
		    bsdinfo->what.position.absaddr.lba);
		set_linux_cdrom_addr(&sc.cdsc_reladdr, sc.cdsc_format,
		    bsdinfo->what.position.reladdr.lba);
		error = copyout(&sc, (caddr_t)args->arg,
		    sizeof(struct linux_cdrom_subchnl));
		return (error);
	}

	/* LINUX_CDROMREADMODE2 */
	/* LINUX_CDROMREADMODE1 */
	/* LINUX_CDROMREADAUDIO */
	/* LINUX_CDROMEJECT_SW */
	/* LINUX_CDROMMULTISESSION */
	/* LINUX_CDROM_GET_UPC */

	case LINUX_CDROMRESET:
		args->cmd = CDIOCRESET;
		return (ioctl(p, (struct ioctl_args *)args));

	/* LINUX_CDROMVOLREAD */
	/* LINUX_CDROMREADRAW */
	/* LINUX_CDROMREADCOOKED */
	/* LINUX_CDROMSEEK */
	/* LINUX_CDROMPLAYBLK */
	/* LINUX_CDROMREADALL */
	/* LINUX_CDROMCLOSETRAY */
	/* LINUX_CDROMLOADFROMSLOT */

	}

	return (ENOIOCTL);
}

/*
 * Sound related ioctls
 */

static unsigned dirbits[4] = { IOC_VOID, IOC_IN, IOC_OUT, IOC_INOUT };

#define	SETDIR(c)	(((c) & ~IOC_DIRMASK) | dirbits[args->cmd >> 30])

static int
linux_ioctl_sound(struct proc *p, struct linux_ioctl_args *args)
{

	switch (args->cmd & 0xffff) {

	case LINUX_SOUND_MIXER_WRITE_VOLUME:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_VOLUME);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_BASS:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_BASS);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_TREBLE:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_TREBLE);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_SYNTH:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_SYNTH);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_PCM:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_PCM);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_SPEAKER:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_SPEAKER);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_MIC:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_MIC);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_CD:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_CD);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_IMIX:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_IMIX);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_ALTPCM:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_ALTPCM);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_RECLEV:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_RECLEV);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_IGAIN:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_IGAIN);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_OGAIN:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_OGAIN);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE1:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE1);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE2:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE2);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE3:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE3);
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_OSS_GETVERSION: {
		int version = linux_get_oss_version(p);
		return (copyout(&version, (caddr_t)args->arg, sizeof(int)));
	}

	case LINUX_SOUND_MIXER_READ_DEVMASK:
		args->cmd = SOUND_MIXER_READ_DEVMASK;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_RESET:
		args->cmd = SNDCTL_DSP_RESET;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SYNC:
		args->cmd = SNDCTL_DSP_SYNC;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SPEED:
		args->cmd = SNDCTL_DSP_SPEED;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_STEREO:
		args->cmd = SNDCTL_DSP_STEREO;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETBLKSIZE: /* LINUX_SNDCTL_DSP_SETBLKSIZE */
		args->cmd = SNDCTL_DSP_GETBLKSIZE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SETFMT:
		args->cmd = SNDCTL_DSP_SETFMT;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_PCM_WRITE_CHANNELS:
		args->cmd = SOUND_PCM_WRITE_CHANNELS;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SOUND_PCM_WRITE_FILTER:
		args->cmd = SOUND_PCM_WRITE_FILTER;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_POST:
		args->cmd = SNDCTL_DSP_POST;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SUBDIVIDE:
		args->cmd = SNDCTL_DSP_SUBDIVIDE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SETFRAGMENT:
		args->cmd = SNDCTL_DSP_SETFRAGMENT;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETFMTS:
		args->cmd = SNDCTL_DSP_GETFMTS;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETOSPACE:
		args->cmd = SNDCTL_DSP_GETOSPACE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETISPACE:
		args->cmd = SNDCTL_DSP_GETISPACE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_NONBLOCK:
		args->cmd = SNDCTL_DSP_NONBLOCK;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETCAPS:
		args->cmd = SNDCTL_DSP_GETCAPS;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SETTRIGGER: /* LINUX_SNDCTL_GETTRIGGER */
		args->cmd = SNDCTL_DSP_SETTRIGGER;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETIPTR:
		args->cmd = SNDCTL_DSP_GETIPTR;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETOPTR:
		args->cmd = SNDCTL_DSP_GETOPTR;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETODELAY:
		args->cmd = SNDCTL_DSP_GETODELAY;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_RESET:
		args->cmd = SNDCTL_SEQ_RESET;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_SYNC:
		args->cmd = SNDCTL_SEQ_SYNC;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SYNTH_INFO:
		args->cmd = SNDCTL_SYNTH_INFO;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_CTRLRATE:
		args->cmd = SNDCTL_SEQ_CTRLRATE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_GETOUTCOUNT:
		args->cmd = SNDCTL_SEQ_GETOUTCOUNT;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_GETINCOUNT:
		args->cmd = SNDCTL_SEQ_GETINCOUNT;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_PERCMODE:
		args->cmd = SNDCTL_SEQ_PERCMODE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_FM_LOAD_INSTR:
		args->cmd = SNDCTL_FM_LOAD_INSTR;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_TESTMIDI:
		args->cmd = SNDCTL_SEQ_TESTMIDI;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_RESETSAMPLES:
		args->cmd = SNDCTL_SEQ_RESETSAMPLES;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_NRSYNTHS:
		args->cmd = SNDCTL_SEQ_NRSYNTHS;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_NRMIDIS:
		args->cmd = SNDCTL_SEQ_NRMIDIS;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_MIDI_INFO:
		args->cmd = SNDCTL_MIDI_INFO;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_TRESHOLD:
		args->cmd = SNDCTL_SEQ_TRESHOLD;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SYNTH_MEMAVL:
		args->cmd = SNDCTL_SYNTH_MEMAVL;
		return (ioctl(p, (struct ioctl_args *)args));

	}

	return (ENOIOCTL);
}

/*
 * Console related ioctls
 */

#define ISSIGVALID(sig)		((sig) > 0 && (sig) < NSIG)

static int
linux_ioctl_console(struct proc *p, struct linux_ioctl_args *args)
{
	struct file *fp = p->p_fd->fd_ofiles[args->fd];

	switch (args->cmd & 0xffff) {

	case LINUX_KIOCSOUND:
		args->cmd = KIOCSOUND;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_KDMKTONE:
		args->cmd = KDMKTONE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_KDGETLED:
		args->cmd = KDGETLED;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_KDSETLED:
		args->cmd = KDSETLED;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_KDSETMODE:
		args->cmd = KDSETMODE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_KDGETMODE:
		args->cmd = KDGETMODE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_KDGKBMODE:
		args->cmd = KDGKBMODE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_KDSKBMODE: {
		int kbdmode;
		switch (args->arg) {
		case LINUX_KBD_RAW:
			kbdmode = K_RAW;
			break;
		case LINUX_KBD_XLATE:
			kbdmode = K_XLATE;
			break;
		case LINUX_KBD_MEDIUMRAW:
			kbdmode = K_RAW;
			break;
		default:
			return (EINVAL);
		}
		return (fo_ioctl(fp, KDSKBMODE, (caddr_t)&kbdmode, p));
	}

	case LINUX_VT_OPENQRY:
		args->cmd = VT_OPENQRY;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_VT_GETMODE:
		args->cmd = VT_GETMODE;
		return  (ioctl(p, (struct ioctl_args *)args));

	case LINUX_VT_SETMODE: {
		struct vt_mode *mode;
		args->cmd = VT_SETMODE;
		mode = (struct vt_mode *)args->arg;
		if (!ISSIGVALID(mode->frsig) && ISSIGVALID(mode->acqsig))
			mode->frsig = mode->acqsig;
		return (ioctl(p, (struct ioctl_args *)args));
	}

	case LINUX_VT_GETSTATE:
		args->cmd = VT_GETACTIVE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_VT_RELDISP:
		args->cmd = VT_RELDISP;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_VT_ACTIVATE:
		args->cmd = VT_ACTIVATE;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_VT_WAITACTIVE:
		args->cmd = VT_WAITACTIVE;
		return (ioctl(p, (struct ioctl_args *)args));

	}
	
	return (ENOIOCTL);
}

/*
 * Socket related ioctls
 */

static int
linux_ioctl_socket(struct proc *p, struct linux_ioctl_args *args)
{

	switch (args->cmd & 0xffff) {

	case LINUX_FIOSETOWN:
		args->cmd = FIOSETOWN;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SIOCSPGRP:
		args->cmd = SIOCSPGRP;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_FIOGETOWN:
		args->cmd = FIOGETOWN;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SIOCGPGRP:
		args->cmd = SIOCGPGRP;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SIOCATMARK:
		args->cmd = SIOCATMARK;
		return (ioctl(p, (struct ioctl_args *)args));

	/* LINUX_SIOCGSTAMP */

	case LINUX_SIOCGIFCONF:
		args->cmd = OSIOCGIFCONF;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SIOCGIFFLAGS:
		args->cmd = SIOCGIFFLAGS;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SIOCGIFADDR:
		args->cmd = OSIOCGIFADDR;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SIOCGIFDSTADDR:
		args->cmd = OSIOCGIFDSTADDR;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SIOCGIFBRDADDR:
		args->cmd = OSIOCGIFBRDADDR;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SIOCGIFNETMASK:
		args->cmd = OSIOCGIFNETMASK;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SIOCGIFHWADDR: {
		int ifn;
		struct ifnet *ifp;
		struct ifaddr *ifa;
		struct sockaddr_dl *sdl;
		struct linux_ifreq *ifr = (struct linux_ifreq *)args->arg;

		/* Note that we don't actually respect the name in the ifreq
		 * structure, as Linux interface names are all different.
		 */
		for (ifn = 0; ifn < if_index; ifn++) {
			ifp = ifnet_addrs[ifn]->ifa_ifp;
			if (ifp->if_type == IFT_ETHER) {
				ifa = TAILQ_FIRST(&ifp->if_addrhead);
				while (ifa) {
					sdl=(struct sockaddr_dl*)ifa->ifa_addr;
					if (sdl != NULL &&
					    (sdl->sdl_family == AF_LINK) &&
					    (sdl->sdl_type == IFT_ETHER)) {
						return (copyout(LLADDR(sdl),
						    &ifr->ifr_hwaddr.sa_data,
						    LINUX_IFHWADDRLEN));
					}
					ifa = TAILQ_NEXT(ifa, ifa_link);
				}
			}
		}
		return (ENOENT);
	}

	case LINUX_SIOCADDMULTI:
		args->cmd = SIOCADDMULTI;
		return (ioctl(p, (struct ioctl_args *)args));

	case LINUX_SIOCDELMULTI:
		args->cmd = SIOCDELMULTI;
		return (ioctl(p, (struct ioctl_args *)args));

	}

	return (ENOIOCTL);
}

/*
 * main ioctl syscall function
 */

int
linux_ioctl(struct proc *p, struct linux_ioctl_args *args)
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct handler_element *he;
	int error, cmd;

#ifdef DEBUG
	printf("Linux-emul(%ld): ioctl(%d, %04lx, *)\n", (long)p->p_pid,
	    args->fd, args->cmd);
#endif

	if ((unsigned)args->fd >= fdp->fd_nfiles)
		return (EBADF);

	fp = fdp->fd_ofiles[args->fd];
	if (fp == NULL || (fp->f_flag & (FREAD|FWRITE)) == 0)
		return (EBADF);

	/* Iterate over the ioctl handlers */
	cmd = args->cmd & 0xffff;
	TAILQ_FOREACH(he, &handlers, list) {
		if (cmd >= he->low && cmd <= he->high) {
			error = (*he->func)(p, args);
			if (error != ENOIOCTL)
				return (error);
		}
	}

	printf("linux: 'ioctl' fd=%d, cmd=%x ('%c',%d) not implemented\n",
	    args->fd, (int)(args->cmd & 0xffff),
	    (int)(args->cmd & 0xff00) >> 8, (int)(args->cmd & 0xff));

	return (EINVAL);
}

int
linux_ioctl_register_handler(struct linux_ioctl_handler *h)
{
	struct handler_element *he, *cur;

	if (h == NULL || h->func == NULL)
		return (EINVAL);

	/*
	 * Reuse the element if the handler is already on the list, otherwise
	 * create a new element.
	 */
	TAILQ_FOREACH(he, &handlers, list) {
		if (he->func == h->func)
			break;
	}
	if (he == NULL) {
		MALLOC(he, struct handler_element *, sizeof(*he),
		    M_LINUX, M_WAITOK);
		he->func = h->func;
	} else
		TAILQ_REMOVE(&handlers, he, list);
	
	/* Initialize range information. */
	he->low = h->low;
	he->high = h->high;
	he->span = h->high - h->low + 1;

	/* Add the element to the list, sorted on span. */
	TAILQ_FOREACH(cur, &handlers, list) {
		if (cur->span > he->span) {
			TAILQ_INSERT_BEFORE(cur, he, list);
			return (0);
		}
	}
	TAILQ_INSERT_TAIL(&handlers, he, list);

	return (0);
}

int
linux_ioctl_unregister_handler(struct linux_ioctl_handler *h)
{
	struct handler_element *he;

	if (h == NULL || h->func == NULL)
		return (EINVAL);

	TAILQ_FOREACH(he, &handlers, list) {
		if (he->func == h->func) {
			TAILQ_REMOVE(&handlers, he, list);
			FREE(he, M_LINUX);
			return (0);
		}
	}

	return (EINVAL);
}

int
linux_ioctl_register_handlers(struct linker_set *s)
{
	int error, i;

	if (s == NULL)
		return (EINVAL);

	for (i = 0; i < s->ls_length; i++) {
		error = linux_ioctl_register_handler(s->ls_items[i]);
		if (error)
			return (error);
	}

	return (0);
}

int
linux_ioctl_unregister_handlers(struct linker_set *s)
{
	int error, i;

	if (s == NULL)
		return (EINVAL);

	for (i = 0; i < s->ls_length; i++) {
		error = linux_ioctl_unregister_handler(s->ls_items[i]);
		if (error)
			return (error);
	}

	return (0);
}
