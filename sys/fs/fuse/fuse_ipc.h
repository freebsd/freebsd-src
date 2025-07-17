/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2007-2009 Google Inc. and Amit Singh
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of Google Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Copyright (C) 2005 Csaba Henk.
 * All rights reserved.
 *
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * Portions of this software were developed by BFF Storage Systems, LLC under
 * sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _FUSE_IPC_H_
#define _FUSE_IPC_H_

#include <sys/param.h>
#include <sys/refcount.h>

enum fuse_data_cache_mode {
	FUSE_CACHE_UC,
	FUSE_CACHE_WT,
	FUSE_CACHE_WB,
};

struct fuse_iov {
	void   *base;
	size_t  len;
	size_t  allocated_size;
	int     credit;
};

void fiov_init(struct fuse_iov *fiov, size_t size);
void fiov_teardown(struct fuse_iov *fiov);
void fiov_refresh(struct fuse_iov *fiov);
void fiov_adjust(struct fuse_iov *fiov, size_t size);

#define FUSE_DIMALLOC(fiov, spc1, spc2, amnt) do {		\
	fiov_adjust(fiov, (sizeof(*(spc1)) + (amnt)));		\
	(spc1) = (fiov)->base;					\
	(spc2) = (char *)(fiov)->base + (sizeof(*(spc1)));	\
} while (0)

#define FU_AT_LEAST(siz) max((siz), 160)

#define FUSE_ASSERT_AW_DONE(ftick)					\
	KASSERT((ftick)->tk_aw_link.tqe_next == NULL &&			\
	    (ftick)->tk_aw_link.tqe_prev == NULL,			\
	    ("FUSE: ticket still on answer delivery list %p", (ftick)))

#define FUSE_ASSERT_MS_DONE(ftick)				\
	KASSERT((ftick)->tk_ms_link.stqe_next == NULL,		\
	    ("FUSE: ticket still on message list %p", (ftick)))

struct fuse_ticket;
struct fuse_data;

typedef int fuse_handler_t(struct fuse_ticket *ftick, struct uio *uio);

struct fuse_ticket {
	/* fields giving the identity of the ticket */
	uint64_t			tk_unique;
	struct fuse_data		*tk_data;
	int				tk_flag;
	u_int				tk_refcount;
	/* 
	 * If this ticket's operation has been interrupted, this will hold the
	 * unique value of the FUSE_INTERRUPT operation.  Otherwise, it will be
	 * 0.
	 */
	uint64_t			irq_unique;

	/* fields for initiating an upgoing message */
	struct fuse_iov			tk_ms_fiov;
	STAILQ_ENTRY(fuse_ticket)	tk_ms_link;

	/* fields for handling answers coming from userspace */
	struct fuse_iov			tk_aw_fiov;
	struct fuse_out_header		tk_aw_ohead;
	int				tk_aw_errno;
	struct mtx			tk_aw_mtx;
	fuse_handler_t			*tk_aw_handler;
	TAILQ_ENTRY(fuse_ticket)	tk_aw_link;
};

#define FT_ANSW  0x01  /* request of ticket has already been answered */
#define FT_DIRTY 0x04  /* ticket has been used */

static inline struct fuse_iov *
fticket_resp(struct fuse_ticket *ftick)
{
	return (&ftick->tk_aw_fiov);
}

static inline bool
fticket_answered(struct fuse_ticket *ftick)
{
	mtx_assert(&ftick->tk_aw_mtx, MA_OWNED);
	return (ftick->tk_flag & FT_ANSW);
}

static inline void
fticket_set_answered(struct fuse_ticket *ftick)
{
	mtx_assert(&ftick->tk_aw_mtx, MA_OWNED);
	ftick->tk_flag |= FT_ANSW;
}

static inline struct fuse_in_header*
fticket_in_header(struct fuse_ticket *ftick)
{
	return (struct fuse_in_header *)(ftick->tk_ms_fiov.base);
}

static inline enum fuse_opcode
fticket_opcode(struct fuse_ticket *ftick)
{
	return fticket_in_header(ftick)->opcode;
}

int fticket_pull(struct fuse_ticket *ftick, struct uio *uio);

/*
 * The data representing a FUSE session.
 */
struct fuse_data {
	struct cdev			*fdev;
	struct mount			*mp;
	struct vnode			*vroot;
	struct ucred			*daemoncred;
	int				dataflags;
	int				ref;

	struct mtx			ms_mtx;
	STAILQ_HEAD(, fuse_ticket)	ms_head;
	int				ms_count;

	struct mtx			aw_mtx;
	TAILQ_HEAD(, fuse_ticket)	aw_head;

	/* 
	 * Holds the next value of the FUSE operation unique value.
	 * Also, serves as a wakeup channel to prevent any operations from
	 * being created before INIT completes.
	 */
	u_long				ticketer;

	struct sx			rename_lock;

	uint32_t			fuse_libabi_major;
	uint32_t			fuse_libabi_minor;

	uint32_t			max_readahead_blocks;
	uint32_t			max_write;
	uint32_t			max_read;

	struct selinfo			ks_rsel;

	int				daemon_timeout;
	int				linux_errnos;
	unsigned			time_gran;
	/* A bitmask of FUSE RPCs that are not implemented by the server */
	uint64_t			notimpl;
	/*
	 * A bitmask of FUSE RPCs that are implemented by the server.
	 * If an operation is not present in either notimpl or isimpl, then it
	 * may be implemented by the server, but the kernel doesn't know for
	 * sure.
	 */
	uint64_t			isimpl;
	uint64_t			mnt_flag;
	enum fuse_data_cache_mode	cache_mode;
};

#define FSESS_DEAD                0x0001 /* session is to be closed */
#define FSESS_INITED              0x0004 /* session has been inited */
#define FSESS_DAEMON_CAN_SPY      0x0010 /* let non-owners access this fs */
                                         /* (and being observed by the daemon) */
#define FSESS_PUSH_SYMLINKS_IN    0x0020 /* prefix absolute symlinks with mp */
#define FSESS_DEFAULT_PERMISSIONS 0x0040 /* kernel does permission checking */
#define FSESS_ASYNC_READ          0x1000 /* allow multiple reads of some file */
#define FSESS_POSIX_LOCKS         0x2000 /* daemon supports POSIX locks */
#define FSESS_EXPORT_SUPPORT      0x10000 /* daemon supports NFS-style lookups */
#define FSESS_INTR                0x20000 /* interruptible mounts */
#define FSESS_WARN_SHORT_WRITE    0x40000 /* Short write without direct_io */
#define FSESS_WARN_WROTE_LONG     0x80000 /* Wrote more data than provided */
#define FSESS_WARN_LSEXTATTR_LONG 0x100000 /* Returned too many extattrs */
#define FSESS_WARN_CACHE_INCOHERENT 0x200000	/* Read cache incoherent */
#define FSESS_WARN_WB_CACHE_INCOHERENT 0x400000	/* WB cache incoherent */
#define	FSESS_WARN_ILLEGAL_INODE  0x800000 /* Illegal inode for new file */
#define FSESS_WARN_READLINK_EMBEDDED_NUL 0x1000000 /* corrupt READLINK output */
#define FSESS_WARN_DOT_LOOKUP	  0x2000000 /* Inconsistent . LOOKUP response */
#define FSESS_WARN_INODE_MISMATCH 0x4000000 /* ino != nodeid */
#define FSESS_MNTOPTS_MASK	( \
	FSESS_DAEMON_CAN_SPY | FSESS_PUSH_SYMLINKS_IN | \
	FSESS_DEFAULT_PERMISSIONS | FSESS_INTR)

extern int fuse_data_cache_mode;

static inline struct fuse_data *
fuse_get_mpdata(struct mount *mp)
{
	return mp->mnt_data;
}

static inline bool
fsess_is_impl(struct mount *mp, int opcode)
{
	struct fuse_data *data = fuse_get_mpdata(mp);

	return ((data->isimpl & (1ULL << opcode)) != 0);

}

static inline bool
fsess_maybe_impl(struct mount *mp, int opcode)
{
	struct fuse_data *data = fuse_get_mpdata(mp);

	return ((data->notimpl & (1ULL << opcode)) == 0);

}

static inline bool
fsess_not_impl(struct mount *mp, int opcode)
{
	struct fuse_data *data = fuse_get_mpdata(mp);

	return ((data->notimpl & (1ULL << opcode)) != 0);

}

static inline void
fsess_set_impl(struct mount *mp, int opcode)
{
	struct fuse_data *data = fuse_get_mpdata(mp);

	data->isimpl |= (1ULL << opcode);
}

static inline void
fsess_set_notimpl(struct mount *mp, int opcode)
{
	struct fuse_data *data = fuse_get_mpdata(mp);

	data->notimpl |= (1ULL << opcode);
}

static inline bool
fsess_opt_datacache(struct mount *mp)
{
	struct fuse_data *data = fuse_get_mpdata(mp);

	return (data->cache_mode != FUSE_CACHE_UC);
}

static inline bool
fsess_opt_mmap(struct mount *mp)
{
	return (fsess_opt_datacache(mp));
}

static inline bool
fsess_opt_writeback(struct mount *mp)
{
	struct fuse_data *data = fuse_get_mpdata(mp);

	return (data->cache_mode == FUSE_CACHE_WB);
}

/* Insert a new upgoing message */
static inline void
fuse_ms_push(struct fuse_ticket *ftick)
{
	mtx_assert(&ftick->tk_data->ms_mtx, MA_OWNED);
	refcount_acquire(&ftick->tk_refcount);
	STAILQ_INSERT_TAIL(&ftick->tk_data->ms_head, ftick, tk_ms_link);
	ftick->tk_data->ms_count++;
}

/* Insert a new upgoing message to the front of the queue */
static inline void
fuse_ms_push_head(struct fuse_ticket *ftick)
{
	mtx_assert(&ftick->tk_data->ms_mtx, MA_OWNED);
	refcount_acquire(&ftick->tk_refcount);
	STAILQ_INSERT_HEAD(&ftick->tk_data->ms_head, ftick, tk_ms_link);
	ftick->tk_data->ms_count++;
}

static inline struct fuse_ticket *
fuse_ms_pop(struct fuse_data *data)
{
	struct fuse_ticket *ftick = NULL;

	mtx_assert(&data->ms_mtx, MA_OWNED);

	if ((ftick = STAILQ_FIRST(&data->ms_head))) {
		STAILQ_REMOVE_HEAD(&data->ms_head, tk_ms_link);
		data->ms_count--;
#ifdef INVARIANTS
		MPASS(data->ms_count >= 0);
		ftick->tk_ms_link.stqe_next = NULL;
#endif
	}

	return (ftick);
}

static inline void
fuse_aw_push(struct fuse_ticket *ftick)
{
	mtx_assert(&ftick->tk_data->aw_mtx, MA_OWNED);
	refcount_acquire(&ftick->tk_refcount);
	TAILQ_INSERT_TAIL(&ftick->tk_data->aw_head, ftick, tk_aw_link);
}

static inline void
fuse_aw_remove(struct fuse_ticket *ftick)
{
	mtx_assert(&ftick->tk_data->aw_mtx, MA_OWNED);
	TAILQ_REMOVE(&ftick->tk_data->aw_head, ftick, tk_aw_link);
#ifdef INVARIANTS
	ftick->tk_aw_link.tqe_next = NULL;
	ftick->tk_aw_link.tqe_prev = NULL;
#endif
}

static inline struct fuse_ticket *
fuse_aw_pop(struct fuse_data *data)
{
	struct fuse_ticket *ftick;

	mtx_assert(&data->aw_mtx, MA_OWNED);

	if ((ftick = TAILQ_FIRST(&data->aw_head)) != NULL)
		fuse_aw_remove(ftick);

	return (ftick);
}

struct fuse_ticket *fuse_ticket_fetch(struct fuse_data *data);
int fuse_ticket_drop(struct fuse_ticket *ftick);
void fuse_insert_callback(struct fuse_ticket *ftick, fuse_handler_t *handler);
void fuse_insert_message(struct fuse_ticket *ftick, bool irq);

static inline bool
fuse_libabi_geq(struct fuse_data *data, uint32_t abi_maj, uint32_t abi_min)
{
	return (data->fuse_libabi_major > abi_maj ||
	    (data->fuse_libabi_major == abi_maj &&
	     data->fuse_libabi_minor >= abi_min));
}

/* Print msg as a warning to the console, but no more than once per session */
void fuse_warn(struct fuse_data *data, unsigned flag, const char *msg);

struct fuse_data *fdata_alloc(struct cdev *dev, struct ucred *cred);
void fdata_trydestroy(struct fuse_data *data);
void fdata_set_dead(struct fuse_data *data);

static inline bool
fdata_get_dead(struct fuse_data *data)
{
	return (data->dataflags & FSESS_DEAD);
}

struct fuse_dispatcher {
	struct fuse_ticket    *tick;
	struct fuse_in_header *finh;

	void    *indata;
	size_t   iosize;
	uint64_t nodeid;
	int      answ_stat;
	void    *answ;
};

static inline void
fdisp_init(struct fuse_dispatcher *fdisp, size_t iosize)
{
	fdisp->iosize = iosize;
	fdisp->tick = NULL;
}

static inline void
fdisp_destroy(struct fuse_dispatcher *fdisp)
{
	fuse_ticket_drop(fdisp->tick);
#ifdef INVARIANTS
	fdisp->tick = NULL;
#endif
}

void fdisp_make(struct fuse_dispatcher *fdip, enum fuse_opcode op,
    struct mount *mp, uint64_t nid, struct thread *td, struct ucred *cred);

void fdisp_make_vp(struct fuse_dispatcher *fdip, enum fuse_opcode op,
    struct vnode *vp, struct thread *td, struct ucred *cred);

void fdisp_refresh_vp(struct fuse_dispatcher *fdip, enum fuse_opcode op,
    struct vnode *vp, struct thread *td, struct ucred *cred);

int fdisp_wait_answ(struct fuse_dispatcher *fdip);

static inline int
fdisp_simple_putget_vp(struct fuse_dispatcher *fdip, enum fuse_opcode op,
    struct vnode *vp, struct thread *td, struct ucred *cred)
{
	fdisp_make_vp(fdip, op, vp, td, cred);
	return (fdisp_wait_answ(fdip));
}

#endif /* _FUSE_IPC_H_ */
