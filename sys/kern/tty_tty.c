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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/ttycom.h>
#include <sys/vnode.h>

static	d_open_t	cttyopen;
static	d_read_t	cttyread;
static	d_write_t	cttywrite;
static	d_ioctl_t	cttyioctl;
static	d_poll_t	cttypoll;

#define	CDEV_MAJOR	1
/* Don't make this static, since fdesc_vnops uses it. */
struct cdevsw ctty_cdevsw = {
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

#define cttyvp(p) ((p)->p_flag & P_CONTROLT ? (p)->p_session->s_ttyvp : NULL)

/*ARGSUSED*/
static	int
cttyopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct vnode *ttyvp = cttyvp(p);
	int error;

	if (ttyvp == NULL)
		return (ENXIO);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY, p);
#ifdef PARANOID
	/*
	 * Since group is tty and mode is 620 on most terminal lines
	 * and since sessions protect terminals from processes outside
	 * your session, this check is probably no longer necessary.
	 * Since it inhibits setuid root programs that later switch
	 * to another user from accessing /dev/tty, we have decided
	 * to delete this test. (mckusick 5/93)
	 */
	error = VOP_ACCESS(ttyvp,
	  (flag&FREAD ? VREAD : 0) | (flag&FWRITE ? VWRITE : 0), p->p_ucred, p);
	if (!error)
#endif /* PARANOID */
		error = VOP_OPEN(ttyvp, flag, NOCRED, p);
	VOP_UNLOCK(ttyvp, 0, p);
	return (error);
}

/*ARGSUSED*/
static	int
cttyread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct proc *p = uio->uio_procp;
	register struct vnode *ttyvp = cttyvp(p);
	int error;

	if (ttyvp == NULL)
		return (EIO);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_READ(ttyvp, uio, flag, NOCRED);
	VOP_UNLOCK(ttyvp, 0, p);
	return (error);
}

/*ARGSUSED*/
static	int
cttywrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct proc *p = uio->uio_procp;
	struct vnode *ttyvp = cttyvp(uio->uio_procp);
	struct mount *mp;
	int error;

	if (ttyvp == NULL)
		return (EIO);
	mp = NULL;
	if (ttyvp->v_type != VCHR &&
	    (error = vn_start_write(ttyvp, &mp, V_WAIT | PCATCH)) != 0)
		return (error);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_WRITE(ttyvp, uio, flag, NOCRED);
	VOP_UNLOCK(ttyvp, 0, p);
	vn_finished_write(mp);
	return (error);
}

/*ARGSUSED*/
static	int
cttyioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct vnode *ttyvp = cttyvp(p);

	if (ttyvp == NULL)
		return (EIO);
	if (cmd == TIOCSCTTY)  /* don't allow controlling tty to be set    */
		return EINVAL; /* to controlling tty -- infinite recursion */
	if (cmd == TIOCNOTTY) {
		if (!SESS_LEADER(p)) {
			p->p_flag &= ~P_CONTROLT;
			return (0);
		} else
			return (EINVAL);
	}
	return (VOP_IOCTL(ttyvp, cmd, addr, flag, NOCRED, p));
}

/*ARGSUSED*/
static	int
cttypoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	struct vnode *ttyvp = cttyvp(p);

	if (ttyvp == NULL)
		/* try operation to get EOF/failure */
		return (seltrue(dev, events, p));
	return (VOP_POLL(ttyvp, events, p->p_ucred, p));
}

static void ctty_drvinit __P((void *unused));
static void
ctty_drvinit(unused)
	void *unused;
{

	make_dev(&ctty_cdevsw, 0, 0, 0, 0666, "tty");
}

SYSINIT(cttydev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ctty_drvinit,NULL)
