/*
 *
 * Copyright (c) 2015 Mark Johnston <markj@FreeBSD.org>
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#ifndef _SYS_UCOREDUMP_H_
#define _SYS_UCOREDUMP_H_

#ifdef _KERNEL

#include <sys/_uio.h>

/* Coredump output parameters. */
struct coredump_params;
struct coredump_writer;
struct thread;
struct ucred;

typedef int coredump_init_fn(const struct coredump_writer *,
    const struct coredump_params *);
typedef int coredump_write_fn(const struct coredump_writer *, const void *, size_t,
    off_t, enum uio_seg, struct ucred *, size_t *, struct thread *);
typedef int coredump_extend_fn(const struct coredump_writer *, off_t,
    struct ucred *);

struct coredump_vnode_ctx {
	struct vnode	*vp;
	struct ucred	*fcred;
};

coredump_write_fn core_vn_write;
coredump_extend_fn core_vn_extend;
int coredump_vnode(struct thread *, off_t);

struct coredump_writer {
	void			*ctx;
	coredump_init_fn	*init_fn;
	coredump_write_fn	*write_fn;
	coredump_extend_fn	*extend_fn;
};

struct coredump_params {
	off_t		offset;
	struct ucred	*active_cred;
	struct thread	*td;
	const struct coredump_writer	*cdw;
	struct compressor *comp;
};

#define   CORE_BUF_SIZE   (16 * 1024)

int core_write(struct coredump_params *, const void *, size_t, off_t,
    enum uio_seg, size_t *);
int core_output(char *, size_t, off_t, struct coredump_params *, void *);
int sbuf_drain_core_output(void *, const char *, int);

extern int coredump_pack_fileinfo;
extern int coredump_pack_vmmapinfo;

extern int compress_user_cores;
extern int compress_user_cores_level;

#endif	/* _KERNEL */
#endif	/* _SYS_UCOREDUMP_H_ */
