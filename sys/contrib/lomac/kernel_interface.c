/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
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
 *
 * $Id: kernel_interface.c,v 1.25 2001/10/25 21:21:59 tfraser Exp $
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/pipe.h>
#include <sys/socketvar.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include "kernel_interface.h"
#include "lomacfs.h"
#include "kernel_log.h"

/*
 * Reuse the eflags field of proc.p_vmspace->vm_map.header (since it is
 * currently not used for anything but a placeholder, and won't change
 * generally...) as storage for our process-based information.
 *
 * This is the only really effective way to make thread-based MAC
 * easy.
 */

#define	p_eflags p_vmspace->vm_map.header.eflags
#define	EF_HIGHEST_LEVEL	0x00010000
#define	EF_LOWEST_LEVEL		0x00020000
#define	EF_LEVEL_MASK		0x00030000
#define	EF_ATTR_NONETDEMOTE	0x00040000
#define	EF_ATTR_NODEMOTE	0x00080000
#define	EF_ATTR_MASK		0x000c0000

static u_int
level2subjectbits(level_t level) {

	switch (level) {
	case LOMAC_HIGHEST_LEVEL:
		return (EF_HIGHEST_LEVEL);
	case LOMAC_LOWEST_LEVEL:
		return (EF_LOWEST_LEVEL);
	default:
		panic("level2subjectbits: invalid level %d\n", level);
	}
}

static level_t
subjectbits2level(u_int flags) {

	switch (flags & EF_LEVEL_MASK) {
	case EF_HIGHEST_LEVEL:
	/*
	 * During an execve(), the kernel's original execve() creates a
	 * new vmspace and puts it into use before it has been initialized
	 * by us to contain a subject level.  Since this is the only case
	 * when a subject may have a level not set, pretend that it
	 * is just a high-level file, and allow the lomacfs_open() to then
	 * succeed.
	 */
	case 0:
		return (LOMAC_HIGHEST_LEVEL);
	case EF_LOWEST_LEVEL:
		return (LOMAC_LOWEST_LEVEL);
	default:
		panic("subjectbits2level: invalid flags %#x\n", flags);
	}
}

static u_int
attr2subjectbits(u_int attr) {
	u_int bits = 0;

	if (attr & LOMAC_ATTR_NONETDEMOTE)
		bits |= EF_ATTR_NONETDEMOTE;
	if (attr & LOMAC_ATTR_NODEMOTE)
		bits |= EF_ATTR_NODEMOTE;
	return (bits);
}

static u_int
subjectbits2attr(u_int bits) {
	u_int attr = 0;

	if (bits & EF_ATTR_NONETDEMOTE)
		attr |= LOMAC_ATTR_NONETDEMOTE;
	if (bits & EF_ATTR_NODEMOTE)
		attr |= LOMAC_ATTR_NODEMOTE;
	return (attr);
}

static int
subject_lock(lomac_subject_t *p, int read) {
#ifdef USES_LOCKMGR
	int hadlock;

	hadlock = PROC_LOCKED(p);
	if (hadlock)
		PROC_UNLOCK(p);
#endif
	if (read)
		vm_map_lock_read(&p->p_vmspace->vm_map);
	else
		vm_map_lock(&p->p_vmspace->vm_map);
#ifdef USES_LOCKMGR
	return (hadlock);
#else
	return (0);
#endif
}

static void
subject_unlock(lomac_subject_t *p, int read, int hadlock) {

	if (read)
		vm_map_unlock_read(&p->p_vmspace->vm_map);
	else
		vm_map_unlock(&p->p_vmspace->vm_map);
#ifdef USES_LOCKMGR
	if (hadlock)
		PROC_LOCK(p);
#endif
}


void
init_subject_lattr(lomac_subject_t *p, lattr_t *lattr) {
	int s;

	s = subject_lock(p, 0);
	p->p_eflags = level2subjectbits(lattr->level) |
	    attr2subjectbits(lattr->flags);
	subject_unlock(p, 0, s);
}

/*
 * Set/get the subject level on a process.  The process must not be able
 * to change, so either the process must be locked on entry or it must
 * be held in exclusivity otherwise (executing on behalf of via a syscall,
 * including as EITHER child or parent in a fork).
 */
void
set_subject_lattr(lomac_subject_t *p, lattr_t lattr) {
	int s;

#ifdef INVARIANTS
	do {
		lattr_t oslattr;

		get_subject_lattr(p, &oslattr);
		if (lomac_must_demote(&lattr, &oslattr))
			panic("raising subject level");
	} while (0);
#endif /* !INVARIANTS */
	s = subject_lock(p, 0);
	p->p_eflags = (p->p_eflags & ~(EF_LEVEL_MASK | EF_ATTR_MASK)) |
	    level2subjectbits(lattr.level) |
	    attr2subjectbits(lattr.flags);
	subject_unlock(p, 0, s);
	kernel_vm_drop_perms(curthread, &lattr);
}

void
get_subject_lattr(lomac_subject_t *p, lattr_t *lattr) {
	int s;

	s = subject_lock(p, 1);
	lattr->level = subjectbits2level(p->p_eflags);
	lattr->flags = subjectbits2attr(p->p_eflags);
	subject_unlock(p, 1, s);
}

static __inline u_int
level2lvnodebits(level_t level) {

	switch (level) {
	case LOMAC_HIGHEST_LEVEL:
		return (LN_HIGHEST_LEVEL);
	case LOMAC_LOWEST_LEVEL:
		return (LN_LOWEST_LEVEL);
	default:
		panic("level2lvnodebits: invalid level %d\n", level);
	}
}

static __inline level_t
lvnodebits2level(u_int flags) {

	switch (flags & LN_LEVEL_MASK) {
	case LN_HIGHEST_LEVEL:
		return (LOMAC_HIGHEST_LEVEL);
	case LN_LOWEST_LEVEL:
		return (LOMAC_LOWEST_LEVEL);
	default:
		panic("lvnodebits2level: invalid flags %#x\n", flags);
	}
}

static __inline unsigned int
attr2lvnodebits(unsigned int attr) {
	unsigned int bits = 0;

	if (attr & LOMAC_ATTR_LOWWRITE)
		bits |= LN_ATTR_LOWWRITE;
	if (attr & LOMAC_ATTR_LOWNOOPEN)
		bits |= LN_ATTR_LOWNOOPEN;
	if (attr & LOMAC_ATTR_NONETDEMOTE)
		bits |= LN_ATTR_NONETDEMOTE;
	if (attr & LOMAC_ATTR_NODEMOTE)
		bits |= LN_ATTR_NODEMOTE;
	return (bits);
}

static __inline unsigned int
lvnodebits2attr(unsigned int bits) {
	unsigned int attr = 0;

	if (bits & LN_ATTR_LOWWRITE)
		attr |= LOMAC_ATTR_LOWWRITE;
	if (bits & LN_ATTR_LOWNOOPEN)
		attr |= LOMAC_ATTR_LOWNOOPEN;
	if (bits & LN_ATTR_NONETDEMOTE)
		attr |= LOMAC_ATTR_NONETDEMOTE;
	if (bits & LN_ATTR_NODEMOTE)
		attr |= LOMAC_ATTR_NODEMOTE;
	return (attr);
}

/*
 * These flags correspond to the same ones set in lomac_node{}s.
 */
#define	UV_LEVEL_MASK		0x08000000
#define	UV_LOWEST_LEVEL		0x00000000
#define	UV_HIGHEST_LEVEL	0x08000000
#define	UV_ATTR_LOWWRITE	0x10000000
#define	UV_ATTR_LOWNOOPEN	0x20000000
#define	UV_ATTR_NONETDEMOTE	0x40000000
#define	UV_ATTR_NODEMOTE	0x80000000
#define	UV_ATTR_MASK		0xf0000000

static __inline u_int
level2uvnodebits(level_t level) {

	switch (level) {
	case LOMAC_HIGHEST_LEVEL:
		return (UV_HIGHEST_LEVEL);
	case LOMAC_LOWEST_LEVEL:
		return (UV_LOWEST_LEVEL);
	default:
		panic("level2uvnodebits: invalid level %d\n", level);
	}
}

static __inline level_t
uvnodebits2level(u_int flags) {

	switch (flags & UV_LEVEL_MASK) {
	case UV_HIGHEST_LEVEL:
		return (LOMAC_HIGHEST_LEVEL);
	case UV_LOWEST_LEVEL:
		return (LOMAC_LOWEST_LEVEL);
	default:
		panic("uvnodebits2level: invalid flags %#x\n", flags);
	}
}

static __inline u_int
attr2uvnodebits(u_int attr) {
	unsigned int bits = 0;

	if (attr & LOMAC_ATTR_LOWWRITE)
		bits |= UV_ATTR_LOWWRITE;
	if (attr & LOMAC_ATTR_LOWNOOPEN)
		bits |= UV_ATTR_LOWNOOPEN;
	if (attr & LOMAC_ATTR_NONETDEMOTE)
		bits |= UV_ATTR_NONETDEMOTE;
	if (attr & LOMAC_ATTR_NODEMOTE)
		bits |= UV_ATTR_NODEMOTE;
	return (bits);
}

static __inline u_int
uvnodebits2attr(u_int bits) {
	unsigned int attr = 0;

	if (bits & UV_ATTR_LOWWRITE)
		attr |= LOMAC_ATTR_LOWWRITE;
	if (bits & UV_ATTR_LOWNOOPEN)
		attr |= LOMAC_ATTR_LOWNOOPEN;
	if (bits & UV_ATTR_NONETDEMOTE)
		attr |= LOMAC_ATTR_NONETDEMOTE;
	if (bits & UV_ATTR_NODEMOTE)
		attr |= LOMAC_ATTR_NODEMOTE;
	return (attr);
}

#define	OBJ_LOWEST_LEVEL	0x8000	/* the highest level is implicit */

/*
 * This code marks pipes with levels.  We use a previously unnused bit
 * in the pipe_state field of struct pipe to store the level
 * information.  Bit clear means LOMAC_HIGHEST_LEVEL, bit set means
 * LOMAC_LOWEST_LEVEL.  Since new pipes have clear bits by default,
 * using clear bit as highest causes new pipes to start at the highest
 * level automatically.
 */
#define PIPE_LEVEL_LOWEST 0x10000000

/* This code marks sockets created by socketpair() with levels.  It
 * uses a previouslt unused bit in the so_state field of struct socket
 * to store the level information.  Bit clear means
 * LOMAC_HIGHEST_LEVEL, bit set means LOMAC_LOWEST_LEVEL.  Since new
 * sockets have clear bits by default, using clear bit as highest
 * causes new sockets to start at the highest level automatically.
 */
#define SOCKET_LEVEL_LOWEST 0x4000

void
set_object_lattr(lomac_object_t *obj, lattr_t lattr) {
	struct vnode *vp;
	struct lomac_node *ln;
	vm_object_t object;
	struct pipe *pipe;
	struct socket *socket;

	switch (obj->lo_type) {
	case LO_TYPE_LVNODE:
		KASSERT(VISLOMAC(obj->lo_object.vnode),
		    ("not a LOMACFS vnode"));
		ln = VTOLOMAC(obj->lo_object.vnode);
		ln->ln_flags =
		    (ln->ln_flags & ~(LN_LEVEL_MASK | LN_ATTR_MASK)) |
		    level2lvnodebits(lattr.level) |
		    attr2lvnodebits(lattr.flags);
		break;
	case LO_TYPE_UVNODE:
		vp = obj->lo_object.vnode;
		KASSERT(!VISLOMAC(vp), ("is a LOMACFS vnode"));
		VI_LOCK(vp);
		vp->v_flag = (vp->v_flag & ~(UV_LEVEL_MASK | UV_ATTR_MASK)) |
		    level2uvnodebits(lattr.level) |
		    attr2uvnodebits(lattr.flags);
		VI_UNLOCK(vp);
		break;
	case LO_TYPE_VM_OBJECT:
		object = obj->lo_object.vm_object;
		KASSERT(object->type != OBJT_VNODE, ("object has a vnode"));
		KASSERT(object->backing_object == NULL,
		    ("is a backing object"));
		if (lattr.level == LOMAC_HIGHEST_LEVEL)
			vm_object_clear_flag(object, OBJ_LOWEST_LEVEL);
		else
			vm_object_set_flag(object, OBJ_LOWEST_LEVEL);
		KASSERT(lattr.flags == 0, 
			("cannot set attr on a vm_object"));
		break;
	case LO_TYPE_PIPE:
		pipe = obj->lo_object.pipe;
		KASSERT(pipe->pipe_peer == NULL ||
		    (pipe->pipe_state & PIPE_LEVEL_LOWEST) ==
		    (pipe->pipe_peer->pipe_state & PIPE_LEVEL_LOWEST),
		    ("pipe attrs unsynchronized"));
		if (lattr.level == LOMAC_HIGHEST_LEVEL)
			pipe->pipe_state &= ~PIPE_LEVEL_LOWEST;
		else
			pipe->pipe_state |= PIPE_LEVEL_LOWEST;
		pipe = pipe->pipe_peer;
		if (pipe != NULL) {
			if (lattr.level == LOMAC_HIGHEST_LEVEL)
				pipe->pipe_state &= ~PIPE_LEVEL_LOWEST;
			else
				pipe->pipe_state |= PIPE_LEVEL_LOWEST;
		}
		KASSERT(lattr.flags == 0, ("cannot set attr on a pipe"));
		break;
	case LO_TYPE_SOCKETPAIR:
		socket = obj->lo_object.socket;
		/* KASSERT that socket peer levels are synchronized */
		if (lattr.level == LOMAC_HIGHEST_LEVEL)
			socket->so_state &= ~SOCKET_LEVEL_LOWEST;
		else
			socket->so_state |= SOCKET_LEVEL_LOWEST;
#ifdef NOT_YET
		pipe = pipe->pipe_peer;
		if (pipe != NULL) {
			if (lattr.level == LOMAC_HIGHEST_LEVEL)
				pipe->pipe_state &= ~PIPE_LEVEL_LOWEST;
			else
				pipe->pipe_state |= PIPE_LEVEL_LOWEST;
		}
		KASSERT(lattr.flags == 0, ("cannot set attr on a pipe"));
#endif
		break;
	default:
		panic("set_object_lattr: invalid lo_type %d", obj->lo_type);
	}
}

void
get_object_lattr(const lomac_object_t *obj, lattr_t *lattr) {
	struct vnode *vp;
	struct lomac_node *ln;
	vm_object_t object;
	struct pipe *pipe;
	struct socket *socket;

	switch (obj->lo_type) {
	case LO_TYPE_LVNODE:
		KASSERT(VISLOMAC(obj->lo_object.vnode),
		    ("not a LOMACFS vnode"));
		ln = VTOLOMAC(obj->lo_object.vnode);
		lattr->level = lvnodebits2level(ln->ln_flags);
		lattr->flags = lvnodebits2attr(ln->ln_flags);
		break;
	case LO_TYPE_UVNODE:
		vp = obj->lo_object.vnode;
		KASSERT(!VISLOMAC(vp), ("is a LOMACFS vnode"));
		VI_LOCK(vp);
		lattr->level = uvnodebits2level(vp->v_flag);
		lattr->flags = uvnodebits2attr(vp->v_flag);
		VI_UNLOCK(vp);
		break;
	case LO_TYPE_VM_OBJECT:
		object = obj->lo_object.vm_object;
		KASSERT(object->type != OBJT_VNODE, ("object has a vnode"));
		KASSERT(object->backing_object == NULL,
		    ("is a backing object"));
		lattr->level = (object->flags & OBJ_LOWEST_LEVEL) ?
		    LOMAC_LOWEST_LEVEL : LOMAC_HIGHEST_LEVEL;
		lattr->flags = 0;
		break;
	case LO_TYPE_PIPE:
		pipe = obj->lo_object.pipe;
		lattr->level = (pipe->pipe_state & PIPE_LEVEL_LOWEST) ?
		    LOMAC_LOWEST_LEVEL : LOMAC_HIGHEST_LEVEL;
		lattr->flags = 0;		
		break;
	case LO_TYPE_SOCKETPAIR:
		socket = obj->lo_object.socket;
		lattr->level = (socket->so_state & SOCKET_LEVEL_LOWEST) ?
		    LOMAC_LOWEST_LEVEL : LOMAC_HIGHEST_LEVEL;
		lattr->flags = 0;		
		break;
	default:
		panic("get_object_level: invalid lo_type %d", obj->lo_type);
	}
}

/*
 * Flag certain procs, like init(8) and kthreads, as "invincible".
 */
int
subject_do_not_demote(lomac_subject_t *subj) {
	int inv = 0;

	if (subj->p_pid == 1) {
		inv = 1;
	} else {
		int had_lock = PROC_LOCKED(subj);

		if (!had_lock)
			PROC_LOCK(subj);
		if (subj->p_flag & P_SYSTEM)
			inv = 1;
		if (!had_lock)
			PROC_UNLOCK(subj);
	}
	return (inv);
}
