/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	from: @(#)cons.c	7.2 (Berkeley) 5/9/91
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/uio.h>

#include <machine/cpu.h>

static	d_open_t	cnopen;
static	d_close_t	cnclose;
static	d_read_t	cnread;
static	d_write_t	cnwrite;
static	d_ioctl_t	cnioctl;
static	d_poll_t	cnpoll;
static	d_kqfilter_t	cnkqfilter;

#define	CDEV_MAJOR	0
static struct cdevsw cn_cdevsw = {
	/* open */	cnopen,
	/* close */	cnclose,
	/* read */	cnread,
	/* write */	cnwrite,
	/* ioctl */	cnioctl,
	/* poll */	cnpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"console",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY | D_KQFILTER,
	/* kqfilter */	cnkqfilter,
};

static dev_t	cn_dev_t; 	/* seems to be never really used */
static udev_t	cn_udev_t;
SYSCTL_OPAQUE(_machdep, CPU_CONSDEV, consdev, CTLFLAG_RD,
	&cn_udev_t, sizeof cn_udev_t, "T,dev_t", "");

static int cn_mute;

int	cons_unavail = 0;	/* XXX:
				 * physical console not available for
				 * input (i.e., it is in graphics mode)
				 */

static u_char cn_is_open;		/* nonzero if logical console is open */
static int openmode, openflag;		/* how /dev/console was openned */
static dev_t cn_devfsdev;		/* represents the device private info */
static u_char cn_phys_is_open;		/* nonzero if physical device is open */
static d_close_t *cn_phys_close;	/* physical device close function */
static d_open_t *cn_phys_open;		/* physical device open function */
       struct consdev *cn_tab;		/* physical console device info */

CONS_DRIVER(cons, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
SET_DECLARE(cons_set, struct consdev);

void
cninit()
{
	struct consdev *best_cp, *cp, **list;

	/*
	 * Find the first console with the highest priority.
	 */
	best_cp = NULL;
	SET_FOREACH(list, cons_set) {
		cp = *list;
		if (cp->cn_probe == NULL)
			continue;
		(*cp->cn_probe)(cp);
		if (cp->cn_pri > CN_DEAD &&
		    (best_cp == NULL || cp->cn_pri > best_cp->cn_pri))
			best_cp = cp;
	}

	/*
	 * Check if we should mute the console (for security reasons perhaps)
	 * It can be changes dynamically using sysctl kern.consmute
	 * once we are up and going.
	 * 
	 */
        cn_mute = ((boothowto & (RB_MUTE
			|RB_SINGLE
			|RB_VERBOSE
			|RB_ASKNAME
			|RB_CONFIG)) == RB_MUTE);
	
	/*
	 * If no console, give up.
	 */
	if (best_cp == NULL) {
		if (cn_tab != NULL && cn_tab->cn_term != NULL)
			(*cn_tab->cn_term)(cn_tab);
		cn_tab = best_cp;
		return;
	}

	/*
	 * Initialize console, then attach to it.  This ordering allows
	 * debugging using the previous console, if any.
	 */
	(*best_cp->cn_init)(best_cp);
	if (cn_tab != NULL && cn_tab != best_cp) {
		/* Turn off the previous console.  */
		if (cn_tab->cn_term != NULL)
			(*cn_tab->cn_term)(cn_tab);
	}
	cn_tab = best_cp;
}

void
cninit_finish()
{
	struct cdevsw *cdp;

	if ((cn_tab == NULL) || cn_mute)
		return;

	/*
	 * Hook the open and close functions.
	 */
	cdp = devsw(cn_tab->cn_dev);
	if (cdp != NULL) {
		cn_phys_close = cdp->d_close;
		cdp->d_close = cnclose;
		cn_phys_open = cdp->d_open;
		cdp->d_open = cnopen;
	}
	cn_dev_t = cn_tab->cn_dev;
	cn_udev_t = dev2udev(cn_dev_t);
}

static void
cnuninit(void)
{
	struct cdevsw *cdp;

	if (cn_tab == NULL)
		return;

	/*
	 * Unhook the open and close functions.
	 */
	cdp = devsw(cn_tab->cn_dev);
	if (cdp != NULL) {
		cdp->d_close = cn_phys_close;
		cdp->d_open = cn_phys_open;
	}
	cn_phys_close = NULL;
	cn_phys_open = NULL;
	cn_dev_t = NODEV;
	cn_udev_t = NOUDEV;
}

/*
 * User has changed the state of the console muting.
 * This may require us to open or close the device in question.
 */
static int
sysctl_kern_consmute(SYSCTL_HANDLER_ARGS)
{
	int error;
	int ocn_mute;

	ocn_mute = cn_mute;
	error = sysctl_handle_int(oidp, &cn_mute, 0, req);
	if((error == 0) && (cn_tab != NULL) && (req->newptr != NULL)) {
		if(ocn_mute && !cn_mute) {
			/*
			 * going from muted to unmuted.. open the physical dev 
			 * if the console has been openned
			 */
			cninit_finish();
			if(cn_is_open)
				/* XXX curproc is not what we want really */
				error = cnopen(cn_dev_t, openflag,
					openmode, curproc);
			/* if it failed, back it out */
			if ( error != 0) cnuninit();
		} else if (!ocn_mute && cn_mute) {
			/*
			 * going from unmuted to muted.. close the physical dev 
			 * if it's only open via /dev/console
			 */
			if(cn_is_open)
				error = cnclose(cn_dev_t, openflag,
					openmode, curproc);
			if ( error == 0) cnuninit();
		}
		if (error != 0) {
			/* 
	 		 * back out the change if there was an error
			 */
			cn_mute = ocn_mute;
		}
	}
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, consmute, CTLTYPE_INT|CTLFLAG_RW,
	0, sizeof cn_mute, sysctl_kern_consmute, "I", "");

static int
cnopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	dev_t cndev, physdev;
	int retval = 0;

	if (cn_tab == NULL || cn_phys_open == NULL)
		return (0);
	cndev = cn_tab->cn_dev;
	physdev = (major(dev) == major(cndev) ? dev : cndev);
	/*
	 * If mute is active, then non console opens don't get here
	 * so we don't need to check for that. They 
	 * bypass this and go straight to the device.
	 */
	if(!cn_mute)
		retval = (*cn_phys_open)(physdev, flag, mode, p);
	if (retval == 0) {
		/* 
		 * check if we openned it via /dev/console or 
		 * via the physical entry (e.g. /dev/sio0).
		 */
		if (dev == cndev)
			cn_phys_is_open = 1;
		else if (physdev == cndev) {
			openmode = mode;
			openflag = flag;
			cn_is_open = 1;
		}
		dev->si_tty = physdev->si_tty;
	}
	return (retval);
}

static int
cnclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	dev_t cndev;
	struct tty *cn_tp;

	if (cn_tab == NULL || cn_phys_open == NULL)
		return (0);
	cndev = cn_tab->cn_dev;
	cn_tp = cndev->si_tty;
	/*
	 * act appropriatly depending on whether it's /dev/console
	 * or the pysical device (e.g. /dev/sio) that's being closed.
	 * in either case, don't actually close the device unless
	 * both are closed.
	 */
	if (dev == cndev) {
		/* the physical device is about to be closed */
		cn_phys_is_open = 0;
		if (cn_is_open) {
			if (cn_tp) {
				/* perform a ttyhalfclose() */
				/* reset session and proc group */
				cn_tp->t_pgrp = NULL;
				cn_tp->t_session = NULL;
			}
			return (0);
		}
	} else if (major(dev) != major(cndev)) {
		/* the logical console is about to be closed */
		cn_is_open = 0;
		if (cn_phys_is_open)
			return (0);
		dev = cndev;
	}
	if(cn_phys_close)
		return ((*cn_phys_close)(dev, flag, mode, p));
	return (0);
}

static int
cnread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{

	if (cn_tab == NULL || cn_phys_open == NULL)
		return (0);
	dev = cn_tab->cn_dev;
	return ((*devsw(dev)->d_read)(dev, uio, flag));
}

static int
cnwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{

	if (cn_tab == NULL || cn_phys_open == NULL) {
		uio->uio_resid = 0; /* dump the data */
		return (0);
	}
	if (constty)
		dev = constty->t_dev;
	else
		dev = cn_tab->cn_dev;
	log_console(uio);
	return ((*devsw(dev)->d_write)(dev, uio, flag));
}

static int
cnioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error;

	if (cn_tab == NULL || cn_phys_open == NULL)
		return (0);
	/*
	 * Superuser can always use this to wrest control of console
	 * output from the "virtual" console.
	 */
	if (cmd == TIOCCONS && constty) {
		error = suser(p);
		if (error)
			return (error);
		constty = NULL;
		return (0);
	}
	dev = cn_tab->cn_dev;
	return ((*devsw(dev)->d_ioctl)(dev, cmd, data, flag, p));
}

static int
cnpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	if ((cn_tab == NULL) || cn_mute)
		return (1);

	dev = cn_tab->cn_dev;

	return ((*devsw(dev)->d_poll)(dev, events, p));
}

static int
cnkqfilter(dev, kn)
	dev_t dev;
	struct knote *kn;
{
	if ((cn_tab == NULL) || cn_mute)
		return (1);

	dev = cn_tab->cn_dev;
	if (devsw(dev)->d_flags & D_KQFILTER)
		return ((*devsw(dev)->d_kqfilter)(dev, kn));
	return (1);
}

int
cngetc()
{
	int c;
	if ((cn_tab == NULL) || cn_mute)
		return (-1);
	c = (*cn_tab->cn_getc)(cn_tab->cn_dev);
	if (c == '\r') c = '\n'; /* console input is always ICRNL */
	return (c);
}

int
cncheckc()
{
	if ((cn_tab == NULL) || cn_mute)
		return (-1);
	return ((*cn_tab->cn_checkc)(cn_tab->cn_dev));
}

void
cnputc(c)
	register int c;
{
	if ((cn_tab == NULL) || cn_mute)
		return;
	if (c) {
		if (c == '\n')
			(*cn_tab->cn_putc)(cn_tab->cn_dev, '\r');
		(*cn_tab->cn_putc)(cn_tab->cn_dev, c);
	}
}

void
cndbctl(on)
	int on;
{
	static int refcount;

	if (cn_tab == NULL)
		return;
	if (!on)
		refcount--;
	if (refcount == 0 && cn_tab->cn_dbctl != NULL)
		(*cn_tab->cn_dbctl)(cn_tab->cn_dev, on);
	if (on)
		refcount++;
}

static void
cn_drvinit(void *unused)
{

	cn_devfsdev = make_dev(&cn_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "console");
}

SYSINIT(cndev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,cn_drvinit,NULL)
