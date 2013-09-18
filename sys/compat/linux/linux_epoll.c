/*-
 * Copyright (c) 2007 Roman Divacky
 * All rights reserved.
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
#include "opt_ktrace.h"

#include <sys/limits.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/capability.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/syscallsubr.h>
#include <sys/timespec.h>
#include <compat/linux/linux_epoll.h>
#include <compat/linux/linux_util.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#define ktrepoll_events(evt, count) \
	ktrstruct("linux_epoll_event", (evt), count * sizeof(*evt))

/*
 * epoll defines 'struct epoll_event' with the field 'data' as 64 bits
 * on all architectures. But on 32 bit architectures BSD 'struct kevent' only
 * has 32 bit opaque pointer as 'udata' field. So we can't pass epoll supplied
 * data verbatuim. Therefore on 32 bit architectures we allocate 64-bit memory
 * block to pass user supplied data for every file descriptor.
 */
typedef	uint64_t	epoll_udata_t;
#if defined(__i386__)
#define EPOLL_WIDE_USER_DATA	1
#else
#define EPOLL_WIDE_USER_DATA	0
#endif

#if EPOLL_WIDE_USER_DATA

/*
 * Approach similar to epoll_user_data could also be used to
 * keep track of event bits per file descriptor for all architectures.
 * However, it isn't obvious that such tracking would be beneficial
 * in practice.
 */

struct epoll_user_data {
	unsigned	sz;
	epoll_udata_t	data[1];
};
static MALLOC_DEFINE(M_LINUX_EPOLL, "epoll", "memory for epoll system");
#define	EPOLL_USER_DATA_SIZE(ndata) \
	(sizeof(struct epoll_user_data)+((ndata)-1)*sizeof(epoll_udata_t))
#define	EPOLL_USER_DATA_MARGIN	16

static void epoll_init_user_data(struct thread *td, struct file *epfp);
static void epoll_set_user_data(struct thread *td, struct file *epfp, int fd, epoll_udata_t user_data);
static epoll_udata_t epoll_get_user_data(struct thread *td, struct file *epfp, int fd);
static fo_close_t epoll_close;

/* overload kqueue fileops */
static struct fileops epollops = {
	.fo_read =	kqueue_read,
	.fo_write =	kqueue_write,
	.fo_truncate =	kqueue_truncate,
	.fo_ioctl =	kqueue_ioctl,
	.fo_poll =	kqueue_poll,
	.fo_kqfilter =	kqueue_kqfilter,
	.fo_stat =	kqueue_stat,
	.fo_close =	epoll_close,
	.fo_chmod =	invfo_chmod,
	.fo_chown =	invfo_chown,
	.fo_sendfile =	invfo_sendfile,
};
#endif

static struct file* epoll_fget(struct thread *td, int epfd);

struct epoll_copyin_args {
	struct kevent	*changelist;
};

struct epoll_copyout_args {
	struct linux_epoll_event	*leventlist;
	int				count;
	int				error;
#if KTRACE || EPOLL_WIDE_USER_DATA
	struct thread 			*td;
#endif
#if EPOLL_WIDE_USER_DATA
	struct file			*epfp;
#endif
};


/* Create a new epoll file descriptor. */

static int
linux_epoll_create_common(struct thread *td)
{
	struct file *fp;
	int error;

	error = kern_kqueue_locked(td, &fp);
#if EPOLL_WIDE_USER_DATA
	if (error == 0) {
		epoll_init_user_data(td, fp);
		fdrop(fp, td);
	}
#endif
	return (error);
}

int
linux_epoll_create(struct thread *td, struct linux_epoll_create_args *args)
{
	if (args->size <= 0)
		return (EINVAL);
	/* args->size is unused. Linux just tests it
	 * and then forgets it as well. */

	return (linux_epoll_create_common(td));
}

int
linux_epoll_create1(struct thread *td, struct linux_epoll_create1_args *args)
{
	int error;

	error = linux_epoll_create_common(td);

	if (!error) {
		if (args->flags & LINUX_EPOLL_CLOEXEC)
			td->td_proc->p_fd->fd_ofiles[td->td_retval[0]].fde_flags |= UF_EXCLOSE;
		if (args->flags & LINUX_EPOLL_NONBLOCK)
			linux_msg(td, "epoll_create1 doesn't yet support EPOLL_NONBLOCK flag\n");
	}

	return (error);
}

/* Structure converting function from epoll to kevent. */
static int
linux_epoll_to_kevent(struct thread *td,
#if EPOLL_WIDE_USER_DATA
	struct file *epfp,
#endif
	int fd, struct linux_epoll_event *l_event, int kev_flags, struct kevent *kevent, int *nkevents)
{
	/* flags related to how event is registered */
	if (l_event->events & LINUX_EPOLLONESHOT)
		kev_flags |= EV_ONESHOT;
	if (l_event->events & LINUX_EPOLLET) {
		kev_flags |= EV_CLEAR;
	}

	/* flags related to what event is registered */
	if (l_event->events & LINUX_EPOLLIN ||
	    l_event->events & LINUX_EPOLLRDNORM ||
	    l_event->events & LINUX_EPOLLPRI ||
	    l_event->events & LINUX_EPOLLRDHUP) {
		EV_SET(kevent++, fd, EVFILT_READ, kev_flags, 0, 0,
			(void*)(EPOLL_WIDE_USER_DATA ? 0 : l_event->data));
		++*nkevents;
	}
	if (l_event->events & LINUX_EPOLLOUT ||
	    l_event->events & LINUX_EPOLLWRNORM) {
		EV_SET(kevent++, fd, EVFILT_WRITE, kev_flags, 0, 0,
			(void*)(EPOLL_WIDE_USER_DATA ? 0 : l_event->data));
		++*nkevents;
	}
	if (l_event->events & LINUX_EPOLLRDBAND ||
	    l_event->events & LINUX_EPOLLWRBAND ||
	    l_event->events & LINUX_EPOLLHUP ||
	    l_event->events & LINUX_EPOLLMSG ||
	    l_event->events & LINUX_EPOLLWAKEUP ||
	    l_event->events & LINUX_EPOLLERR) {
		linux_msg(td, "epoll_ctl doesn't yet support some event flags supplied: 0x%x\n",
			l_event->events);
		return (EINVAL);
	}

#if EPOLL_WIDE_USER_DATA
	epoll_set_user_data(td, epfp, fd, l_event->data);
#endif
	return (0);
}

/* 
 * Structure converting function from kevent to epoll. In a case
 * this is called on error in registration we store the error in
 * event->data and pick it up later in linux_epoll_ctl().
 */
static void
linux_kevent_to_epoll(
#if EPOLL_WIDE_USER_DATA
	struct thread *td, struct file *epfp,
#endif
	struct kevent *kevent, struct linux_epoll_event *l_event)
{
	if ((kevent->flags & EV_ERROR) == 0)
		switch (kevent->filter) {
		case EVFILT_READ:
			l_event->events = LINUX_EPOLLIN|LINUX_EPOLLRDNORM|LINUX_EPOLLPRI;
		break;
		case EVFILT_WRITE:
			l_event->events = LINUX_EPOLLOUT|LINUX_EPOLLWRNORM;
		break;
		}
#if EPOLL_WIDE_USER_DATA
	l_event->data = epoll_get_user_data(td, epfp, kevent->ident);
#else
	l_event->data = (epoll_udata_t)kevent->udata;
#endif
}

/* 
 * Copyout callback used by kevent. This converts kevent
 * events to epoll events and copies them back to the
 * userspace. This is also called on error on registering
 * of the filter.
 */
static int
epoll_kev_copyout(void *arg, struct kevent *kevp, int count)
{
	struct epoll_copyout_args *args;
	struct linux_epoll_event *eep;
	int error, i;

	args = (struct epoll_copyout_args*) arg;
	eep = malloc(sizeof(*eep) * count, M_TEMP, M_WAITOK | M_ZERO);

	for (i = 0; i < count; i++)
		linux_kevent_to_epoll(
#if EPOLL_WIDE_USER_DATA
			args->td, args->epfp,
#endif
			&kevp[i], &eep[i]);

	error = copyout(eep, args->leventlist, count * sizeof(*eep));
	if (!error) {
		args->leventlist += count;
		args->count += count;
	} else if (!args->error)
		args->error = error;

#ifdef KTRACE
	if (KTRPOINT(args->td, KTR_STRUCT))
		ktrepoll_events(eep, count);
#endif

	free(eep, M_TEMP);
	return (error);
}

/*
 * Copyin callback used by kevent. This copies already
 * converted filters from kernel memory to the kevent 
 * internal kernel memory. Hence the memcpy instead of
 * copyin.
 */
static int
epoll_kev_copyin(void *arg, struct kevent *kevp, int count)
{
	struct epoll_copyin_args *args;

	args = (struct epoll_copyin_args*) arg;
	
	memcpy(kevp, args->changelist, count * sizeof(*kevp));
	args->changelist += count;

	return (0);
}

static int
ignore_enoent(int error) {
	if (error == ENOENT)
		error = 0;
	return (error);
}

static int
delete_event(struct thread *td, struct file *epfp, int fd, int filter)
{
	struct epoll_copyin_args ciargs;
	struct kevent kev;
	struct kevent_copyops k_ops = { &ciargs,
					NULL,
					epoll_kev_copyin};
	ciargs.changelist = &kev;

	EV_SET(&kev, fd, filter, EV_DELETE | EV_DISABLE, 0, 0, 0);
	return (kern_kevent_locked(td, epfp, 1, 0, &k_ops, NULL));
}

static int
delete_all_events(struct thread *td, struct file *epfp, int fd)
{
	/* here we ignore ENONT, because we don't keep track of events here */
	int error1, error2;

	error1 = ignore_enoent(delete_event(td, epfp, fd, EVFILT_READ));
	error2 = ignore_enoent(delete_event(td, epfp, fd, EVFILT_WRITE));

	/* report any errors we got */
	if (error1)
		return (error1);
	if (error2)
		return (error2);
	return (0);
}

/*
 * Load epoll filter, convert it to kevent filter
 * and load it into kevent subsystem.
 */
int
linux_epoll_ctl(struct thread *td, struct linux_epoll_ctl_args *args)
{
	struct file *epfp;
	struct epoll_copyin_args ciargs;
	struct kevent kev[2];
	struct kevent_copyops k_ops = { &ciargs,
					NULL,
					epoll_kev_copyin};
	struct linux_epoll_event le;
	int kev_flags;
	int nchanges = 0;
	int error;

	if (args->epfd == args->fd)
		return (EINVAL);

	if (args->op != LINUX_EPOLL_CTL_DEL) {
		error = copyin(args->event, &le, sizeof(le));
		if (error)
			return (error);
	}
#ifdef DEBUG
	if (ldebug(epoll_ctl))
		printf(ARGS(epoll_ctl,"%i, %i, %i, %u"), args->epfd, args->op,
			args->fd, le.events);
#endif
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT) && args->op != LINUX_EPOLL_CTL_DEL)
		ktrepoll_events(&le, 1);
#endif
	epfp = epoll_fget(td, args->epfd);

	ciargs.changelist = kev;

	switch (args->op) {
	case LINUX_EPOLL_CTL_MOD:
			/* we don't memorize which events were set for this FD
			   on this level, so just delete all we could have set:
			   EVFILT_READ and EVFILT_WRITE, ignoring any errors
			*/
			error = delete_all_events(td, epfp, args->fd);
			if (error)
				goto leave;
		/* FALLTHROUGH */
	case LINUX_EPOLL_CTL_ADD:
			kev_flags = EV_ADD | EV_ENABLE;
		break;
	case LINUX_EPOLL_CTL_DEL:
			/* CTL_DEL means unregister this fd with this epoll */
			error = delete_all_events(td, epfp, args->fd);
		goto leave;
	default:
		error = EINVAL;
		goto leave;
	}

	error = linux_epoll_to_kevent(td,
#if EPOLL_WIDE_USER_DATA
		epfp,
#endif
		args->fd, &le, kev_flags, kev, &nchanges);
	if (error)
		goto leave;

	error = kern_kevent_locked(td, epfp, nchanges, 0, &k_ops, NULL);
leave:
	fdrop(epfp, td);
	return (error);
}

/*
 * Wait for a filter to be triggered on the epoll file descriptor. */
int
linux_epoll_wait(struct thread *td, struct linux_epoll_wait_args *args)
{
	struct file *epfp;
	struct timespec ts, *tsp;
	struct epoll_copyout_args coargs;
	struct kevent_copyops k_ops = { &coargs,
					epoll_kev_copyout,
					NULL};
	int error;

	if (args->maxevents <= 0 || args->maxevents > LINUX_MAX_EVENTS)
		return (EINVAL);

	epfp = epoll_fget(td, args->epfd);

	coargs.leventlist = args->events;
	coargs.count = 0;
	coargs.error = 0;
#if defined(KTRACE) || EPOLL_WIDE_USER_DATA
	coargs.td = td;
#endif
#if EPOLL_WIDE_USER_DATA
	coargs.epfp = epfp;
#endif

	if (args->timeout != -1) {
		if (args->timeout < 0) {
			error = EINVAL;
			goto leave;
		}
		/* Convert from milliseconds to timespec. */
		ts.tv_sec = args->timeout / 1000;
		ts.tv_nsec = (args->timeout % 1000) * 1000000;
		tsp = &ts;
	} else {
		tsp = NULL;
	}

	error = kern_kevent_locked(td, epfp, 0, args->maxevents, &k_ops, tsp);
	if (!error && coargs.error)
		error = coargs.error;

	/* 
	 * kern_keven might return ENOMEM which is not expected from epoll_wait.
	 * Maybe we should translate that but I don't think it matters at all.
	 */

	if (!error)
		td->td_retval[0] = coargs.count;
leave:
	fdrop(epfp, td);
	return (error);
}

#if EPOLL_WIDE_USER_DATA
/*
 * we store user_data vector in an unused for kqueue descriptor
 * field fvn_epollpriv in struct file.
 */
#define EPOLL_USER_DATA_GET(epfp) \
	((struct epoll_user_data*)(epfp)->f_vnun.fvn_epollpriv)
#define EPOLL_USER_DATA_SET(epfp, udv) \
	(epfp)->f_vnun.fvn_epollpriv = (udv)

static void
epoll_init_user_data(struct thread *td, struct file *epfp)
{
	struct epoll_user_data *udv;

	/* override file ops to have our close operation */
	atomic_store_rel_ptr((volatile uintptr_t *)&epfp->f_ops, (uintptr_t)&epollops);

	/* allocate epoll_user_data initially for up to 16 file descriptor values */
	udv = malloc(EPOLL_USER_DATA_SIZE(EPOLL_USER_DATA_MARGIN), M_LINUX_EPOLL, M_WAITOK);
	udv->sz = EPOLL_USER_DATA_MARGIN;
	EPOLL_USER_DATA_SET(epfp, udv);
}

static void
epoll_set_user_data(struct thread *td, struct file *epfp, int fd, epoll_udata_t user_data)
{
	struct epoll_user_data *udv = EPOLL_USER_DATA_GET(epfp);

	if (fd >= udv->sz) {
		udv = realloc(udv, EPOLL_USER_DATA_SIZE(fd + EPOLL_USER_DATA_MARGIN), M_LINUX_EPOLL, M_WAITOK);
		udv->sz = fd + EPOLL_USER_DATA_MARGIN;
		EPOLL_USER_DATA_SET(epfp, udv);
	}
	udv->data[fd] = user_data;
}

static epoll_udata_t
epoll_get_user_data(struct thread *td, struct file *epfp, int fd)
{
	struct epoll_user_data *udv = EPOLL_USER_DATA_GET(epfp);
	if (fd >= udv->sz)
		panic("epoll: user data vector is too small");

	return (udv->data[fd]);
}

/*ARGSUSED*/
static int
epoll_close(struct file *epfp, struct thread *td)
{
	/* free user data vector */
	free(EPOLL_USER_DATA_GET(epfp), M_LINUX_EPOLL);
	/* over to kqueue parent */
	return (kqueue_close(epfp, td));
}
#endif

static struct file*
epoll_fget(struct thread *td, int epfd)
{
	struct file *fp;
	cap_rights_t rights;

	if (fget(td, epfd, cap_rights_init(&rights, CAP_POLL_EVENT), &fp) != 0)
		panic("epoll: no file object found for kqueue descriptor");

	return (fp);
}

