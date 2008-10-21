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

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#ifdef COMPAT_43TTY
#include <sys/ioctl_compat.h>
#endif /* COMPAT_43TTY */
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/serial.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/ttycom.h>
#define TTYDEFCHARS
#include <sys/ttydefaults.h>
#undef TTYDEFCHARS
#include <sys/ucred.h>
#include <sys/vnode.h>

#include <machine/stdarg.h>

static MALLOC_DEFINE(M_TTY, "tty", "tty device");

static void tty_rel_free(struct tty *tp);

static TAILQ_HEAD(, tty) tty_list = TAILQ_HEAD_INITIALIZER(tty_list);
static struct sx tty_list_sx;
SX_SYSINIT(tty_list, &tty_list_sx, "tty list");
static unsigned int tty_list_count = 0;

/*
 * Flags that are supported and stored by this implementation.
 */
#define TTYSUP_IFLAG	(IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK|ISTRIP|\
			INLCR|IGNCR|ICRNL|IXON|IXOFF|IXANY|IMAXBEL)
#define TTYSUP_OFLAG	(OPOST|ONLCR|TAB3|ONOEOT|OCRNL|ONOCR|ONLRET)
#define TTYSUP_LFLAG	(ECHOKE|ECHOE|ECHOK|ECHO|ECHONL|ECHOPRT|\
			ECHOCTL|ISIG|ICANON|ALTWERASE|IEXTEN|TOSTOP|\
			FLUSHO|NOKERNINFO|NOFLSH)
#define TTYSUP_CFLAG	(CIGNORE|CSIZE|CSTOPB|CREAD|PARENB|PARODD|\
			HUPCL|CLOCAL|CCTS_OFLOW|CRTS_IFLOW|CDTR_IFLOW|\
			CDSR_OFLOW|CCAR_OFLOW)

#define TTY_CALLOUT(tp,d) ((tp)->t_dev != (d))

/*
 * Set TTY buffer sizes.
 */

#define	TTYBUF_MAX	65536

static void
tty_watermarks(struct tty *tp)
{
	size_t bs;

	/* Provide an input buffer for 0.2 seconds of data. */
	bs = MIN(tp->t_termios.c_ispeed / 5, TTYBUF_MAX);
	ttyinq_setsize(&tp->t_inq, tp, bs);

	/* Set low watermark at 10% (when 90% is available). */
	tp->t_inlow = (ttyinq_getsize(&tp->t_inq) * 9) / 10;

	/* Provide an ouput buffer for 0.2 seconds of data. */
	bs = MIN(tp->t_termios.c_ospeed / 5, TTYBUF_MAX);
	ttyoutq_setsize(&tp->t_outq, tp, bs);

	/* Set low watermark at 10% (when 90% is available). */
	tp->t_outlow = (ttyoutq_getsize(&tp->t_outq) * 9) / 10;
}

static int
tty_drain(struct tty *tp)
{
	int error;

	if (ttyhook_hashook(tp, getc_inject))
		/* buffer is inaccessable */
		return (0);

	while (ttyoutq_bytesused(&tp->t_outq) > 0) {
		ttydevsw_outwakeup(tp);
		/* Could be handled synchronously. */
		if (ttyoutq_bytesused(&tp->t_outq) == 0)
			return (0);

		/* Wait for data to be drained. */
		error = tty_wait(tp, &tp->t_outwait);
		if (error)
			return (error);
	}

	return (0);
}

/*
 * Though ttydev_enter() and ttydev_leave() seem to be related, they
 * don't have to be used together. ttydev_enter() is used by the cdev
 * operations to prevent an actual operation from being processed when
 * the TTY has been abandoned. ttydev_leave() is used by ttydev_open()
 * and ttydev_close() to determine whether per-TTY data should be
 * deallocated.
 */

static __inline int
ttydev_enter(struct tty *tp)
{
	tty_lock(tp);

	if (tty_gone(tp) || !tty_opened(tp)) {
		/* Device is already gone. */
		tty_unlock(tp);
		return (ENXIO);
	}

	return (0);
}

static void
ttydev_leave(struct tty *tp)
{
	tty_lock_assert(tp, MA_OWNED);

	if (tty_opened(tp) || tp->t_flags & TF_OPENCLOSE) {
		/* Device is still opened somewhere. */
		tty_unlock(tp);
		return;
	}

	tp->t_flags |= TF_OPENCLOSE;

	/* Stop asynchronous I/O. */
	funsetown(&tp->t_sigio);

	/* Remove console TTY. */
	if (constty == tp)
		constty_clear();

	/* Drain any output. */
	MPASS((tp->t_flags & TF_STOPPED) == 0);
	if (!tty_gone(tp))
		tty_drain(tp);

	ttydisc_close(tp);

	/* Destroy associated buffers already. */
	ttyinq_free(&tp->t_inq);
	tp->t_inlow = 0;
	ttyoutq_free(&tp->t_outq);
	tp->t_outlow = 0;

	knlist_clear(&tp->t_inpoll.si_note, 1);
	knlist_clear(&tp->t_outpoll.si_note, 1);

	if (!tty_gone(tp))
		ttydevsw_close(tp);

	tp->t_flags &= ~TF_OPENCLOSE;
	tty_rel_free(tp);
}

/*
 * Operations that are exposed through the character device in /dev.
 */
static int
ttydev_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct tty *tp = dev->si_drv1;
	int error = 0;

	/* Disallow access when the TTY belongs to a different prison. */
	if (dev->si_cred != NULL &&
	    dev->si_cred->cr_prison != td->td_ucred->cr_prison &&
	    priv_check(td, PRIV_TTY_PRISON)) {
		return (EPERM);
	}

	tty_lock(tp);
	if (tty_gone(tp)) {
		/* Device is already gone. */
		tty_unlock(tp);
		return (ENXIO);
	}
	/*
	 * Prevent the TTY from being opened when being torn down or
	 * built up by unrelated processes.
	 */
	if (tp->t_flags & TF_OPENCLOSE) {
		tty_unlock(tp);
		return (EBUSY);
	}
	tp->t_flags |= TF_OPENCLOSE;

	/*
	 * Make sure the "tty" and "cua" device cannot be opened at the
	 * same time.
	 */
	if (TTY_CALLOUT(tp, dev)) {
		if (tp->t_flags & TF_OPENED_IN) {
			error = EBUSY;
			goto done;
		}
	} else {
		if (tp->t_flags & TF_OPENED_OUT) {
			error = EBUSY;
			goto done;
		}
	}

	if (tp->t_flags & TF_EXCLUDE && priv_check(td, PRIV_TTY_EXCLUSIVE)) {
		error = EBUSY;
		goto done;
	}

	if (!tty_opened(tp)) {
		/* Set proper termios flags. */
		if (TTY_CALLOUT(tp, dev)) {
			tp->t_termios = tp->t_termios_init_out;
		} else {
			tp->t_termios = tp->t_termios_init_in;
		}
		ttydevsw_param(tp, &tp->t_termios);

		ttydevsw_modem(tp, SER_DTR|SER_RTS, 0);

		error = ttydevsw_open(tp);
		if (error != 0)
			goto done;

		ttydisc_open(tp);
		tty_watermarks(tp);
	}

	/* Wait for Carrier Detect. */
	if (!TTY_CALLOUT(tp, dev) && (oflags & O_NONBLOCK) == 0 &&
	    (tp->t_termios.c_cflag & CLOCAL) == 0) {
		while ((ttydevsw_modem(tp, 0, 0) & SER_DCD) == 0) {
			error = tty_wait(tp, &tp->t_dcdwait);
			if (error != 0)
				goto done;
		}
	}

	if (TTY_CALLOUT(tp, dev)) {
		tp->t_flags |= TF_OPENED_OUT;
	} else {
		tp->t_flags |= TF_OPENED_IN;
	}

done:	tp->t_flags &= ~TF_OPENCLOSE;
	ttydev_leave(tp);

	return (error);
}

static int
ttydev_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct tty *tp = dev->si_drv1;

	tty_lock(tp);

	/*
	 * This can only be called once. The callin and the callout
	 * devices cannot be opened at the same time.
	 */
	MPASS((tp->t_flags & TF_OPENED) != TF_OPENED);
	tp->t_flags &= ~(TF_OPENED|TF_EXCLUDE|TF_STOPPED);

	/* Properly wake up threads that are stuck - revoke(). */
	tp->t_revokecnt++;
	tty_wakeup(tp, FREAD|FWRITE);
	cv_broadcast(&tp->t_bgwait);

	ttydev_leave(tp);

	return (0);
}

static __inline int
tty_is_ctty(struct tty *tp, struct proc *p)
{
	tty_lock_assert(tp, MA_OWNED);

	return (p->p_session == tp->t_session && p->p_flag & P_CONTROLT);
}

static int
tty_wait_background(struct tty *tp, struct thread *td, int sig)
{
	struct proc *p = td->td_proc;
	struct pgrp *pg;
	int error;

	MPASS(sig == SIGTTIN || sig == SIGTTOU);
	tty_lock_assert(tp, MA_OWNED);

	for (;;) {
		PROC_LOCK(p);
		/*
		 * The process should only sleep, when:
		 * - This terminal is the controling terminal
		 * - Its process group is not the foreground process
		 *   group
		 * - The parent process isn't waiting for the child to
		 *   exit
		 * - the signal to send to the process isn't masked
		 */
		if (!tty_is_ctty(tp, p) ||
		    p->p_pgrp == tp->t_pgrp || p->p_flag & P_PPWAIT ||
		    SIGISMEMBER(p->p_sigacts->ps_sigignore, sig) ||
		    SIGISMEMBER(td->td_sigmask, sig)) {
			/* Allow the action to happen. */
			PROC_UNLOCK(p);
			return (0);
		}

		/*
		 * Send the signal and sleep until we're the new
		 * foreground process group.
		 */
		pg = p->p_pgrp;
		PROC_UNLOCK(p);
		if (pg->pg_jobc == 0)
			return (EIO);
		PGRP_LOCK(pg);
		pgsignal(pg, sig, 1);
		PGRP_UNLOCK(pg);

		error = tty_wait(tp, &tp->t_bgwait);
		if (error)
			return (error);
	}
}

static int
ttydev_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct tty *tp = dev->si_drv1;
	int error;

	error = ttydev_enter(tp);
	if (error)
		goto done;

	error = tty_wait_background(tp, curthread, SIGTTIN);
	if (error) {
		tty_unlock(tp);
		goto done;
	}

	error = ttydisc_read(tp, uio, ioflag);
	tty_unlock(tp);

	/*
	 * The read() call should not throw an error when the device is
	 * being destroyed. Silently convert it to an EOF.
	 */
done:	if (error == ENXIO)
		error = 0;
	return (error);
}

static int
ttydev_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct tty *tp = dev->si_drv1;
	int error;

	error = ttydev_enter(tp);
	if (error)
		return (error);

	if (tp->t_termios.c_lflag & TOSTOP) {
		error = tty_wait_background(tp, curthread, SIGTTOU);
		if (error) {
			tty_unlock(tp);
			return (error);
		}
	}

	error = ttydisc_write(tp, uio, ioflag);
	tty_unlock(tp);

	return (error);
}

static int
ttydev_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct tty *tp = dev->si_drv1;
	int error;

	error = ttydev_enter(tp);
	if (error)
		return (error);

	switch (cmd) {
	case TIOCCBRK:
	case TIOCCONS:
	case TIOCDRAIN:
	case TIOCEXCL:
	case TIOCFLUSH:
	case TIOCNXCL:
	case TIOCSBRK:
	case TIOCSCTTY:
	case TIOCSETA:
	case TIOCSETAF:
	case TIOCSETAW:
	case TIOCSPGRP:
	case TIOCSTART:
	case TIOCSTAT:
	case TIOCSTOP:
	case TIOCSWINSZ:
#if 0
	case TIOCSDRAINWAIT:
	case TIOCSETD:
	case TIOCSTI:
#endif
#ifdef COMPAT_43TTY
	case  TIOCLBIC:
	case  TIOCLBIS:
	case  TIOCLSET:
	case  TIOCSETC:
	case OTIOCSETD:
	case  TIOCSETN:
	case  TIOCSETP:
	case  TIOCSLTC:
#endif /* COMPAT_43TTY */
		/*
		 * If the ioctl() causes the TTY to be modified, let it
		 * wait in the background.
		 */
		error = tty_wait_background(tp, curthread, SIGTTOU);
		if (error)
			goto done;
	}

	error = tty_ioctl(tp, cmd, data, td);
done:	tty_unlock(tp);

	return (error);
}

static int
ttydev_poll(struct cdev *dev, int events, struct thread *td)
{
	struct tty *tp = dev->si_drv1;
	int error, revents = 0;

	error = ttydev_enter(tp);
	if (error) {
		/* Don't return the error here, but the event mask. */
		return (events &
		    (POLLHUP|POLLIN|POLLRDNORM|POLLOUT|POLLWRNORM));
	}

	if (events & (POLLIN|POLLRDNORM)) {
		/* See if we can read something. */
		if (ttydisc_read_poll(tp) > 0)
			revents |= events & (POLLIN|POLLRDNORM);
	}
	if (events & (POLLOUT|POLLWRNORM)) {
		/* See if we can write something. */
		if (ttydisc_write_poll(tp) > 0)
			revents |= events & (POLLOUT|POLLWRNORM);
	}
	if (tp->t_flags & TF_ZOMBIE)
		/* Hangup flag on zombie state. */
		revents |= events & POLLHUP;

	if (revents == 0) {
		if (events & (POLLIN|POLLRDNORM))
			selrecord(td, &tp->t_inpoll);
		if (events & (POLLOUT|POLLWRNORM))
			selrecord(td, &tp->t_outpoll);
	}

	tty_unlock(tp);

	return (revents);
}

static int
ttydev_mmap(struct cdev *dev, vm_offset_t offset, vm_paddr_t *paddr, int nprot)
{
	struct tty *tp = dev->si_drv1;
	int error;

	/* Handle mmap() through the driver. */

	error = ttydev_enter(tp);
	if (error)
		return (-1);
	error = ttydevsw_mmap(tp, offset, paddr, nprot);
	tty_unlock(tp);

	return (error);
}

/*
 * kqueue support.
 */

static void
tty_kqops_read_detach(struct knote *kn)
{
	struct tty *tp = kn->kn_hook;

	knlist_remove(&tp->t_inpoll.si_note, kn, 0);
}

static int
tty_kqops_read_event(struct knote *kn, long hint)
{
	struct tty *tp = kn->kn_hook;

	tty_lock_assert(tp, MA_OWNED);

	if (tty_gone(tp) || tp->t_flags & TF_ZOMBIE) {
		kn->kn_flags |= EV_EOF;
		return (1);
	} else {
		kn->kn_data = ttydisc_read_poll(tp);
		return (kn->kn_data > 0);
	}
}

static void
tty_kqops_write_detach(struct knote *kn)
{
	struct tty *tp = kn->kn_hook;

	knlist_remove(&tp->t_outpoll.si_note, kn, 0);
}

static int
tty_kqops_write_event(struct knote *kn, long hint)
{
	struct tty *tp = kn->kn_hook;

	tty_lock_assert(tp, MA_OWNED);

	if (tty_gone(tp)) {
		kn->kn_flags |= EV_EOF;
		return (1);
	} else {
		kn->kn_data = ttydisc_write_poll(tp);
		return (kn->kn_data > 0);
	}
}

static struct filterops tty_kqops_read =
    { 1, NULL, tty_kqops_read_detach, tty_kqops_read_event };
static struct filterops tty_kqops_write =
    { 1, NULL, tty_kqops_write_detach, tty_kqops_write_event };

static int
ttydev_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct tty *tp = dev->si_drv1;
	int error;

	error = ttydev_enter(tp);
	if (error)
		return (error);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_hook = tp;
		kn->kn_fop = &tty_kqops_read;
		knlist_add(&tp->t_inpoll.si_note, kn, 1);
		break;
	case EVFILT_WRITE:
		kn->kn_hook = tp;
		kn->kn_fop = &tty_kqops_write;
		knlist_add(&tp->t_outpoll.si_note, kn, 1);
		break;
	default:
		error = EINVAL;
		break;
	}

	tty_unlock(tp);
	return (error);
}

static struct cdevsw ttydev_cdevsw = {
	.d_version	= D_VERSION,
	.d_open		= ttydev_open,
	.d_close	= ttydev_close,
	.d_read		= ttydev_read,
	.d_write	= ttydev_write,
	.d_ioctl	= ttydev_ioctl,
	.d_kqfilter	= ttydev_kqfilter,
	.d_poll		= ttydev_poll,
	.d_mmap		= ttydev_mmap,
	.d_name		= "ttydev",
	.d_flags	= D_TTY,
};

/*
 * Init/lock-state devices
 */

static int
ttyil_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct tty *tp = dev->si_drv1;
	int error = 0;

	tty_lock(tp);
	if (tty_gone(tp))
		error = ENODEV;
	tty_unlock(tp);

	return (error);
}

static int
ttyil_close(struct cdev *dev, int flag, int mode, struct thread *td)
{
	return (0);
}

static int
ttyil_rdwr(struct cdev *dev, struct uio *uio, int ioflag)
{
	return (ENODEV);
}

static int
ttyil_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct tty *tp = dev->si_drv1;
	int error = 0;

	tty_lock(tp);
	if (tty_gone(tp)) {
		error = ENODEV;
		goto done;
	}

	switch (cmd) {
	case TIOCGETA:
		/* Obtain terminal flags through tcgetattr(). */
		bcopy(dev->si_drv2, data, sizeof(struct termios));
		break;
	case TIOCSETA:
		/* Set terminal flags through tcsetattr(). */
		error = priv_check(td, PRIV_TTY_SETA);
		if (error)
			break;
		bcopy(data, dev->si_drv2, sizeof(struct termios));
		break;
	case TIOCGETD:
		*(int *)data = TTYDISC;
		break;
	case TIOCGWINSZ:
		bzero(data, sizeof(struct winsize));
		break;
	default:
		error = ENOTTY;
	}

done:	tty_unlock(tp);
	return (error);
}

static struct cdevsw ttyil_cdevsw = {
	.d_version	= D_VERSION,
	.d_open		= ttyil_open,
	.d_close	= ttyil_close,
	.d_read		= ttyil_rdwr,
	.d_write	= ttyil_rdwr,
	.d_ioctl	= ttyil_ioctl,
	.d_name		= "ttyil",
	.d_flags	= D_TTY,
};

static void
tty_init_termios(struct tty *tp)
{
	struct termios *t = &tp->t_termios_init_in;

	t->c_cflag = TTYDEF_CFLAG;
	t->c_iflag = TTYDEF_IFLAG;
	t->c_lflag = TTYDEF_LFLAG;
	t->c_oflag = TTYDEF_OFLAG;
	t->c_ispeed = TTYDEF_SPEED;
	t->c_ospeed = TTYDEF_SPEED;
	bcopy(ttydefchars, &t->c_cc, sizeof ttydefchars);

	tp->t_termios_init_out = *t;
}

void
tty_init_console(struct tty *tp, speed_t s)
{
	struct termios *ti = &tp->t_termios_init_in;
	struct termios *to = &tp->t_termios_init_out;

	if (s != 0) {
		ti->c_ispeed = ti->c_ospeed = s;
		to->c_ispeed = to->c_ospeed = s;
	}

	ti->c_cflag |= CLOCAL;
	to->c_cflag |= CLOCAL;
}

/*
 * Standard device routine implementations, mostly meant for
 * pseudo-terminal device drivers. When a driver creates a new terminal
 * device class, missing routines are patched.
 */

static int
ttydevsw_defopen(struct tty *tp)
{

	return (0);
}

static void
ttydevsw_defclose(struct tty *tp)
{
}

static void
ttydevsw_defoutwakeup(struct tty *tp)
{

	panic("Terminal device has output, while not implemented");
}

static void
ttydevsw_definwakeup(struct tty *tp)
{
}

static int
ttydevsw_defioctl(struct tty *tp, u_long cmd, caddr_t data, struct thread *td)
{

	return (ENOIOCTL);
}

static int
ttydevsw_defparam(struct tty *tp, struct termios *t)
{

	/* Use a fake baud rate, we're not a real device. */
	t->c_ispeed = t->c_ospeed = TTYDEF_SPEED_PSEUDO;

	return (0);
}

static int
ttydevsw_defmodem(struct tty *tp, int sigon, int sigoff)
{

	/* Simulate a carrier to make the TTY layer happy. */
	return (SER_DCD);
}

static int
ttydevsw_defmmap(struct tty *tp, vm_offset_t offset, vm_paddr_t *paddr,
    int nprot)
{

	return (-1);
}

static void
ttydevsw_defpktnotify(struct tty *tp, char event)
{
}

static void
ttydevsw_deffree(void *softc)
{

	panic("Terminal device freed without a free-handler");
}

/*
 * TTY allocation and deallocation. TTY devices can be deallocated when
 * the driver doesn't use it anymore, when the TTY isn't a session's
 * controlling TTY and when the device node isn't opened through devfs.
 */

struct tty *
tty_alloc(struct ttydevsw *tsw, void *sc, struct mtx *mutex)
{
	struct tty *tp;

	/* Make sure the driver defines all routines. */
#define PATCH_FUNC(x) do {				\
	if (tsw->tsw_ ## x == NULL)			\
		tsw->tsw_ ## x = ttydevsw_def ## x;	\
} while (0)
	PATCH_FUNC(open);
	PATCH_FUNC(close);
	PATCH_FUNC(outwakeup);
	PATCH_FUNC(inwakeup);
	PATCH_FUNC(ioctl);
	PATCH_FUNC(param);
	PATCH_FUNC(modem);
	PATCH_FUNC(mmap);
	PATCH_FUNC(pktnotify);
	PATCH_FUNC(free);
#undef PATCH_FUNC

	tp = malloc(sizeof(struct tty), M_TTY, M_WAITOK|M_ZERO);
	tp->t_devsw = tsw;
	tp->t_devswsoftc = sc;
	tp->t_flags = tsw->tsw_flags;

	tty_init_termios(tp);

	cv_init(&tp->t_inwait, "tty input");
	cv_init(&tp->t_outwait, "tty output");
	cv_init(&tp->t_bgwait, "tty background");
	cv_init(&tp->t_dcdwait, "tty dcd");

	ttyinq_init(&tp->t_inq);
	ttyoutq_init(&tp->t_outq);

	/* Allow drivers to use a custom mutex to lock the TTY. */
	if (mutex != NULL) {
		tp->t_mtx = mutex;
	} else {
		tp->t_mtx = &tp->t_mtxobj;
		mtx_init(&tp->t_mtxobj, "tty lock", NULL, MTX_DEF);
	}

	knlist_init(&tp->t_inpoll.si_note, tp->t_mtx, NULL, NULL, NULL);
	knlist_init(&tp->t_outpoll.si_note, tp->t_mtx, NULL, NULL, NULL);

	sx_xlock(&tty_list_sx);
	TAILQ_INSERT_TAIL(&tty_list, tp, t_list);
	tty_list_count++;
	sx_xunlock(&tty_list_sx);

	return (tp);
}

static void
tty_dealloc(void *arg)
{
	struct tty *tp = arg;

	sx_xlock(&tty_list_sx);
	TAILQ_REMOVE(&tty_list, tp, t_list);
	tty_list_count--;
	sx_xunlock(&tty_list_sx);

	/* Make sure we haven't leaked buffers. */
	MPASS(ttyinq_getsize(&tp->t_inq) == 0);
	MPASS(ttyoutq_getsize(&tp->t_outq) == 0);

	knlist_destroy(&tp->t_inpoll.si_note);
	knlist_destroy(&tp->t_outpoll.si_note);

	cv_destroy(&tp->t_inwait);
	cv_destroy(&tp->t_outwait);
	cv_destroy(&tp->t_bgwait);
	cv_destroy(&tp->t_dcdwait);

	if (tp->t_mtx == &tp->t_mtxobj)
		mtx_destroy(&tp->t_mtxobj);
	ttydevsw_free(tp);
	free(tp, M_TTY);
}

static void
tty_rel_free(struct tty *tp)
{
	struct cdev *dev;

	tty_lock_assert(tp, MA_OWNED);

#define	TF_ACTIVITY	(TF_GONE|TF_OPENED|TF_HOOK|TF_OPENCLOSE)
	if (tp->t_sessioncnt != 0 || (tp->t_flags & TF_ACTIVITY) != TF_GONE) {
		/* TTY is still in use. */
		tty_unlock(tp);
		return;
	}

	/* TTY can be deallocated. */
	dev = tp->t_dev;
	tp->t_dev = NULL;
	tty_unlock(tp);

	destroy_dev_sched_cb(dev, tty_dealloc, tp);
}

void
tty_rel_pgrp(struct tty *tp, struct pgrp *pg)
{
	MPASS(tp->t_sessioncnt > 0);
	tty_lock_assert(tp, MA_OWNED);

	if (tp->t_pgrp == pg)
		tp->t_pgrp = NULL;
	
	tty_unlock(tp);
}

void
tty_rel_sess(struct tty *tp, struct session *sess)
{
	MPASS(tp->t_sessioncnt > 0);

	/* Current session has left. */
	if (tp->t_session == sess) {
		tp->t_session = NULL;
		MPASS(tp->t_pgrp == NULL);
	}
	tp->t_sessioncnt--;
	tty_rel_free(tp);
}

void
tty_rel_gone(struct tty *tp)
{
	MPASS(!tty_gone(tp));

	/* Simulate carrier removal. */
	ttydisc_modem(tp, 0);

	/* Wake up all blocked threads. */
	tty_wakeup(tp, FREAD|FWRITE);
	cv_broadcast(&tp->t_bgwait);
	cv_broadcast(&tp->t_dcdwait);

	tp->t_flags |= TF_GONE;
	tty_rel_free(tp);
}

/*
 * Exposing information about current TTY's through sysctl
 */

static void
tty_to_xtty(struct tty *tp, struct xtty *xt)
{
	tty_lock_assert(tp, MA_OWNED);

	xt->xt_size = sizeof(struct xtty);
	xt->xt_insize = ttyinq_getsize(&tp->t_inq);
	xt->xt_incc = ttyinq_bytescanonicalized(&tp->t_inq);
	xt->xt_inlc = ttyinq_bytesline(&tp->t_inq);
	xt->xt_inlow = tp->t_inlow;
	xt->xt_outsize = ttyoutq_getsize(&tp->t_outq);
	xt->xt_outcc = ttyoutq_bytesused(&tp->t_outq);
	xt->xt_outlow = tp->t_outlow;
	xt->xt_column = tp->t_column;
	xt->xt_pgid = tp->t_pgrp ? tp->t_pgrp->pg_id : 0;
	xt->xt_sid = tp->t_session ? tp->t_session->s_sid : 0;
	xt->xt_flags = tp->t_flags;
	xt->xt_dev = tp->t_dev ? dev2udev(tp->t_dev) : NODEV;
}

static int
sysctl_kern_ttys(SYSCTL_HANDLER_ARGS)
{
	unsigned long lsize;
	struct xtty *xtlist, *xt;
	struct tty *tp;
	int error;

	sx_slock(&tty_list_sx);
	lsize = tty_list_count * sizeof(struct xtty);
	if (lsize == 0) {
		sx_sunlock(&tty_list_sx);
		return (0);
	}

	xtlist = xt = malloc(lsize, M_TEMP, M_WAITOK);

	TAILQ_FOREACH(tp, &tty_list, t_list) {
		tty_lock(tp);
		tty_to_xtty(tp, xt);
		tty_unlock(tp);
		xt++;
	}
	sx_sunlock(&tty_list_sx);

	error = SYSCTL_OUT(req, xtlist, lsize);
	free(xtlist, M_TEMP);
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, ttys, CTLTYPE_OPAQUE|CTLFLAG_RD,
	0, 0, sysctl_kern_ttys, "S,xtty", "List of TTYs");

/*
 * Device node creation. Device has been set up, now we can expose it to
 * the user.
 */

void
tty_makedev(struct tty *tp, struct ucred *cred, const char *fmt, ...)
{
	va_list ap;
	struct cdev *dev;
	const char *prefix = "tty";
	char name[SPECNAMELEN - 3]; /* for "tty" and "cua". */
	uid_t uid;
	gid_t gid;
	mode_t mode;

	/* Remove "tty" prefix from devices like PTY's. */
	if (tp->t_flags & TF_NOPREFIX)
		prefix = "";

	va_start(ap, fmt);
	vsnrprintf(name, sizeof name, 32, fmt, ap);
	va_end(ap);

	if (cred == NULL) {
		/* System device. */
		uid = UID_ROOT;
		gid = GID_WHEEL;
		mode = S_IRUSR|S_IWUSR;
	} else {
		/* User device. */
		uid = cred->cr_ruid;
		gid = GID_TTY;
		mode = S_IRUSR|S_IWUSR|S_IWGRP;
	}

	/* Master call-in device. */
	dev = make_dev_cred(&ttydev_cdevsw, 0, cred,
	    uid, gid, mode, "%s%s", prefix, name);
	dev->si_drv1 = tp;
	tp->t_dev = dev;

	/* Slave call-in devices. */
	if (tp->t_flags & TF_INITLOCK) {
		dev = make_dev_cred(&ttyil_cdevsw, 0, cred,
		    uid, gid, mode, "%s%s.init", prefix, name);
		dev_depends(tp->t_dev, dev);
		dev->si_drv1 = tp;
		dev->si_drv2 = &tp->t_termios_init_in;

		dev = make_dev_cred(&ttyil_cdevsw, 0, cred,
		    uid, gid, mode, "%s%s.lock", prefix, name);
		dev_depends(tp->t_dev, dev);
		dev->si_drv1 = tp;
		dev->si_drv2 = &tp->t_termios_lock_in;
	}

	/* Call-out devices. */
	if (tp->t_flags & TF_CALLOUT) {
		dev = make_dev_cred(&ttydev_cdevsw, 0, cred,
		    UID_UUCP, GID_DIALER, 0660, "cua%s", name);
		dev_depends(tp->t_dev, dev);
		dev->si_drv1 = tp;

		/* Slave call-out devices. */
		if (tp->t_flags & TF_INITLOCK) {
			dev = make_dev_cred(&ttyil_cdevsw, 0, cred,
			    UID_UUCP, GID_DIALER, 0660, "cua%s.init", name);
			dev_depends(tp->t_dev, dev);
			dev->si_drv1 = tp;
			dev->si_drv2 = &tp->t_termios_init_out;

			dev = make_dev_cred(&ttyil_cdevsw, 0, cred,
			    UID_UUCP, GID_DIALER, 0660, "cua%s.lock", name);
			dev_depends(tp->t_dev, dev);
			dev->si_drv1 = tp;
			dev->si_drv2 = &tp->t_termios_lock_out;
		}
	}
}

/*
 * Signalling processes.
 */

void
tty_signal_sessleader(struct tty *tp, int sig)
{
	struct proc *p;

	tty_lock_assert(tp, MA_OWNED);
	MPASS(sig >= 1 && sig < NSIG);

	/* Make signals start output again. */
	tp->t_flags &= ~TF_STOPPED;
	
	if (tp->t_session != NULL && tp->t_session->s_leader != NULL) {
		p = tp->t_session->s_leader;
		PROC_LOCK(p);
		psignal(p, sig);
		PROC_UNLOCK(p);
	}
}

void
tty_signal_pgrp(struct tty *tp, int sig)
{
	tty_lock_assert(tp, MA_OWNED);
	MPASS(sig >= 1 && sig < NSIG);

	/* Make signals start output again. */
	tp->t_flags &= ~TF_STOPPED;

	if (sig == SIGINFO && !(tp->t_termios.c_lflag & NOKERNINFO))
		tty_info(tp);
	if (tp->t_pgrp != NULL) {
		PGRP_LOCK(tp->t_pgrp);
		pgsignal(tp->t_pgrp, sig, 1);
		PGRP_UNLOCK(tp->t_pgrp);
	}
}

void
tty_wakeup(struct tty *tp, int flags)
{
	if (tp->t_flags & TF_ASYNC && tp->t_sigio != NULL)
		pgsigio(&tp->t_sigio, SIGIO, (tp->t_session != NULL));

	if (flags & FWRITE) {
		cv_broadcast(&tp->t_outwait);
		selwakeup(&tp->t_outpoll);
		KNOTE_LOCKED(&tp->t_outpoll.si_note, 0);
	}
	if (flags & FREAD) {
		cv_broadcast(&tp->t_inwait);
		selwakeup(&tp->t_inpoll);
		KNOTE_LOCKED(&tp->t_inpoll.si_note, 0);
	}
}

int
tty_wait(struct tty *tp, struct cv *cv)
{
	int error;
	int revokecnt = tp->t_revokecnt;

#if 0
	/* XXX: /dev/console also picks up Giant. */
	tty_lock_assert(tp, MA_OWNED|MA_NOTRECURSED);
#endif
	tty_lock_assert(tp, MA_OWNED);
	MPASS(!tty_gone(tp));

	error = cv_wait_sig(cv, tp->t_mtx);

	/* Restart the system call when we may have been revoked. */
	if (tp->t_revokecnt != revokecnt)
		return (ERESTART);
	
	/* Bail out when the device slipped away. */
	if (tty_gone(tp))
		return (ENXIO);

	return (error);
}

int
tty_timedwait(struct tty *tp, struct cv *cv, int hz)
{
	int error;
	int revokecnt = tp->t_revokecnt;

#if 0
	/* XXX: /dev/console also picks up Giant. */
	tty_lock_assert(tp, MA_OWNED|MA_NOTRECURSED);
#endif
	tty_lock_assert(tp, MA_OWNED);
	MPASS(!tty_gone(tp));

	error = cv_timedwait_sig(cv, tp->t_mtx, hz);

	/* Restart the system call when we may have been revoked. */
	if (tp->t_revokecnt != revokecnt)
		return (ERESTART);
	
	/* Bail out when the device slipped away. */
	if (tty_gone(tp))
		return (ENXIO);

	return (error);
}

void
tty_flush(struct tty *tp, int flags)
{
	if (flags & FWRITE) {
		tp->t_flags &= ~TF_HIWAT_OUT;
		ttyoutq_flush(&tp->t_outq);
		tty_wakeup(tp, FWRITE);
		ttydevsw_pktnotify(tp, TIOCPKT_FLUSHWRITE);
	}
	if (flags & FREAD) {
		tty_hiwat_in_unblock(tp);
		ttyinq_flush(&tp->t_inq);
		ttydevsw_inwakeup(tp);
		ttydevsw_pktnotify(tp, TIOCPKT_FLUSHREAD);
	}
}

static int
tty_generic_ioctl(struct tty *tp, u_long cmd, void *data, struct thread *td)
{
	int error;

	switch (cmd) {
	/*
	 * Modem commands.
	 * The SER_* and TIOCM_* flags are the same, but one bit
	 * shifted. I don't know why.
	 */
	case TIOCSDTR:
		ttydevsw_modem(tp, SER_DTR, 0);
		return (0);
	case TIOCCDTR:
		ttydevsw_modem(tp, 0, SER_DTR);
		return (0);
	case TIOCMSET: {
		int bits = *(int *)data;
		ttydevsw_modem(tp,
		    (bits & (TIOCM_DTR | TIOCM_RTS)) >> 1,
		    ((~bits) & (TIOCM_DTR | TIOCM_RTS)) >> 1);
		return (0);
	}
	case TIOCMBIS: {
		int bits = *(int *)data;
		ttydevsw_modem(tp, (bits & (TIOCM_DTR | TIOCM_RTS)) >> 1, 0);
		return (0);
	}
	case TIOCMBIC: {
		int bits = *(int *)data;
		ttydevsw_modem(tp, 0, (bits & (TIOCM_DTR | TIOCM_RTS)) >> 1);
		return (0);
	}
	case TIOCMGET:
		*(int *)data = TIOCM_LE + (ttydevsw_modem(tp, 0, 0) << 1);
		return (0);

	case FIOASYNC:
		if (*(int *)data)
			tp->t_flags |= TF_ASYNC;
		else
			tp->t_flags &= ~TF_ASYNC;
		return (0);
	case FIONBIO:
		/* This device supports non-blocking operation. */
		return (0);
	case FIONREAD:
		*(int *)data = ttyinq_bytescanonicalized(&tp->t_inq);
		return (0);
	case FIOSETOWN:
		if (tp->t_session != NULL && !tty_is_ctty(tp, td->td_proc))
			/* Not allowed to set ownership. */
			return (ENOTTY);

		/* Temporarily unlock the TTY to set ownership. */
		tty_unlock(tp);
		error = fsetown(*(int *)data, &tp->t_sigio);
		tty_lock(tp);
		return (error);
	case FIOGETOWN:
		if (tp->t_session != NULL && !tty_is_ctty(tp, td->td_proc))
			/* Not allowed to set ownership. */
			return (ENOTTY);

		/* Get ownership. */
		*(int *)data = fgetown(&tp->t_sigio);
		return (0);
	case TIOCGETA:
		/* Obtain terminal flags through tcgetattr(). */
		bcopy(&tp->t_termios, data, sizeof(struct termios));
		return (0);
	case TIOCSETA:
	case TIOCSETAW:
	case TIOCSETAF: {
		struct termios *t = data;

		/*
		 * Who makes up these funny rules? According to POSIX,
		 * input baud rate is set equal to the output baud rate
		 * when zero.
		 */
		if (t->c_ispeed == 0)
			t->c_ispeed = t->c_ospeed;

		/* Discard any unsupported bits. */
		t->c_iflag &= TTYSUP_IFLAG;
		t->c_oflag &= TTYSUP_OFLAG;
		t->c_lflag &= TTYSUP_LFLAG;
		t->c_cflag &= TTYSUP_CFLAG;

		/* Set terminal flags through tcsetattr(). */
		if (cmd == TIOCSETAW || cmd == TIOCSETAF) {
			error = tty_drain(tp);
			if (error)
				return (error);
			if (cmd == TIOCSETAF)
				tty_flush(tp, FREAD);
		}

		/*
		 * Only call param() when the flags really change.
		 */
		if ((t->c_cflag & CIGNORE) == 0 &&
		    (tp->t_termios.c_cflag != t->c_cflag ||
		    tp->t_termios.c_ispeed != t->c_ispeed ||
		    tp->t_termios.c_ospeed != t->c_ospeed)) {
			error = ttydevsw_param(tp, t);
			if (error)
				return (error);

			/* XXX: CLOCAL? */
			
			tp->t_termios.c_cflag = t->c_cflag & ~CIGNORE;
			tp->t_termios.c_ispeed = t->c_ispeed;
			tp->t_termios.c_ospeed = t->c_ospeed;

			/* Baud rate has changed - update watermarks. */
			tty_watermarks(tp);
		}

		/* Copy new non-device driver parameters. */
		tp->t_termios.c_iflag = t->c_iflag;
		tp->t_termios.c_oflag = t->c_oflag;
		tp->t_termios.c_lflag = t->c_lflag;
		bcopy(t->c_cc, &tp->t_termios.c_cc, sizeof(t->c_cc));

		ttydisc_optimize(tp);

		if ((t->c_lflag & ICANON) == 0) {
			/*
			 * When in non-canonical mode, wake up all
			 * readers. Canonicalize any partial input. VMIN
			 * and VTIME could also be adjusted.
			 */
			ttyinq_canonicalize(&tp->t_inq);
			tty_wakeup(tp, FREAD);
		}

		/*
		 * For packet mode: notify the PTY consumer that VSTOP
		 * and VSTART may have been changed.
		 */
		if (tp->t_termios.c_iflag & IXON &&
		    tp->t_termios.c_cc[VSTOP] == CTRL('S') &&
		    tp->t_termios.c_cc[VSTART] == CTRL('Q'))
			ttydevsw_pktnotify(tp, TIOCPKT_DOSTOP);
		else
			ttydevsw_pktnotify(tp, TIOCPKT_NOSTOP);
		return (0);
	}
	case TIOCGETD:
		/* For compatibility - we only support TTYDISC. */
		*(int *)data = TTYDISC;
		return (0);
	case TIOCGPGRP:
		if (!tty_is_ctty(tp, td->td_proc))
			return (ENOTTY);

		if (tp->t_pgrp != NULL)
			*(int *)data = tp->t_pgrp->pg_id;
		else
			*(int *)data = NO_PID;
		return (0);
	case TIOCGSID:
		if (!tty_is_ctty(tp, td->td_proc))
			return (ENOTTY);

		MPASS(tp->t_session);
		*(int *)data = tp->t_session->s_sid;
		return (0);
	case TIOCSCTTY: {
		struct proc *p = td->td_proc;

		/* XXX: This looks awful. */
		tty_unlock(tp);
		sx_xlock(&proctree_lock);
		tty_lock(tp);

		if (!SESS_LEADER(p)) {
			/* Only the session leader may do this. */
			sx_xunlock(&proctree_lock);
			return (EPERM);
		}

		if (tp->t_session != NULL && tp->t_session == p->p_session) {
			/* This is already our controlling TTY. */
			sx_xunlock(&proctree_lock);
			return (0);
		}

		if (!SESS_LEADER(p) || p->p_session->s_ttyvp != NULL ||
		    (tp->t_session != NULL && tp->t_session->s_ttyvp != NULL)) {
			/*
			 * There is already a relation between a TTY and
			 * a session, or the caller is not the session
			 * leader.
			 *
			 * Allow the TTY to be stolen when the vnode is
			 * NULL, but the reference to the TTY is still
			 * active.
			 */
			sx_xunlock(&proctree_lock);
			return (EPERM);
		}

		/* Connect the session to the TTY. */
		tp->t_session = p->p_session;
		tp->t_session->s_ttyp = tp;
		tp->t_sessioncnt++;
		sx_xunlock(&proctree_lock);

		/* Assign foreground process group. */
		tp->t_pgrp = p->p_pgrp;
		PROC_LOCK(p);
		p->p_flag |= P_CONTROLT;
		PROC_UNLOCK(p);

		return (0);
	}
	case TIOCSPGRP: {
		struct pgrp *pg;

		/*
		 * XXX: Temporarily unlock the TTY to locate the process
		 * group. This code would be lot nicer if we would ever
		 * decompose proctree_lock.
		 */
		tty_unlock(tp);
		sx_slock(&proctree_lock);
		pg = pgfind(*(int *)data);
		if (pg != NULL)
			PGRP_UNLOCK(pg);
		if (pg == NULL || pg->pg_session != td->td_proc->p_session) {
			sx_sunlock(&proctree_lock);
			tty_lock(tp);
			return (EPERM);
		}
		tty_lock(tp);

		/*
		 * Determine if this TTY is the controlling TTY after
		 * relocking the TTY.
		 */
		if (!tty_is_ctty(tp, td->td_proc)) {
			sx_sunlock(&proctree_lock);
			return (ENOTTY);
		}
		tp->t_pgrp = pg;
		sx_sunlock(&proctree_lock);

		/* Wake up the background process groups. */
		cv_broadcast(&tp->t_bgwait);
		return (0);
	}
	case TIOCFLUSH: {
		int flags = *(int *)data;

		if (flags == 0)
			flags = (FREAD|FWRITE);
		else
			flags &= (FREAD|FWRITE);
		tty_flush(tp, flags);
		return (0);
	}
	case TIOCDRAIN:
		/* Drain TTY output. */
		return tty_drain(tp);
	case TIOCCONS:
		/* Set terminal as console TTY. */
		if (*(int *)data) {
			error = priv_check(td, PRIV_TTY_CONSOLE);
			if (error)
				return (error);

			/*
			 * XXX: constty should really need to be locked!
			 * XXX: allow disconnected constty's to be stolen!
			 */

			if (constty == tp)
				return (0);
			if (constty != NULL)
				return (EBUSY);

			tty_unlock(tp);
			constty_set(tp);
			tty_lock(tp);
		} else if (constty == tp) {
			constty_clear();
		}
		return (0);
	case TIOCGWINSZ:
		/* Obtain window size. */
		bcopy(&tp->t_winsize, data, sizeof(struct winsize));
		return (0);
	case TIOCSWINSZ:
		/* Set window size. */
		if (bcmp(&tp->t_winsize, data, sizeof(struct winsize)) == 0)
			return (0);
		bcopy(data, &tp->t_winsize, sizeof(struct winsize));
		tty_signal_pgrp(tp, SIGWINCH);
		return (0);
	case TIOCEXCL:
		tp->t_flags |= TF_EXCLUDE;
		return (0);
	case TIOCNXCL:
		tp->t_flags &= ~TF_EXCLUDE;
		return (0);
	case TIOCOUTQ:
		*(unsigned int *)data = ttyoutq_bytesused(&tp->t_outq);
		return (0);
	case TIOCSTOP:
		tp->t_flags |= TF_STOPPED;
		ttydevsw_pktnotify(tp, TIOCPKT_STOP);
		return (0);
	case TIOCSTART:
		tp->t_flags &= ~TF_STOPPED;
		ttydevsw_outwakeup(tp);
		ttydevsw_pktnotify(tp, TIOCPKT_START);
		return (0);
	case TIOCSTAT:
		tty_info(tp);
		return (0);
	}

#ifdef COMPAT_43TTY
	return tty_ioctl_compat(tp, cmd, data, td);
#else /* !COMPAT_43TTY */
	return (ENOIOCTL);
#endif /* COMPAT_43TTY */
}

int
tty_ioctl(struct tty *tp, u_long cmd, void *data, struct thread *td)
{
	int error;

	tty_lock_assert(tp, MA_OWNED);

	if (tty_gone(tp))
		return (ENXIO);
	
	error = ttydevsw_ioctl(tp, cmd, data, td);
	if (error == ENOIOCTL)
		error = tty_generic_ioctl(tp, cmd, data, td);

	return (error);
}

dev_t
tty_udev(struct tty *tp)
{
	if (tp->t_dev)
		return dev2udev(tp->t_dev);
	else
		return NODEV;
}

int
tty_checkoutq(struct tty *tp)
{

	/* 256 bytes should be enough to print a log message. */
	return (ttyoutq_bytesleft(&tp->t_outq) >= 256);
}

void
tty_hiwat_in_block(struct tty *tp)
{

	if ((tp->t_flags & TF_HIWAT_IN) == 0 &&
	    tp->t_termios.c_iflag & IXOFF &&
	    tp->t_termios.c_cc[VSTOP] != _POSIX_VDISABLE) {
		/*
		 * Input flow control. Only enter the high watermark when we
		 * can successfully store the VSTOP character.
		 */
		if (ttyoutq_write_nofrag(&tp->t_outq,
		    &tp->t_termios.c_cc[VSTOP], 1) == 0)
			tp->t_flags |= TF_HIWAT_IN;
	} else {
		/* No input flow control. */
		tp->t_flags |= TF_HIWAT_IN;
	}
}

void
tty_hiwat_in_unblock(struct tty *tp)
{

	if (tp->t_flags & TF_HIWAT_IN &&
	    tp->t_termios.c_iflag & IXOFF &&
	    tp->t_termios.c_cc[VSTART] != _POSIX_VDISABLE) {
		/*
		 * Input flow control. Only leave the high watermark when we
		 * can successfully store the VSTART character.
		 */
		if (ttyoutq_write_nofrag(&tp->t_outq,
		    &tp->t_termios.c_cc[VSTART], 1) == 0)
			tp->t_flags &= ~TF_HIWAT_IN;
	} else {
		/* No input flow control. */
		tp->t_flags &= ~TF_HIWAT_IN;
	}

	if (!tty_gone(tp))
		ttydevsw_inwakeup(tp);
}

static int
ttyhook_defrint(struct tty *tp, char c, int flags)
{

	if (ttyhook_rint_bypass(tp, &c, 1) != 1)
		return (-1);
	
	return (0);
}

int
ttyhook_register(struct tty **rtp, struct thread *td, int fd,
    struct ttyhook *th, void *softc)
{
	struct tty *tp;
	struct file *fp;
	struct cdev *dev;
	struct cdevsw *cdp;
	int error;

	/* Validate the file descriptor. */
	if (fget(td, fd, &fp) != 0)
		return (EINVAL);
	
	/* Make sure the vnode is bound to a character device. */
	error = EINVAL;
	if (fp->f_type != DTYPE_VNODE || fp->f_vnode->v_type != VCHR ||
	    fp->f_vnode->v_rdev == NULL)
		goto done1;
	dev = fp->f_vnode->v_rdev;

	/* Make sure it is a TTY. */
	cdp = dev_refthread(dev);
	if (cdp == NULL)
		goto done1;
	if (cdp != &ttydev_cdevsw)
		goto done2;
	tp = dev->si_drv1;

	/* Try to attach the hook to the TTY. */
	error = EBUSY;
	tty_lock(tp);
	MPASS((tp->t_hook == NULL) == ((tp->t_flags & TF_HOOK) == 0));
	if (tp->t_flags & TF_HOOK)
		goto done3;

	tp->t_flags |= TF_HOOK;
	tp->t_hook = th;
	tp->t_hooksoftc = softc;
	*rtp = tp;
	error = 0;

	/* Maybe we can switch into bypass mode now. */
	ttydisc_optimize(tp);

	/* Silently convert rint() calls to rint_bypass() when possible. */
	if (!ttyhook_hashook(tp, rint) && ttyhook_hashook(tp, rint_bypass))
		th->th_rint = ttyhook_defrint;

done3:	tty_unlock(tp);
done2:	dev_relthread(dev);
done1:	fdrop(fp, td);
	return (error);
}

void
ttyhook_unregister(struct tty *tp)
{

	tty_lock_assert(tp, MA_OWNED);
	MPASS(tp->t_flags & TF_HOOK);

	/* Disconnect the hook. */
	tp->t_flags &= ~TF_HOOK;
	tp->t_hook = NULL;

	/* Maybe we need to leave bypass mode. */
	ttydisc_optimize(tp);

	/* Maybe deallocate the TTY as well. */
	tty_rel_free(tp);
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_sym.h>

static struct {
	int flag;
	char val;
} ttystates[] = {
#if 0
	{ TF_NOPREFIX,	'N' },
#endif
	{ TF_INITLOCK,	'I' },
	{ TF_CALLOUT,	'C' },

	/* Keep these together -> 'Oi' and 'Oo'. */
	{ TF_OPENED,	'O' },
	{ TF_OPENED_IN,	'i' },
	{ TF_OPENED_OUT,'o' },

	{ TF_GONE,	'G' },
	{ TF_OPENCLOSE,	'B' },
	{ TF_ASYNC,	'Y' },
	{ TF_LITERAL,	'L' },

	/* Keep these together -> 'Hi' and 'Ho'. */
	{ TF_HIWAT,	'H' },
	{ TF_HIWAT_IN,	'i' },
	{ TF_HIWAT_OUT,	'o' },

	{ TF_STOPPED,	'S' },
	{ TF_EXCLUDE,	'X' },
	{ TF_BYPASS,	'l' },
	{ TF_ZOMBIE,	'Z' },
	{ TF_HOOK,	's' },

	{ 0,	       '\0' },
};

#define	TTY_FLAG_BITS \
	"\20\1NOPREFIX\2INITLOCK\3CALLOUT\4OPENED_IN\5OPENED_OUT\6GONE" \
	"\7OPENCLOSE\10ASYNC\11LITERAL\12HIWAT_IN\13HIWAT_OUT\14STOPPED" \
	"\15EXCLUDE\16BYPASS\17ZOMBIE\20HOOK"

#define DB_PRINTSYM(name, addr) \
	db_printf("%s  " #name ": ", sep); \
	db_printsym((db_addr_t) addr, DB_STGY_ANY); \
	db_printf("\n");

static void
_db_show_devsw(const char *sep, const struct ttydevsw *tsw)
{
	db_printf("%sdevsw: ", sep);
	db_printsym((db_addr_t)tsw, DB_STGY_ANY);
	db_printf(" (%p)\n", tsw);
	DB_PRINTSYM(open, tsw->tsw_open);
	DB_PRINTSYM(close, tsw->tsw_close);
	DB_PRINTSYM(outwakeup, tsw->tsw_outwakeup);
	DB_PRINTSYM(inwakeup, tsw->tsw_inwakeup);
	DB_PRINTSYM(ioctl, tsw->tsw_ioctl);
	DB_PRINTSYM(param, tsw->tsw_param);
	DB_PRINTSYM(modem, tsw->tsw_modem);
	DB_PRINTSYM(mmap, tsw->tsw_mmap);
	DB_PRINTSYM(pktnotify, tsw->tsw_pktnotify);
	DB_PRINTSYM(free, tsw->tsw_free);
}
static void
_db_show_hooks(const char *sep, const struct ttyhook *th)
{
	db_printf("%shook: ", sep);
	db_printsym((db_addr_t)th, DB_STGY_ANY);
	db_printf(" (%p)\n", th);
	if (th == NULL)
		return;
	DB_PRINTSYM(rint, th->th_rint);
	DB_PRINTSYM(rint_bypass, th->th_rint_bypass);
	DB_PRINTSYM(rint_done, th->th_rint_done);
	DB_PRINTSYM(rint_poll, th->th_rint_poll);
	DB_PRINTSYM(getc_inject, th->th_getc_inject);
	DB_PRINTSYM(getc_capture, th->th_getc_capture);
	DB_PRINTSYM(getc_poll, th->th_getc_poll);
	DB_PRINTSYM(close, th->th_close);
}

static void
_db_show_termios(const char *name, const struct termios *t)
{

	db_printf("%s: iflag 0x%x oflag 0x%x cflag 0x%x "
	    "lflag 0x%x ispeed %u ospeed %u\n", name,
	    t->c_iflag, t->c_oflag, t->c_cflag, t->c_lflag,
	    t->c_ispeed, t->c_ospeed);
}

/* DDB command to show TTY statistics. */
DB_SHOW_COMMAND(tty, db_show_tty)
{
	struct tty *tp;

	if (!have_addr) {
		db_printf("usage: show tty <addr>\n");
		return;
	}
	tp = (struct tty *)addr;

	db_printf("0x%p: %s\n", tp, tty_devname(tp));
	db_printf("\tmtx: %p\n", tp->t_mtx);
	db_printf("\tflags: %b\n", tp->t_flags, TTY_FLAG_BITS);
	db_printf("\trevokecnt: %u\n", tp->t_revokecnt);

	/* Buffering mechanisms. */
	db_printf("\tinq: %p begin %u linestart %u reprint %u end %u "
	    "nblocks %u quota %u\n", &tp->t_inq, tp->t_inq.ti_begin,
	    tp->t_inq.ti_linestart, tp->t_inq.ti_reprint, tp->t_inq.ti_end,
	    tp->t_inq.ti_nblocks, tp->t_inq.ti_quota);
	db_printf("\toutq: %p begin %u end %u nblocks %u quota %u\n",
	    &tp->t_outq, tp->t_outq.to_begin, tp->t_outq.to_end,
	    tp->t_outq.to_nblocks, tp->t_outq.to_quota);
	db_printf("\tinlow: %zu\n", tp->t_inlow);
	db_printf("\toutlow: %zu\n", tp->t_outlow);
	_db_show_termios("\ttermios", &tp->t_termios);
	db_printf("\twinsize: row %u col %u xpixel %u ypixel %u\n",
	    tp->t_winsize.ws_row, tp->t_winsize.ws_col,
	    tp->t_winsize.ws_xpixel, tp->t_winsize.ws_ypixel);
	db_printf("\tcolumn: %u\n", tp->t_column);
	db_printf("\twritepos: %u\n", tp->t_writepos);
	db_printf("\tcompatflags: 0x%x\n", tp->t_compatflags);

	/* Init/lock-state devices. */
	_db_show_termios("\ttermios_init_in", &tp->t_termios_init_in);
	_db_show_termios("\ttermios_init_out", &tp->t_termios_init_out);
	_db_show_termios("\ttermios_lock_in", &tp->t_termios_lock_in);
	_db_show_termios("\ttermios_lock_out", &tp->t_termios_lock_out);

	/* Hooks */
	_db_show_devsw("\t", tp->t_devsw);
	_db_show_hooks("\t", tp->t_hook);

	/* Process info. */
	db_printf("\tpgrp: %p gid %d jobc %d\n", tp->t_pgrp,
	    tp->t_pgrp ? tp->t_pgrp->pg_id : 0,
	    tp->t_pgrp ? tp->t_pgrp->pg_jobc : 0);
	db_printf("\tsession: %p", tp->t_session);
	if (tp->t_session != NULL)
	    db_printf(" count %u leader %p tty %p sid %d login %s",
		tp->t_session->s_count, tp->t_session->s_leader,
		tp->t_session->s_ttyp, tp->t_session->s_sid,
		tp->t_session->s_login);
	db_printf("\n");
	db_printf("\tsessioncnt: %u\n", tp->t_sessioncnt);
	db_printf("\tdevswsoftc: %p\n", tp->t_devswsoftc);
	db_printf("\thooksoftc: %p\n", tp->t_hooksoftc);
	db_printf("\tdev: %p\n", tp->t_dev);
}

/* DDB command to list TTYs. */
DB_SHOW_ALL_COMMAND(ttys, db_show_all_ttys)
{
	struct tty *tp;
	size_t isiz, osiz;
	int i, j;

	/* Make the output look like `pstat -t'. */
	db_printf("PTR        ");
#if defined(__LP64__)
	db_printf("        ");
#endif
	db_printf("      LINE   INQ  CAN  LIN  LOW  OUTQ  USE  LOW   "
	    "COL  SESS  PGID STATE\n");

	TAILQ_FOREACH(tp, &tty_list, t_list) {
		isiz = tp->t_inq.ti_nblocks * TTYINQ_DATASIZE;
		osiz = tp->t_outq.to_nblocks * TTYOUTQ_DATASIZE;

		db_printf("%p %10s %5zu %4u %4u %4zu %5zu %4u %4zu %5u %5d %5d ",
		    tp,
		    tty_devname(tp),
		    isiz,
		    tp->t_inq.ti_linestart - tp->t_inq.ti_begin,
		    tp->t_inq.ti_end - tp->t_inq.ti_linestart,
		    isiz - tp->t_inlow,
		    osiz,
		    tp->t_outq.to_end - tp->t_outq.to_begin,
		    osiz - tp->t_outlow,
		    tp->t_column,
		    tp->t_session ? tp->t_session->s_sid : 0,
		    tp->t_pgrp ? tp->t_pgrp->pg_id : 0);

		/* Flag bits. */
		for (i = j = 0; ttystates[i].flag; i++)
			if (tp->t_flags & ttystates[i].flag) {
				db_printf("%c", ttystates[i].val);
				j++;
			}
		if (j == 0)
			db_printf("-");
		db_printf("\n");
	}
}
#endif /* DDB */
