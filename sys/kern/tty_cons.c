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
 *	$Id: cons.c,v 1.57 1998/03/28 10:32:56 bde Exp $
 */

#include "opt_devfs.h"

#include <sys/param.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/uio.h>

#include <machine/cpu.h>
#include <machine/cons.h>

/* XXX this should be config(8)ed. */
#include "sc.h"
#include "vt.h"
#include "sio.h"
static struct consdev constab[] = {
#if NSC > 0
	{ sccnprobe,	sccninit,	sccngetc,	sccncheckc,	sccnputc },
#endif
#if NVT > 0
	{ pccnprobe,	pccninit,	pccngetc,	pccncheckc,	pccnputc },
#endif
#if NSIO > 0
	{ siocnprobe,	siocninit,	siocngetc,	siocncheckc,	siocnputc },
#endif
	{ 0 },
};

static	d_open_t	cnopen;
static	d_close_t	cnclose;
static	d_read_t	cnread;
static	d_write_t	cnwrite;
static	d_ioctl_t	cnioctl;
static	d_poll_t	cnpoll;

#define CDEV_MAJOR 0
static struct cdevsw cn_cdevsw = 
	{ cnopen,	cnclose,	cnread,		cnwrite,	/*0*/
	  cnioctl,	nullstop,	nullreset,	nodevtotty,/* console */
	  cnpoll,	nommap,		NULL,	"console",	NULL,	-1 };

static dev_t	cn_dev_t; 	/* seems to be never really used */
SYSCTL_OPAQUE(_machdep, CPU_CONSDEV, consdev, CTLTYPE_OPAQUE|CTLFLAG_RD,
	&cn_dev_t, sizeof cn_dev_t, "T,dev_t", "");

static int cn_mute;

int	cons_unavail = 0;	/* XXX:
				 * physical console not available for
				 * input (i.e., it is in graphics mode)
				 */

static u_char cn_is_open;		/* nonzero if logical console is open */
static int openmode, openflag;		/* how /dev/console was openned */
static u_char cn_phys_is_open;		/* nonzero if physical device is open */
static d_close_t *cn_phys_close;	/* physical device close function */
static d_open_t *cn_phys_open;		/* physical device open function */
static struct consdev *cn_tab;		/* physical console device info */
static struct tty *cn_tp;		/* physical console tty struct */
#ifdef DEVFS
static void *cn_devfs_token;		/* represents the devfs entry */
#endif /* DEVFS */

void
cninit()
{
	struct consdev *best_cp, *cp;

	/*
	 * Find the first console with the highest priority.
	 */
	best_cp = NULL;
	for (cp = constab; cp->cn_probe; cp++) {
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
		cn_tab = best_cp;
		return;
	}

	/*
	 * Initialize console, then attach to it.  This ordering allows
	 * debugging using the previous console, if any.
	 * XXX if there was a previous console, then its driver should
	 * be informed when we forget about it.
	 */
	(*best_cp->cn_init)(best_cp);
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
	cdp = cdevsw[major(cn_tab->cn_dev)];
	cn_phys_close = cdp->d_close;
	cdp->d_close = cnclose;
	cn_phys_open = cdp->d_open;
	cdp->d_open = cnopen;
	cn_tp = (*cdp->d_devtotty)(cn_tab->cn_dev);
	cn_dev_t = cn_tp->t_dev;
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
	cdp = cdevsw[major(cn_tab->cn_dev)];
	cdp->d_close = cn_phys_close;
	cn_phys_close = NULL;
	cdp->d_open = cn_phys_open;
	cn_phys_open = NULL;
	cn_tp = NULL;
	cn_dev_t = 0;
}

/*
 * User has changed the state of the console muting.
 * This may require us to open or close the device in question.
 */
static int
sysctl_kern_consmute SYSCTL_HANDLER_ARGS
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

	if (cn_tab == NULL)
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

	if (cn_tab == NULL)
		return (0);
	cndev = cn_tab->cn_dev;
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
	if ((cn_tab == NULL) || cn_mute)
		return (0);
	dev = cn_tab->cn_dev;
	return ((*cdevsw[major(dev)]->d_read)(dev, uio, flag));
}

static int
cnwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	if ((cn_tab == NULL) || cn_mute) {
		uio->uio_resid = 0; /* dump the data */
		return (0);
	}
	if (constty)
		dev = constty->t_dev;
	else
		dev = cn_tab->cn_dev;
	return ((*cdevsw[major(dev)]->d_write)(dev, uio, flag));
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

	if ((cn_tab == NULL) || cn_mute)
		return (0);
	/*
	 * Superuser can always use this to wrest control of console
	 * output from the "virtual" console.
	 */
	if (cmd == TIOCCONS && constty) {
		error = suser(p->p_ucred, (u_short *) NULL);
		if (error)
			return (error);
		constty = NULL;
		return (0);
	}
	dev = cn_tab->cn_dev;
	return ((*cdevsw[major(dev)]->d_ioctl)(dev, cmd, data, flag, p));
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

	return ((*cdevsw[major(dev)]->d_poll)(dev, events, p));
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

static cn_devsw_installed = 0;

static void
cn_drvinit(void *unused)
{
	dev_t dev;

	if( ! cn_devsw_installed ) {
		dev = makedev(CDEV_MAJOR,0);
		cdevsw_add(&dev,&cn_cdevsw,NULL);
		cn_devsw_installed = 1;
#ifdef DEVFS
		cn_devfs_token = devfs_add_devswf(&cn_cdevsw, 0, DV_CHR,
						  UID_ROOT, GID_WHEEL, 0600,
						  "console");
#endif
	}
}

SYSINIT(cndev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,cn_drvinit,NULL)


