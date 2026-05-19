/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Kernel support for NT synchronization primitive emulation
 *
 * Copyright (C) 2021-2022 Elizabeth Figura <zfigura@codeweavers.com>
 */

#ifndef __LINUX_NTSYNC_H
#define __LINUX_NTSYNC_H

#include <sys/types.h>

typedef	uint32_t	__u32;
typedef	uint64_t	__u64;

struct linux_ntsync_sem_args {
	__u32 count;
	__u32 max;
};

struct linux_ntsync_mutex_args {
	__u32 owner;
	__u32 count;
};

struct linux_ntsync_event_args {
	__u32 manual;
	__u32 signaled;
};

#define LINUX_NTSYNC_WAIT_REALTIME	0x1

struct linux_ntsync_wait_args {
	__u64 timeout;
	__u64 objs;
	__u32 count;
	__u32 index;
	__u32 flags;
	__u32 owner;
	__u32 alert;
	__u32 pad;
};

#define	LNTSYNC_IOC_CREATE_SEM		0x40084e80
#define	LNTSYNC_IOC_WAIT_ANY		0xc0284e82
#define	LNTSYNC_IOC_WAIT_ALL		0xc0284e83
#define	LNTSYNC_IOC_CREATE_MUTEX	0x40084e84
#define	LNTSYNC_IOC_CREATE_EVENT	0x40084e87
#define	LNTSYNC_IOC_SEM_RELEASE		0xc0044e81
#define	LNTSYNC_IOC_MUTEX_UNLOCK	0xc0084e85
#define	LNTSYNC_IOC_MUTEX_KILL		0x40044e86
#define	LNTSYNC_IOC_EVENT_SET		0x80044e88
#define	LNTSYNC_IOC_EVENT_RESET		0x80044e89
#define	LNTSYNC_IOC_EVENT_PULSE		0x80044e8a
#define	LNTSYNC_IOC_SEM_READ		0x80084e8b
#define	LNTSYNC_IOC_MUTEX_READ		0x80084e8c
#define	LNTSYNC_IOC_EVENT_READ		0x80084e8d

#define LNTSYNC_IOCTL_MIN		0x4e80
#define LNTSYNC_IOCTL_MAX		0x4eff

#endif
