/*-
 * Copyright (c) 2003 Poul-Henning Kamp.  All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>

static	d_open_t	cttyopen;

#define	CDEV_MAJOR	1

static struct cdevsw ctty_cdevsw = {
	/* open */	cttyopen,
	/* close */	nullclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"ctty",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY,
};

static dev_t ctty;

static	int
cttyopen(dev_t dev, int flag, int mode, struct thread *td)
{

	return (ENXIO);
}

static void
ctty_clone(void *arg, char *name, int namelen, dev_t *dev)
{

	if (*dev != NODEV)
		return;
	if (strcmp(name, "tty"))
		return;
	if (!(curthread->td_proc->p_flag & P_CONTROLT))
		*dev = ctty;
	else if (curthread->td_proc->p_session->s_ttyvp == NULL)
		*dev = ctty;
	else
		*dev = curthread->td_proc->p_session->s_ttyvp->v_rdev;
}

static void
ctty_drvinit(void *unused)
{

	EVENTHANDLER_REGISTER(dev_clone, ctty_clone, 0, 1000);
	ctty = make_dev(&ctty_cdevsw, 0, 0, 0, 0666, "ctty");
}

SYSINIT(cttydev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ctty_drvinit,NULL)
