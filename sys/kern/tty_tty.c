/*-
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)tty_tty.c	8.2 (Berkeley) 9/23/93
 * $FreeBSD$
 */

/*
 * Indirect driver for controlling tty.
 */

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/ttycom.h>
#include <sys/vnode.h>

static	d_open_t	cttyopen;
static	d_read_t	cttyread;
static	d_write_t	cttywrite;
static	d_ioctl_t	cttyioctl;
static	d_poll_t	cttypoll;

#define	CDEV_MAJOR	1

static struct cdevsw ctty_cdevsw = {
	/* open */	cttyopen,
	/* close */	nullclose,
	/* read */	cttyread,
	/* write */	cttywrite,
	/* ioctl */	cttyioctl,
	/* poll */	cttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"ctty",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY,
};

#define cttyvp(td) ((td)->td_proc->p_flag & P_CONTROLT ? (td)->td_proc->p_session->s_ttyvp : NULL)

/*ARGSUSED*/
static	int
cttyopen(dev, flag, mode, td)
	dev_t dev;
	int flag, mode;
	struct thread *td;
{
	struct vnode *ttyvp;
	int error;

	PROC_LOCK(td->td_proc);
	SESS_LOCK(td->td_proc->p_session);
	ttyvp = cttyvp(td);
	SESS_UNLOCK(td->td_proc->p_session);
	PROC_UNLOCK(td->td_proc);

	if (ttyvp == NULL)
		return (ENXIO);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY, td);
#ifdef MAC
	error = mac_check_vnode_open(td->td_ucred, ttyvp, flag);
	if (error) {
		VOP_UNLOCK(ttyvp, 0, td);
		return (error);
	}
#endif
	/* XXX: Shouldn't this cred be td->td_ucred not NOCRED? */
	error = VOP_OPEN(ttyvp, flag, NOCRED, td);
	VOP_UNLOCK(ttyvp, 0, td);
	return (error);
}

/*ARGSUSED*/
static	int
cttyread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct thread *td = uio->uio_td;
	register struct vnode *ttyvp;
	int error;

	PROC_LOCK(td->td_proc);
	SESS_LOCK(td->td_proc->p_session);
	ttyvp = cttyvp(td);
	SESS_UNLOCK(td->td_proc->p_session);
	PROC_UNLOCK(td->td_proc);

	if (ttyvp == NULL)
		return (EIO);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY, td);
#ifdef MAC
	error = mac_check_vnode_read(td->td_ucred, NOCRED, ttyvp);
	if (error == 0)
#endif
		/* XXX: Shouldn't this cred be td->td_ucred not NOCRED? */
		error = VOP_READ(ttyvp, uio, flag, NOCRED);
	VOP_UNLOCK(ttyvp, 0, td);
	return (error);
}

/*ARGSUSED*/
static	int
cttywrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct thread *td = uio->uio_td;
	struct vnode *ttyvp;
	struct mount *mp;
	int error;

	PROC_LOCK(td->td_proc);
	SESS_LOCK(td->td_proc->p_session);
	ttyvp = cttyvp(td);
	SESS_UNLOCK(td->td_proc->p_session);
	PROC_UNLOCK(td->td_proc);

	if (ttyvp == NULL)
		return (EIO);
	mp = NULL;
	if (ttyvp->v_type != VCHR &&
	    (error = vn_start_write(ttyvp, &mp, V_WAIT | PCATCH)) != 0)
		return (error);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY, td);
#ifdef MAC
	error = mac_check_vnode_write(td->td_ucred, NOCRED, ttyvp);
	if (error == 0)
#endif
		/* XXX: shouldn't this cred be td->td_ucred not NOCRED? */
		error = VOP_WRITE(ttyvp, uio, flag, NOCRED);
	VOP_UNLOCK(ttyvp, 0, td);
	vn_finished_write(mp);
	return (error);
}

/*ARGSUSED*/
static	int
cttyioctl(dev, cmd, addr, flag, td)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct thread *td;
{
	struct vnode *ttyvp;
	int error;

	PROC_LOCK(td->td_proc);
	SESS_LOCK(td->td_proc->p_session);
	ttyvp = cttyvp(td);
	SESS_UNLOCK(td->td_proc->p_session);
	PROC_UNLOCK(td->td_proc);

	if (ttyvp == NULL)
		return (EIO);
	if (cmd == TIOCSCTTY)  /* don't allow controlling tty to be set    */
		return EINVAL; /* to controlling tty -- infinite recursion */
	if (cmd == TIOCNOTTY) {
		PROC_LOCK(td->td_proc);
		SESS_LOCK(td->td_proc->p_session);
		error = 0;
		if (!SESS_LEADER(td->td_proc))
			td->td_proc->p_flag &= ~P_CONTROLT;
		else
			error = EINVAL;
		SESS_UNLOCK(td->td_proc->p_session);
		PROC_UNLOCK(td->td_proc);
		return (error);
	}
	/* XXXMAC: Should this be td->td_ucred below? */
	return (VOP_IOCTL(ttyvp, cmd, addr, flag, NOCRED, td));
}

/*ARGSUSED*/
static	int
cttypoll(dev, events, td)
	dev_t dev;
	int events;
	struct thread *td;
{
	struct vnode *ttyvp;
#ifdef MAC
	int error;
#endif

	PROC_LOCK(td->td_proc);
	SESS_LOCK(td->td_proc->p_session);
	ttyvp = cttyvp(td);
	SESS_UNLOCK(td->td_proc->p_session);
	PROC_UNLOCK(td->td_proc);

	if (ttyvp == NULL)
		/* try operation to get EOF/failure */
		return (seltrue(dev, events, td));
#ifdef MAC
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY, td);
	error = mac_check_vnode_poll(td->td_ucred, NOCRED, ttyvp);
	VOP_UNLOCK(ttyvp, 0, td);
	if (error)
		return (error);
#endif
	return (VOP_POLL(ttyvp, events, td->td_ucred, td));
}

static void ctty_clone(void *arg, char *name, int namelen, dev_t *dev);

static dev_t ctty;

static void
ctty_clone(void *arg, char *name, int namelen, dev_t *dev)
{
	struct vnode *vp;

	if (*dev != NODEV)
		return;
	if (strcmp(name, "tty"))
		return;
	vp = cttyvp(curthread);
	if (vp == NULL) {
		if (ctty)
			*dev = ctty;
	} else
		*dev = vp->v_rdev;
}


static void ctty_drvinit(void *unused);
static void
ctty_drvinit(unused)
	void *unused;
{

	if (devfs_present) {
		EVENTHANDLER_REGISTER(dev_clone, ctty_clone, 0, 1000);
		ctty = make_dev(&ctty_cdevsw, 0, 0, 0, 0666, "ctty");
	} else {
		make_dev(&ctty_cdevsw, 0, 0, 0, 0666, "tty");
	}
}

SYSINIT(cttydev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ctty_drvinit,NULL)
