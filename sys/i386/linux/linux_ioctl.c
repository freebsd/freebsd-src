/*-
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
 *  $Id: linux_ioctl.c,v 1.4 1995/12/29 22:12:12 sos Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/ioctl_compat.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/tty.h>
#include <sys/termios.h>

#include <machine/console.h>
#include <machine/soundcard.h>

#include <i386/linux/linux.h>
#include <i386/linux/sysproto.h>

struct linux_termios {
    unsigned long   c_iflag;
    unsigned long   c_oflag;
    unsigned long   c_cflag;
    unsigned long   c_lflag;
    unsigned char   c_line;
    unsigned char   c_cc[LINUX_NCCS];
};

struct linux_winsize {
    unsigned short ws_row, ws_col;
    unsigned short ws_xpixel, ws_ypixel;
};

static struct speedtab sptab[] = {
    { 0, 0 }, { 50, 1 }, { 75, 2 }, { 110, 3 },
    { 134, 4 }, { 135, 4 }, { 150, 5 }, { 200, 6 },
    { 300, 7 }, { 600, 8 }, { 1200, 9 }, { 1800, 10 },
    { 2400, 11 }, { 4800, 12 }, { 9600, 13 },
    { 19200, 14 }, { 38400, 15 }, 
    { 57600, 4097 }, { 115200, 4098 }, {-1, -1 }
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
bsd_to_linux_termios(struct termios *bsd_termios, 
		struct linux_termios *linux_termios)
{
    int i, speed;

#ifdef DEBUG
    printf("LINUX: BSD termios structure (input):\n");
    printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d\n",
	   bsd_termios->c_iflag, bsd_termios->c_oflag,
	   bsd_termios->c_cflag, bsd_termios->c_lflag,
	   bsd_termios->c_ispeed, bsd_termios->c_ospeed);
    printf("c_cc ");
    for (i=0; i<NCCS; i++)
	printf("%02x ", bsd_termios->c_cc[i]);
    printf("\n");
#endif
    linux_termios->c_iflag = 0;
    if (bsd_termios->c_iflag & IGNBRK)
	linux_termios->c_iflag |= LINUX_IGNBRK;
    if (bsd_termios->c_iflag & BRKINT)
	linux_termios->c_iflag |= LINUX_BRKINT;
    if (bsd_termios->c_iflag & IGNPAR)
	linux_termios->c_iflag |= LINUX_IGNPAR;
    if (bsd_termios->c_iflag & PARMRK)
	linux_termios->c_iflag |= LINUX_PARMRK;
    if (bsd_termios->c_iflag & INPCK)
	linux_termios->c_iflag |= LINUX_INPCK;
    if (bsd_termios->c_iflag & ISTRIP)
	linux_termios->c_iflag |= LINUX_ISTRIP;
    if (bsd_termios->c_iflag & INLCR)
	linux_termios->c_iflag |= LINUX_INLCR;
    if (bsd_termios->c_iflag & IGNCR)
	linux_termios->c_iflag |= LINUX_IGNCR;
    if (bsd_termios->c_iflag & ICRNL)
	linux_termios->c_iflag |= LINUX_ICRNL;
    if (bsd_termios->c_iflag & IXON)
	linux_termios->c_iflag |= LINUX_IXANY;
    if (bsd_termios->c_iflag & IXON)
	linux_termios->c_iflag |= LINUX_IXON;
    if (bsd_termios->c_iflag & IXOFF)
	linux_termios->c_iflag |= LINUX_IXOFF;
    if (bsd_termios->c_iflag & IMAXBEL)
	linux_termios->c_iflag |= LINUX_IMAXBEL;

    linux_termios->c_oflag = 0;
    if (bsd_termios->c_oflag & OPOST)
	linux_termios->c_oflag |= LINUX_OPOST;
    if (bsd_termios->c_oflag & ONLCR)
	linux_termios->c_oflag |= LINUX_ONLCR;
    if (bsd_termios->c_oflag & OXTABS)
	linux_termios->c_oflag |= LINUX_XTABS;

    linux_termios->c_cflag =
	bsd_to_linux_speed(bsd_termios->c_ispeed, sptab);
    linux_termios->c_cflag |= (bsd_termios->c_cflag & CSIZE) >> 4;
    if (bsd_termios->c_cflag & CSTOPB)
	linux_termios->c_cflag |= LINUX_CSTOPB;
    if (bsd_termios->c_cflag & CREAD)
	linux_termios->c_cflag |= LINUX_CREAD;
    if (bsd_termios->c_cflag & PARENB)
	linux_termios->c_cflag |= LINUX_PARENB;
    if (bsd_termios->c_cflag & PARODD)
	linux_termios->c_cflag |= LINUX_PARODD;
    if (bsd_termios->c_cflag & HUPCL)
	linux_termios->c_cflag |= LINUX_HUPCL;
    if (bsd_termios->c_cflag & CLOCAL)
	linux_termios->c_cflag |= LINUX_CLOCAL;
    if (bsd_termios->c_cflag & CRTSCTS)
	linux_termios->c_cflag |= LINUX_CRTSCTS;

    linux_termios->c_lflag = 0;
    if (bsd_termios->c_lflag & ISIG)
	linux_termios->c_lflag |= LINUX_ISIG;
    if (bsd_termios->c_lflag & ICANON)
	linux_termios->c_lflag |= LINUX_ICANON;
    if (bsd_termios->c_lflag & ECHO)
	linux_termios->c_lflag |= LINUX_ECHO;
    if (bsd_termios->c_lflag & ECHOE)
	linux_termios->c_lflag |= LINUX_ECHOE;
    if (bsd_termios->c_lflag & ECHOK)
	linux_termios->c_lflag |= LINUX_ECHOK;
    if (bsd_termios->c_lflag & ECHONL)
	linux_termios->c_lflag |= LINUX_ECHONL;
    if (bsd_termios->c_lflag & NOFLSH)
	linux_termios->c_lflag |= LINUX_NOFLSH;
    if (bsd_termios->c_lflag & TOSTOP)
	linux_termios->c_lflag |= LINUX_TOSTOP;
    if (bsd_termios->c_lflag & ECHOCTL)
	linux_termios->c_lflag |= LINUX_ECHOCTL;
    if (bsd_termios->c_lflag & ECHOPRT)
	linux_termios->c_lflag |= LINUX_ECHOPRT;
    if (bsd_termios->c_lflag & ECHOKE)
	linux_termios->c_lflag |= LINUX_ECHOKE;
    if (bsd_termios->c_lflag & FLUSHO)
	linux_termios->c_lflag |= LINUX_FLUSHO;
    if (bsd_termios->c_lflag & PENDIN)
	linux_termios->c_lflag |= LINUX_PENDIN;
    if (bsd_termios->c_lflag & IEXTEN)
	linux_termios->c_lflag |= LINUX_IEXTEN;

    for (i=0; i<LINUX_NCCS; i++) 
	linux_termios->c_cc[i] = _POSIX_VDISABLE;
    linux_termios->c_cc[LINUX_VINTR] = bsd_termios->c_cc[VINTR];
    linux_termios->c_cc[LINUX_VQUIT] = bsd_termios->c_cc[VQUIT];
    linux_termios->c_cc[LINUX_VERASE] = bsd_termios->c_cc[VERASE];
    linux_termios->c_cc[LINUX_VKILL] = bsd_termios->c_cc[VKILL];
    linux_termios->c_cc[LINUX_VEOF] = bsd_termios->c_cc[VEOF];
    linux_termios->c_cc[LINUX_VEOL] = bsd_termios->c_cc[VEOL];
    linux_termios->c_cc[LINUX_VMIN] = bsd_termios->c_cc[VMIN];
    linux_termios->c_cc[LINUX_VTIME] = bsd_termios->c_cc[VTIME];
    linux_termios->c_cc[LINUX_VEOL2] = bsd_termios->c_cc[VEOL2];
    linux_termios->c_cc[LINUX_VSWTC] = _POSIX_VDISABLE;
    linux_termios->c_cc[LINUX_VSUSP] = bsd_termios->c_cc[VSUSP];
    linux_termios->c_cc[LINUX_VSTART] = bsd_termios->c_cc[VSTART];
    linux_termios->c_cc[LINUX_VSTOP] = bsd_termios->c_cc[VSTOP];
    linux_termios->c_cc[LINUX_VREPRINT] = bsd_termios->c_cc[VREPRINT];
    linux_termios->c_cc[LINUX_VDISCARD] = bsd_termios->c_cc[VDISCARD];
    linux_termios->c_cc[LINUX_VWERASE] = bsd_termios->c_cc[VWERASE];
    linux_termios->c_cc[LINUX_VLNEXT] = bsd_termios->c_cc[VLNEXT];

    linux_termios->c_line = 0;
#ifdef DEBUG
    printf("LINUX: LINUX termios structure (output):\n");
    printf("i=%08x o=%08x c=%08x l=%08x line=%d\n",
	   linux_termios->c_iflag, linux_termios->c_oflag,
	   linux_termios->c_cflag, linux_termios->c_lflag,
	   linux_termios->c_line);
    printf("c_cc ");
    for (i=0; i<LINUX_NCCS; i++) 
	printf("%02x ", linux_termios->c_cc[i]);
    printf("\n");
#endif
}

static void
linux_to_bsd_termios(struct linux_termios *linux_termios,
		struct termios *bsd_termios)
{
    int i, speed;
#ifdef DEBUG
    printf("LINUX: LINUX termios structure (input):\n");
    printf("i=%08x o=%08x c=%08x l=%08x line=%d\n",
	   linux_termios->c_iflag, linux_termios->c_oflag,
	   linux_termios->c_cflag, linux_termios->c_lflag,
	   linux_termios->c_line);
    printf("c_cc ");
    for (i=0; i<LINUX_NCCS; i++) 
	printf("%02x ", linux_termios->c_cc[i]);
    printf("\n");
#endif
    bsd_termios->c_iflag = 0;
    if (linux_termios->c_iflag & LINUX_IGNBRK)
	bsd_termios->c_iflag |= IGNBRK;
    if (linux_termios->c_iflag & LINUX_BRKINT)
	bsd_termios->c_iflag |= BRKINT;
    if (linux_termios->c_iflag & LINUX_IGNPAR)
	bsd_termios->c_iflag |= IGNPAR;
    if (linux_termios->c_iflag & LINUX_PARMRK)
	bsd_termios->c_iflag |= PARMRK;
    if (linux_termios->c_iflag & LINUX_INPCK)
	bsd_termios->c_iflag |= INPCK;
    if (linux_termios->c_iflag & LINUX_ISTRIP)
	bsd_termios->c_iflag |= ISTRIP;
    if (linux_termios->c_iflag & LINUX_INLCR)
	bsd_termios->c_iflag |= INLCR;
    if (linux_termios->c_iflag & LINUX_IGNCR)
	bsd_termios->c_iflag |= IGNCR;
    if (linux_termios->c_iflag & LINUX_ICRNL)
	bsd_termios->c_iflag |= ICRNL;
    if (linux_termios->c_iflag & LINUX_IXON)
	bsd_termios->c_iflag |= IXANY;
    if (linux_termios->c_iflag & LINUX_IXON)
	bsd_termios->c_iflag |= IXON;
    if (linux_termios->c_iflag & LINUX_IXOFF)
	bsd_termios->c_iflag |= IXOFF;
    if (linux_termios->c_iflag & LINUX_IMAXBEL)
	bsd_termios->c_iflag |= IMAXBEL;

    bsd_termios->c_oflag = 0;
    if (linux_termios->c_oflag & LINUX_OPOST)
	bsd_termios->c_oflag |= OPOST;
    if (linux_termios->c_oflag & LINUX_ONLCR)
	bsd_termios->c_oflag |= ONLCR;
    if (linux_termios->c_oflag & LINUX_XTABS)
	bsd_termios->c_oflag |= OXTABS;

    bsd_termios->c_cflag = (linux_termios->c_cflag & LINUX_CSIZE) << 4;
    if (linux_termios->c_cflag & LINUX_CSTOPB)
	bsd_termios->c_cflag |= CSTOPB;
    if (linux_termios->c_cflag & LINUX_PARENB)
	bsd_termios->c_cflag |= PARENB;
    if (linux_termios->c_cflag & LINUX_PARODD)
	bsd_termios->c_cflag |= PARODD;
    if (linux_termios->c_cflag & LINUX_HUPCL)
	bsd_termios->c_cflag |= HUPCL;
    if (linux_termios->c_cflag & LINUX_CLOCAL)
	bsd_termios->c_cflag |= CLOCAL;
    if (linux_termios->c_cflag & LINUX_CRTSCTS)
	bsd_termios->c_cflag |= CRTSCTS;

    bsd_termios->c_lflag = 0;
    if (linux_termios->c_lflag & LINUX_ISIG)
	bsd_termios->c_lflag |= ISIG;
    if (linux_termios->c_lflag & LINUX_ICANON)
	bsd_termios->c_lflag |= ICANON;
    if (linux_termios->c_lflag & LINUX_ECHO)
	bsd_termios->c_lflag |= ECHO;
    if (linux_termios->c_lflag & LINUX_ECHOE)
	bsd_termios->c_lflag |= ECHOE;
    if (linux_termios->c_lflag & LINUX_ECHOK)
	bsd_termios->c_lflag |= ECHOK;
    if (linux_termios->c_lflag & LINUX_ECHONL)
	bsd_termios->c_lflag |= ECHONL;
    if (linux_termios->c_lflag & LINUX_NOFLSH)
	bsd_termios->c_lflag |= NOFLSH;
    if (linux_termios->c_lflag & LINUX_TOSTOP)
	bsd_termios->c_lflag |= TOSTOP;
    if (linux_termios->c_lflag & LINUX_ECHOCTL)
	bsd_termios->c_lflag |= ECHOCTL;
    if (linux_termios->c_lflag & LINUX_ECHOPRT)
	bsd_termios->c_lflag |= ECHOPRT;
    if (linux_termios->c_lflag & LINUX_ECHOKE)
	bsd_termios->c_lflag |= ECHOKE;
    if (linux_termios->c_lflag & LINUX_FLUSHO)
	bsd_termios->c_lflag |= FLUSHO;
    if (linux_termios->c_lflag & LINUX_PENDIN)
	bsd_termios->c_lflag |= PENDIN;
    if (linux_termios->c_lflag & IEXTEN)
	bsd_termios->c_lflag |= IEXTEN;

    for (i=0; i<NCCS; i++)
	bsd_termios->c_cc[i] = _POSIX_VDISABLE;
    bsd_termios->c_cc[VINTR] = linux_termios->c_cc[LINUX_VINTR];
    bsd_termios->c_cc[VQUIT] = linux_termios->c_cc[LINUX_VQUIT];
    bsd_termios->c_cc[VERASE] = linux_termios->c_cc[LINUX_VERASE];
    bsd_termios->c_cc[VKILL] = linux_termios->c_cc[LINUX_VKILL];
    bsd_termios->c_cc[VEOF] = linux_termios->c_cc[LINUX_VEOF];
    bsd_termios->c_cc[VEOL] = linux_termios->c_cc[LINUX_VEOL];
    bsd_termios->c_cc[VMIN] = linux_termios->c_cc[LINUX_VMIN];
    bsd_termios->c_cc[VTIME] = linux_termios->c_cc[LINUX_VTIME];
    bsd_termios->c_cc[VEOL2] = linux_termios->c_cc[LINUX_VEOL2];
    bsd_termios->c_cc[VSUSP] = linux_termios->c_cc[LINUX_VSUSP];
    bsd_termios->c_cc[VSTART] = linux_termios->c_cc[LINUX_VSTART];
    bsd_termios->c_cc[VSTOP] = linux_termios->c_cc[LINUX_VSTOP];
    bsd_termios->c_cc[VREPRINT] = linux_termios->c_cc[LINUX_VREPRINT];
    bsd_termios->c_cc[VDISCARD] = linux_termios->c_cc[LINUX_VDISCARD];
    bsd_termios->c_cc[VWERASE] = linux_termios->c_cc[LINUX_VWERASE];
    bsd_termios->c_cc[VLNEXT] = linux_termios->c_cc[LINUX_VLNEXT];

    bsd_termios->c_ispeed = bsd_termios->c_ospeed =
	linux_to_bsd_speed(linux_termios->c_cflag & LINUX_CBAUD, sptab);
#ifdef DEBUG
	printf("LINUX: BSD termios structure (output):\n");
	printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d\n",
	       bsd_termios->c_iflag, bsd_termios->c_oflag,
	       bsd_termios->c_cflag, bsd_termios->c_lflag,
	       bsd_termios->c_ispeed, bsd_termios->c_ospeed);
	printf("c_cc ");
	for (i=0; i<NCCS; i++) 
	    printf("%02x ", bsd_termios->c_cc[i]);
	printf("\n");
#endif
}


struct linux_ioctl_args {
    int fd;
    int cmd;
    int arg;
};

int
linux_ioctl(struct proc *p, struct linux_ioctl_args *args, int *retval)
{
    struct termios bsd_termios;
    struct winsize bsd_winsize;
    struct linux_termios linux_termios;
    struct linux_winsize linux_winsize;
    struct filedesc *fdp = p->p_fd;
    struct file *fp;
    int (*func)(struct file *fp, int com, caddr_t data, struct proc *p);
    int bsd_line, linux_line;
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): ioctl(%d, %04x, *)\n", 
	   p->p_pid, args->fd, args->cmd);
#endif
    if ((unsigned)args->fd >= fdp->fd_nfiles 
	|| (fp = fdp->fd_ofiles[args->fd]) == 0)
	return EBADF;

    if (!fp || (fp->f_flag & (FREAD | FWRITE)) == 0) {
	return EBADF;
    }

    func = fp->f_ops->fo_ioctl;
    switch (args->cmd & 0xffff) {
    case LINUX_TCGETS:
	if ((error = (*func)(fp, TIOCGETA, (caddr_t)&bsd_termios, p)) != 0)
	    return error;
	bsd_to_linux_termios(&bsd_termios, &linux_termios);
	return copyout((caddr_t)&linux_termios, (caddr_t)args->arg,
		       sizeof(linux_termios));

    case LINUX_TCSETS:
	linux_to_bsd_termios((struct linux_termios *)args->arg, &bsd_termios);
	return (*func)(fp, TIOCSETA, (caddr_t)&bsd_termios, p);

    case LINUX_TCSETSW:
	linux_to_bsd_termios((struct linux_termios *)args->arg, &bsd_termios);
	return (*func)(fp, TIOCSETAW, (caddr_t)&bsd_termios, p);

    case LINUX_TCSETSF:
	linux_to_bsd_termios((struct linux_termios *)args->arg, &bsd_termios);
	return (*func)(fp, TIOCSETAF, (caddr_t)&bsd_termios, p);
	    
    case LINUX_TIOCGPGRP:
	args->cmd = TIOCGPGRP;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_TIOCSPGRP:
	args->cmd = TIOCSPGRP;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_TIOCGWINSZ:
	args->cmd = TIOCGWINSZ;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_TIOCSWINSZ:
	args->cmd = TIOCSWINSZ;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_FIONREAD:
	args->cmd = FIONREAD;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_FIONBIO:
	args->cmd = FIONBIO;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_FIOASYNC:
	args->cmd = FIOASYNC;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_FIONCLEX:
	args->cmd = FIONCLEX;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_FIOCLEX:
	args->cmd = FIOCLEX;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_TIOCEXCL:
	args->cmd = TIOCEXCL;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_TIOCNXCL:
	args->cmd = TIOCNXCL;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_TIOCCONS:
	args->cmd = TIOCCONS;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_TIOCNOTTY:
	args->cmd = TIOCNOTTY;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_TIOCSETD:
	switch (args->arg) {
	case LINUX_N_TTY:
	    bsd_line = TTYDISC;
	    return (*func)(fp, TIOCSETD, (caddr_t)&bsd_line, p);
	case LINUX_N_SLIP:
	    bsd_line = SLIPDISC;
	    return (*func)(fp, TIOCSETD, (caddr_t)&bsd_line, p);
	case LINUX_N_PPP:
	    bsd_line = PPPDISC;
	    return (*func)(fp, TIOCSETD, (caddr_t)&bsd_line, p);
	default:
	    return EINVAL;
	}

    case LINUX_TIOCGETD:
	bsd_line = TTYDISC;
	if (error =(*func)(fp, TIOCSETD, (caddr_t)&bsd_line, p))
	    return error;
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
	    return EINVAL;
	}
	return copyout(&linux_line, (caddr_t)args->arg, 
		       sizeof(int));

    case LINUX_SNDCTL_DSP_RESET:
	args->cmd = SNDCTL_DSP_RESET;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_SYNC:
	args->cmd = SNDCTL_DSP_SYNC;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_SPEED:
	args->cmd = SNDCTL_DSP_SPEED;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_STEREO:
	args->cmd = SNDCTL_DSP_STEREO;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_GETBLKSIZE:
      /* LINUX_SNDCTL_DSP_SETBLKSIZE */
	args->cmd = SNDCTL_DSP_GETBLKSIZE;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_SETFMT:
	args->cmd = SNDCTL_DSP_SETFMT;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SOUND_PCM_WRITE_CHANNELS:
	args->cmd = SOUND_PCM_WRITE_CHANNELS;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SOUND_PCM_WRITE_FILTER:
	args->cmd = SOUND_PCM_WRITE_FILTER;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_POST:
	args->cmd = SNDCTL_DSP_POST;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_SUBDIVIDE:
	args->cmd = SNDCTL_DSP_SUBDIVIDE;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_SETFRAGMENT:
	args->cmd = SNDCTL_DSP_SETFRAGMENT;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_GETFMTS:
	args->cmd = SNDCTL_DSP_GETFMTS;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_GETOSPACE:
	args->cmd = SNDCTL_DSP_GETOSPACE;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_GETISPACE:
	args->cmd = SNDCTL_DSP_GETISPACE;
	return ioctl(p, (struct ioctl_args *)args, retval);

    case LINUX_SNDCTL_DSP_NONBLOCK:
	args->cmd = SNDCTL_DSP_NONBLOCK;
	return ioctl(p, (struct ioctl_args *)args, retval);
    }
    uprintf("LINUX: 'ioctl' fd=%d, typ=0x%x(%c), num=0x%x not implemented\n",
	    args->fd, (args->cmd&0xffff00)>>8,
	    (args->cmd&0xffff00)>>8, args->cmd&0xff);
    return EINVAL;
}
