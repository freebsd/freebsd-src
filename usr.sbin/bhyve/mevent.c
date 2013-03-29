/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

/*
 * Micro event library for FreeBSD, designed for a single i/o thread 
 * using kqueue, and having events be persistent by default.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <pthread.h>
#include <pthread_np.h>

#include "mevent.h"

#define	MEVENT_MAX	64

#define MEV_ENABLE	1
#define MEV_DISABLE	2
#define MEV_DEL_PENDING	3

extern char *vmname;

static pthread_t mevent_tid;
static int mevent_pipefd[2];
static pthread_mutex_t mevent_lmutex = PTHREAD_MUTEX_INITIALIZER;

struct mevent {	
	void	(*me_func)(int, enum ev_type, void *);
	int	me_fd;
	enum ev_type me_type;
	void    *me_param;
	int	me_cq;
	int	me_state;
	int	me_closefd;
	LIST_ENTRY(mevent) me_list;			   
};

static LIST_HEAD(listhead, mevent) global_head, change_head;

static void
mevent_qlock(void)
{
	pthread_mutex_lock(&mevent_lmutex);
}

static void
mevent_qunlock(void)
{
	pthread_mutex_unlock(&mevent_lmutex);
}

static void
mevent_pipe_read(int fd, enum ev_type type, void *param)
{
	char buf[MEVENT_MAX];
	int status;

	/*
	 * Drain the pipe read side. The fd is non-blocking so this is
	 * safe to do.
	 */
	do {
		status = read(fd, buf, sizeof(buf));
	} while (status == MEVENT_MAX);
}

static void
mevent_notify(void)
{
	char c;
	
	/*
	 * If calling from outside the i/o thread, write a byte on the
	 * pipe to force the i/o thread to exit the blocking kevent call.
	 */
	if (mevent_pipefd[1] != 0 && pthread_self() != mevent_tid) {
		write(mevent_pipefd[1], &c, 1);
	}
}

static int
mevent_kq_filter(struct mevent *mevp)
{
	int retval;

	retval = 0;

	if (mevp->me_type == EVF_READ)
		retval = EVFILT_READ;

	if (mevp->me_type == EVF_WRITE)
		retval = EVFILT_WRITE;

	return (retval);
}

static int
mevent_kq_flags(struct mevent *mevp)
{
	int ret;

	switch (mevp->me_state) {
	case MEV_ENABLE:
		ret = EV_ADD;
		break;
	case MEV_DISABLE:
		ret = EV_DISABLE;
		break;
	case MEV_DEL_PENDING:
		ret = EV_DELETE;
		break;
	}

	return (ret);
}

static int
mevent_kq_fflags(struct mevent *mevp)
{
	/* XXX nothing yet, perhaps EV_EOF for reads ? */
	return (0);
}

static int
mevent_build(int mfd, struct kevent *kev)
{
	struct mevent *mevp, *tmpp;
	int i;

	i = 0;

	mevent_qlock();

	LIST_FOREACH_SAFE(mevp, &change_head, me_list, tmpp) {
		if (mevp->me_closefd) {
			/*
			 * A close of the file descriptor will remove the
			 * event
			 */
			close(mevp->me_fd);
		} else {
			kev[i].ident = mevp->me_fd;
			kev[i].filter = mevent_kq_filter(mevp);
			kev[i].flags = mevent_kq_flags(mevp);
			kev[i].fflags = mevent_kq_fflags(mevp);
			kev[i].data = 0;
			kev[i].udata = mevp;
			i++;
		}

		mevp->me_cq = 0;
		LIST_REMOVE(mevp, me_list);

		if (mevp->me_state == MEV_DEL_PENDING) {
			free(mevp);
		} else {
			LIST_INSERT_HEAD(&global_head, mevp, me_list);
		}

		assert(i < MEVENT_MAX);
	}

	mevent_qunlock();

	return (i);
}

static void
mevent_handle(struct kevent *kev, int numev)
{
	struct mevent *mevp;
	int i;

	for (i = 0; i < numev; i++) {
		mevp = kev[i].udata;

		/* XXX check for EV_ERROR ? */

		(*mevp->me_func)(mevp->me_fd, mevp->me_type, mevp->me_param);
	}
}

struct mevent *
mevent_add(int fd, enum ev_type type,
	   void (*func)(int, enum ev_type, void *), void *param)
{
	struct mevent *lp, *mevp;

	if (fd < 0 || func == NULL) {
		return (NULL);
	}

	mevp = NULL;

	mevent_qlock();

	/*
	 * Verify that the fd/type tuple is not present in any list
	 */
	LIST_FOREACH(lp, &global_head, me_list) {
		if (lp->me_fd == fd && lp->me_type == type) {
			goto exit;
		}
	}

	LIST_FOREACH(lp, &change_head, me_list) {
		if (lp->me_fd == fd && lp->me_type == type) {
			goto exit;
		}
	}

	/*
	 * Allocate an entry, populate it, and add it to the change list.
	 */
	mevp = malloc(sizeof(struct mevent));
	if (mevp == NULL) {
		goto exit;
	}

	memset(mevp, 0, sizeof(struct mevent));
	mevp->me_fd = fd;
	mevp->me_type = type;
	mevp->me_func = func;
	mevp->me_param = param;

	LIST_INSERT_HEAD(&change_head, mevp, me_list);
	mevp->me_cq = 1;
	mevp->me_state = MEV_ENABLE;
	mevent_notify();

exit:
	mevent_qunlock();

	return (mevp);
}

static int
mevent_update(struct mevent *evp, int newstate)
{
	/*
	 * It's not possible to enable/disable a deleted event
	 */
	if (evp->me_state == MEV_DEL_PENDING)
		return (EINVAL);

	/*
	 * No update needed if state isn't changing
	 */
	if (evp->me_state == newstate)
		return (0);
	
	mevent_qlock();

	evp->me_state = newstate;

	/*
	 * Place the entry onto the changed list if not already there.
	 */
	if (evp->me_cq == 0) {
		evp->me_cq = 1;
		LIST_REMOVE(evp, me_list);
		LIST_INSERT_HEAD(&change_head, evp, me_list);
		mevent_notify();
	}

	mevent_qunlock();

	return (0);
}

int
mevent_enable(struct mevent *evp)
{

	return (mevent_update(evp, MEV_ENABLE));
}

int
mevent_disable(struct mevent *evp)
{

	return (mevent_update(evp, MEV_DISABLE));
}

static int
mevent_delete_event(struct mevent *evp, int closefd)
{
	mevent_qlock();

	/*
         * Place the entry onto the changed list if not already there, and
	 * mark as to be deleted.
         */
        if (evp->me_cq == 0) {
		evp->me_cq = 1;
		LIST_REMOVE(evp, me_list);
		LIST_INSERT_HEAD(&change_head, evp, me_list);
		mevent_notify();
        }
	evp->me_state = MEV_DEL_PENDING;

	if (closefd)
		evp->me_closefd = 1;

	mevent_qunlock();

	return (0);
}

int
mevent_delete(struct mevent *evp)
{

	return (mevent_delete_event(evp, 0));
}

int
mevent_delete_close(struct mevent *evp)
{

	return (mevent_delete_event(evp, 1));
}

static void
mevent_set_name(void)
{
	char tname[MAXCOMLEN + 1];

	snprintf(tname, sizeof(tname), "%s mevent", vmname);
	pthread_set_name_np(mevent_tid, tname);
}

void
mevent_dispatch(void)
{
	struct kevent changelist[MEVENT_MAX];
	struct kevent eventlist[MEVENT_MAX];
	struct mevent *pipev;
	int mfd;
	int numev;
	int ret;

	mevent_tid = pthread_self();
	mevent_set_name();

	mfd = kqueue();
	assert(mfd > 0);

	/*
	 * Open the pipe that will be used for other threads to force
	 * the blocking kqueue call to exit by writing to it. Set the
	 * descriptor to non-blocking.
	 */
	ret = pipe(mevent_pipefd);
	if (ret < 0) {
		perror("pipe");
		exit(0);
	}

	/*
	 * Add internal event handler for the pipe write fd
	 */
	pipev = mevent_add(mevent_pipefd[0], EVF_READ, mevent_pipe_read, NULL);
	assert(pipev != NULL);

	for (;;) {
		/*
		 * Build changelist if required.
		 * XXX the changelist can be put into the blocking call
		 * to eliminate the extra syscall. Currently better for
		 * debug.
		 */
		numev = mevent_build(mfd, changelist);
		if (numev) {
			ret = kevent(mfd, changelist, numev, NULL, 0, NULL);
			if (ret == -1) {
				perror("Error return from kevent change");
			}
		}

		/*
		 * Block awaiting events
		 */
		ret = kevent(mfd, NULL, 0, eventlist, MEVENT_MAX, NULL);
		if (ret == -1) {
			perror("Error return from kevent monitor");
		}
		
		/*
		 * Handle reported events
		 */
		mevent_handle(eventlist, ret);
	}			
}
