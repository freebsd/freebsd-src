/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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

static const char vpsid[] =
    "$Id: vps_console.c 161 2013-06-06 17:51:19Z klaus $";

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/libkern.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/ioccom.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/syscallsubr.h>
#include <sys/resourcevar.h>
#include <sys/ttycom.h>
#include <sys/tty.h>
#include <sys/filedesc.h>
#include <sys/kthread.h>
#include <sys/poll.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <net/if.h>
#include <netinet/in.h>

#include <security/mac/mac_framework.h>

#include "vps_user.h"
#include "vps.h"
#include "vps2.h"

#define CF_OPENED 1

MALLOC_DECLARE(M_VPS_CORE);

static struct mtx		vps_console_mtx;
static struct proc		*vps_console_kproc_p = NULL;
static int			vps_console_exit = 0;
static void			*vps_console_readbuf;
static size_t			vps_console_readbuf_len;

static th_getc_capture_t	vps_console_getc_capture;

static struct ttyhook vps_console_hook = {
	.th_getc_capture = vps_console_getc_capture,
};

static void vps_console_kproc(void *);

/*
 * VPS pseudo system console.
 *
 * Allocates a PTS TTY and exposes it as /dev/console and /dev/ttyv0
 * to the VPS instance.
 * It can be attached via the VPS_IOC_GETCONSFD ioctl.
 * NOTE: Output processing is forced, always converting NL to CR-LF.
 */

static void
vps_console_doread(struct thread *td, struct file *fp)
{
	struct iovec iovec;
	struct uio auio;
	int error;

	DBGCORE("%s: td=%p fp=%p\n", __func__, td, fp);

	KASSERT(vps_console_readbuf != NULL,
		("%s: vps_console_readbuf == NULL\n", __func__));

	memset(&auio, 0, sizeof(auio));
	auio.uio_rw = UIO_READ;
	auio.uio_td = td;
	auio.uio_resid = vps_console_readbuf_len;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_iovcnt = 1;
	auio.uio_iov = &iovec;
	iovec.iov_base = vps_console_readbuf;
	iovec.iov_len = vps_console_readbuf_len;

	if ((error = fo_read(fp, &auio, td->td_ucred, 0, td)) != 0) {
		DBGCORE("%s: fo_read(): %d\n",
			__func__, error);
	} else {
		DBGCORE("%s: fo_read(): read %lu bytes\n",
			__func__, vps_console_readbuf_len -
			(long unsigned int)auio.uio_resid);
	}
}

/*
 * Kernel process.
 * Reads away console data to avoid blocking userspace.
 * When userspace readers are attached, it does nothing.
 * Also clears the CF_OPENED flag when console has no readers.
 */
static void
vps_console_kproc(void *dummy)
{
	struct selinfo *si;
	struct seltd *stp;
	struct selfd *sfp;
	struct selfd *sfn;
	struct thread *td;
	struct vps *vps;
	struct file *fp;
	int ev, flags, n;
	int error;

	td = curthread;

	seltdinit(td);

	stp = td->td_sel;

	vps_console_readbuf_len = 0x10000;
	vps_console_readbuf = malloc(vps_console_readbuf_len,
		M_VPS_CORE, M_WAITOK);

	/*
	DBGCORE("%s: stp->st_wait = %p\n", __func__, &stp->st_wait);
	*/

	for (;;) {

		/* Scan */
		n = 0;
		sx_slock(&vps_all_lock);
		LIST_FOREACH(vps, &vps_head, vps_all) {
			fp = vps->console_fp_ma;
			ev = 0;
			if (fp != NULL) {
				fhold(fp);
				if (fp->f_count == 2) {
					vps->console_flags &= ~CF_OPENED;
					/*
					DBGCORE("%s: vps=%p console_flags="
					    %08x\n", __func__, vps,
					    vps->console_flags);
					*/
				}
				flags = POLLIN|POLLRDNORM;
				selfdalloc(td, (void *)fp);
				ev = fo_poll(fp, flags, td->td_ucred, td);
				/*
				DBGCORE("%s: scan: fp=%p, fp->f_count=%d, "
				    "ev=%08x\n", __func__, fp, fp->f_count,
				    ev);
				*/
				if (ev & POLLIN || ev & POLLRDNORM) {
					/* Read possible now, not
					   select record. */
					if (fp->f_count == 2)
						vps_console_doread(td, fp);
					fdrop(fp, td);
				} else {
					/* Select record created. */
					n += 1;
				}
			}
		}
		sx_sunlock(&vps_all_lock);

		if (n != 0) {
			sbintime_t asbt, precision, rsbt;

                        rsbt = SBT_1MS * 100;
                        precision = rsbt;
                        precision >>= tc_precexp;
                        if (TIMESEL(&asbt, rsbt))
                                asbt += tc_tick_sbt;
                        asbt += rsbt;

			error = seltdwait(td, asbt, precision);

			if (error != 0 && error != EAGAIN) {
				DBGCORE("%s: error seltdwait(): %d\n",
					__func__, error);
			}
		} else {
			/*
			 * Nothing to select. We will be woken up when a new
			 * vps console is created.
			 * (Could also exit here and be re-created
			 * when needed.)
			 */
			tsleep(vps_console_kproc, 0, "bored", hz * 1);
		}

		/* Re-scan */
		stp = td->td_sel;
		STAILQ_FOREACH_SAFE(sfp, &stp->st_selq, sf_link, sfn) {
			fp = (struct file *)sfp->sf_cookie;
			vps = (struct vps *)((struct tty *)
			    (fp->f_data))->t_hooksoftc;
			if (fp->f_count == 2) {
				vps->console_flags &= ~CF_OPENED;
				/*
				DBGCORE("%s: vps=%p console_flags=%08x\n",
					__func__, vps, vps->console_flags);
				*/
			}
			si = sfp->sf_si;
			selfdfree(stp, sfp);
			/* If the selinfo wasn't cleared the event
			   didn't fire. */
			if (si != NULL) {
				fdrop(fp, td);
				continue;
			}
			flags = POLLIN|POLLRDNORM;
			ev = fo_poll(fp, flags, td->td_ucred, td);
			/*
			DBGCORE("%s: re-scan: fp=%p, ev=%08x\n",
			    __func__, fp, ev);
			*/
			if ((ev & POLLIN || ev & POLLRDNORM) &&
			    fp->f_count == 2)
				vps_console_doread(td, fp);
			fdrop(fp, td);
		}
		stp->st_flags = 0;

		if (vps_console_exit != 0)
			break;
	}

	seltdclear(td);

	free(vps_console_readbuf, M_VPS_CORE);
	vps_console_readbuf = NULL;

	kproc_exit(0);
}

/*
 * Global init.
 */
int
vps_console_init(void)
{

	mtx_init(&vps_console_mtx, "vps console lock", NULL, MTX_RECURSE);

        selfd_zone = uma_zcreate("vps_console_selfd", sizeof(struct selfd),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

	return (0);
}

__attribute__((unused))
int
vps_console_uninit(void)
{

	/* Tell kproc to exit. */
	vps_console_exit = 1;

	tsleep(vps_console_kproc_p, 0, "uninit", 0);

	mtx_destroy(&vps_console_mtx);

	return (0);
}

/*
 * Init a console device for one vps instance.
 */
int
vps_console_alloc(struct vps *vps, struct thread *td)
{
	struct vps *savevps;
	struct ucred *vps_ucred;
	struct cdev *dev_orig;
	struct cdev *dev;
	struct file *fp_ma;
	struct tty *tp;
	int error;
#if 1
	/* Additional debugging. */
	struct ucred *saveucred;
	struct ucred *dbgucred;

	dbgucred = crget();
	crcopy(dbgucred, vps->vps_ucred);
	saveucred = td->td_ucred;
	td->td_ucred = dbgucred;
	vps_ucred = dbgucred;
#else
	vps_ucred = crhold(vps->vps_ucred);
#endif

	DBGCORE("%s: td=%p vps=%p\n", __func__, td, vps);

	if (vps_console_kproc_p == NULL) {
        	error = kproc_create(vps_console_kproc, NULL,
		    &vps_console_kproc_p, 0, 0, "vps_console");
		KASSERT(error == 0, ("%s: kproc_create() error=%d\n",
		    __func__, error));
	} else {
		wakeup(vps_console_kproc_p);
	}

	KASSERT(curthread->td_vps == vps0,
		("%s: curthread->td_vps=%p != vps0 !\n",
		__func__, curthread->td_vps));

	if (vps->console_tty != NULL) {
		DBGCORE("%s: vps->console_tty=%p != NULL !\n",
			__func__, vps->console_tty);
		return (EEXIST);
	}

	if ((error = falloc_noinstall(td, &fp_ma)) != 0) {
		DBGCORE("%s: fp_alloc_noinstall(): %d\n",
			__func__, error);
		return (error);
	}

	savevps = td->td_vps;
	td->td_vps = vps;

	if ((error = pts_alloc(FREAD|FWRITE, td, fp_ma)) != 0) {
		DBGCORE("%s: pts_alloc(): %d\n",
			__func__, error);
		fdrop(fp_ma, td);
		return (error);
	}

	/* This is the master side. */
	vps->console_flags = 0;
	vps->console_fp_ma = fp_ma;
	vps->console_tty = tp = fp_ma->f_data;

	/* Destroy the original slave device later. */
	dev_orig = tp->t_dev;

	/* Expose slave device in vps instance. */

	DBGCORE("%s: creating tty dev with ucred=%p vps=%p; fp_ma=%p\n",
		__func__, vps_ucred, vps_ucred->cr_vps, fp_ma);

	tty_makedev(tp, vps_ucred, "console");

	/* vps->console_tty->t_dev is the slave character device. */
	dev = tp->t_dev;

	destroy_dev(dev_orig);

	/* Will be deleted automatically on delete of parent device. */
	make_dev_alias_cred(dev, vps_ucred, "ttyv0");

	DBGCORE("%s: tty=%p dev=%p (slave device)\n", __func__, tp, dev);

	dev_lock();
	dev->si_usecount++;
	dev_unlock();

	td->td_vps = savevps;

	/* install tty hooks */
	tty_lock(tp);
	if (tp->t_flags & TF_HOOK) {
		DBGCORE("%s: ERROR: tp->t_hook=%p "
		    "(tp->t_flags & TF_HOOK)=%d\n",
		    __func__, tp->t_hook, (tp->t_flags & TF_HOOK));
	} else {
		tp->t_flags |= TF_HOOK;
		tp->t_hook = &vps_console_hook;
		tp->t_hooksoftc = vps;
	}
	tty_unlock(tp);

#if 1
	/* Additional debugging. */
	td->td_ucred = saveucred;
	crfree(dbgucred);

	DBGCORE("%s: fp_ma=%p f_count=%d dev=%p si_refcount=%d "
	    "si_cred=%p si_cred->cr_ref=%d\n",
	    __func__, fp_ma, fp_ma->f_count, dev,
	    dev->si_refcount, dev->si_cred, dev->si_cred->cr_ref);
#else
	crfree(vps_ucred);
#endif

	return (error);
}

/*
 * Free (destruct) the console device of one vps instance.
 */
int
vps_console_free(struct vps *vps, struct thread *td)
{
	struct cdev *dev;
	struct file *fp;
	struct tty *tp;

	DBGCORE("%s: td=%p vps=%p\n", __func__, td, vps);

	KASSERT(vps->console_fp_ma != NULL,
		("%s: vps->console_fp_ma == NULL\n", __func__));
	KASSERT(vps->console_tty != NULL,
		("%s: vps->console_tty == NULL\n", __func__));

	fp = vps->console_fp_ma;
	tp = vps->console_tty;
	dev = tp->t_dev;
	vps->console_fp_ma = NULL;
	vps->console_tty = NULL;
	vps->console_flags = 0;

	dev_lock();
	dev->si_usecount--;
	dev_unlock();

	/* XXX get rid of remaining data, otherwise close will block. */

	/* This will revoke the tty so readers of the master get EOF. */
	dev->si_devsw->d_close(dev, FREAD|FWRITE, 0, td);

	/*
	DBGCORE("%s: before dropping my ref fp=%p f_count=%d dev=%p "
	    "si_refcount=%d si_cred=%p\n",
	    __func__, fp, fp->f_count, dev, dev->si_refcount, dev->si_cred);
	*/

	fdrop(fp, td);

	/*
	DBGCORE("%s: after dropping my ref fp=%p f_count=%d dev=%p "
	    "si_refcount=%d si_cred=%p\n",
	    __func__, fp, fp->f_count, dev, dev->si_refcount, dev->si_cred);
	*/

	tty_lock(tp);
	ttyhook_unregister(tp);

	return (0);
}

/*
 * Get a file descriptor to a vps instance's console.
 */
int
vps_console_getfd(struct vps *vps, struct thread *td, int *retfd)
{
	struct vps *vps2;
	struct file *fp;
	int fd;
	int error;

	DBGCORE("%s: td=%p vps=%p\n", __func__, td, vps);

	/*
	 * Make sure vps is a child of td->td_vps.
	 */
	LIST_FOREACH(vps2, &td->td_vps->vps_child_head, vps_sibling)
		if (vps == vps2)
			break;
	if (vps != vps2) {
		DBGCORE("%s: vps=%p is not a child of td->td_vps=%p\n",
		    __func__, vps, td->td_vps);
		return (EPERM);
	}

	fp = vps->console_fp_ma;

	if (fp == NULL)
		return (ENOENT);

	if ((vps->console_flags & CF_OPENED) != 0) {
		DBGCORE("%s: vps=%p console busy: flags=%08x "
		    "fp->f_count=%d\n",
		    __func__, vps, vps->console_flags, fp->f_count);
		return (EBUSY);
	}
	vps->console_flags |= CF_OPENED;

	if ((error = finstall(td, fp, &fd, FREAD|FWRITE, NULL)) != 0) {
		DBGCORE("%s: finstall(): %d\n", __func__, error);
		fdrop(fp, td);
		return (error);
	}

	DBGCORE("%s: installed fp=%p for thread %p as fd=%d\n",
		__func__, fp, td, fd);

	*retfd = fd;

	return (0);
}

/*
 * Force newline to LF-CR conversion, otherwise all of init/rc output
 * is totally unreadable.
 */
static void
vps_console_getc_capture(struct tty *tp, const void *buf, size_t len)
{

	tp->t_termios.c_oflag |= OPOST|ONLCR;
}

/* EOF */
