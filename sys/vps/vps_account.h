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

/* $Id: vps_account.h 153 2013-06-03 16:18:17Z klaus $ */

#include <sys/cdefs.h>

#ifndef _VPS_ACCOUNT_H
#define _VPS_ACCOUNT_H

#ifdef VPS

#define VPS_ACC_ALLOC	0x2
#define VPS_ACC_FREE	0x4

#define VPS_ACC_UNUSED00	0x0001
#define VPS_ACC_UNUSED01	0x0002
#define VPS_ACC_VIRT		0x0004
#define VPS_ACC_PHYS		0x0008
#define VPS_ACC_KMEM		0x0010
#define VPS_ACC_KERNEL		0x0020
#define VPS_ACC_BUFFER		0x0040
#define VPS_ACC_PCTCPU		0x0080
#define VPS_ACC_BLOCKIO		0x0100
#define VPS_ACC_FSSPACE		0x0200
#define VPS_ACC_FSFILES		0x0400
#define VPS_ACC_THREADS		0x0800
#define VPS_ACC_PROCS		0x1000

#ifdef _KERNEL

#include <vps/vps_int.h>

struct vps;
struct mount;

struct vps_acc_val {

	/* Current accounting value (counter or rate). */
	size_t cur;

	/* For rate calculation. */
	size_t cnt_cur;

	/* Preconfigured soft limit (a warning is emitted when limit
	    is hit). */
	size_t soft;

	/* Preconfigured hard limit (allocation is denied and error returned
	    where possible). */
	size_t hard;

	/* Counter of exceeding the soft limit. */
	u_int16_t hits_soft;

	/* Counter of attempts exceeding the hard limit. */
	u_int16_t hits_hard;

	/* Last updated (ticks) */
	int updated;
};

struct vps_acc {
	struct mtx lock;
	struct vps *vps;
#define	vps_acc_first	virt
	struct vps_acc_val virt;
	struct vps_acc_val phys;
	struct vps_acc_val kmem;
	struct vps_acc_val kernel;
	struct vps_acc_val buffer;
	struct vps_acc_val pctcpu;
	struct vps_acc_val blockio;
	struct vps_acc_val threads;
	struct vps_acc_val procs;
	struct vps_acc_val nthreads;	/* Only a pseudo record
					   for cpu accounting. */
#define vps_acc_last	nthreads
};

int vps_account_init(void);
int vps_account_uninit(void);
int _vps_account(struct vps *, int, int, size_t);
void _vps_account_stats(struct vps *);
int _vps_account_waitpfault(struct vps *);
void vps_account_threads(void *dummy);
int _vps_account_runnable(struct thread *);
void _vps_account_thread_pause(struct thread *);
void vps_account_print_pctcpu(struct vps *);
int _vps_account_bio(struct thread *);
int vps_account_vpsfs_calc_path(struct vps *, const char *,
    caddr_t, size_t *);
int vps_account_vpsfs_calc_mount(struct vps *, struct mount *,
    caddr_t, size_t *);
struct vps_arg_item;
int _vps_limit_setitem(struct vps *, struct vps *, struct vps_arg_item *);
int _vps_limit_getitemall(struct vps *, struct vps *, caddr_t, size_t *);

static __inline int
vps_account(struct vps *vps, int type, int action, size_t size)
{
	if (vps_func->vps_account == NULL)
		return (0);
	return (vps_func->vps_account(vps, type, action, size));
}

static __inline int
vps_account_waitpfault(struct vps *vps)
{
	if (vps_func->vps_account_waitpfault == NULL)
		return (0);
	return (vps_func->vps_account_waitpfault(vps));
}

static __inline int
vps_account_bio(struct thread *td)
{
	if (vps_func->vps_account_bio == NULL)
		return (0);
	return (vps_func->vps_account_bio(td));
}

static __inline void
vps_account_stats(struct vps *vps)
{
	if (vps_func->vps_account_stats == NULL)
		return;
	vps_func->vps_account_stats(vps);
}

static __inline int
vps_account_runnable(struct thread *td)
{
	if (vps_func->vps_account_runnable == NULL)
		return (1);
	return (vps_func->vps_account_runnable(td));
}

static __inline void
vps_account_thread_pause(struct thread *td)
{
	if (vps_func->vps_account_thread_pause == NULL)
		return;
	vps_func->vps_account_thread_pause(td);
}

static __inline int
vps_limit_setitem(struct vps *vpsp, struct vps *vps,
    struct vps_arg_item *item)
{
	if (vps_func->vps_limit_setitem == NULL)
		return (EOPNOTSUPP);
	return (vps_func->vps_limit_setitem(vpsp, vps, item));
}

static __inline int
vps_limit_getitemall(struct vps *vpsp, struct vps *vps, caddr_t kdata,
    size_t *kdatalen)
{
	if (vps_func->vps_limit_getitemall == NULL)
		return (EOPNOTSUPP);
	return (vps_func->vps_limit_getitemall(vpsp, vps, kdata, kdatalen));
}

#endif /* _KERNEL */

#endif /* VPS */

#endif /* _VPS_ACCOUNT_H */

/* EOF */
