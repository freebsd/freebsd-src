/*-
 * Copyright (c) 2008 Ed Schouten <ed@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed under sponsorship from Snow
 * B.V., the Netherlands.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_tty.h"

/* Add compatibility bits for FreeBSD. */
#define PTS_COMPAT
#ifdef DEV_PTY
/* Add /dev/ptyXX compat bits. */
#define PTS_EXTERNAL
#endif /* DEV_PTY */
/* Add bits to make Linux binaries work. */
#define PTS_LINUX

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/serial.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/ttycom.h>

#include <machine/stdarg.h>

static struct unrhdr *pts_pool;
#define MAXPTSDEVS 999

static MALLOC_DEFINE(M_PTS, "pts", "pseudo tty device");

/*
 * Per-PTS structure.
 *
 * List of locks
 * (t)	locked by tty_lock()
 * (c)	const until freeing
 */
struct pts_softc {
	int		pts_unit;	/* (c) Device unit number. */
	unsigned int	pts_flags;	/* (t) Device flags. */
#define	PTS_PKT		0x1	/* Packet mode. */
#define	PTS_FINISHED	0x2	/* Return errors on read()/write(). */
	char		pts_pkt;	/* (t) Unread packet mode data. */

	struct cv	pts_inwait;	/* (t) Blocking write() on master. */
	struct selinfo	pts_inpoll;	/* (t) Select queue for write(). */
	struct cv	pts_outwait;	/* (t) Blocking read() on master. */
	struct selinfo	pts_outpoll;	/* (t) Select queue for read(). */

#ifdef PTS_EXTERNAL
	struct cdev	*pts_cdev;	/* (c) Master device node. */
#endif /* PTS_EXTERNAL */

	struct uidinfo	*pts_uidinfo;	/* (c) Resource limit. */
};

/*
 * Controller-side file operations.
 */

static int
ptsdev_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct tty *tp = fp->f_data;
	struct pts_softc *psc = tty_softc(tp);
	int error = 0;
	char pkt;

	if (uio->uio_resid == 0)
		return (0);

	tty_lock(tp);

	for (;;) {
		/*
		 * Implement packet mode. When packet mode is turned on,
		 * the first byte contains a bitmask of events that
		 * occured (start, stop, flush, window size, etc).
		 */
		if (psc->pts_flags & PTS_PKT && psc->pts_pkt) {
			pkt = psc->pts_pkt;
			psc->pts_pkt = 0;
			tty_unlock(tp);

			error = ureadc(pkt, uio);
			return (error);
		}

		/*
		 * Transmit regular data.
		 *
		 * XXX: We shouldn't use ttydisc_getc_poll()! Even
		 * though in this implementation, there is likely going
		 * to be data, we should just call ttydisc_getc_uio()
		 * and use its return value to sleep.
		 */
		if (ttydisc_getc_poll(tp)) {
			if (psc->pts_flags & PTS_PKT) {
				/*
				 * XXX: Small race. Fortunately PTY
				 * consumers aren't multithreaded.
				 */

				tty_unlock(tp);
				error = ureadc(TIOCPKT_DATA, uio);
				if (error)
					return (error);
				tty_lock(tp);
			}

			error = ttydisc_getc_uio(tp, uio);
			break;
		}

		/* Maybe the device isn't used anyway. */
		if (psc->pts_flags & PTS_FINISHED)
			break;

		/* Wait for more data. */
		if (fp->f_flag & O_NONBLOCK) {
			error = EWOULDBLOCK;
			break;
		}
		error = cv_wait_sig(&psc->pts_outwait, tp->t_mtx);
		if (error != 0)
			break;
	}

	tty_unlock(tp);

	return (error);
}

static int
ptsdev_write(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct tty *tp = fp->f_data;
	struct pts_softc *psc = tty_softc(tp);
	char ib[256], *ibstart;
	size_t iblen, rintlen;
	int error = 0;

	if (uio->uio_resid == 0)
		return (0);

	for (;;) {
		ibstart = ib;
		iblen = MIN(uio->uio_resid, sizeof ib);
		error = uiomove(ib, iblen, uio);

		tty_lock(tp);
		if (error != 0)
			goto done;

		/*
		 * When possible, avoid the slow path. rint_bypass()
		 * copies all input to the input queue at once.
		 */
		MPASS(iblen > 0);
		do {
			if (ttydisc_can_bypass(tp)) {
				/* Store data at once. */
				rintlen = ttydisc_rint_bypass(tp,
				    ibstart, iblen);
				ibstart += rintlen;
				iblen -= rintlen;

				if (iblen == 0) {
					/* All data written. */
					break;
				}
			} else {
				error = ttydisc_rint(tp, *ibstart, 0);
				if (error == 0) {
					/* Character stored successfully. */
					ibstart++;
					iblen--;
					continue;
				}
			}

			/* Maybe the device isn't used anyway. */
			if (psc->pts_flags & PTS_FINISHED) {
				error = EIO;
				goto done;
			}

			/* Wait for more data. */
			if (fp->f_flag & O_NONBLOCK) {
				error = EWOULDBLOCK;
				goto done;
			}

			/* Wake up users on the slave side. */
			ttydisc_rint_done(tp);
			error = cv_wait_sig(&psc->pts_inwait, tp->t_mtx);
			if (error != 0)
				goto done;
		} while (iblen > 0);

		if (uio->uio_resid == 0)
			break;
		tty_unlock(tp);
	}

done:	ttydisc_rint_done(tp);
	tty_unlock(tp);
	return (error);
}

static int
ptsdev_ioctl(struct file *fp, u_long cmd, void *data,
    struct ucred *active_cred, struct thread *td)
{
	struct tty *tp = fp->f_data;
	struct pts_softc *psc = tty_softc(tp);
	int error = 0, sig;

	switch (cmd) {
	case FIONBIO:
		/* This device supports non-blocking operation. */
		return (0);
	case FIODGNAME: {
		struct fiodgname_arg *fgn;
		const char *p;
		int i;

		/* Reverse device name lookups, for ptsname() and ttyname(). */
		fgn = data;
#ifdef PTS_EXTERNAL
		if (psc->pts_cdev != NULL)
			p = devtoname(psc->pts_cdev);
		else
#endif /* PTS_EXTERNAL */
			p = tty_devname(tp);
		i = strlen(p) + 1;
		if (i > fgn->len)
			return (EINVAL);
		return copyout(p, fgn->buf, i);
	}
	
	/*
	 * We need to implement TIOCGPGRP and TIOCGSID here again. When
	 * called on the pseudo-terminal master, it should not check if
	 * the terminal is the foreground terminal of the calling
	 * process.
	 *
	 * TIOCGETA is also implemented here. Various Linux PTY routines
	 * often call isatty(), which is implemented by tcgetattr().
	 */
#ifdef PTS_LINUX
	case TIOCGETA:
		/* Obtain terminal flags through tcgetattr(). */
		tty_lock(tp);
		bcopy(&tp->t_termios, data, sizeof(struct termios));
		tty_unlock(tp);
		return (0);
#endif /* PTS_LINUX */
	case TIOCSETAF:
	case TIOCSETAW:
		/*
		 * We must make sure we turn tcsetattr() calls of TCSAFLUSH and
		 * TCSADRAIN into something different. If an application would
		 * call TCSAFLUSH or TCSADRAIN on the master descriptor, it may
		 * deadlock waiting for all data to be read.
		 */
		cmd = TIOCSETA;
		break;
#if defined(PTS_COMPAT) || defined(PTS_LINUX)
	case TIOCGPTN:
		/*
		 * Get the device unit number.
		 */
		if (psc->pts_unit < 0)
			return (ENOTTY);
		*(unsigned int *)data = psc->pts_unit;
		return (0);
#endif /* PTS_COMPAT || PTS_LINUX */
	case TIOCGPGRP:
		/* Get the foreground process group ID. */
		tty_lock(tp);
		if (tp->t_pgrp != NULL)
			*(int *)data = tp->t_pgrp->pg_id;
		else
			*(int *)data = NO_PID;
		tty_unlock(tp);
		return (0);
	case TIOCGSID:
		/* Get the session leader process ID. */
		tty_lock(tp);
		if (tp->t_session == NULL)
			error = ENOTTY;
		else
			*(int *)data = tp->t_session->s_sid;
		tty_unlock(tp);
		return (error);
	case TIOCPTMASTER:
		/* Yes, we are a pseudo-terminal master. */
		return (0);
	case TIOCSIG:
		/* Signal the foreground process group. */
		sig = *(int *)data;
		if (sig < 1 || sig >= NSIG)
			return (EINVAL);

		tty_lock(tp);
		tty_signal_pgrp(tp, sig);
		tty_unlock(tp);
		return (0);
	case TIOCPKT:
		/* Enable/disable packet mode. */
		tty_lock(tp);
		if (*(int *)data)
			psc->pts_flags |= PTS_PKT;
		else
			psc->pts_flags &= ~PTS_PKT;
		tty_unlock(tp);
		return (0);
	}

	/* Just redirect this ioctl to the slave device. */
	tty_lock(tp);
	error = tty_ioctl(tp, cmd, data, td);
	tty_unlock(tp);

	return (error);
}

static int
ptsdev_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{
	struct tty *tp = fp->f_data;
	struct pts_softc *psc = tty_softc(tp);
	int revents = 0;

	tty_lock(tp);

	if (psc->pts_flags & PTS_FINISHED) {
		/* Slave device is not opened. */
		tty_unlock(tp);
		return (events &
		    (POLLHUP|POLLIN|POLLRDNORM|POLLOUT|POLLWRNORM));
	}

	if (events & (POLLIN|POLLRDNORM)) {
		/* See if we can getc something. */
		if (ttydisc_getc_poll(tp) ||
		    (psc->pts_flags & PTS_PKT && psc->pts_pkt))
			revents |= events & (POLLIN|POLLRDNORM);
	}
	if (events & (POLLOUT|POLLWRNORM)) {
		/* See if we can rint something. */
		if (ttydisc_rint_poll(tp))
			revents |= events & (POLLOUT|POLLWRNORM);
	}

	/*
	 * No need to check for POLLHUP here. This device cannot be used
	 * as a callout device, which means we always have a carrier,
	 * because the master is.
	 */

	if (revents == 0) {
		/*
		 * This code might look misleading, but the naming of
		 * poll events on this side is the opposite of the slave
		 * device.
		 */
		if (events & (POLLIN|POLLRDNORM))
			selrecord(td, &psc->pts_outpoll);
		if (events & (POLLOUT|POLLWRNORM))
			selrecord(td, &psc->pts_inpoll);
	}

	tty_unlock(tp);

	return (revents);
}

static int
ptsdev_stat(struct file *fp, struct stat *sb, struct ucred *active_cred,
    struct thread *td)
{
	struct tty *tp = fp->f_data;
#ifdef PTS_EXTERNAL
	struct pts_softc *psc = tty_softc(tp);
#endif /* PTS_EXTERNAL */
	struct cdev *dev = tp->t_dev;

	/*
	 * According to POSIX, we must implement an fstat(). This also
	 * makes this implementation compatible with Linux binaries,
	 * because Linux calls fstat() on the pseudo-terminal master to
	 * obtain st_rdev.
	 *
	 * XXX: POSIX also mentions we must fill in st_dev, but how?
	 */

	bzero(sb, sizeof *sb);
#ifdef PTS_EXTERNAL
	if (psc->pts_cdev != NULL)
		sb->st_ino = sb->st_rdev = dev2udev(psc->pts_cdev);
	else
#endif /* PTS_EXTERNAL */
		sb->st_ino = sb->st_rdev = tty_udev(tp);

	sb->st_atimespec = dev->si_atime;
	sb->st_ctimespec = dev->si_ctime;
	sb->st_mtimespec = dev->si_mtime;
	sb->st_uid = dev->si_uid;
	sb->st_gid = dev->si_gid;
	sb->st_mode = dev->si_mode | S_IFCHR;
	
	return (0);
}

static int
ptsdev_close(struct file *fp, struct thread *td)
{
	struct tty *tp = fp->f_data;

	/* Deallocate TTY device. */
	tty_lock(tp);
	tty_rel_gone(tp);

	return (0);
}

static struct fileops ptsdev_ops = {
	.fo_read	= ptsdev_read,
	.fo_write	= ptsdev_write,
	.fo_ioctl	= ptsdev_ioctl,
	.fo_poll	= ptsdev_poll,
	.fo_stat	= ptsdev_stat,
	.fo_close	= ptsdev_close,
	.fo_flags	= DFLAG_PASSABLE,
};

/*
 * Driver-side hooks.
 */

static void
ptsdrv_outwakeup(struct tty *tp)
{
	struct pts_softc *psc = tty_softc(tp);

	cv_broadcast(&psc->pts_outwait);
	selwakeup(&psc->pts_outpoll);
}

static void
ptsdrv_inwakeup(struct tty *tp)
{
	struct pts_softc *psc = tty_softc(tp);

	cv_broadcast(&psc->pts_inwait);
	selwakeup(&psc->pts_inpoll);
}

static int
ptsdrv_open(struct tty *tp)
{
	struct pts_softc *psc = tty_softc(tp);

	psc->pts_flags &= ~PTS_FINISHED;

	return (0);
}

static void
ptsdrv_close(struct tty *tp)
{
	struct pts_softc *psc = tty_softc(tp);

	/* Wake up any blocked readers/writers. */
	ptsdrv_outwakeup(tp);
	ptsdrv_inwakeup(tp);

	psc->pts_flags |= PTS_FINISHED;
}

static void
ptsdrv_pktnotify(struct tty *tp, char event)
{
	struct pts_softc *psc = tty_softc(tp);

	/*
	 * Clear conflicting flags.
	 */

	switch (event) {
	case TIOCPKT_STOP:
		psc->pts_pkt &= ~TIOCPKT_START;
		break;
	case TIOCPKT_START:
		psc->pts_pkt &= ~TIOCPKT_STOP;
		break;
	case TIOCPKT_NOSTOP:
		psc->pts_pkt &= ~TIOCPKT_DOSTOP;
		break;
	case TIOCPKT_DOSTOP:
		psc->pts_pkt &= ~TIOCPKT_NOSTOP;
		break;
	}

	psc->pts_pkt |= event;
	ptsdrv_outwakeup(tp);
}

static void
ptsdrv_free(void *softc)
{
	struct pts_softc *psc = softc;

	/* Make device number available again. */
	if (psc->pts_unit >= 0)
		free_unr(pts_pool, psc->pts_unit);

	chgptscnt(psc->pts_uidinfo, -1, 0);
	uifree(psc->pts_uidinfo);

#ifdef PTS_EXTERNAL
	/* Destroy master device as well. */
	if (psc->pts_cdev != NULL)
		destroy_dev_sched(psc->pts_cdev);
#endif /* PTS_EXTERNAL */

	free(psc, M_PTS);
}

static struct ttydevsw pts_class = {
	.tsw_flags	= TF_NOPREFIX,
	.tsw_outwakeup	= ptsdrv_outwakeup,
	.tsw_inwakeup	= ptsdrv_inwakeup,
	.tsw_open	= ptsdrv_open,
	.tsw_close	= ptsdrv_close,
	.tsw_pktnotify	= ptsdrv_pktnotify,
	.tsw_free	= ptsdrv_free,
};

static int
pts_alloc(int fflags, struct thread *td, struct file *fp)
{
	int unit, ok;
	struct tty *tp;
	struct pts_softc *psc;
	struct proc *p = td->td_proc;
	struct uidinfo *uid = td->td_ucred->cr_ruidinfo;

	/* Resource limiting. */
	PROC_LOCK(p);
	ok = chgptscnt(uid, 1, lim_cur(p, RLIMIT_NPTS));
	PROC_UNLOCK(p);
	if (!ok)
		return (EAGAIN);

	/* Try to allocate a new pts unit number. */
	unit = alloc_unr(pts_pool);
	if (unit < 0) {
		chgptscnt(uid, -1, 0);
		return (EAGAIN);
	}

	/* Allocate TTY and softc. */
	psc = malloc(sizeof(struct pts_softc), M_PTS, M_WAITOK|M_ZERO);
	cv_init(&psc->pts_inwait, "pts inwait");
	cv_init(&psc->pts_outwait, "pts outwait");

	psc->pts_unit = unit;
	psc->pts_uidinfo = uid;
	uihold(uid);

	tp = tty_alloc(&pts_class, psc, NULL);

	/* Expose the slave device as well. */
	tty_makedev(tp, td->td_ucred, "pts/%u", psc->pts_unit);

	finit(fp, fflags, DTYPE_PTS, tp, &ptsdev_ops);

	return (0);
}

#ifdef PTS_EXTERNAL
int
pts_alloc_external(int fflags, struct thread *td, struct file *fp,
    struct cdev *dev, const char *name)
{
	int ok;
	struct tty *tp;
	struct pts_softc *psc;
	struct proc *p = td->td_proc;
	struct uidinfo *uid = td->td_ucred->cr_ruidinfo;

	/* Resource limiting. */
	PROC_LOCK(p);
	ok = chgptscnt(uid, 1, lim_cur(p, RLIMIT_NPTS));
	PROC_UNLOCK(p);
	if (!ok)
		return (EAGAIN);

	/* Allocate TTY and softc. */
	psc = malloc(sizeof(struct pts_softc), M_PTS, M_WAITOK|M_ZERO);
	cv_init(&psc->pts_inwait, "pts inwait");
	cv_init(&psc->pts_outwait, "pts outwait");

	psc->pts_unit = -1;
	psc->pts_cdev = dev;
	psc->pts_uidinfo = uid;
	uihold(uid);

	tp = tty_alloc(&pts_class, psc, NULL);

	/* Expose the slave device as well. */
	tty_makedev(tp, td->td_ucred, "%s", name);

	finit(fp, fflags, DTYPE_PTS, tp, &ptsdev_ops);

	return (0);
}
#endif /* PTS_EXTERNAL */

int
posix_openpt(struct thread *td, struct posix_openpt_args *uap)
{
	int error, fd;
	struct file *fp;

	/*
	 * POSIX states it's unspecified when other flags are passed. We
	 * don't allow this.
	 */
	if (uap->flags & ~(O_RDWR|O_NOCTTY))
		return (EINVAL);
	
	error = falloc(td, &fp, &fd);
	if (error)
		return (error);

	/* Allocate the actual pseudo-TTY. */
	error = pts_alloc(FFLAGS(uap->flags & O_ACCMODE), td, fp);
	if (error != 0) {
		fdclose(td->td_proc->p_fd, fp, fd, td);
		return (error);
	}

	/* Pass it back to userspace. */
	td->td_retval[0] = fd;
	fdrop(fp, td);

	return (0);
}

#if defined(PTS_COMPAT) || defined(PTS_LINUX)
static int
ptmx_fdopen(struct cdev *dev, int fflags, struct thread *td, struct file *fp)
{

	return (pts_alloc(fflags & (FREAD|FWRITE), td, fp));
}

static struct cdevsw ptmx_cdevsw = {
	.d_version	= D_VERSION,
	.d_fdopen	= ptmx_fdopen,
	.d_name		= "ptmx",
};
#endif /* PTS_COMPAT || PTS_LINUX */

static void
pts_init(void *unused)
{

	pts_pool = new_unrhdr(0, MAXPTSDEVS, NULL);
#if defined(PTS_COMPAT) || defined(PTS_LINUX)
	make_dev(&ptmx_cdevsw, 0, UID_ROOT, GID_WHEEL, 0666, "ptmx");
#endif /* PTS_COMPAT || PTS_LINUX */
}

SYSINIT(pts, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, pts_init, NULL);
