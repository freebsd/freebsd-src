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
 *	$Id: cons.c,v 1.34 1995/09/10 21:34:49 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/tty.h>

#include <machine/cons.h>
#include <machine/stdarg.h>

#ifdef JREMOD
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#define CDEV_MAJOR 0
#endif /*JREMOD*/

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

struct	tty *constty = 0;	/* virtual console output device */
struct	tty *cn_tty;		/* XXX: console tty struct for tprintf */
int	cons_unavail = 0;	/* XXX:
				 * physical console not available for
				 * input (i.e., it is in graphics mode)
				 */

static u_char cn_is_open;	/* nonzero if logical console is open */
static u_char cn_phys_is_open;	/* nonzero if physical console is open */
static d_close_t *cn_phys_close;	/* physical device close function */
static d_open_t *cn_phys_open;	/* physical device open function */
static struct consdev *cn_tab;	/* physical console device info */
static struct tty *cn_tp;	/* physical console tty struct */

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

	if (cn_tab == NULL)
		return;

	/*
	 * Hook the open and close functions.
	 */
	cdp = &cdevsw[major(cn_tab->cn_dev)];
	cn_phys_close = cdp->d_close;
	cdp->d_close = cnclose;
	cn_phys_open = cdp->d_open;
	cdp->d_open = cnopen;
	cn_tp = (*cdp->d_devtotty)(cn_tab->cn_dev);
	/*
	 * XXX there are too many tty pointers.  cn_tty is only used for
	 * sysctl(CPU_CONSDEV) (not for tprintf like the above comment
	 * says).  cn_tp in struct consdev hasn't been initialized
	 * (except statically to NULL) or used (except to initialize
	 * cn_tty to the wrong value) for a year or two.
	 */
	cn_tty = cn_tp;
}

int
cnopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	dev_t cndev, physdev;
	int retval;

	if (cn_tab == NULL)
		return (0);
	cndev = cn_tab->cn_dev;
	physdev = (major(dev) == major(cndev) ? dev : cndev);
	retval = (*cn_phys_open)(physdev, flag, mode, p);
	if (retval == 0) {
		if (dev == cndev)
			cn_phys_is_open = 1;
		else if (physdev == cndev)
			cn_is_open = 1;
	}
	return (retval);
}

int
cnclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	dev_t cndev;

	if (cn_tab == NULL)
		return (0);
	cndev = cn_tab->cn_dev;
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
	return ((*cn_phys_close)(dev, flag, mode, p));
}

int
cnread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	if (cn_tab == NULL)
		return (0);
	dev = cn_tab->cn_dev;
	return ((*cdevsw[major(dev)].d_read)(dev, uio, flag));
}

int
cnwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	if (cn_tab == NULL)
		return (0);
	if (constty)
		dev = constty->t_dev;
	else
		dev = cn_tab->cn_dev;
	return ((*cdevsw[major(dev)].d_write)(dev, uio, flag));
}

int
cnioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error;

	if (cn_tab == NULL)
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
	return ((*cdevsw[major(dev)].d_ioctl)(dev, cmd, data, flag, p));
}

int
cnselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	if (cn_tab == NULL)
		return (1);

	dev = cn_tab->cn_dev;

	return ((*cdevsw[major(dev)].d_select)(dev, rw, p));
}

int
cngetc()
{
	int c;
	if (cn_tab == NULL)
		return (0);
	c = (*cn_tab->cn_getc)(cn_tab->cn_dev);
	if (c == '\r') c = '\n'; /* console input is always ICRNL */
	return (c);
}

int
cncheckc()
{
	if (cn_tab == NULL)
		return (0);
	return ((*cn_tab->cn_checkc)(cn_tab->cn_dev));
}

void
cnputc(c)
	register int c;
{
	if (cn_tab == NULL)
		return;
	if (c) {
		if (c == '\n')
			(*cn_tab->cn_putc)(cn_tab->cn_dev, '\r');
		(*cn_tab->cn_putc)(cn_tab->cn_dev, c);
	}
}

int
pg(const char *p, ...) {
  va_list args;
  va_start(args, p);
  printf("%r\n>", p, args);
  return(cngetc());
}


#ifdef JREMOD
struct cdevsw cn_cdevsw = 
	{ cnopen,	cnclose,	cnread,		cnwrite,	/*0*/
	  cnioctl,	nullstop,	nullreset,	nodevtotty,/* console */
	  cnselect,	nommap,		NULL };

static cn_devsw_installed = 0;

static void 	cn_drvinit(void *unused)
{
  	void * x;
	dev_t dev;

	if( ! cn_devsw_installed ) {
		dev = makedev(CDEV_MAJOR,0);
		cdevsw_add(&dev,&cn_cdevsw,NULL);
		cn_devsw_installed = 1;
#ifdef DEVFS
	/*                path,name,major,minor,type,uid,gid,perm */
	x=devfs_add_devsw("/","console",major(dev),0,DV_CHR,0,0,0640);
#endif
}

SYSINIT(cndev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,cn_drvinit,NULL)

#endif /* JREMOD */

