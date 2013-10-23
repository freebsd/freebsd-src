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

/* $Id: vps_int.h 160 2013-06-05 16:15:56Z klaus $ */;

#ifndef _VPS_INT_H
#define _VPS_INT_H

#ifdef VPS

struct vps_snapst_ctx;
struct vps_dumpobj;
struct vpsfs_limits;
struct vps_arg_snapst;
struct vps_arg_item;
struct vps_dev_ctx;
struct vps_dumpheader;
struct mount;
struct vnode;

struct vps_functions {

/* vps/vps_libdump.c */
/* object functions */
struct vps_dumpobj *	(*vps_dumpobj_create)(struct vps_snapst_ctx *ctx,
				int type, int how);
void *			(*vps_dumpobj_space)(struct vps_snapst_ctx *ctx,
				long size, int how);
int 			(*vps_dumpobj_append)(struct vps_snapst_ctx *ctx,
				const void *data, long size, int how);
void			(*vps_dumpobj_close)(struct vps_snapst_ctx *ctx);
void			(*vps_dumpobj_discard)(struct vps_snapst_ctx *ctx,
				struct vps_dumpobj *o);
int			(*vps_dumpobj_checkobj)(struct vps_snapst_ctx *ctx,
				struct vps_dumpobj *o);
void			(*vps_dumpobj_setcur)(struct vps_snapst_ctx *ctx,
				struct vps_dumpobj *o);
struct vps_dumpobj *	(*vps_dumpobj_next)(struct vps_snapst_ctx *ctx);
struct vps_dumpobj *	(*vps_dumpobj_prev)(struct vps_snapst_ctx *ctx);
struct vps_dumpobj *	(*vps_dumpobj_peek)(struct vps_snapst_ctx *ctx);
struct vps_dumpobj *	(*vps_dumpobj_getcur)(struct vps_snapst_ctx *ctx);
int			(*vps_dumpobj_typeofnext)(struct vps_snapst_ctx
				*ctx);
int			(*vps_dumpobj_nextischild)(struct vps_snapst_ctx
				*ctx, struct vps_dumpobj *op);
int			(*vps_dumpobj_recurse)(struct vps_snapst_ctx *ctx,
				struct vps_dumpobj *o,
				void (*func)(struct vps_snapst_ctx *ctx,
				struct vps_dumpobj *));
/* tree functions */
int			(*vps_dumpobj_makerelative)(struct vps_snapst_ctx
				*ctx);
int			(*vps_dumpobj_makeabsolute)(struct vps_snapst_ctx
				*ctx);
int			(*vps_dumpobj_printtree)(struct vps_snapst_ctx
				*ctx);
int			(*vps_dumpobj_checktree)(struct vps_snapst_ctx
				*ctx);
/* various subroutines */
int			(*vps_dumpobj_checkptr)(struct vps_snapst_ctx *ctx,
				void *p, size_t off);
const char *		(*vps_libdump_objtype2str)(int objt);
int			(*vps_libdump_checkheader)(struct vps_dumpheader
				*h);
void			(*vps_libdump_printheader)(struct vps_dumpheader
				*h);


/* vps/vps_snapst.c */
int (*vps_snapshot)(struct vps_dev_ctx *, struct vps *,
    struct vps_arg_snapst *);
int (*vps_snapshot_finish)(struct vps_dev_ctx *, struct vps *);
int (*vps_ctx_extend_hard)(struct vps_snapst_ctx *, struct vps *,
    size_t, int);
int (*vps_snapshot_ucred)(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct ucred *cr, int how);

/* vps/vps_restore.c */
int (*vps_restore)(struct vps_dev_ctx *, struct vps_arg_snapst *);
int (*vps_restore_ucred)(struct vps_snapst_ctx *ctx, struct vps *vps);
struct ucred *(*vps_restore_ucred_lookup)(struct vps_snapst_ctx *ctx,
		struct vps *vps, void *orig_ptr);
void (*vps_restore_return)(struct thread *td, struct trapframe *frame);

/* vps/vps_suspend.c */
int (*vps_suspend)(struct vps *, int flags);
int (*vps_resume)(struct vps *, int flags);
int (*vps_abort)(struct vps *, int flags);
void (*vps_syscall_fixup_inthread)(register_t code,
    struct trapframe *frame);
int (*vps_access_vmspace)(struct vmspace *vm, vm_offset_t vaddr,
    size_t len, void *buf, int prot);

/* vps/vps_account.c */
int (*vpsfs_calcusage_path)(const char *, struct vpsfs_limits *);
int (*vps_account)(struct vps *vps, int type, int action, size_t size);
int (*vps_account_waitpfault)(struct vps *vps);
int (*vps_account_bio)(struct thread *td);
void (*vps_account_stats)(struct vps *vps);
int (*vps_account_runnable)(struct thread *td);
void (*vps_account_thread_pause)(struct thread *td);
int (*vps_limit_setitem)(struct vps *, struct vps *, struct vps_arg_item *);
int (*vps_limit_getitemall)(struct vps *, struct vps *, caddr_t, size_t *);

/* fs/vpsfs/vpsfs_vfsops.c */
int (*vpsfs_nodeget)(struct mount *, struct vnode *, struct vnode **);
const char *vpsfs_tag;

/* kern/sysv_sem.c */
int (*sem_snapshot_vps)(struct vps_snapst_ctx *, struct vps *);
int (*sem_snapshot_proc)(struct vps_snapst_ctx *, struct vps *,
    struct proc *);
int (*sem_restore_vps)(struct vps_snapst_ctx *, struct vps *);
int (*sem_restore_proc)(struct vps_snapst_ctx *, struct vps *,
    struct proc *);
int (*sem_restore_fixup)(struct vps_snapst_ctx *, struct vps *);

/* kern/sysv_shm.c */
int (*shm_snapshot_vps)(struct vps_snapst_ctx *, struct vps *);
int (*shm_snapshot_proc)(struct vps_snapst_ctx *, struct vps *,
    struct proc *);
int (*shm_restore_vps)(struct vps_snapst_ctx *, struct vps *);
int (*shm_restore_proc)(struct vps_snapst_ctx *, struct vps *,
    struct proc *);
int (*shm_restore_fixup)(struct vps_snapst_ctx *, struct vps *);

/* kern/sysv_msg.c */
int (*msg_snapshot_vps)(struct vps_snapst_ctx *, struct vps *);
int (*msg_snapshot_proc)(struct vps_snapst_ctx *, struct vps *,
    struct proc *);
int (*msg_restore_vps)(struct vps_snapst_ctx *, struct vps *);
int (*msg_restore_proc)(struct vps_snapst_ctx *, struct vps *,
    struct proc *);
int (*msg_restore_fixup)(struct vps_snapst_ctx *, struct vps *);

};

extern struct vps_functions *vps_func;

/* vps/vps_libdump.c */
/* vps_dumpobj_X --> vdo_X */
/* vps_libdump_X --> vld_X */
#define vdo_create 		vps_func->vps_dumpobj_create
#define vdo_space		vps_func->vps_dumpobj_space
#define vdo_append		vps_func->vps_dumpobj_append
#define vdo_close		vps_func->vps_dumpobj_close
#define vdo_discard		vps_func->vps_dumpobj_discard
#define vdo_checkobj		vps_func->vps_dumpobj_checkobj
#define vdo_setcur		vps_func->vps_dumpobj_setcur
#define vdo_next		vps_func->vps_dumpobj_next
#define vdo_prev		vps_func->vps_dumpobj_prev
#define vdo_peek		vps_func->vps_dumpobj_peek
#define vdo_getcur		vps_func->vps_dumpobj_getcur
#define vdo_typeofnext		vps_func->vps_dumpobj_typeofnext
#define vdo_nextischild		vps_func->vps_dumpobj_nextischild
#define vdo_recurse		vps_func->vps_dumpobj_recurse
#define vdo_makerelative	vps_func->vps_dumpobj_makerelative
#define vdo_makeabsolute	vps_func->vps_dumpobj_makeabsolute
#define vdo_printtree		vps_func->vps_dumpobj_printtree
#define vdo_checktree		vps_func->vps_dumpobj_checktree
#define vdo_checkptr		vps_func->vps_dumpobj_checkptr
#define vld_objtype2str		vps_func->vps_libdump_objtype2str
#define vld_checkheader		vps_func->vps_libdump_checkheader
#define vld_printheader		vps_func->vps_libdump_printheader

#endif /* VPS */

#endif /* _VPS_INT_H */

/* EOF */
