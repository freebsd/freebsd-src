/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_syscons.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/consio.h>
#include <sys/mouse.h>

#include <dev/syscons/syscons.h>

#ifndef SC_NO_SYSMOUSE

#define	CDEV_MAJOR	12		/* major number, shared with syscons */
#define SC_MOUSE 	128		/* minor number */

static d_open_t		smopen;
static d_close_t	smclose;
static d_ioctl_t	smioctl;

static struct cdevsw sm_cdevsw = {
	/* open */	smopen,
	/* close */	smclose,
	/* read */	ttyread,
	/* write */	nowrite,
	/* ioctl */	smioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"sysmouse",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY,
};

/* local variables */
static struct tty	*sysmouse_tty;
static int		mouse_level;	/* sysmouse protocol level */
static mousestatus_t	mouse_status;

static void		smstart(struct tty *tp);
static int		smparam(struct tty *tp, struct termios *t);

static int
smopen(dev_t dev, int flag, int mode, struct thread *td)
{
	struct tty *tp;

	DPRINTF(5, ("smopen: dev:%d,%d, vty:%d\n",
		major(dev), minor(dev), SC_VTY(dev)));

#if 0
	if (SC_VTY(dev) != SC_MOUSE)
		return ENXIO;
#endif

	tp = dev->si_tty = ttymalloc(dev->si_tty);
	if (!(tp->t_state & TS_ISOPEN)) {
		sysmouse_tty = tp;
		tp->t_oproc = smstart;
		tp->t_param = smparam;
		tp->t_stop = nottystop;
		tp->t_dev = dev;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		smparam(tp, &tp->t_termios);
		(*linesw[tp->t_line].l_modem)(tp, 1);
	} else if (tp->t_state & TS_XCLUDE && suser(td)) {
		return EBUSY;
	}

	return (*linesw[tp->t_line].l_open)(dev, tp);
}

static int
smclose(dev_t dev, int flag, int mode, struct thread *td)
{
	struct tty *tp;
	int s;

	tp = dev->si_tty;
	s = spltty();
	mouse_level = 0;
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	splx(s);

	return 0;
}

static void
smstart(struct tty *tp)
{
	struct clist *rbp;
	u_char buf[PCBURST];
	int s;

	s = spltty();
	if (!(tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))) {
		tp->t_state |= TS_BUSY;
		rbp = &tp->t_outq;
		while (rbp->c_cc)
			q_to_b(rbp, buf, PCBURST);
		tp->t_state &= ~TS_BUSY;
		ttwwakeup(tp);
	}
	splx(s);
}

static int
smparam(struct tty *tp, struct termios *t)
{
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return 0;
}

static int
smioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct tty *tp;
	mousehw_t *hw;
	mousemode_t *mode;
	int error;
	int s;

	tp = dev->si_tty;
	switch (cmd) {

	case MOUSE_GETHWINFO:	/* get device information */
		hw = (mousehw_t *)data;
		hw->buttons = 10;		/* XXX unknown */
		hw->iftype = MOUSE_IF_SYSMOUSE;
		hw->type = MOUSE_MOUSE;
		hw->model = MOUSE_MODEL_GENERIC;
		hw->hwid = 0;
		return 0;

	case MOUSE_GETMODE:	/* get protocol/mode */
		mode = (mousemode_t *)data;
		mode->level = mouse_level;
		switch (mode->level) {
		case 0: /* emulate MouseSystems protocol */
			mode->protocol = MOUSE_PROTO_MSC;
			mode->rate = -1;		/* unknown */
			mode->resolution = -1;	/* unknown */
			mode->accelfactor = 0;	/* disabled */
			mode->packetsize = MOUSE_MSC_PACKETSIZE;
			mode->syncmask[0] = MOUSE_MSC_SYNCMASK;
			mode->syncmask[1] = MOUSE_MSC_SYNC;
			break;

		case 1: /* sysmouse protocol */
			mode->protocol = MOUSE_PROTO_SYSMOUSE;
			mode->rate = -1;
			mode->resolution = -1;
			mode->accelfactor = 0;
			mode->packetsize = MOUSE_SYS_PACKETSIZE;
			mode->syncmask[0] = MOUSE_SYS_SYNCMASK;
			mode->syncmask[1] = MOUSE_SYS_SYNC;
			break;
		}
		return 0;

	case MOUSE_SETMODE:	/* set protocol/mode */
		mode = (mousemode_t *)data;
		if (mode->level == -1)
			; 	/* don't change the current setting */
		else if ((mode->level < 0) || (mode->level > 1))
			return EINVAL;
		else
			mouse_level = mode->level;
		return 0;

	case MOUSE_GETLEVEL:	/* get operation level */
		*(int *)data = mouse_level;
		return 0;

	case MOUSE_SETLEVEL:	/* set operation level */
		if ((*(int *)data  < 0) || (*(int *)data > 1))
			return EINVAL;
		mouse_level = *(int *)data;
		return 0;

	case MOUSE_GETSTATUS:	/* get accumulated mouse events */
		s = spltty();
		*(mousestatus_t *)data = mouse_status;
		mouse_status.flags = 0;
		mouse_status.obutton = mouse_status.button;
		mouse_status.dx = 0;
		mouse_status.dy = 0;
		mouse_status.dz = 0;
		splx(s);
		return 0;

#if notyet
	case MOUSE_GETVARS:	/* get internal mouse variables */
	case MOUSE_SETVARS:	/* set internal mouse variables */
		return ENODEV;
#endif

	case MOUSE_READSTATE:	/* read status from the device */
	case MOUSE_READDATA:	/* read data from the device */
		return ENODEV;
	}

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, td);
	if (error != ENOIOCTL)
		return error;
	error = ttioctl(tp, cmd, data, flag);
	if (error != ENOIOCTL)
		return error;
	return ENOTTY;
}

static void
sm_attach_mouse(void *unused)
{
	dev_t dev;

	dev = make_dev(&sm_cdevsw, SC_MOUSE, UID_ROOT, GID_WHEEL, 0600,
		       "sysmouse");
	/* sysmouse doesn't have scr_stat */
}

SYSINIT(sysmouse, SI_SUB_DRIVERS, SI_ORDER_MIDDLE + CDEV_MAJOR,
	sm_attach_mouse, NULL)

int
sysmouse_event(mouse_info_t *info)
{
	/* MOUSE_BUTTON?DOWN -> MOUSE_MSC_BUTTON?UP */
	static int butmap[8] = {
	    MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON2UP | MOUSE_MSC_BUTTON3UP,
	    MOUSE_MSC_BUTTON2UP | MOUSE_MSC_BUTTON3UP,
	    MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON3UP,
	    MOUSE_MSC_BUTTON3UP,
	    MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON2UP,
	    MOUSE_MSC_BUTTON2UP,
	    MOUSE_MSC_BUTTON1UP,
	    0,
	};
	u_char buf[8];
	int x, y, z;
	int i;

	switch (info->operation) {
	case MOUSE_ACTION:
        	mouse_status.button = info->u.data.buttons;
		/* FALL THROUGH */
	case MOUSE_MOTION_EVENT:
		x = info->u.data.x;
		y = info->u.data.y;
		z = info->u.data.z;
		break;
	case MOUSE_BUTTON_EVENT:
		x = y = z = 0;
		if (info->u.event.value > 0)
			mouse_status.button |= info->u.event.id;
		else
			mouse_status.button &= ~info->u.event.id;
		break;
	default:
		return 0;
	}

	mouse_status.dx += x;
	mouse_status.dy += y;
	mouse_status.dz += z;
	mouse_status.flags |= ((x || y || z) ? MOUSE_POSCHANGED : 0)
			      | (mouse_status.obutton ^ mouse_status.button);
	if (mouse_status.flags == 0)
		return 0;

	if ((sysmouse_tty == NULL) || !(sysmouse_tty->t_state & TS_ISOPEN))
		return mouse_status.flags;

	/* the first five bytes are compatible with MouseSystems' */
	buf[0] = MOUSE_MSC_SYNC
		 | butmap[mouse_status.button & MOUSE_STDBUTTONS];
	x = imax(imin(x, 255), -256);
	buf[1] = x >> 1;
	buf[3] = x - buf[1];
	y = -imax(imin(y, 255), -256);
	buf[2] = y >> 1;
	buf[4] = y - buf[2];
	for (i = 0; i < MOUSE_MSC_PACKETSIZE; ++i)
		(*linesw[sysmouse_tty->t_line].l_rint)(buf[i], sysmouse_tty);
	if (mouse_level >= 1) {
		/* extended part */
        	z = imax(imin(z, 127), -128);
        	buf[5] = (z >> 1) & 0x7f;
        	buf[6] = (z - (z >> 1)) & 0x7f;
        	/* buttons 4-10 */
        	buf[7] = (~mouse_status.button >> 3) & 0x7f;
        	for (i = MOUSE_MSC_PACKETSIZE; i < MOUSE_SYS_PACKETSIZE; ++i)
			(*linesw[sysmouse_tty->t_line].l_rint)(buf[i],
							       sysmouse_tty);
	}

	return mouse_status.flags;
}

#endif /* !SC_NO_SYSMOUSE */
