/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/cdio.h>
#include <sys/consio.h>
#include <sys/disk.h>
#include <sys/dvdio.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/jail.h>
#include <sys/kbio.h>
#include <sys/kcov.h>
#include <sys/kernel.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/sockio.h>
#include <sys/soundcard.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/sx.h>
#include <sys/tty.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <dev/evdev/input.h>
#include <dev/usb/usb_ioctl.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux_common.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_socket.h>
#include <compat/linux/linux_time.h>
#include <compat/linux/linux_util.h>

#include <contrib/v4l/videodev.h>
#include <compat/linux/linux_videodev_compat.h>

#include <contrib/v4l/videodev2.h>
#include <compat/linux/linux_videodev2_compat.h>

#include <cam/scsi/scsi_sg.h>

#include <dev/nvme/nvme_linux.h>

#define	DEFINE_LINUX_IOCTL_SET(shortname, SHORTNAME)		\
static linux_ioctl_function_t linux_ioctl_ ## shortname;	\
static struct linux_ioctl_handler shortname ## _handler = {	\
	.func = linux_ioctl_ ## shortname,			\
	.low = LINUX_IOCTL_ ## SHORTNAME ## _MIN,		\
	.high = LINUX_IOCTL_ ## SHORTNAME ## _MAX,		\
};								\
DATA_SET(linux_ioctl_handler_set, shortname ## _handler)

DEFINE_LINUX_IOCTL_SET(cdrom, CDROM);
DEFINE_LINUX_IOCTL_SET(vfat, VFAT);
DEFINE_LINUX_IOCTL_SET(console, CONSOLE);
DEFINE_LINUX_IOCTL_SET(hdio, HDIO);
DEFINE_LINUX_IOCTL_SET(disk, DISK);
DEFINE_LINUX_IOCTL_SET(socket, SOCKET);
DEFINE_LINUX_IOCTL_SET(sound, SOUND);
DEFINE_LINUX_IOCTL_SET(termio, TERMIO);
DEFINE_LINUX_IOCTL_SET(private, PRIVATE);
DEFINE_LINUX_IOCTL_SET(drm, DRM);
DEFINE_LINUX_IOCTL_SET(sg, SG);
DEFINE_LINUX_IOCTL_SET(v4l, VIDEO);
DEFINE_LINUX_IOCTL_SET(v4l2, VIDEO2);
DEFINE_LINUX_IOCTL_SET(fbsd_usb, FBSD_LUSB);
DEFINE_LINUX_IOCTL_SET(evdev, EVDEV);
DEFINE_LINUX_IOCTL_SET(kcov, KCOV);
#ifndef COMPAT_LINUX32
DEFINE_LINUX_IOCTL_SET(nvme, NVME);
#endif

#undef DEFINE_LINUX_IOCTL_SET

static int linux_ioctl_special(struct thread *, struct linux_ioctl_args *);

/*
 * Keep sorted by low.
 */
static struct linux_ioctl_handler linux_ioctls[] = {
	{ .func = linux_ioctl_termio, .low = LINUX_IOCTL_TERMIO_MIN,
	    .high = LINUX_IOCTL_TERMIO_MAX },
};

#ifdef __i386__
static TAILQ_HEAD(, linux_ioctl_handler_element) linux_ioctl_handlers =
    TAILQ_HEAD_INITIALIZER(linux_ioctl_handlers);
static struct sx linux_ioctl_sx;
SX_SYSINIT(linux_ioctl, &linux_ioctl_sx, "Linux ioctl handlers");
#else
extern TAILQ_HEAD(, linux_ioctl_handler_element) linux_ioctl_handlers;
extern struct sx linux_ioctl_sx;
#endif
#ifdef COMPAT_LINUX32
static TAILQ_HEAD(, linux_ioctl_handler_element) linux32_ioctl_handlers =
    TAILQ_HEAD_INITIALIZER(linux32_ioctl_handlers);
#endif

/*
 * hdio related ioctls for VMWare support
 */

struct linux_hd_geometry {
	uint8_t		heads;
	uint8_t		sectors;
	uint16_t	cylinders;
	uint32_t	start;
};

struct linux_hd_big_geometry {
	uint8_t		heads;
	uint8_t		sectors;
	uint32_t	cylinders;
	uint32_t	start;
};

static int
linux_ioctl_hdio(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error;
	u_int sectorsize, fwcylinders, fwheads, fwsectors;
	off_t mediasize, bytespercyl;

	error = fget(td, args->fd, &cap_ioctl_rights, &fp);
	if (error != 0)
		return (error);
	switch (args->cmd & 0xffff) {
	case LINUX_HDIO_GET_GEO:
	case LINUX_HDIO_GET_GEO_BIG:
		error = fo_ioctl(fp, DIOCGMEDIASIZE,
			(caddr_t)&mediasize, td->td_ucred, td);
		if (!error)
			error = fo_ioctl(fp, DIOCGSECTORSIZE,
				(caddr_t)&sectorsize, td->td_ucred, td);
		if (!error)
			error = fo_ioctl(fp, DIOCGFWHEADS,
				(caddr_t)&fwheads, td->td_ucred, td);
		if (!error)
			error = fo_ioctl(fp, DIOCGFWSECTORS,
				(caddr_t)&fwsectors, td->td_ucred, td);
		/*
		 * XXX: DIOCGFIRSTOFFSET is not yet implemented, so
		 * so pretend that GEOM always says 0. This is NOT VALID
		 * for slices or partitions, only the per-disk raw devices.
		 */

		fdrop(fp, td);
		if (error)
			return (error);
		/*
		 * 1. Calculate the number of bytes in a cylinder,
		 *    given the firmware's notion of heads and sectors
		 *    per cylinder.
		 * 2. Calculate the number of cylinders, given the total
		 *    size of the media.
		 * All internal calculations should have 64-bit precision.
		 */
		bytespercyl = (off_t) sectorsize * fwheads * fwsectors;
		fwcylinders = mediasize / bytespercyl;

		if ((args->cmd & 0xffff) == LINUX_HDIO_GET_GEO) {
			struct linux_hd_geometry hdg;

			hdg.cylinders = fwcylinders;
			hdg.heads = fwheads;
			hdg.sectors = fwsectors;
			hdg.start = 0;
			error = copyout(&hdg, (void *)args->arg, sizeof(hdg));
		} else if ((args->cmd & 0xffff) == LINUX_HDIO_GET_GEO_BIG) {
			struct linux_hd_big_geometry hdbg;

			memset(&hdbg, 0, sizeof(hdbg));
			hdbg.cylinders = fwcylinders;
			hdbg.heads = fwheads;
			hdbg.sectors = fwsectors;
			hdbg.start = 0;
			error = copyout(&hdbg, (void *)args->arg, sizeof(hdbg));
		}
		return (error);
		break;
	default:
		/* XXX */
		linux_msg(td,
			"%s fd=%d, cmd=0x%x ('%c',%d) is not implemented",
			__func__, args->fd, args->cmd,
			(int)(args->cmd & 0xff00) >> 8,
			(int)(args->cmd & 0xff));
		break;
	}
	fdrop(fp, td);
	return (ENOIOCTL);
}

static int
linux_ioctl_disk(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error;
	u_int sectorsize, psectorsize;
	uint64_t blksize64;
	off_t mediasize, stripesize;

	error = fget(td, args->fd, &cap_ioctl_rights, &fp);
	if (error != 0)
		return (error);
	switch (args->cmd & 0xffff) {
	case LINUX_BLKGETSIZE:
		error = fo_ioctl(fp, DIOCGSECTORSIZE,
		    (caddr_t)&sectorsize, td->td_ucred, td);
		if (!error)
			error = fo_ioctl(fp, DIOCGMEDIASIZE,
			    (caddr_t)&mediasize, td->td_ucred, td);
		fdrop(fp, td);
		if (error)
			return (error);
		sectorsize = mediasize / sectorsize;
		/*
		 * XXX: How do we know we return the right size of integer ?
		 */
		return (copyout(&sectorsize, (void *)args->arg,
		    sizeof(sectorsize)));
		break;
	case LINUX_BLKGETSIZE64:
		error = fo_ioctl(fp, DIOCGMEDIASIZE,
		    (caddr_t)&mediasize, td->td_ucred, td);
		fdrop(fp, td);
		if (error)
			return (error);
		blksize64 = mediasize;
		return (copyout(&blksize64, (void *)args->arg,
		    sizeof(blksize64)));
	case LINUX_BLKSSZGET:
		error = fo_ioctl(fp, DIOCGSECTORSIZE,
		    (caddr_t)&sectorsize, td->td_ucred, td);
		fdrop(fp, td);
		if (error)
			return (error);
		return (copyout(&sectorsize, (void *)args->arg,
		    sizeof(sectorsize)));
		break;
	case LINUX_BLKPBSZGET:
		error = fo_ioctl(fp, DIOCGSTRIPESIZE,
		    (caddr_t)&stripesize, td->td_ucred, td);
		if (error != 0) {
			fdrop(fp, td);
			return (error);
		}
		if (stripesize > 0 && stripesize <= 4096) {
			psectorsize = stripesize;
		} else  {
			error = fo_ioctl(fp, DIOCGSECTORSIZE,
			    (caddr_t)&sectorsize, td->td_ucred, td);
			if (error != 0) {
				fdrop(fp, td);
				return (error);
			}
			psectorsize = sectorsize;
		}
		fdrop(fp, td);
		return (copyout(&psectorsize, (void *)args->arg,
		    sizeof(psectorsize)));
	}
	fdrop(fp, td);
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

struct speedtab {
	int sp_speed;			/* Speed. */
	int sp_code;			/* Code. */
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
	return (-1);
}

static int
bsd_to_linux_speed(int speed, struct speedtab *table)
{
	for ( ; table->sp_speed != -1; table++)
		if (table->sp_speed == speed)
			return (table->sp_code);
	return (-1);
}

static void
bsd_to_linux_termios(struct termios *bios, struct linux_termios *lios)
{
	int i;

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
	if (bios->c_iflag & IUTF8)
		lios->c_iflag |= LINUX_IUTF8;

	lios->c_oflag = 0;
	if (bios->c_oflag & OPOST)
		lios->c_oflag |= LINUX_OPOST;
	if (bios->c_oflag & ONLCR)
		lios->c_oflag |= LINUX_ONLCR;
	if (bios->c_oflag & TAB3)
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
	if (linux_preserve_vstatus)
		lios->c_cc[LINUX_VSTATUS] = bios->c_cc[VSTATUS];

	for (i=0; i<LINUX_NCCS; i++) {
		if (i != LINUX_VMIN && i != LINUX_VTIME &&
		    lios->c_cc[i] == _POSIX_VDISABLE)
			lios->c_cc[i] = LINUX_POSIX_VDISABLE;
	}
	lios->c_line = 0;
}

static void
linux_to_bsd_termios(struct linux_termios *lios, struct termios *bios)
{
	int i;

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
	if (lios->c_iflag & LINUX_IUTF8)
		bios->c_iflag |= IUTF8;

	bios->c_oflag = 0;
	if (lios->c_oflag & LINUX_OPOST)
		bios->c_oflag |= OPOST;
	if (lios->c_oflag & LINUX_ONLCR)
		bios->c_oflag |= ONLCR;
	if (lios->c_oflag & LINUX_XTABS)
		bios->c_oflag |= TAB3;

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
	if (linux_preserve_vstatus)
		bios->c_cc[VSTATUS] = lios->c_cc[LINUX_VSTATUS];

	for (i=0; i<NCCS; i++) {
		if (i != VMIN && i != VTIME &&
		    bios->c_cc[i] == LINUX_POSIX_VDISABLE)
			bios->c_cc[i] = _POSIX_VDISABLE;
	}

	bios->c_ispeed = bios->c_ospeed =
	    linux_to_bsd_speed(lios->c_cflag & LINUX_CBAUD, sptab);
}

static void
bsd_to_linux_termio(struct termios *bios, struct linux_termio *lio)
{
	struct linux_termios lios;

	memset(lio, 0, sizeof(*lio));
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
linux_ioctl_termio(struct thread *td, struct linux_ioctl_args *args)
{
	struct termios bios;
	struct linux_termios lios;
	struct linux_termio lio;
	struct file *fp;
	int error;

	error = fget(td, args->fd, &cap_ioctl_rights, &fp);
	if (error != 0)
		return (error);

	switch (args->cmd & 0xffff) {
	case LINUX_TCGETS:
		error = fo_ioctl(fp, TIOCGETA, (caddr_t)&bios, td->td_ucred,
		    td);
		if (error)
			break;
		bsd_to_linux_termios(&bios, &lios);
		error = copyout(&lios, (void *)args->arg, sizeof(lios));
		break;

	case LINUX_TCSETS:
		error = copyin((void *)args->arg, &lios, sizeof(lios));
		if (error)
			break;
		linux_to_bsd_termios(&lios, &bios);
		error = (fo_ioctl(fp, TIOCSETA, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	case LINUX_TCSETSW:
		error = copyin((void *)args->arg, &lios, sizeof(lios));
		if (error)
			break;
		linux_to_bsd_termios(&lios, &bios);
		error = (fo_ioctl(fp, TIOCSETAW, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	case LINUX_TCSETSF:
		error = copyin((void *)args->arg, &lios, sizeof(lios));
		if (error)
			break;
		linux_to_bsd_termios(&lios, &bios);
		error = (fo_ioctl(fp, TIOCSETAF, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	case LINUX_TCGETA:
		error = fo_ioctl(fp, TIOCGETA, (caddr_t)&bios, td->td_ucred,
		    td);
		if (error)
			break;
		bsd_to_linux_termio(&bios, &lio);
		error = (copyout(&lio, (void *)args->arg, sizeof(lio)));
		break;

	case LINUX_TCSETA:
		error = copyin((void *)args->arg, &lio, sizeof(lio));
		if (error)
			break;
		linux_to_bsd_termio(&lio, &bios);
		error = (fo_ioctl(fp, TIOCSETA, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	case LINUX_TCSETAW:
		error = copyin((void *)args->arg, &lio, sizeof(lio));
		if (error)
			break;
		linux_to_bsd_termio(&lio, &bios);
		error = (fo_ioctl(fp, TIOCSETAW, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	case LINUX_TCSETAF:
		error = copyin((void *)args->arg, &lio, sizeof(lio));
		if (error)
			break;
		linux_to_bsd_termio(&lio, &bios);
		error = (fo_ioctl(fp, TIOCSETAF, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	case LINUX_TCSBRK:
		if (args->arg != 0) {
			error = (fo_ioctl(fp, TIOCDRAIN, (caddr_t)&bios, td->td_ucred,
			    td));
		} else {
			linux_msg(td, "ioctl TCSBRK arg 0 not implemented");
			error = ENOIOCTL;
		}
		break;

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
			error = fo_ioctl(fp, TIOCGETA, (caddr_t)&bios,
			    td->td_ucred, td);
			if (error)
				break;
			fdrop(fp, td);
			c = (args->arg == LINUX_TCIOFF) ? VSTOP : VSTART;
			c = bios.c_cc[c];
			if (c != _POSIX_VDISABLE) {
				wr.fd = args->fd;
				wr.buf = &c;
				wr.nbyte = sizeof(c);
				return (sys_write(td, &wr));
			} else
				return (0);
		}
		default:
			fdrop(fp, td);
			return (EINVAL);
		}
		args->arg = 0;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;
	}

	case LINUX_TCFLSH: {
		int val;
		switch (args->arg) {
		case LINUX_TCIFLUSH:
			val = FREAD;
			break;
		case LINUX_TCOFLUSH:
			val = FWRITE;
			break;
		case LINUX_TCIOFLUSH:
			val = FREAD | FWRITE;
			break;
		default:
			fdrop(fp, td);
			return (EINVAL);
		}
		error = (fo_ioctl(fp,TIOCFLUSH,(caddr_t)&val,td->td_ucred,td));
		break;
	}

	case LINUX_TIOCEXCL:
		args->cmd = TIOCEXCL;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCNXCL:
		args->cmd = TIOCNXCL;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCSCTTY:
		args->cmd = TIOCSCTTY;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCGPGRP:
		args->cmd = TIOCGPGRP;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCSPGRP:
		args->cmd = TIOCSPGRP;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	/* LINUX_TIOCOUTQ */
	/* LINUX_TIOCSTI */

	case LINUX_TIOCGWINSZ:
		args->cmd = TIOCGWINSZ;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCSWINSZ:
		args->cmd = TIOCSWINSZ;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCMGET:
		args->cmd = TIOCMGET;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCMBIS:
		args->cmd = TIOCMBIS;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCMBIC:
		args->cmd = TIOCMBIC;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCMSET:
		args->cmd = TIOCMSET;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	/* TIOCGSOFTCAR */
	/* TIOCSSOFTCAR */

	case LINUX_FIONREAD: /* LINUX_TIOCINQ */
		args->cmd = FIONREAD;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	/* LINUX_TIOCLINUX */

	case LINUX_TIOCCONS:
		args->cmd = TIOCCONS;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCGSERIAL: {
		struct linux_serial_struct lss;

		bzero(&lss, sizeof(lss));
		lss.type = LINUX_PORT_16550A;
		lss.flags = 0;
		lss.close_delay = 0;
		error = copyout(&lss, (void *)args->arg, sizeof(lss));
		break;
	}

	case LINUX_TIOCSSERIAL: {
		struct linux_serial_struct lss;
		error = copyin((void *)args->arg, &lss, sizeof(lss));
		if (error)
			break;
		/* XXX - It really helps to have an implementation that
		 * does nothing. NOT!
		 */
		error = 0;
		break;
	}

	case LINUX_TIOCPKT:
		args->cmd = TIOCPKT;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_FIONBIO:
		args->cmd = FIONBIO;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCNOTTY:
		args->cmd = TIOCNOTTY;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

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
			fdrop(fp, td);
			return (EINVAL);
		}
		error = (fo_ioctl(fp, TIOCSETD, (caddr_t)&line, td->td_ucred,
		    td));
		break;
	}

	case LINUX_TIOCGETD: {
		int linux_line;
		int bsd_line = TTYDISC;
		error = fo_ioctl(fp, TIOCGETD, (caddr_t)&bsd_line,
		    td->td_ucred, td);
		if (error)
			break;
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
			fdrop(fp, td);
			return (EINVAL);
		}
		error = (copyout(&linux_line, (void *)args->arg, sizeof(int)));
		break;
	}

	/* LINUX_TCSBRKP */
	/* LINUX_TIOCTTYGSTRUCT */

	case LINUX_FIONCLEX:
		args->cmd = FIONCLEX;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_FIOCLEX:
		args->cmd = FIOCLEX;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_FIOASYNC:
		args->cmd = FIOASYNC;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	/* LINUX_TIOCSERCONFIG */
	/* LINUX_TIOCSERGWILD */
	/* LINUX_TIOCSERSWILD */
	/* LINUX_TIOCGLCKTRMIOS */
	/* LINUX_TIOCSLCKTRMIOS */

	case LINUX_TIOCSBRK:
		args->cmd = TIOCSBRK;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCCBRK:
		args->cmd = TIOCCBRK;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;
	case LINUX_TIOCGPTN: {
		int nb;

		error = fo_ioctl(fp, TIOCGPTN, (caddr_t)&nb, td->td_ucred, td);
		if (!error)
			error = copyout(&nb, (void *)args->arg,
			    sizeof(int));
		break;
	}
	case LINUX_TIOCGPTPEER:
		linux_msg(td, "unsupported ioctl TIOCGPTPEER");
		error = ENOIOCTL;
		break;
	case LINUX_TIOCSPTLCK:
		/*
		 * Our unlockpt() does nothing. Check that fd refers
		 * to a pseudo-terminal master device.
		 */
		args->cmd = TIOCPTMASTER;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;
	default:
		error = ENOIOCTL;
		break;
	}

	fdrop(fp, td);
	return (error);
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

struct l_cdrom_read_audio {
	union linux_cdrom_addr addr;
	u_char		addr_format;
	l_int		nframes;
	u_char		*buf;
};

struct l_dvd_layer {
	u_char		book_version:4;
	u_char		book_type:4;
	u_char		min_rate:4;
	u_char		disc_size:4;
	u_char		layer_type:4;
	u_char		track_path:1;
	u_char		nlayers:2;
	u_char		track_density:4;
	u_char		linear_density:4;
	u_char		bca:1;
	uint32_t	start_sector;
	uint32_t	end_sector;
	uint32_t	end_sector_l0;
};

struct l_dvd_physical {
	u_char		type;
	u_char		layer_num;
	struct l_dvd_layer layer[4];
};

struct l_dvd_copyright {
	u_char		type;
	u_char		layer_num;
	u_char		cpst;
	u_char		rmi;
};

struct l_dvd_disckey {
	u_char		type;
	l_uint		agid:2;
	u_char		value[2048];
};

struct l_dvd_bca {
	u_char		type;
	l_int		len;
	u_char		value[188];
};

struct l_dvd_manufact {
	u_char		type;
	u_char		layer_num;
	l_int		len;
	u_char		value[2048];
};

typedef union {
	u_char			type;
	struct l_dvd_physical	physical;
	struct l_dvd_copyright	copyright;
	struct l_dvd_disckey	disckey;
	struct l_dvd_bca	bca;
	struct l_dvd_manufact	manufact;
} l_dvd_struct;

typedef u_char l_dvd_key[5];
typedef u_char l_dvd_challenge[10];

struct l_dvd_lu_send_agid {
	u_char		type;
	l_uint		agid:2;
};

struct l_dvd_host_send_challenge {
	u_char		type;
	l_uint		agid:2;
	l_dvd_challenge	chal;
};

struct l_dvd_send_key {
	u_char		type;
	l_uint		agid:2;
	l_dvd_key	key;
};

struct l_dvd_lu_send_challenge {
	u_char		type;
	l_uint		agid:2;
	l_dvd_challenge	chal;
};

struct l_dvd_lu_send_title_key {
	u_char		type;
	l_uint		agid:2;
	l_dvd_key	title_key;
	l_int		lba;
	l_uint		cpm:1;
	l_uint		cp_sec:1;
	l_uint		cgms:2;
};

struct l_dvd_lu_send_asf {
	u_char		type;
	l_uint		agid:2;
	l_uint		asf:1;
};

struct l_dvd_host_send_rpcstate {
	u_char		type;
	u_char		pdrc;
};

struct l_dvd_lu_send_rpcstate {
	u_char		type:2;
	u_char		vra:3;
	u_char		ucca:3;
	u_char		region_mask;
	u_char		rpc_scheme;
};

typedef union {
	u_char				type;
	struct l_dvd_lu_send_agid	lsa;
	struct l_dvd_host_send_challenge hsc;
	struct l_dvd_send_key		lsk;
	struct l_dvd_lu_send_challenge	lsc;
	struct l_dvd_send_key		hsk;
	struct l_dvd_lu_send_title_key	lstk;
	struct l_dvd_lu_send_asf	lsasf;
	struct l_dvd_host_send_rpcstate	hrpcs;
	struct l_dvd_lu_send_rpcstate	lrpcs;
} l_dvd_authinfo;

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
linux_to_bsd_dvd_struct(l_dvd_struct *lp, struct dvd_struct *bp)
{
	bp->format = lp->type;
	switch (bp->format) {
	case DVD_STRUCT_PHYSICAL:
		if (bp->layer_num >= 4)
			return (EINVAL);
		bp->layer_num = lp->physical.layer_num;
		break;
	case DVD_STRUCT_COPYRIGHT:
		bp->layer_num = lp->copyright.layer_num;
		break;
	case DVD_STRUCT_DISCKEY:
		bp->agid = lp->disckey.agid;
		break;
	case DVD_STRUCT_BCA:
	case DVD_STRUCT_MANUFACT:
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
bsd_to_linux_dvd_struct(struct dvd_struct *bp, l_dvd_struct *lp)
{
	switch (bp->format) {
	case DVD_STRUCT_PHYSICAL: {
		struct dvd_layer *blp = (struct dvd_layer *)bp->data;
		struct l_dvd_layer *llp = &lp->physical.layer[bp->layer_num];
		memset(llp, 0, sizeof(*llp));
		llp->book_version = blp->book_version;
		llp->book_type = blp->book_type;
		llp->min_rate = blp->max_rate;
		llp->disc_size = blp->disc_size;
		llp->layer_type = blp->layer_type;
		llp->track_path = blp->track_path;
		llp->nlayers = blp->nlayers;
		llp->track_density = blp->track_density;
		llp->linear_density = blp->linear_density;
		llp->bca = blp->bca;
		llp->start_sector = blp->start_sector;
		llp->end_sector = blp->end_sector;
		llp->end_sector_l0 = blp->end_sector_l0;
		break;
	}
	case DVD_STRUCT_COPYRIGHT:
		lp->copyright.cpst = bp->cpst;
		lp->copyright.rmi = bp->rmi;
		break;
	case DVD_STRUCT_DISCKEY:
		memcpy(lp->disckey.value, bp->data, sizeof(lp->disckey.value));
		break;
	case DVD_STRUCT_BCA:
		lp->bca.len = bp->length;
		memcpy(lp->bca.value, bp->data, sizeof(lp->bca.value));
		break;
	case DVD_STRUCT_MANUFACT:
		lp->manufact.len = bp->length;
		memcpy(lp->manufact.value, bp->data,
		    sizeof(lp->manufact.value));
		/* lp->manufact.layer_num is unused in Linux (redhat 7.0). */
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
linux_to_bsd_dvd_authinfo(l_dvd_authinfo *lp, int *bcode,
    struct dvd_authinfo *bp)
{
	switch (lp->type) {
	case LINUX_DVD_LU_SEND_AGID:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_AGID;
		bp->agid = lp->lsa.agid;
		break;
	case LINUX_DVD_HOST_SEND_CHALLENGE:
		*bcode = DVDIOCSENDKEY;
		bp->format = DVD_SEND_CHALLENGE;
		bp->agid = lp->hsc.agid;
		memcpy(bp->keychal, lp->hsc.chal, 10);
		break;
	case LINUX_DVD_LU_SEND_KEY1:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_KEY1;
		bp->agid = lp->lsk.agid;
		break;
	case LINUX_DVD_LU_SEND_CHALLENGE:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_CHALLENGE;
		bp->agid = lp->lsc.agid;
		break;
	case LINUX_DVD_HOST_SEND_KEY2:
		*bcode = DVDIOCSENDKEY;
		bp->format = DVD_SEND_KEY2;
		bp->agid = lp->hsk.agid;
		memcpy(bp->keychal, lp->hsk.key, 5);
		break;
	case LINUX_DVD_LU_SEND_TITLE_KEY:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_TITLE_KEY;
		bp->agid = lp->lstk.agid;
		bp->lba = lp->lstk.lba;
		break;
	case LINUX_DVD_LU_SEND_ASF:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_ASF;
		bp->agid = lp->lsasf.agid;
		break;
	case LINUX_DVD_INVALIDATE_AGID:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_INVALIDATE_AGID;
		bp->agid = lp->lsa.agid;
		break;
	case LINUX_DVD_LU_SEND_RPC_STATE:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_RPC;
		break;
	case LINUX_DVD_HOST_SEND_RPC_STATE:
		*bcode = DVDIOCSENDKEY;
		bp->format = DVD_SEND_RPC;
		bp->region = lp->hrpcs.pdrc;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
bsd_to_linux_dvd_authinfo(struct dvd_authinfo *bp, l_dvd_authinfo *lp)
{
	switch (lp->type) {
	case LINUX_DVD_LU_SEND_AGID:
		lp->lsa.agid = bp->agid;
		break;
	case LINUX_DVD_HOST_SEND_CHALLENGE:
		lp->type = LINUX_DVD_LU_SEND_KEY1;
		break;
	case LINUX_DVD_LU_SEND_KEY1:
		memcpy(lp->lsk.key, bp->keychal, sizeof(lp->lsk.key));
		break;
	case LINUX_DVD_LU_SEND_CHALLENGE:
		memcpy(lp->lsc.chal, bp->keychal, sizeof(lp->lsc.chal));
		break;
	case LINUX_DVD_HOST_SEND_KEY2:
		lp->type = LINUX_DVD_AUTH_ESTABLISHED;
		break;
	case LINUX_DVD_LU_SEND_TITLE_KEY:
		memcpy(lp->lstk.title_key, bp->keychal,
		    sizeof(lp->lstk.title_key));
		lp->lstk.cpm = bp->cpm;
		lp->lstk.cp_sec = bp->cp_sec;
		lp->lstk.cgms = bp->cgms;
		break;
	case LINUX_DVD_LU_SEND_ASF:
		lp->lsasf.asf = bp->asf;
		break;
	case LINUX_DVD_INVALIDATE_AGID:
		break;
	case LINUX_DVD_LU_SEND_RPC_STATE:
		lp->lrpcs.type = bp->reg_type;
		lp->lrpcs.vra = bp->vend_rsts;
		lp->lrpcs.ucca = bp->user_rsts;
		lp->lrpcs.region_mask = bp->region;
		lp->lrpcs.rpc_scheme = bp->rpc_scheme;
		break;
	case LINUX_DVD_HOST_SEND_RPC_STATE:
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
linux_ioctl_cdrom(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error;

	error = fget(td, args->fd, &cap_ioctl_rights, &fp);
	if (error != 0)
		return (error);
	switch (args->cmd & 0xffff) {
	case LINUX_CDROMPAUSE:
		args->cmd = CDIOCPAUSE;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMRESUME:
		args->cmd = CDIOCRESUME;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMPLAYMSF:
		args->cmd = CDIOCPLAYMSF;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMPLAYTRKIND:
		args->cmd = CDIOCPLAYTRACKS;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMREADTOCHDR: {
		struct ioc_toc_header th;
		struct linux_cdrom_tochdr lth;
		error = fo_ioctl(fp, CDIOREADTOCHEADER, (caddr_t)&th,
		    td->td_ucred, td);
		if (!error) {
			lth.cdth_trk0 = th.starting_track;
			lth.cdth_trk1 = th.ending_track;
			error = copyout(&lth, (void *)args->arg, sizeof(lth));
		}
		break;
	}

	case LINUX_CDROMREADTOCENTRY: {
		struct linux_cdrom_tocentry lte;
		struct ioc_read_toc_single_entry irtse;

		error = copyin((void *)args->arg, &lte, sizeof(lte));
		if (error)
			break;
		irtse.address_format = lte.cdte_format;
		irtse.track = lte.cdte_track;
		error = fo_ioctl(fp, CDIOREADTOCENTRY, (caddr_t)&irtse,
		    td->td_ucred, td);
		if (!error) {
			lte.cdte_ctrl = irtse.entry.control;
			lte.cdte_adr = irtse.entry.addr_type;
			bsd_to_linux_msf_lba(irtse.address_format,
			    &irtse.entry.addr, &lte.cdte_addr);
			error = copyout(&lte, (void *)args->arg, sizeof(lte));
		}
		break;
	}

	case LINUX_CDROMSTOP:
		args->cmd = CDIOCSTOP;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMSTART:
		args->cmd = CDIOCSTART;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMEJECT:
		args->cmd = CDIOCEJECT;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	/* LINUX_CDROMVOLCTRL */

	case LINUX_CDROMSUBCHNL: {
		struct linux_cdrom_subchnl sc;
		struct ioc_read_subchannel bsdsc;
		struct cd_sub_channel_info bsdinfo;

		error = copyin((void *)args->arg, &sc, sizeof(sc));
		if (error)
			break;

		/*
		 * Invoke the native ioctl and bounce the returned data through
		 * the userspace buffer.  This works because the Linux structure
		 * is the same size as our structures for the subchannel header
		 * and position data.
		 */
		bsdsc.address_format = CD_LBA_FORMAT;
		bsdsc.data_format = CD_CURRENT_POSITION;
		bsdsc.track = 0;
		bsdsc.data_len = sizeof(sc);
		bsdsc.data = (void *)args->arg;
		error = fo_ioctl(fp, CDIOCREADSUBCHANNEL, (caddr_t)&bsdsc,
		    td->td_ucred, td);
		if (error)
			break;
		error = copyin((void *)args->arg, &bsdinfo, sizeof(bsdinfo));
		if (error)
			break;
		sc.cdsc_audiostatus = bsdinfo.header.audio_status;
		sc.cdsc_adr = bsdinfo.what.position.addr_type;
		sc.cdsc_ctrl = bsdinfo.what.position.control;
		sc.cdsc_trk = bsdinfo.what.position.track_number;
		sc.cdsc_ind = bsdinfo.what.position.index_number;
		set_linux_cdrom_addr(&sc.cdsc_absaddr, sc.cdsc_format,
		    bsdinfo.what.position.absaddr.lba);
		set_linux_cdrom_addr(&sc.cdsc_reladdr, sc.cdsc_format,
		    bsdinfo.what.position.reladdr.lba);
		error = copyout(&sc, (void *)args->arg, sizeof(sc));
		break;
	}

	/* LINUX_CDROMREADMODE2 */
	/* LINUX_CDROMREADMODE1 */
	/* LINUX_CDROMREADAUDIO */
	/* LINUX_CDROMEJECT_SW */
	/* LINUX_CDROMMULTISESSION */
	/* LINUX_CDROM_GET_UPC */

	case LINUX_CDROMRESET:
		args->cmd = CDIOCRESET;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	/* LINUX_CDROMVOLREAD */
	/* LINUX_CDROMREADRAW */
	/* LINUX_CDROMREADCOOKED */
	/* LINUX_CDROMSEEK */
	/* LINUX_CDROMPLAYBLK */
	/* LINUX_CDROMREADALL */
	/* LINUX_CDROMCLOSETRAY */
	/* LINUX_CDROMLOADFROMSLOT */
	/* LINUX_CDROMGETSPINDOWN */
	/* LINUX_CDROMSETSPINDOWN */
	/* LINUX_CDROM_SET_OPTIONS */
	/* LINUX_CDROM_CLEAR_OPTIONS */
	/* LINUX_CDROM_SELECT_SPEED */
	/* LINUX_CDROM_SELECT_DISC */
	/* LINUX_CDROM_MEDIA_CHANGED */
	/* LINUX_CDROM_DRIVE_STATUS */
	/* LINUX_CDROM_DISC_STATUS */
	/* LINUX_CDROM_CHANGER_NSLOTS */
	/* LINUX_CDROM_LOCKDOOR */
	/* LINUX_CDROM_DEBUG */
	/* LINUX_CDROM_GET_CAPABILITY */
	/* LINUX_CDROMAUDIOBUFSIZ */

	case LINUX_DVD_READ_STRUCT: {
		l_dvd_struct *lds;
		struct dvd_struct *bds;

		lds = malloc(sizeof(*lds), M_LINUX, M_WAITOK);
		bds = malloc(sizeof(*bds), M_LINUX, M_WAITOK);
		error = copyin((void *)args->arg, lds, sizeof(*lds));
		if (error)
			goto out;
		error = linux_to_bsd_dvd_struct(lds, bds);
		if (error)
			goto out;
		error = fo_ioctl(fp, DVDIOCREADSTRUCTURE, (caddr_t)bds,
		    td->td_ucred, td);
		if (error)
			goto out;
		error = bsd_to_linux_dvd_struct(bds, lds);
		if (error)
			goto out;
		error = copyout(lds, (void *)args->arg, sizeof(*lds));
	out:
		free(bds, M_LINUX);
		free(lds, M_LINUX);
		break;
	}

	/* LINUX_DVD_WRITE_STRUCT */

	case LINUX_DVD_AUTH: {
		l_dvd_authinfo lda;
		struct dvd_authinfo bda;
		int bcode;

		error = copyin((void *)args->arg, &lda, sizeof(lda));
		if (error)
			break;
		error = linux_to_bsd_dvd_authinfo(&lda, &bcode, &bda);
		if (error)
			break;
		error = fo_ioctl(fp, bcode, (caddr_t)&bda, td->td_ucred,
		    td);
		if (error) {
			if (lda.type == LINUX_DVD_HOST_SEND_KEY2) {
				lda.type = LINUX_DVD_AUTH_FAILURE;
				(void)copyout(&lda, (void *)args->arg,
				    sizeof(lda));
			}
			break;
		}
		error = bsd_to_linux_dvd_authinfo(&bda, &lda);
		if (error)
			break;
		error = copyout(&lda, (void *)args->arg, sizeof(lda));
		break;
	}

	case LINUX_SCSI_GET_BUS_NUMBER:
	{
		struct sg_scsi_id id;

		error = fo_ioctl(fp, SG_GET_SCSI_ID, (caddr_t)&id,
		    td->td_ucred, td);
		if (error)
			break;
		error = copyout(&id.channel, (void *)args->arg, sizeof(int));
		break;
	}

	case LINUX_SCSI_GET_IDLUN:
	{
		struct sg_scsi_id id;
		struct scsi_idlun idl;

		error = fo_ioctl(fp, SG_GET_SCSI_ID, (caddr_t)&id,
		    td->td_ucred, td);
		if (error)
			break;
		idl.dev_id = (id.scsi_id & 0xff) + ((id.lun & 0xff) << 8) +
		    ((id.channel & 0xff) << 16) + ((id.host_no & 0xff) << 24);
		idl.host_unique_id = id.host_no;
		error = copyout(&idl, (void *)args->arg, sizeof(idl));
		break;
	}

	/* LINUX_CDROM_SEND_PACKET */
	/* LINUX_CDROM_NEXT_WRITABLE */
	/* LINUX_CDROM_LAST_WRITTEN */

	default:
		error = ENOIOCTL;
		break;
	}

	fdrop(fp, td);
	return (error);
}

static int
linux_ioctl_vfat(struct thread *td, struct linux_ioctl_args *args)
{

	return (ENOTTY);
}

/*
 * Sound related ioctls
 */

struct linux_old_mixer_info {
	char	id[16];
	char	name[32];
};

static uint32_t dirbits[4] = { IOC_VOID, IOC_IN, IOC_OUT, IOC_INOUT };

#define	SETDIR(c)	(((c) & ~IOC_DIRMASK) | dirbits[args->cmd >> 30])

static int
linux_ioctl_sound(struct thread *td, struct linux_ioctl_args *args)
{

	switch (args->cmd & 0xffff) {
	case LINUX_SOUND_MIXER_WRITE_VOLUME:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_VOLUME);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_BASS:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_BASS);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_TREBLE:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_TREBLE);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_SYNTH:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_SYNTH);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_PCM:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_PCM);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_SPEAKER:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_SPEAKER);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_MIC:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_MIC);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_CD:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_CD);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_IMIX:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_IMIX);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_ALTPCM:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_ALTPCM);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_RECLEV:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_RECLEV);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_IGAIN:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_IGAIN);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_OGAIN:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_OGAIN);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE1:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE1);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE2:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE2);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE3:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE3);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_MONITOR:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_MONITOR);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_INFO: {
		/* Key on encoded length */
		switch ((args->cmd >> 16) & 0x1fff) {
		case 0x005c: {	/* SOUND_MIXER_INFO */
			args->cmd = SOUND_MIXER_INFO;
			return (sys_ioctl(td, (struct ioctl_args *)args));
		}
		case 0x0030: {	/* SOUND_OLD_MIXER_INFO */
			struct linux_old_mixer_info info;
			bzero(&info, sizeof(info));
			strncpy(info.id, "OSS", sizeof(info.id) - 1);
			strncpy(info.name, "FreeBSD OSS Mixer",
			    sizeof(info.name) - 1);
			return (copyout(&info, (void *)args->arg,
			    sizeof(info)));
		}
		default:
			return (ENOIOCTL);
		}
		break;
	}

	case LINUX_OSS_GETVERSION: {
		int version = linux_get_oss_version(td);
		return (copyout(&version, (void *)args->arg, sizeof(int)));
	}

	case LINUX_SOUND_MIXER_READ_STEREODEVS:
		args->cmd = SOUND_MIXER_READ_STEREODEVS;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_READ_CAPS:
		args->cmd = SOUND_MIXER_READ_CAPS;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_READ_RECMASK:
		args->cmd = SOUND_MIXER_READ_RECMASK;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_READ_DEVMASK:
		args->cmd = SOUND_MIXER_READ_DEVMASK;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_RECSRC:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_RECSRC);
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_RESET:
		args->cmd = SNDCTL_DSP_RESET;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SYNC:
		args->cmd = SNDCTL_DSP_SYNC;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SPEED:
		args->cmd = SNDCTL_DSP_SPEED;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_STEREO:
		args->cmd = SNDCTL_DSP_STEREO;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETBLKSIZE: /* LINUX_SNDCTL_DSP_SETBLKSIZE */
		args->cmd = SNDCTL_DSP_GETBLKSIZE;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SETFMT:
		args->cmd = SNDCTL_DSP_SETFMT;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_PCM_WRITE_CHANNELS:
		args->cmd = SOUND_PCM_WRITE_CHANNELS;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_PCM_WRITE_FILTER:
		args->cmd = SOUND_PCM_WRITE_FILTER;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_POST:
		args->cmd = SNDCTL_DSP_POST;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SUBDIVIDE:
		args->cmd = SNDCTL_DSP_SUBDIVIDE;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SETFRAGMENT:
		args->cmd = SNDCTL_DSP_SETFRAGMENT;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETFMTS:
		args->cmd = SNDCTL_DSP_GETFMTS;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETOSPACE:
		args->cmd = SNDCTL_DSP_GETOSPACE;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETISPACE:
		args->cmd = SNDCTL_DSP_GETISPACE;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_NONBLOCK:
		args->cmd = SNDCTL_DSP_NONBLOCK;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETCAPS:
		args->cmd = SNDCTL_DSP_GETCAPS;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SETTRIGGER: /* LINUX_SNDCTL_GETTRIGGER */
		args->cmd = SNDCTL_DSP_SETTRIGGER;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETIPTR:
		args->cmd = SNDCTL_DSP_GETIPTR;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETOPTR:
		args->cmd = SNDCTL_DSP_GETOPTR;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SETDUPLEX:
		args->cmd = SNDCTL_DSP_SETDUPLEX;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETODELAY:
		args->cmd = SNDCTL_DSP_GETODELAY;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_RESET:
		args->cmd = SNDCTL_SEQ_RESET;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_SYNC:
		args->cmd = SNDCTL_SEQ_SYNC;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SYNTH_INFO:
		args->cmd = SNDCTL_SYNTH_INFO;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_CTRLRATE:
		args->cmd = SNDCTL_SEQ_CTRLRATE;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_GETOUTCOUNT:
		args->cmd = SNDCTL_SEQ_GETOUTCOUNT;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_GETINCOUNT:
		args->cmd = SNDCTL_SEQ_GETINCOUNT;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_PERCMODE:
		args->cmd = SNDCTL_SEQ_PERCMODE;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_FM_LOAD_INSTR:
		args->cmd = SNDCTL_FM_LOAD_INSTR;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_TESTMIDI:
		args->cmd = SNDCTL_SEQ_TESTMIDI;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_RESETSAMPLES:
		args->cmd = SNDCTL_SEQ_RESETSAMPLES;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_NRSYNTHS:
		args->cmd = SNDCTL_SEQ_NRSYNTHS;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_NRMIDIS:
		args->cmd = SNDCTL_SEQ_NRMIDIS;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_MIDI_INFO:
		args->cmd = SNDCTL_MIDI_INFO;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_TRESHOLD:
		args->cmd = SNDCTL_SEQ_TRESHOLD;
		return (sys_ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SYNTH_MEMAVL:
		args->cmd = SNDCTL_SYNTH_MEMAVL;
		return (sys_ioctl(td, (struct ioctl_args *)args));
	}

	return (ENOIOCTL);
}

/*
 * Console related ioctls
 */

static int
linux_ioctl_console(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error;

	error = fget(td, args->fd, &cap_ioctl_rights, &fp);
	if (error != 0)
		return (error);
	switch (args->cmd & 0xffff) {
	case LINUX_KIOCSOUND:
		args->cmd = KIOCSOUND;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDMKTONE:
		args->cmd = KDMKTONE;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDGETLED:
		args->cmd = KDGETLED;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDSETLED:
		args->cmd = KDSETLED;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDSETMODE:
		args->cmd = KDSETMODE;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDGETMODE:
		args->cmd = KDGETMODE;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDGKBMODE:
		args->cmd = KDGKBMODE;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

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
			fdrop(fp, td);
			return (EINVAL);
		}
		error = (fo_ioctl(fp, KDSKBMODE, (caddr_t)&kbdmode,
		    td->td_ucred, td));
		break;
	}

	case LINUX_VT_OPENQRY:
		args->cmd = VT_OPENQRY;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_VT_GETMODE:
		args->cmd = VT_GETMODE;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_VT_SETMODE: {
		struct vt_mode mode;
		if ((error = copyin((void *)args->arg, &mode, sizeof(mode))))
			break;
		if (LINUX_SIG_VALID(mode.relsig))
			mode.relsig = linux_to_bsd_signal(mode.relsig);
		else
			mode.relsig = 0;
		if (LINUX_SIG_VALID(mode.acqsig))
			mode.acqsig = linux_to_bsd_signal(mode.acqsig);
		else
			mode.acqsig = 0;
		/* XXX. Linux ignores frsig and set it to 0. */
		mode.frsig = 0;
		if ((error = copyout(&mode, (void *)args->arg, sizeof(mode))))
			break;
		args->cmd = VT_SETMODE;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;
	}

	case LINUX_VT_GETSTATE:
		args->cmd = VT_GETACTIVE;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_VT_RELDISP:
		args->cmd = VT_RELDISP;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_VT_ACTIVATE:
		args->cmd = VT_ACTIVATE;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_VT_WAITACTIVE:
		args->cmd = VT_WAITACTIVE;
		error = (sys_ioctl(td, (struct ioctl_args *)args));
		break;

	default:
		error = ENOIOCTL;
		break;
	}

	fdrop(fp, td);
	return (error);
}

/*
 * Implement the SIOCGIFNAME ioctl
 */

static int
linux_ioctl_ifname(struct thread *td, struct l_ifreq *uifr)
{
	struct l_ifreq ifr;
	int error, ret;

	error = copyin(uifr, &ifr, sizeof(ifr));
	if (error != 0)
		return (error);
	ret = ifname_bsd_to_linux_idx(ifr.ifr_index, ifr.ifr_name,
	    LINUX_IFNAMSIZ);
	if (ret > 0)
		return (copyout(&ifr, uifr, sizeof(ifr)));
	else
		return (ENODEV);
}

/*
 * Implement the SIOCGIFCONF ioctl
 */
static u_int
linux_ifconf_ifaddr_cb(void *arg, struct ifaddr *ifa, u_int count)
{
#ifdef COMPAT_LINUX32
	struct l_ifconf *ifc;
#else
	struct ifconf *ifc;
#endif

	ifc = arg;
	ifc->ifc_len += sizeof(struct l_ifreq);
	return (1);
}

static int
linux_ifconf_ifnet_cb(if_t ifp, void *arg)
{

	if_foreach_addr_type(ifp, AF_INET, linux_ifconf_ifaddr_cb, arg);
	return (0);
}

struct linux_ifconfig_ifaddr_cb2_s {
	struct l_ifreq ifr;
	struct sbuf *sb;
	size_t max_len;
	size_t valid_len;
};

static u_int
linux_ifconf_ifaddr_cb2(void *arg, struct ifaddr *ifa, u_int len)
{
	struct linux_ifconfig_ifaddr_cb2_s *cbs = arg;
	struct sockaddr *sa = ifa->ifa_addr;

	cbs->ifr.ifr_addr.sa_family = LINUX_AF_INET;
	memcpy(cbs->ifr.ifr_addr.sa_data, sa->sa_data,
	    sizeof(cbs->ifr.ifr_addr.sa_data));
	sbuf_bcat(cbs->sb, &cbs->ifr, sizeof(cbs->ifr));
	cbs->max_len += sizeof(cbs->ifr);

	if (sbuf_error(cbs->sb) == 0)
		cbs->valid_len = sbuf_len(cbs->sb);
	return (1);
}

static int
linux_ifconf_ifnet_cb2(if_t ifp, void *arg)
{
	struct linux_ifconfig_ifaddr_cb2_s *cbs = arg;

	bzero(&cbs->ifr, sizeof(cbs->ifr));
	ifname_bsd_to_linux_ifp(ifp, cbs->ifr.ifr_name,
	    sizeof(cbs->ifr.ifr_name));

	/* Walk the address list */
	if_foreach_addr_type(ifp, AF_INET, linux_ifconf_ifaddr_cb2, cbs);
	return (0);
}

static int
linux_ifconf(struct thread *td, struct ifconf *uifc)
{
	struct linux_ifconfig_ifaddr_cb2_s cbs;
	struct epoch_tracker et;
#ifdef COMPAT_LINUX32
	struct l_ifconf ifc;
#else
	struct ifconf ifc;
#endif
	struct sbuf *sb;
	int error, full;

	error = copyin(uifc, &ifc, sizeof(ifc));
	if (error != 0)
		return (error);

	/* handle the 'request buffer size' case */
	if (PTRIN(ifc.ifc_buf) == NULL) {
		ifc.ifc_len = 0;
		NET_EPOCH_ENTER(et);
		if_foreach(linux_ifconf_ifnet_cb, &ifc);
		NET_EPOCH_EXIT(et);
		return (copyout(&ifc, uifc, sizeof(ifc)));
	}
	if (ifc.ifc_len <= 0)
		return (EINVAL);

	full = 0;
	cbs.max_len = maxphys - 1;

again:
	if (ifc.ifc_len <= cbs.max_len) {
		cbs.max_len = ifc.ifc_len;
		full = 1;
	}
	cbs.sb = sb = sbuf_new(NULL, NULL, cbs.max_len + 1, SBUF_FIXEDLEN);
	cbs.max_len = 0;
	cbs.valid_len = 0;

	/* Return all AF_INET addresses of all interfaces */
	NET_EPOCH_ENTER(et);
	if_foreach(linux_ifconf_ifnet_cb2, &cbs);
	NET_EPOCH_EXIT(et);

	if (cbs.valid_len != cbs.max_len && !full) {
		sbuf_delete(sb);
		goto again;
	}

	ifc.ifc_len = cbs.valid_len;
	sbuf_finish(sb);
	error = copyout(sbuf_data(sb), PTRIN(ifc.ifc_buf), ifc.ifc_len);
	if (error == 0)
		error = copyout(&ifc, uifc, sizeof(ifc));
	sbuf_delete(sb);

	return (error);
}

static int
linux_ioctl_socket_ifreq(struct thread *td, int fd, u_int cmd,
    struct l_ifreq *uifr)
{
	struct l_ifreq lifr;
	struct ifreq bifr;
	size_t ifrusiz;
	int error, temp_flags;

	switch (cmd) {
	case LINUX_SIOCGIFINDEX:
		cmd = SIOCGIFINDEX;
		break;
	case LINUX_SIOCGIFFLAGS:
		cmd = SIOCGIFFLAGS;
		break;
	case LINUX_SIOCGIFADDR:
		cmd = SIOCGIFADDR;
		break;
	case LINUX_SIOCSIFADDR:
		cmd = SIOCSIFADDR;
		break;
	case LINUX_SIOCGIFDSTADDR:
		cmd = SIOCGIFDSTADDR;
		break;
	case LINUX_SIOCGIFBRDADDR:
		cmd = SIOCGIFBRDADDR;
		break;
	case LINUX_SIOCGIFNETMASK:
		cmd = SIOCGIFNETMASK;
		break;
	case LINUX_SIOCSIFNETMASK:
		cmd = SIOCSIFNETMASK;
		break;
	case LINUX_SIOCGIFMTU:
		cmd = SIOCGIFMTU;
		break;
	case LINUX_SIOCSIFMTU:
		cmd = SIOCSIFMTU;
		break;
	case LINUX_SIOCGIFHWADDR:
		cmd = SIOCGHWADDR;
		break;
	case LINUX_SIOCGIFMETRIC:
		cmd = SIOCGIFMETRIC;
		break;
	case LINUX_SIOCSIFMETRIC:
		cmd = SIOCSIFMETRIC;
		break;
	/*
	 * XXX This is slightly bogus, but these ioctls are currently
	 * XXX only used by the aironet (if_an) network driver.
	 */
	case LINUX_SIOCDEVPRIVATE:
		cmd = SIOCGPRIVATE_0;
		break;
	case LINUX_SIOCDEVPRIVATE+1:
		cmd = SIOCGPRIVATE_1;
		break;
	default:
		LINUX_RATELIMIT_MSG_OPT2(
		    "ioctl_socket_ifreq fd=%d, cmd=0x%x is not implemented",
		    fd, cmd);
		return (ENOIOCTL);
	}

	error = copyin(uifr, &lifr, sizeof(lifr));
	if (error != 0)
		return (error);
	bzero(&bifr, sizeof(bifr));

	/*
	 * The size of Linux enum ifr_ifru is bigger than
	 * the FreeBSD size due to the struct ifmap.
	 */
	ifrusiz = (sizeof(lifr) > sizeof(bifr) ? sizeof(bifr) :
	    sizeof(lifr)) - offsetof(struct l_ifreq, ifr_ifru);
	bcopy(&lifr.ifr_ifru, &bifr.ifr_ifru, ifrusiz);

	error = ifname_linux_to_bsd(td, lifr.ifr_name, bifr.ifr_name);
	if (error != 0)
		return (error);

	/* Translate in values. */
	switch (cmd) {
	case SIOCGIFINDEX:
		bifr.ifr_index = lifr.ifr_index;
		break;
	case SIOCSIFADDR:
	case SIOCSIFNETMASK:
		bifr.ifr_addr.sa_len = sizeof(struct sockaddr);
		bifr.ifr_addr.sa_family =
		    linux_to_bsd_domain(lifr.ifr_addr.sa_family);
		break;
	default:
		break;
	}

	error = kern_ioctl(td, fd, cmd, (caddr_t)&bifr);
	if (error != 0)
		return (error);
	bzero(&lifr.ifr_ifru, sizeof(lifr.ifr_ifru));

	/* Translate out values. */
 	switch (cmd) {
	case SIOCGIFINDEX:
		lifr.ifr_index = bifr.ifr_index;
		break;
	case SIOCGIFFLAGS:
		temp_flags = bifr.ifr_flags | (bifr.ifr_flagshigh << 16);
		lifr.ifr_flags = bsd_to_linux_ifflags(temp_flags);
		break;
	case SIOCGIFADDR:
	case SIOCSIFADDR:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
	case SIOCGIFNETMASK:
		bcopy(&bifr.ifr_addr, &lifr.ifr_addr, sizeof(bifr.ifr_addr));
		lifr.ifr_addr.sa_family =
		    bsd_to_linux_domain(bifr.ifr_addr.sa_family);
		break;
	case SIOCGHWADDR:
		bcopy(&bifr.ifr_addr, &lifr.ifr_hwaddr, sizeof(bifr.ifr_addr));
		lifr.ifr_hwaddr.sa_family = LINUX_ARPHRD_ETHER;
		break;
	default:
		bcopy(&bifr.ifr_ifru, &lifr.ifr_ifru, ifrusiz);
		break;
	}

	return (copyout(&lifr, uifr, sizeof(lifr)));
}

/*
 * Socket related ioctls
 */

static int
linux_ioctl_socket(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error, type;

	error = fget(td, args->fd, &cap_ioctl_rights, &fp);
	if (error != 0)
		return (error);
	type = fp->f_type;
	fdrop(fp, td);

	CURVNET_SET(TD_TO_VNET(td));

	if (type != DTYPE_SOCKET) {
		/* not a socket - probably a tap / vmnet device */
		switch (args->cmd) {
		case LINUX_SIOCGIFADDR:
		case LINUX_SIOCSIFADDR:
		case LINUX_SIOCGIFFLAGS:
			error = linux_ioctl_special(td, args);
			break;
		default:
			error = ENOIOCTL;
			break;
		}
		CURVNET_RESTORE();
		return (error);
	}

	switch (args->cmd) {
	case LINUX_FIOSETOWN:
		args->cmd = FIOSETOWN;
		error = sys_ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCSPGRP:
		args->cmd = SIOCSPGRP;
		error = sys_ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_FIOGETOWN:
		args->cmd = FIOGETOWN;
		error = sys_ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCGPGRP:
		args->cmd = SIOCGPGRP;
		error = sys_ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCATMARK:
		args->cmd = SIOCATMARK;
		error = sys_ioctl(td, (struct ioctl_args *)args);
		break;

	/* LINUX_SIOCGSTAMP */

	case LINUX_SIOCGIFNAME:
		error = linux_ioctl_ifname(td, (struct l_ifreq *)args->arg);
		break;

	case LINUX_SIOCGIFCONF:
		error = linux_ifconf(td, (struct ifconf *)args->arg);
		break;

	case LINUX_SIOCADDMULTI:
		args->cmd = SIOCADDMULTI;
		error = sys_ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCDELMULTI:
		args->cmd = SIOCDELMULTI;
		error = sys_ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCGIFCOUNT:
		error = 0;
		break;

	default:
		error = linux_ioctl_socket_ifreq(td, args->fd, args->cmd,
		    PTRIN(args->arg));
		break;
	}

	CURVNET_RESTORE();
	return (error);
}

/*
 * Device private ioctl handler
 */
static int
linux_ioctl_private(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error, type;

	error = fget(td, args->fd, &cap_ioctl_rights, &fp);
	if (error != 0)
		return (error);
	type = fp->f_type;
	fdrop(fp, td);
	if (type == DTYPE_SOCKET)
		return (linux_ioctl_socket(td, args));
	return (ENOIOCTL);
}

/*
 * DRM ioctl handler (sys/dev/drm)
 */
static int
linux_ioctl_drm(struct thread *td, struct linux_ioctl_args *args)
{
	args->cmd = SETDIR(args->cmd);
	return (sys_ioctl(td, (struct ioctl_args *)args));
}

#ifdef COMPAT_LINUX32
static int
linux_ioctl_sg_io(struct thread *td, struct linux_ioctl_args *args)
{
	struct sg_io_hdr io;
	struct sg_io_hdr32 io32;
	struct file *fp;
	int error;

	error = fget(td, args->fd, &cap_ioctl_rights, &fp);
	if (error != 0) {
		printf("sg_linux_ioctl: fget returned %d\n", error);
		return (error);
	}

	if ((error = copyin((void *)args->arg, &io32, sizeof(io32))) != 0)
		goto out;

	CP(io32, io, interface_id);
	CP(io32, io, dxfer_direction);
	CP(io32, io, cmd_len);
	CP(io32, io, mx_sb_len);
	CP(io32, io, iovec_count);
	CP(io32, io, dxfer_len);
	PTRIN_CP(io32, io, dxferp);
	PTRIN_CP(io32, io, cmdp);
	PTRIN_CP(io32, io, sbp);
	CP(io32, io, timeout);
	CP(io32, io, flags);
	CP(io32, io, pack_id);
	PTRIN_CP(io32, io, usr_ptr);
	CP(io32, io, status);
	CP(io32, io, masked_status);
	CP(io32, io, msg_status);
	CP(io32, io, sb_len_wr);
	CP(io32, io, host_status);
	CP(io32, io, driver_status);
	CP(io32, io, resid);
	CP(io32, io, duration);
	CP(io32, io, info);

	if ((error = fo_ioctl(fp, SG_IO, (caddr_t)&io, td->td_ucred, td)) != 0)
		goto out;

	CP(io, io32, interface_id);
	CP(io, io32, dxfer_direction);
	CP(io, io32, cmd_len);
	CP(io, io32, mx_sb_len);
	CP(io, io32, iovec_count);
	CP(io, io32, dxfer_len);
	PTROUT_CP(io, io32, dxferp);
	PTROUT_CP(io, io32, cmdp);
	PTROUT_CP(io, io32, sbp);
	CP(io, io32, timeout);
	CP(io, io32, flags);
	CP(io, io32, pack_id);
	PTROUT_CP(io, io32, usr_ptr);
	CP(io, io32, status);
	CP(io, io32, masked_status);
	CP(io, io32, msg_status);
	CP(io, io32, sb_len_wr);
	CP(io, io32, host_status);
	CP(io, io32, driver_status);
	CP(io, io32, resid);
	CP(io, io32, duration);
	CP(io, io32, info);

	error = copyout(&io32, (void *)args->arg, sizeof(io32));

out:
	fdrop(fp, td);
	return (error);
}
#endif

static int
linux_ioctl_sg(struct thread *td, struct linux_ioctl_args *args)
{

	switch (args->cmd) {
	case LINUX_SG_GET_VERSION_NUM:
		args->cmd = SG_GET_VERSION_NUM;
		break;
	case LINUX_SG_SET_TIMEOUT:
		args->cmd = SG_SET_TIMEOUT;
		break;
	case LINUX_SG_GET_TIMEOUT:
		args->cmd = SG_GET_TIMEOUT;
		break;
	case LINUX_SG_IO:
		args->cmd = SG_IO;
#ifdef COMPAT_LINUX32
		return (linux_ioctl_sg_io(td, args));
#endif
		break;
	case LINUX_SG_GET_RESERVED_SIZE:
		args->cmd = SG_GET_RESERVED_SIZE;
		break;
	case LINUX_SG_GET_SCSI_ID:
		args->cmd = SG_GET_SCSI_ID;
		break;
	case LINUX_SG_GET_SG_TABLESIZE:
		args->cmd = SG_GET_SG_TABLESIZE;
		break;
	default:
		return (ENODEV);
	}
	return (sys_ioctl(td, (struct ioctl_args *)args));
}

/*
 * Video4Linux (V4L) ioctl handler
 */
static int
linux_to_bsd_v4l_tuner(struct l_video_tuner *lvt, struct video_tuner *vt)
{
	vt->tuner = lvt->tuner;
	strlcpy(vt->name, lvt->name, LINUX_VIDEO_TUNER_NAME_SIZE);
	vt->rangelow = lvt->rangelow;	/* possible long size conversion */
	vt->rangehigh = lvt->rangehigh;	/* possible long size conversion */
	vt->flags = lvt->flags;
	vt->mode = lvt->mode;
	vt->signal = lvt->signal;
	return (0);
}

static int
bsd_to_linux_v4l_tuner(struct video_tuner *vt, struct l_video_tuner *lvt)
{
	lvt->tuner = vt->tuner;
	strlcpy(lvt->name, vt->name, LINUX_VIDEO_TUNER_NAME_SIZE);
	lvt->rangelow = vt->rangelow;	/* possible long size conversion */
	lvt->rangehigh = vt->rangehigh;	/* possible long size conversion */
	lvt->flags = vt->flags;
	lvt->mode = vt->mode;
	lvt->signal = vt->signal;
	return (0);
}

#ifdef COMPAT_LINUX_V4L_CLIPLIST
static int
linux_to_bsd_v4l_clip(struct l_video_clip *lvc, struct video_clip *vc)
{
	vc->x = lvc->x;
	vc->y = lvc->y;
	vc->width = lvc->width;
	vc->height = lvc->height;
	vc->next = PTRIN(lvc->next);	/* possible pointer size conversion */
	return (0);
}
#endif

static int
linux_to_bsd_v4l_window(struct l_video_window *lvw, struct video_window *vw)
{
	vw->x = lvw->x;
	vw->y = lvw->y;
	vw->width = lvw->width;
	vw->height = lvw->height;
	vw->chromakey = lvw->chromakey;
	vw->flags = lvw->flags;
	vw->clips = PTRIN(lvw->clips);	/* possible pointer size conversion */
	vw->clipcount = lvw->clipcount;
	return (0);
}

static int
bsd_to_linux_v4l_window(struct video_window *vw, struct l_video_window *lvw)
{
	memset(lvw, 0, sizeof(*lvw));

	lvw->x = vw->x;
	lvw->y = vw->y;
	lvw->width = vw->width;
	lvw->height = vw->height;
	lvw->chromakey = vw->chromakey;
	lvw->flags = vw->flags;
	lvw->clips = PTROUT(vw->clips);	/* possible pointer size conversion */
	lvw->clipcount = vw->clipcount;
	return (0);
}

static int
linux_to_bsd_v4l_buffer(struct l_video_buffer *lvb, struct video_buffer *vb)
{
	vb->base = PTRIN(lvb->base);	/* possible pointer size conversion */
	vb->height = lvb->height;
	vb->width = lvb->width;
	vb->depth = lvb->depth;
	vb->bytesperline = lvb->bytesperline;
	return (0);
}

static int
bsd_to_linux_v4l_buffer(struct video_buffer *vb, struct l_video_buffer *lvb)
{
	lvb->base = PTROUT(vb->base);	/* possible pointer size conversion */
	lvb->height = vb->height;
	lvb->width = vb->width;
	lvb->depth = vb->depth;
	lvb->bytesperline = vb->bytesperline;
	return (0);
}

static int
linux_to_bsd_v4l_code(struct l_video_code *lvc, struct video_code *vc)
{
	strlcpy(vc->loadwhat, lvc->loadwhat, LINUX_VIDEO_CODE_LOADWHAT_SIZE);
	vc->datasize = lvc->datasize;
	vc->data = PTRIN(lvc->data);	/* possible pointer size conversion */
	return (0);
}

#ifdef COMPAT_LINUX_V4L_CLIPLIST
static int
linux_v4l_clip_copy(void *lvc, struct video_clip **ppvc)
{
	int error;
	struct video_clip vclip;
	struct l_video_clip l_vclip;

	error = copyin(lvc, &l_vclip, sizeof(l_vclip));
	if (error) return (error);
	linux_to_bsd_v4l_clip(&l_vclip, &vclip);
	/* XXX: If there can be no concurrency: s/M_NOWAIT/M_WAITOK/ */
	if ((*ppvc = malloc(sizeof(**ppvc), M_LINUX, M_NOWAIT)) == NULL)
		return (ENOMEM);    /* XXX: Linux has no ENOMEM here. */
	memcpy(*ppvc, &vclip, sizeof(vclip));
	(*ppvc)->next = NULL;
	return (0);
}

static int
linux_v4l_cliplist_free(struct video_window *vw)
{
	struct video_clip **ppvc;
	struct video_clip **ppvc_next;

	for (ppvc = &(vw->clips); *ppvc != NULL; ppvc = ppvc_next) {
		ppvc_next = &((*ppvc)->next);
		free(*ppvc, M_LINUX);
	}
	vw->clips = NULL;

	return (0);
}

static int
linux_v4l_cliplist_copy(struct l_video_window *lvw, struct video_window *vw)
{
	int error;
	int clipcount;
	void *plvc;
	struct video_clip **ppvc;

	/*
	 * XXX: The cliplist is used to pass in a list of clipping
	 *	rectangles or, if clipcount == VIDEO_CLIP_BITMAP, a
	 *	clipping bitmap.  Some Linux apps, however, appear to
	 *	leave cliplist and clips uninitialized.  In any case,
	 *	the cliplist is not used by pwc(4), at the time of
	 *	writing, FreeBSD's only V4L driver.  When a driver
	 *	that uses the cliplist is developed, this code may
	 *	need re-examiniation.
	 */
	error = 0;
	clipcount = vw->clipcount;
	if (clipcount == VIDEO_CLIP_BITMAP) {
		/*
		 * In this case, the pointer (clips) is overloaded
		 * to be a "void *" to a bitmap, therefore there
		 * is no struct video_clip to copy now.
		 */
	} else if (clipcount > 0 && clipcount <= 16384) {
		/*
		 * Clips points to list of clip rectangles, so
		 * copy the list.
		 *
		 * XXX: Upper limit of 16384 was used here to try to
		 *	avoid cases when clipcount and clips pointer
		 *	are uninitialized and therefore have high random
		 *	values, as is the case in the Linux Skype
		 *	application.  The value 16384 was chosen as that
		 *	is what is used in the Linux stradis(4) MPEG
		 *	decoder driver, the only place we found an
		 *	example of cliplist use.
		 */
		plvc = PTRIN(lvw->clips);
		vw->clips = NULL;
		ppvc = &(vw->clips);
		while (clipcount-- > 0) {
			if (plvc == NULL) {
				error = EFAULT;
				break;
			} else {
				error = linux_v4l_clip_copy(plvc, ppvc);
				if (error) {
					linux_v4l_cliplist_free(vw);
					break;
				}
			}
			ppvc = &((*ppvc)->next);
			plvc = PTRIN(((struct l_video_clip *) plvc)->next);
		}
	} else {
		/*
		 * clipcount == 0 or negative (but not VIDEO_CLIP_BITMAP)
		 * Force cliplist to null.
		 */
		vw->clipcount = 0;
		vw->clips = NULL;
	}
	return (error);
}
#endif

static int
linux_ioctl_v4l(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error;
	struct video_tuner vtun;
	struct video_window vwin;
	struct video_buffer vbuf;
	struct video_code vcode;
	struct l_video_tuner l_vtun;
	struct l_video_window l_vwin;
	struct l_video_buffer l_vbuf;
	struct l_video_code l_vcode;

	switch (args->cmd & 0xffff) {
	case LINUX_VIDIOCGCAP:		args->cmd = VIDIOCGCAP; break;
	case LINUX_VIDIOCGCHAN:		args->cmd = VIDIOCGCHAN; break;
	case LINUX_VIDIOCSCHAN:		args->cmd = VIDIOCSCHAN; break;

	case LINUX_VIDIOCGTUNER:
		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error != 0)
			return (error);
		error = copyin((void *) args->arg, &l_vtun, sizeof(l_vtun));
		if (error) {
			fdrop(fp, td);
			return (error);
		}
		linux_to_bsd_v4l_tuner(&l_vtun, &vtun);
		error = fo_ioctl(fp, VIDIOCGTUNER, &vtun, td->td_ucred, td);
		if (!error) {
			bsd_to_linux_v4l_tuner(&vtun, &l_vtun);
			error = copyout(&l_vtun, (void *) args->arg,
			    sizeof(l_vtun));
		}
		fdrop(fp, td);
		return (error);

	case LINUX_VIDIOCSTUNER:
		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error != 0)
			return (error);
		error = copyin((void *) args->arg, &l_vtun, sizeof(l_vtun));
		if (error) {
			fdrop(fp, td);
			return (error);
		}
		linux_to_bsd_v4l_tuner(&l_vtun, &vtun);
		error = fo_ioctl(fp, VIDIOCSTUNER, &vtun, td->td_ucred, td);
		fdrop(fp, td);
		return (error);

	case LINUX_VIDIOCGPICT:		args->cmd = VIDIOCGPICT; break;
	case LINUX_VIDIOCSPICT:		args->cmd = VIDIOCSPICT; break;
	case LINUX_VIDIOCCAPTURE:	args->cmd = VIDIOCCAPTURE; break;

	case LINUX_VIDIOCGWIN:
		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error != 0)
			return (error);
		error = fo_ioctl(fp, VIDIOCGWIN, &vwin, td->td_ucred, td);
		if (!error) {
			bsd_to_linux_v4l_window(&vwin, &l_vwin);
			error = copyout(&l_vwin, (void *) args->arg,
			    sizeof(l_vwin));
		}
		fdrop(fp, td);
		return (error);

	case LINUX_VIDIOCSWIN:
		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error != 0)
			return (error);
		error = copyin((void *) args->arg, &l_vwin, sizeof(l_vwin));
		if (error) {
			fdrop(fp, td);
			return (error);
		}
		linux_to_bsd_v4l_window(&l_vwin, &vwin);
#ifdef COMPAT_LINUX_V4L_CLIPLIST
		error = linux_v4l_cliplist_copy(&l_vwin, &vwin);
		if (error) {
			fdrop(fp, td);
			return (error);
		}
#endif
		error = fo_ioctl(fp, VIDIOCSWIN, &vwin, td->td_ucred, td);
		fdrop(fp, td);
#ifdef COMPAT_LINUX_V4L_CLIPLIST
		linux_v4l_cliplist_free(&vwin);
#endif
		return (error);

	case LINUX_VIDIOCGFBUF:
		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error != 0)
			return (error);
		error = fo_ioctl(fp, VIDIOCGFBUF, &vbuf, td->td_ucred, td);
		if (!error) {
			bsd_to_linux_v4l_buffer(&vbuf, &l_vbuf);
			error = copyout(&l_vbuf, (void *) args->arg,
			    sizeof(l_vbuf));
		}
		fdrop(fp, td);
		return (error);

	case LINUX_VIDIOCSFBUF:
		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error != 0)
			return (error);
		error = copyin((void *) args->arg, &l_vbuf, sizeof(l_vbuf));
		if (error) {
			fdrop(fp, td);
			return (error);
		}
		linux_to_bsd_v4l_buffer(&l_vbuf, &vbuf);
		error = fo_ioctl(fp, VIDIOCSFBUF, &vbuf, td->td_ucred, td);
		fdrop(fp, td);
		return (error);

	case LINUX_VIDIOCKEY:		args->cmd = VIDIOCKEY; break;
	case LINUX_VIDIOCGFREQ:		args->cmd = VIDIOCGFREQ; break;
	case LINUX_VIDIOCSFREQ:		args->cmd = VIDIOCSFREQ; break;
	case LINUX_VIDIOCGAUDIO:	args->cmd = VIDIOCGAUDIO; break;
	case LINUX_VIDIOCSAUDIO:	args->cmd = VIDIOCSAUDIO; break;
	case LINUX_VIDIOCSYNC:		args->cmd = VIDIOCSYNC; break;
	case LINUX_VIDIOCMCAPTURE:	args->cmd = VIDIOCMCAPTURE; break;
	case LINUX_VIDIOCGMBUF:		args->cmd = VIDIOCGMBUF; break;
	case LINUX_VIDIOCGUNIT:		args->cmd = VIDIOCGUNIT; break;
	case LINUX_VIDIOCGCAPTURE:	args->cmd = VIDIOCGCAPTURE; break;
	case LINUX_VIDIOCSCAPTURE:	args->cmd = VIDIOCSCAPTURE; break;
	case LINUX_VIDIOCSPLAYMODE:	args->cmd = VIDIOCSPLAYMODE; break;
	case LINUX_VIDIOCSWRITEMODE:	args->cmd = VIDIOCSWRITEMODE; break;
	case LINUX_VIDIOCGPLAYINFO:	args->cmd = VIDIOCGPLAYINFO; break;

	case LINUX_VIDIOCSMICROCODE:
		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error != 0)
			return (error);
		error = copyin((void *) args->arg, &l_vcode, sizeof(l_vcode));
		if (error) {
			fdrop(fp, td);
			return (error);
		}
		linux_to_bsd_v4l_code(&l_vcode, &vcode);
		error = fo_ioctl(fp, VIDIOCSMICROCODE, &vcode, td->td_ucred, td);
		fdrop(fp, td);
		return (error);

	case LINUX_VIDIOCGVBIFMT:	args->cmd = VIDIOCGVBIFMT; break;
	case LINUX_VIDIOCSVBIFMT:	args->cmd = VIDIOCSVBIFMT; break;
	default:			return (ENOIOCTL);
	}

	error = sys_ioctl(td, (struct ioctl_args *)args);
	return (error);
}

/*
 * Special ioctl handler
 */
static int
linux_ioctl_special(struct thread *td, struct linux_ioctl_args *args)
{
	int error;

	switch (args->cmd) {
	case LINUX_SIOCGIFADDR:
		args->cmd = SIOCGIFADDR;
		error = sys_ioctl(td, (struct ioctl_args *)args);
		break;
	case LINUX_SIOCSIFADDR:
		args->cmd = SIOCSIFADDR;
		error = sys_ioctl(td, (struct ioctl_args *)args);
		break;
	case LINUX_SIOCGIFFLAGS:
		args->cmd = SIOCGIFFLAGS;
		error = sys_ioctl(td, (struct ioctl_args *)args);
		break;
	default:
		error = ENOIOCTL;
	}

	return (error);
}

static int
linux_to_bsd_v4l2_standard(struct l_v4l2_standard *lvstd, struct v4l2_standard *vstd)
{
	vstd->index = lvstd->index;
	vstd->id = lvstd->id;
	CTASSERT(sizeof(vstd->name) == sizeof(lvstd->name));
	memcpy(vstd->name, lvstd->name, sizeof(vstd->name));
	vstd->frameperiod = lvstd->frameperiod;
	vstd->framelines = lvstd->framelines;
	CTASSERT(sizeof(vstd->reserved) == sizeof(lvstd->reserved));
	memcpy(vstd->reserved, lvstd->reserved, sizeof(vstd->reserved));
	return (0);
}

static int
bsd_to_linux_v4l2_standard(struct v4l2_standard *vstd, struct l_v4l2_standard *lvstd)
{
	lvstd->index = vstd->index;
	lvstd->id = vstd->id;
	CTASSERT(sizeof(vstd->name) == sizeof(lvstd->name));
	memcpy(lvstd->name, vstd->name, sizeof(lvstd->name));
	lvstd->frameperiod = vstd->frameperiod;
	lvstd->framelines = vstd->framelines;
	CTASSERT(sizeof(vstd->reserved) == sizeof(lvstd->reserved));
	memcpy(lvstd->reserved, vstd->reserved, sizeof(lvstd->reserved));
	return (0);
}

static int
linux_to_bsd_v4l2_buffer(struct l_v4l2_buffer *lvb, struct v4l2_buffer *vb)
{
	vb->index = lvb->index;
	vb->type = lvb->type;
	vb->bytesused = lvb->bytesused;
	vb->flags = lvb->flags;
	vb->field = lvb->field;
	vb->timestamp.tv_sec = lvb->timestamp.tv_sec;
	vb->timestamp.tv_usec = lvb->timestamp.tv_usec;
	memcpy(&vb->timecode, &lvb->timecode, sizeof (lvb->timecode));
	vb->sequence = lvb->sequence;
	vb->memory = lvb->memory;
	if (lvb->memory == V4L2_MEMORY_USERPTR)
		/* possible pointer size conversion */
		vb->m.userptr = (unsigned long)PTRIN(lvb->m.userptr);
	else
		vb->m.offset = lvb->m.offset;
	vb->length = lvb->length;
	vb->input = lvb->input;
	vb->reserved = lvb->reserved;
	return (0);
}

static int
bsd_to_linux_v4l2_buffer(struct v4l2_buffer *vb, struct l_v4l2_buffer *lvb)
{
	lvb->index = vb->index;
	lvb->type = vb->type;
	lvb->bytesused = vb->bytesused;
	lvb->flags = vb->flags;
	lvb->field = vb->field;
	lvb->timestamp.tv_sec = vb->timestamp.tv_sec;
	lvb->timestamp.tv_usec = vb->timestamp.tv_usec;
	memcpy(&lvb->timecode, &vb->timecode, sizeof (vb->timecode));
	lvb->sequence = vb->sequence;
	lvb->memory = vb->memory;
	if (vb->memory == V4L2_MEMORY_USERPTR)
		/* possible pointer size conversion */
		lvb->m.userptr = PTROUT(vb->m.userptr);
	else
		lvb->m.offset = vb->m.offset;
	lvb->length = vb->length;
	lvb->input = vb->input;
	lvb->reserved = vb->reserved;
	return (0);
}

static int
linux_to_bsd_v4l2_format(struct l_v4l2_format *lvf, struct v4l2_format *vf)
{
	vf->type = lvf->type;
	if (lvf->type == V4L2_BUF_TYPE_VIDEO_OVERLAY
#ifdef V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY
	    || lvf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY
#endif
	    )
		/*
		 * XXX TODO - needs 32 -> 64 bit conversion:
		 * (unused by webcams?)
		 */
		return (EINVAL);
	memcpy(&vf->fmt, &lvf->fmt, sizeof(vf->fmt));
	return (0);
}

static int
bsd_to_linux_v4l2_format(struct v4l2_format *vf, struct l_v4l2_format *lvf)
{
	lvf->type = vf->type;
	if (vf->type == V4L2_BUF_TYPE_VIDEO_OVERLAY
#ifdef V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY
	    || vf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY
#endif
	    )
		/*
		 * XXX TODO - needs 32 -> 64 bit conversion:
		 * (unused by webcams?)
		 */
		return (EINVAL);
	memcpy(&lvf->fmt, &vf->fmt, sizeof(vf->fmt));
	return (0);
}
static int
linux_ioctl_v4l2(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error;
	struct v4l2_format vformat;
	struct l_v4l2_format l_vformat;
	struct v4l2_standard vstd;
	struct l_v4l2_standard l_vstd;
	struct l_v4l2_buffer l_vbuf;
	struct v4l2_buffer vbuf;
	struct v4l2_input vinp;

	switch (args->cmd & 0xffff) {
	case LINUX_VIDIOC_RESERVED:
	case LINUX_VIDIOC_LOG_STATUS:
		if ((args->cmd & IOC_DIRMASK) != LINUX_IOC_VOID)
			return (ENOIOCTL);
		args->cmd = (args->cmd & 0xffff) | IOC_VOID;
		break;

	case LINUX_VIDIOC_OVERLAY:
	case LINUX_VIDIOC_STREAMON:
	case LINUX_VIDIOC_STREAMOFF:
	case LINUX_VIDIOC_S_STD:
	case LINUX_VIDIOC_S_TUNER:
	case LINUX_VIDIOC_S_AUDIO:
	case LINUX_VIDIOC_S_AUDOUT:
	case LINUX_VIDIOC_S_MODULATOR:
	case LINUX_VIDIOC_S_FREQUENCY:
	case LINUX_VIDIOC_S_CROP:
	case LINUX_VIDIOC_S_JPEGCOMP:
	case LINUX_VIDIOC_S_PRIORITY:
	case LINUX_VIDIOC_DBG_S_REGISTER:
	case LINUX_VIDIOC_S_HW_FREQ_SEEK:
	case LINUX_VIDIOC_SUBSCRIBE_EVENT:
	case LINUX_VIDIOC_UNSUBSCRIBE_EVENT:
		args->cmd = (args->cmd & ~IOC_DIRMASK) | IOC_IN;
		break;

	case LINUX_VIDIOC_QUERYCAP:
	case LINUX_VIDIOC_G_STD:
	case LINUX_VIDIOC_G_AUDIO:
	case LINUX_VIDIOC_G_INPUT:
	case LINUX_VIDIOC_G_OUTPUT:
	case LINUX_VIDIOC_G_AUDOUT:
	case LINUX_VIDIOC_G_JPEGCOMP:
	case LINUX_VIDIOC_QUERYSTD:
	case LINUX_VIDIOC_G_PRIORITY:
	case LINUX_VIDIOC_QUERY_DV_PRESET:
		args->cmd = (args->cmd & ~IOC_DIRMASK) | IOC_OUT;
		break;

	case LINUX_VIDIOC_ENUM_FMT:
	case LINUX_VIDIOC_REQBUFS:
	case LINUX_VIDIOC_G_PARM:
	case LINUX_VIDIOC_S_PARM:
	case LINUX_VIDIOC_G_CTRL:
	case LINUX_VIDIOC_S_CTRL:
	case LINUX_VIDIOC_G_TUNER:
	case LINUX_VIDIOC_QUERYCTRL:
	case LINUX_VIDIOC_QUERYMENU:
	case LINUX_VIDIOC_S_INPUT:
	case LINUX_VIDIOC_S_OUTPUT:
	case LINUX_VIDIOC_ENUMOUTPUT:
	case LINUX_VIDIOC_G_MODULATOR:
	case LINUX_VIDIOC_G_FREQUENCY:
	case LINUX_VIDIOC_CROPCAP:
	case LINUX_VIDIOC_G_CROP:
	case LINUX_VIDIOC_ENUMAUDIO:
	case LINUX_VIDIOC_ENUMAUDOUT:
	case LINUX_VIDIOC_G_SLICED_VBI_CAP:
#ifdef VIDIOC_ENUM_FRAMESIZES
	case LINUX_VIDIOC_ENUM_FRAMESIZES:
	case LINUX_VIDIOC_ENUM_FRAMEINTERVALS:
	case LINUX_VIDIOC_ENCODER_CMD:
	case LINUX_VIDIOC_TRY_ENCODER_CMD:
#endif
	case LINUX_VIDIOC_DBG_G_REGISTER:
	case LINUX_VIDIOC_DBG_G_CHIP_IDENT:
	case LINUX_VIDIOC_ENUM_DV_PRESETS:
	case LINUX_VIDIOC_S_DV_PRESET:
	case LINUX_VIDIOC_G_DV_PRESET:
	case LINUX_VIDIOC_S_DV_TIMINGS:
	case LINUX_VIDIOC_G_DV_TIMINGS:
		args->cmd = (args->cmd & ~IOC_DIRMASK) | IOC_INOUT;
		break;

	case LINUX_VIDIOC_G_FMT:
	case LINUX_VIDIOC_S_FMT:
	case LINUX_VIDIOC_TRY_FMT:
		error = copyin((void *)args->arg, &l_vformat, sizeof(l_vformat));
		if (error)
			return (error);
		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error)
			return (error);
		if (linux_to_bsd_v4l2_format(&l_vformat, &vformat) != 0)
			error = EINVAL;
		else if ((args->cmd & 0xffff) == LINUX_VIDIOC_G_FMT)
			error = fo_ioctl(fp, VIDIOC_G_FMT, &vformat,
			    td->td_ucred, td);
		else if ((args->cmd & 0xffff) == LINUX_VIDIOC_S_FMT)
			error = fo_ioctl(fp, VIDIOC_S_FMT, &vformat,
			    td->td_ucred, td);
		else
			error = fo_ioctl(fp, VIDIOC_TRY_FMT, &vformat,
			    td->td_ucred, td);
		bsd_to_linux_v4l2_format(&vformat, &l_vformat);
		if (error == 0)
			error = copyout(&l_vformat, (void *)args->arg,
			    sizeof(l_vformat));
		fdrop(fp, td);
		return (error);

	case LINUX_VIDIOC_ENUMSTD:
		error = copyin((void *)args->arg, &l_vstd, sizeof(l_vstd));
		if (error)
			return (error);
		linux_to_bsd_v4l2_standard(&l_vstd, &vstd);
		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error)
			return (error);
		error = fo_ioctl(fp, VIDIOC_ENUMSTD, (caddr_t)&vstd,
		    td->td_ucred, td);
		if (error) {
			fdrop(fp, td);
			return (error);
		}
		bsd_to_linux_v4l2_standard(&vstd, &l_vstd);
		error = copyout(&l_vstd, (void *)args->arg, sizeof(l_vstd));
		fdrop(fp, td);
		return (error);

	case LINUX_VIDIOC_ENUMINPUT:
		/*
		 * The Linux struct l_v4l2_input differs only in size,
		 * it has no padding at the end.
		 */
		error = copyin((void *)args->arg, &vinp,
				sizeof(struct l_v4l2_input));
		if (error != 0)
			return (error);
		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error != 0)
			return (error);
		error = fo_ioctl(fp, VIDIOC_ENUMINPUT, (caddr_t)&vinp,
		    td->td_ucred, td);
		if (error) {
			fdrop(fp, td);
			return (error);
		}
		error = copyout(&vinp, (void *)args->arg,
				sizeof(struct l_v4l2_input));
		fdrop(fp, td);
		return (error);

	case LINUX_VIDIOC_QUERYBUF:
	case LINUX_VIDIOC_QBUF:
	case LINUX_VIDIOC_DQBUF:
		error = copyin((void *)args->arg, &l_vbuf, sizeof(l_vbuf));
		if (error)
			return (error);
		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error)
			return (error);
		linux_to_bsd_v4l2_buffer(&l_vbuf, &vbuf);
		if ((args->cmd & 0xffff) == LINUX_VIDIOC_QUERYBUF)
			error = fo_ioctl(fp, VIDIOC_QUERYBUF, &vbuf,
			    td->td_ucred, td);
		else if ((args->cmd & 0xffff) == LINUX_VIDIOC_QBUF)
			error = fo_ioctl(fp, VIDIOC_QBUF, &vbuf,
			    td->td_ucred, td);
		else
			error = fo_ioctl(fp, VIDIOC_DQBUF, &vbuf,
			    td->td_ucred, td);
		bsd_to_linux_v4l2_buffer(&vbuf, &l_vbuf);
		if (error == 0)
			error = copyout(&l_vbuf, (void *)args->arg,
			    sizeof(l_vbuf));
		fdrop(fp, td);
		return (error);

	/*
	 * XXX TODO - these need 32 -> 64 bit conversion:
	 * (are any of them needed for webcams?)
	 */
	case LINUX_VIDIOC_G_FBUF:
	case LINUX_VIDIOC_S_FBUF:

	case LINUX_VIDIOC_G_EXT_CTRLS:
	case LINUX_VIDIOC_S_EXT_CTRLS:
	case LINUX_VIDIOC_TRY_EXT_CTRLS:

	case LINUX_VIDIOC_DQEVENT:

	default:			return (ENOIOCTL);
	}

	error = sys_ioctl(td, (struct ioctl_args *)args);
	return (error);
}

/*
 * Support for emulators/linux-libusb. This port uses FBSD_LUSB* macros
 * instead of USB* ones. This lets us to provide correct values for cmd.
 * 0xffffffe0 -- 0xffffffff range seemed to be the least collision-prone.
 */
static int
linux_ioctl_fbsd_usb(struct thread *td, struct linux_ioctl_args *args)
{
	int error;

	error = 0;
	switch (args->cmd) {
	case FBSD_LUSB_DEVICEENUMERATE:
		args->cmd = USB_DEVICEENUMERATE;
		break;
	case FBSD_LUSB_DEV_QUIRK_ADD:
		args->cmd = USB_DEV_QUIRK_ADD;
		break;
	case FBSD_LUSB_DEV_QUIRK_GET:
		args->cmd = USB_DEV_QUIRK_GET;
		break;
	case FBSD_LUSB_DEV_QUIRK_REMOVE:
		args->cmd = USB_DEV_QUIRK_REMOVE;
		break;
	case FBSD_LUSB_DO_REQUEST:
		args->cmd = USB_DO_REQUEST;
		break;
	case FBSD_LUSB_FS_CLEAR_STALL_SYNC:
		args->cmd = USB_FS_CLEAR_STALL_SYNC;
		break;
	case FBSD_LUSB_FS_CLOSE:
		args->cmd = USB_FS_CLOSE;
		break;
	case FBSD_LUSB_FS_COMPLETE:
		args->cmd = USB_FS_COMPLETE;
		break;
	case FBSD_LUSB_FS_INIT:
		args->cmd = USB_FS_INIT;
		break;
	case FBSD_LUSB_FS_OPEN:
		args->cmd = USB_FS_OPEN;
		break;
	case FBSD_LUSB_FS_START:
		args->cmd = USB_FS_START;
		break;
	case FBSD_LUSB_FS_STOP:
		args->cmd = USB_FS_STOP;
		break;
	case FBSD_LUSB_FS_UNINIT:
		args->cmd = USB_FS_UNINIT;
		break;
	case FBSD_LUSB_GET_CONFIG:
		args->cmd = USB_GET_CONFIG;
		break;
	case FBSD_LUSB_GET_DEVICEINFO:
		args->cmd = USB_GET_DEVICEINFO;
		break;
	case FBSD_LUSB_GET_DEVICE_DESC:
		args->cmd = USB_GET_DEVICE_DESC;
		break;
	case FBSD_LUSB_GET_FULL_DESC:
		args->cmd = USB_GET_FULL_DESC;
		break;
	case FBSD_LUSB_GET_IFACE_DRIVER:
		args->cmd = USB_GET_IFACE_DRIVER;
		break;
	case FBSD_LUSB_GET_PLUGTIME:
		args->cmd = USB_GET_PLUGTIME;
		break;
	case FBSD_LUSB_GET_POWER_MODE:
		args->cmd = USB_GET_POWER_MODE;
		break;
	case FBSD_LUSB_GET_REPORT_DESC:
		args->cmd = USB_GET_REPORT_DESC;
		break;
	case FBSD_LUSB_GET_REPORT_ID:
		args->cmd = USB_GET_REPORT_ID;
		break;
	case FBSD_LUSB_GET_TEMPLATE:
		args->cmd = USB_GET_TEMPLATE;
		break;
	case FBSD_LUSB_IFACE_DRIVER_ACTIVE:
		args->cmd = USB_IFACE_DRIVER_ACTIVE;
		break;
	case FBSD_LUSB_IFACE_DRIVER_DETACH:
		args->cmd = USB_IFACE_DRIVER_DETACH;
		break;
	case FBSD_LUSB_QUIRK_NAME_GET:
		args->cmd = USB_QUIRK_NAME_GET;
		break;
	case FBSD_LUSB_READ_DIR:
		args->cmd = USB_READ_DIR;
		break;
	case FBSD_LUSB_SET_ALTINTERFACE:
		args->cmd = USB_SET_ALTINTERFACE;
		break;
	case FBSD_LUSB_SET_CONFIG:
		args->cmd = USB_SET_CONFIG;
		break;
	case FBSD_LUSB_SET_IMMED:
		args->cmd = USB_SET_IMMED;
		break;
	case FBSD_LUSB_SET_POWER_MODE:
		args->cmd = USB_SET_POWER_MODE;
		break;
	case FBSD_LUSB_SET_TEMPLATE:
		args->cmd = USB_SET_TEMPLATE;
		break;
	case FBSD_LUSB_FS_OPEN_STREAM:
		args->cmd = USB_FS_OPEN_STREAM;
		break;
	case FBSD_LUSB_GET_DEV_PORT_PATH:
		args->cmd = USB_GET_DEV_PORT_PATH;
		break;
	case FBSD_LUSB_GET_POWER_USAGE:
		args->cmd = USB_GET_POWER_USAGE;
		break;
	case FBSD_LUSB_DEVICESTATS:
		args->cmd = USB_DEVICESTATS;
		break;
	default:
		error = ENOIOCTL;
	}
	if (error != ENOIOCTL)
		error = sys_ioctl(td, (struct ioctl_args *)args);
	return (error);
}

/*
 * Some evdev ioctls must be translated.
 *  - EVIOCGMTSLOTS is a IOC_READ ioctl on Linux although it has input data
 *    (must be IOC_INOUT on FreeBSD).
 *  - On Linux, EVIOCGRAB, EVIOCREVOKE and EVIOCRMFF are defined as _IOW with
 *    an int argument. You don't pass an int pointer to the ioctl(), however,
 *    but just the int directly. On FreeBSD, they are defined as _IOWINT for
 *    this to work.
 */
static int
linux_ioctl_evdev(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	clockid_t clock;
	int error;

	args->cmd = SETDIR(args->cmd);

	switch (args->cmd) {
	case (EVIOCGRAB & ~IOC_DIRMASK) | IOC_IN:
		args->cmd = EVIOCGRAB;
		break;
	case (EVIOCREVOKE & ~IOC_DIRMASK) | IOC_IN:
		args->cmd = EVIOCREVOKE;
		break;
	case (EVIOCRMFF & ~IOC_DIRMASK) | IOC_IN:
		args->cmd = EVIOCRMFF;
		break;
	case EVIOCSCLOCKID: {
		error = copyin(PTRIN(args->arg), &clock, sizeof(clock));
		if (error != 0)
			return (error);
		if (clock & ~(LINUX_IOCTL_EVDEV_CLK))
			return (EINVAL);
		error = linux_to_native_clockid(&clock, clock);
		if (error != 0)
			return (error);

		error = fget(td, args->fd,
		    &cap_ioctl_rights, &fp);
		if (error != 0)
			return (error);

		error = fo_ioctl(fp, EVIOCSCLOCKID, &clock, td->td_ucred, td);
		fdrop(fp, td);
		return (error);
	}
	default:
		break;
	}

	if (IOCBASECMD(args->cmd) ==
	    ((EVIOCGMTSLOTS(0) & ~IOC_DIRMASK) | IOC_OUT))
		args->cmd = (args->cmd & ~IOC_DIRMASK) | IOC_INOUT;

	return (sys_ioctl(td, (struct ioctl_args *)args));
}

static int
linux_ioctl_kcov(struct thread *td, struct linux_ioctl_args *args)
{
	int error;

	error = 0;
	switch (args->cmd & 0xffff) {
	case LINUX_KCOV_INIT_TRACE:
		args->cmd = KIOSETBUFSIZE;
		break;
	case LINUX_KCOV_ENABLE:
		args->cmd = KIOENABLE;
		if (args->arg == 0)
			args->arg = KCOV_MODE_TRACE_PC;
		else if (args->arg == 1)
			args->arg = KCOV_MODE_TRACE_CMP;
		else
			error = EINVAL;
		break;
	case LINUX_KCOV_DISABLE:
		args->cmd = KIODISABLE;
		break;
	default:
		error = ENOTTY;
		break;
	}

	if (error == 0)
		error = sys_ioctl(td, (struct ioctl_args *)args);
	return (error);
}

#ifndef COMPAT_LINUX32
static int
linux_ioctl_nvme(struct thread *td, struct linux_ioctl_args *args)
{

	/*
	 * The NVMe drivers for namespace and controller implement these
	 * commands using their native format. All the others are not
	 * implemented yet.
	 */
	switch (args->cmd & 0xffff) {
	case LINUX_NVME_IOCTL_ID:
		args->cmd = NVME_IOCTL_ID;
		break;
	case LINUX_NVME_IOCTL_RESET:
		args->cmd = NVME_IOCTL_RESET;
		break;
	case LINUX_NVME_IOCTL_ADMIN_CMD:
		args->cmd = NVME_IOCTL_ADMIN_CMD;
		break;
	case LINUX_NVME_IOCTL_IO_CMD:
		args->cmd = NVME_IOCTL_IO_CMD;
		break;
	default:
		return (ENODEV);
	}
	return (sys_ioctl(td, (struct ioctl_args *)args));
}
#endif

/*
 * main ioctl syscall function
 */

static int
linux_ioctl_fallback(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	struct linux_ioctl_handler_element *he;
	int error, cmd;

	error = fget(td, args->fd, &cap_ioctl_rights, &fp);
	if (error != 0)
		return (error);
	if ((fp->f_flag & (FREAD|FWRITE)) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}

	/* Iterate over the ioctl handlers */
	cmd = args->cmd & 0xffff;
	sx_slock(&linux_ioctl_sx);
	mtx_lock(&Giant);
#ifdef COMPAT_LINUX32
	TAILQ_FOREACH(he, &linux32_ioctl_handlers, list) {
		if (cmd >= he->low && cmd <= he->high) {
			error = (*he->func)(td, args);
			if (error != ENOIOCTL) {
				mtx_unlock(&Giant);
				sx_sunlock(&linux_ioctl_sx);
				fdrop(fp, td);
				return (error);
			}
		}
	}
#endif
	TAILQ_FOREACH(he, &linux_ioctl_handlers, list) {
		if (cmd >= he->low && cmd <= he->high) {
			error = (*he->func)(td, args);
			if (error != ENOIOCTL) {
				mtx_unlock(&Giant);
				sx_sunlock(&linux_ioctl_sx);
				fdrop(fp, td);
				return (error);
			}
		}
	}
	mtx_unlock(&Giant);
	sx_sunlock(&linux_ioctl_sx);
	fdrop(fp, td);

	switch (args->cmd & 0xffff) {
	case LINUX_BTRFS_IOC_CLONE:
	case LINUX_F2FS_IOC_GET_FEATURES:
	case LINUX_FS_IOC_FIEMAP:
		return (ENOTSUP);

	default:
		linux_msg(td, "%s fd=%d, cmd=0x%x ('%c',%d) is not implemented",
		    __func__, args->fd, args->cmd,
		    (int)(args->cmd & 0xff00) >> 8, (int)(args->cmd & 0xff));
		break;
	}

	return (EINVAL);
}

int
linux_ioctl(struct thread *td, struct linux_ioctl_args *args)
{
	struct linux_ioctl_handler *handler;
	int error, cmd, i;

	cmd = args->cmd & 0xffff;

	/*
	 * array of ioctls known at compilation time. Elides a lot of work on
	 * each call compared to the list variant. Everything frequently used
	 * should be moved here.
	 *
	 * Arguably the magic creating the list should create an array instead.
	 *
	 * For now just a linear scan.
	 */
	for (i = 0; i < nitems(linux_ioctls); i++) {
		handler = &linux_ioctls[i];
		if (cmd >= handler->low && cmd <= handler->high) {
			error = (*handler->func)(td, args);
			if (error != ENOIOCTL) {
				return (error);
			}
		}
	}
	return (linux_ioctl_fallback(td, args));
}

int
linux_ioctl_register_handler(struct linux_ioctl_handler *h)
{
	struct linux_ioctl_handler_element *he, *cur;

	if (h == NULL || h->func == NULL)
		return (EINVAL);

	/*
	 * Reuse the element if the handler is already on the list, otherwise
	 * create a new element.
	 */
	sx_xlock(&linux_ioctl_sx);
	TAILQ_FOREACH(he, &linux_ioctl_handlers, list) {
		if (he->func == h->func)
			break;
	}
	if (he == NULL) {
		he = malloc(sizeof(*he),
		    M_LINUX, M_WAITOK);
		he->func = h->func;
	} else
		TAILQ_REMOVE(&linux_ioctl_handlers, he, list);

	/* Initialize range information. */
	he->low = h->low;
	he->high = h->high;
	he->span = h->high - h->low + 1;

	/* Add the element to the list, sorted on span. */
	TAILQ_FOREACH(cur, &linux_ioctl_handlers, list) {
		if (cur->span > he->span) {
			TAILQ_INSERT_BEFORE(cur, he, list);
			sx_xunlock(&linux_ioctl_sx);
			return (0);
		}
	}
	TAILQ_INSERT_TAIL(&linux_ioctl_handlers, he, list);
	sx_xunlock(&linux_ioctl_sx);

	return (0);
}

int
linux_ioctl_unregister_handler(struct linux_ioctl_handler *h)
{
	struct linux_ioctl_handler_element *he;

	if (h == NULL || h->func == NULL)
		return (EINVAL);

	sx_xlock(&linux_ioctl_sx);
	TAILQ_FOREACH(he, &linux_ioctl_handlers, list) {
		if (he->func == h->func) {
			TAILQ_REMOVE(&linux_ioctl_handlers, he, list);
			sx_xunlock(&linux_ioctl_sx);
			free(he, M_LINUX);
			return (0);
		}
	}
	sx_xunlock(&linux_ioctl_sx);

	return (EINVAL);
}

#ifdef COMPAT_LINUX32
int
linux32_ioctl_register_handler(struct linux_ioctl_handler *h)
{
	struct linux_ioctl_handler_element *he, *cur;

	if (h == NULL || h->func == NULL)
		return (EINVAL);

	/*
	 * Reuse the element if the handler is already on the list, otherwise
	 * create a new element.
	 */
	sx_xlock(&linux_ioctl_sx);
	TAILQ_FOREACH(he, &linux32_ioctl_handlers, list) {
		if (he->func == h->func)
			break;
	}
	if (he == NULL) {
		he = malloc(sizeof(*he), M_LINUX, M_WAITOK);
		he->func = h->func;
	} else
		TAILQ_REMOVE(&linux32_ioctl_handlers, he, list);

	/* Initialize range information. */
	he->low = h->low;
	he->high = h->high;
	he->span = h->high - h->low + 1;

	/* Add the element to the list, sorted on span. */
	TAILQ_FOREACH(cur, &linux32_ioctl_handlers, list) {
		if (cur->span > he->span) {
			TAILQ_INSERT_BEFORE(cur, he, list);
			sx_xunlock(&linux_ioctl_sx);
			return (0);
		}
	}
	TAILQ_INSERT_TAIL(&linux32_ioctl_handlers, he, list);
	sx_xunlock(&linux_ioctl_sx);

	return (0);
}

int
linux32_ioctl_unregister_handler(struct linux_ioctl_handler *h)
{
	struct linux_ioctl_handler_element *he;

	if (h == NULL || h->func == NULL)
		return (EINVAL);

	sx_xlock(&linux_ioctl_sx);
	TAILQ_FOREACH(he, &linux32_ioctl_handlers, list) {
		if (he->func == h->func) {
			TAILQ_REMOVE(&linux32_ioctl_handlers, he, list);
			sx_xunlock(&linux_ioctl_sx);
			free(he, M_LINUX);
			return (0);
		}
	}
	sx_xunlock(&linux_ioctl_sx);

	return (EINVAL);
}
#endif
