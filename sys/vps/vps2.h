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

/* $Id: vps2.h 174 2013-06-12 15:39:22Z klaus $ */

#ifndef _VPS2_H
#define _VPS2_H

#include <sys/cdefs.h>

#ifdef VPS
#ifndef VIMAGE
#error "You can't have option VPS without option VIMAGE !"
#endif
#endif

#include <sys/priv.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>

/* For sysctl stuff. */
#include <net/vnet.h>

struct vps_param;
struct vps_acc;

#define TD_TO_VPS(x)	(x)->td_ucred->cr_vps
#define P_TO_VPS(x)	(x)->p_ucred->cr_vps

LIST_HEAD(vps_list_head, vps);
extern struct vps_list_head vps_head;

#define PRIV_SET_SIZE	((_PRIV_HIGHEST + 8) / 8)

#ifdef INVARIANTS
struct vps_ref {
	TAILQ_ENTRY(vps_ref) list;
	void *arg;
	uint64_t ticks;
};
#endif

#ifdef DIAGNOSTIC

#define VPSFUNC __attribute__((noinline))
#define DBGCORE if (vps_debug_core) printf
extern int vps_debug_core;

#else /* ! DIAGNOSTIC */

#define VPSFUNC
#define DBGCORE(x, ...)

#endif /* ! DIAGNOSTIC */

#ifdef VPS

/* Keep in sync with ''struct vps2'' declared in vps/vps.h ! */

struct vps {

	struct vnet		*vnet;

        LIST_ENTRY(vps)		vps_all;
        LIST_ENTRY(vps)		vps_sibling;
        LIST_HEAD(, vps)	vps_child_head;
        struct vps		*vps_parent;

	struct sx		vps_lock;
	char			*vps_lock_name;

	u_int			vps_id;
	char			vps_name[MAXHOSTNAMELEN];
	u_char			vps_status;

	u_int			vps_refcnt;
	struct mtx		vps_refcnt_lock;
#ifdef INVARIANTS
	TAILQ_HEAD(, vps_ref)	vps_ref_head;
#endif
	struct timeout_task	vps_task;

        u_char			priv_allow_set[PRIV_SET_SIZE];
        u_char			priv_impl_set[PRIV_SET_SIZE];

	struct vps_arg_ip4	*vps_ip4;
	struct vps_arg_ip6	*vps_ip6;
	u_int16_t		vps_ip4_cnt;
	u_int16_t		vps_ip6_cnt;

	u_int			vps_flags;

	int			restore_count;

	int64_t			suspend_time;

	struct vps_acc		*vps_acc;	/* XXX do inline */

	struct vnode		*consolelog;
	struct tty		*console_tty;
	struct file		*console_fp_ma;
	int			consolelog_refcnt;
	int			console_flags;

	struct ucred		*vps_ucred;

	struct devfs_rule	*devfs_ruleset;

        struct vnode		*_rootvnode;
        char			_rootpath[MAXPATHLEN];
};

struct vps_snapst_ctx;

struct vps_dev_ctx {
        LIST_ENTRY(vps_dev_ctx)	list;
        struct file		*fp;
	struct thread		*td;
	caddr_t			data;
	size_t			length;
	u_long			cmd;
	struct vps_snapst_ctx	*snapst;
};

struct devfs_mount;
struct cdev;
struct cdev_priv;
struct mount;

struct vps *vps_by_name(struct vps *, char *);
struct vps *vps_alloc(struct vps *, struct vps_param *, char *,
    int *errorval);
int vps_free(struct vps *);
int vps_free_locked(struct vps *);
int vps_destroy(struct vps *);
void vps_ref(struct vps *, struct ucred *);
void vps_deref(struct vps *, struct ucred *);

int vps_devfs_ruleset_create(struct vps *vps);
int vps_devfs_ruleset_destroy(struct vps *vps);
int vps_devfs_mount_cb(struct devfs_mount *dm, int *rsnum);
int vps_devfs_unmount_cb(struct devfs_mount *dm);
int vps_devfs_whiteout_cb(struct devfs_mount *dm, struct cdev_priv *cdp);

int vps_switch_proc(struct thread *, struct vps *, int);
int vps_switch_proc_wait(struct thread *, struct vps *, int);
int vps_proc_release(struct vps *, struct proc *);
int vps_proc_exit(struct thread *, struct proc *);
int vps_proc_signal(struct vps *, pid_t, int);

int vps_reboot(struct thread *, int);
int vps_shutdown_all(struct thread *);

int vps_ioc_list(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_create(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_destroy(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_switch(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_switchwait(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_ifmove(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_suspend(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_resume(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_abort(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_snapshot(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_snapshot_finish(struct vps *, struct vps_dev_ctx *, u_long,
    caddr_t, int, struct thread *);
int vps_ioc_restore(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_argget(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_argset(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_getextinfo(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_fscalc(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_fscalcpath(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);
int vps_ioc_getconsfd(struct vps *, struct vps_dev_ctx *, u_long, caddr_t,
    int, struct thread *);

struct in_addr;
struct in6_addr;
struct vps_arg_item;
struct ucred;
struct mount;
struct statfs;

void vps_priv_setdefault(struct vps *, struct vps_param *);
int vps_priv_setitem(struct vps *, struct vps *, struct vps_arg_item *);
int vps_priv_getitemall(struct vps *, struct vps *, caddr_t, size_t *);
int vps_ip_setitem(struct vps *, struct vps *, struct vps_arg_item *);
int vps_ip_getitemall(struct vps *, struct vps *, caddr_t, size_t *);
int vps_priv_check(struct ucred *, int);
int vps_ip4_check(struct vps *, struct in_addr *, struct in_addr *);
int vps_ip6_check(struct vps *, struct in6_addr *, u_int8_t);
int vps_canseemount(struct ucred *, struct mount *);
void vps_statfs(struct ucred *cred, struct mount *mp, struct statfs *sp);

int vps_console_fdopen(struct cdev *, int, struct thread *, struct file *);
int vps_console_init(void);
int vps_console_uninit(void);
int vps_console_alloc(struct vps *, struct thread *);
int vps_console_free(struct vps *, struct thread *);
int vps_console_getfd(struct vps *, struct thread *, int *);

int vps_unmount_all(struct vps *vps);

int vps_umtx_snapshot(struct thread *td);

/* machdep stuff */
struct vps_dump_thread;
struct execve_args;
void vps_md_print_thread(struct thread *td);
void vps_md_print_pcb(struct thread *td);
int vps_md_snapshot_thread(struct vps_dump_thread *vdtd, struct thread *td);
int vps_md_restore_thread(struct vps_dump_thread *vdtd, struct thread *ntd,
    struct proc *p);
int vps_md_snapshot_sysentvec(struct sysentvec *sv, long *svtype);
int vps_md_restore_sysentvec(long svtype, struct sysentvec **sv);
int vps_md_restore_checkarch(u_int8_t ptrsize, u_int8_t byteorder);
int vps_md_snapshot_thread_savefpu(struct vps_snapst_ctx *ctx,
    struct vps *vps, struct thread *td);
int vps_md_restore_thread_savefpu(struct vps_snapst_ctx *ctx,
    struct vps *vps, struct thread *td);
int vps_md_reboot_copyout(struct thread *td, struct execve_args *);
int vps_md_syscall_fixup(struct vps *, struct thread *,
    register_t *ret_code, register_t **ret_args, int *narg);
int vps_md_syscall_fixup_setup_inthread(struct vps *, struct thread *,
    register_t);


extern struct sx vps_all_lock;

/*
 * Flags used in vnet by vps.
 */
#define VPS_VNET_SUSPENDED	0x1000
#define VPS_VNET_ABORT		0x2000

/*
 * Various vps internal flags.
 */

#endif /* VPS */

/*
 * At least for now, just use vnet's facility for virtualized
 * global variables.
 * But map to our own names for easier change in the future.
 */

#define VPS_NAME			VNET_NAME
#define VPS_DECLARE			VNET_DECLARE
#define VPS_DEFINE			VNET_DEFINE

#define SYSCTL_VPS_INT			SYSCTL_VNET_INT
#define SYSCTL_VPS_PROC			SYSCTL_VNET_PROC
#define SYSCTL_VPS_STRING		SYSCTL_VNET_STRING
#define SYSCTL_VPS_STRUCT		SYSCTL_VNET_STRUCT
#define SYSCTL_VPS_UINT			SYSCTL_VNET_UINT
#define SYSCTL_VPS_LONG			SYSCTL_VNET_LONG
#define SYSCTL_VPS_ULONG		SYSCTL_VNET_ULONG

#define vps_sysctl_handle_int		vnet_sysctl_handle_int
#define vps_sysctl_handle_opaque	vnet_sysctl_handle_opaque
#define vps_sysctl_handle_string	vnet_sysctl_handle_string
#define vps_sysctl_handle_uint		vnet_sysctl_handle_uint

/*
 * Declare virtualized globals.
 */

#define      HOSTUUIDLEN     64

VPS_DECLARE(char, hostname[MAXHOSTNAMELEN]);
VPS_DECLARE(char, domainname[MAXHOSTNAMELEN]);
VPS_DECLARE(char, hostuuid[HOSTUUIDLEN]);

#endif /* _VPS2_H */

/* EOF */
