/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#ifndef __DEV_NTSYNC_H__
#define	__DEV_NTSYNC_H__

#include <sys/types.h>
#include <sys/ioccom.h>

struct ntsync_sem_args {
	uint32_t count;
	uint32_t max;
};

struct ntsync_mutex_args {
	uint32_t owner;
	uint32_t count;
};

struct ntsync_event_args {
	uint32_t manual;
	uint32_t signaled;
};

struct ntsync_wait_args {
	uint64_t timeout;
	uint64_t objs;
	uint32_t count;
	uint32_t index;
	uint32_t flags;
	uint32_t owner;
	uint32_t alert;
	uint32_t pad;
};

#define	NTSYNC_WAIT_REALTIME	0x00000001

#define	NTSYNC_MAX_WAIT_COUNT	64

/*
 * 'sp' means that the ioctl is special, it might return both error
 * and copy out parameters.  See ntsync_ioctl_copyout().
 */

#define	NTSYNC_IOC_CREATE_SEM	_IOW('n', 1, struct ntsync_sem_args)
#define	NTSYNC_IOC_CREATE_MUTEX	_IOW('n', 2, struct ntsync_mutex_args)
#define	NTSYNC_IOC_CREATE_EVENT	_IOW('n', 3, struct ntsync_event_args)
#define	NTSYNC_IOC_SEM_RELEASE	_IOWR('n', 4, uint32_t)
#define	NTSYNC_IOC_MUTEX_UNLOCK	_IOWR('n', 5, struct ntsync_mutex_args)
#define	NTSYNC_IOC_EVENT_SET	_IOR('n', 6, uint32_t)
#define	NTSYNC_IOC_EVENT_RESET	_IOR('n', 7, uint32_t)
#define	NTSYNC_IOC_EVENT_PULSE	_IOR('n', 8, uint32_t)
#define	NTSYNC_IOC_SEM_READ	_IOR('n', 9, struct ntsync_sem_args)
#define	NTSYNC_IOC_MUTEX_READ	_IO('n', 10) /* sp */
#define	NTSYNC_IOC_EVENT_READ	_IOR('n', 11, struct ntsync_event_args)
#define	NTSYNC_IOC_MUTEX_KILL	_IOW('n', 12, uint32_t)
#define	NTSYNC_IOC_WAIT_ANY	_IOW('n', 13, struct ntsync_wait_args) /* sp */
#define	NTSYNC_IOC_WAIT_ALL	_IOW('n', 14, struct ntsync_wait_args) /* sp */

#endif
