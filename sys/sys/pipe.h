/*
 * Copyright (c) 1996 John S. Dyson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    John S. Dyson.
 * 4. This work was done expressly for inclusion into FreeBSD.  Other use
 *    is allowed if this notation is included.
 * 5. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 * $FreeBSD$
 */

#ifndef _SYS_PIPE_H_
#define _SYS_PIPE_H_

#ifndef _KERNEL
#include <sys/time.h>			/* for struct timespec */
#include <sys/selinfo.h>		/* for struct selinfo */
#include <vm/vm.h>			/* for vm_page_t */
#include <sys/_label.h>			/* for struct label */
#include <machine/param.h>		/* for PAGE_SIZE */
#endif

/*
 * Pipe buffer size, keep moderate in value, pipes take kva space.
 */
#ifndef PIPE_SIZE
#define PIPE_SIZE	16384
#endif

#ifndef BIG_PIPE_SIZE
#define BIG_PIPE_SIZE	(64*1024)
#endif

/*
 * PIPE_MINDIRECT MUST be smaller than PIPE_SIZE and MUST be bigger
 * than PIPE_BUF.
 */
#ifndef PIPE_MINDIRECT
#define PIPE_MINDIRECT	8192
#endif

#define PIPENPAGES	(BIG_PIPE_SIZE / PAGE_SIZE + 1)

/*
 * Pipe buffer information.
 * Separate in, out, cnt are used to simplify calculations.
 * Buffered write is active when the buffer.cnt field is set.
 */
struct pipebuf {
	u_int	cnt;		/* number of chars currently in buffer */
	u_int	in;		/* in pointer */
	u_int	out;		/* out pointer */
	u_int	size;		/* size of buffer */
	caddr_t	buffer;		/* kva of buffer */
	struct	vm_object *object;	/* VM object containing buffer */
};

/*
 * Information to support direct transfers between processes for pipes.
 */
struct pipemapping {
	vm_offset_t	kva;		/* kernel virtual address */
	vm_size_t	cnt;		/* number of chars in buffer */
	vm_size_t	pos;		/* current position of transfer */
	int		npages;		/* number of pages */
	vm_page_t	ms[PIPENPAGES];	/* pages in source process */
};

/*
 * Bits in pipe_state.
 */
#define PIPE_ASYNC	0x004	/* Async? I/O. */
#define PIPE_WANTR	0x008	/* Reader wants some characters. */
#define PIPE_WANTW	0x010	/* Writer wants space to put characters. */
#define PIPE_WANT	0x020	/* Pipe is wanted to be run-down. */
#define PIPE_SEL	0x040	/* Pipe has a select active. */
#define PIPE_EOF	0x080	/* Pipe is in EOF condition. */
#define PIPE_LOCKFL	0x100	/* Process has exclusive access to pointers/data. */
#define PIPE_LWANT	0x200	/* Process wants exclusive access to pointers/data. */
#define PIPE_DIRECTW	0x400	/* Pipe direct write active. */
#define PIPE_DIRECTOK	0x800	/* Direct mode ok. */

/*
 * Per-pipe data structure.
 * Two of these are linked together to produce bi-directional pipes.
 */
struct pipe {
	struct	pipebuf pipe_buffer;	/* data storage */
	struct	pipemapping pipe_map;	/* pipe mapping for direct I/O */
	struct	selinfo pipe_sel;	/* for compat with select */
	struct	timespec pipe_atime;	/* time of last access */
	struct	timespec pipe_mtime;	/* time of last modify */
	struct	timespec pipe_ctime;	/* time of status change */
	struct	sigio *pipe_sigio;	/* information for async I/O */
	struct	pipe *pipe_peer;	/* link with other direction */
	u_int	pipe_state;		/* pipe status info */
	int	pipe_busy;		/* busy flag, mostly to handle rundown sanely */
	struct	label *pipe_label;	/* pipe MAC label - shared */
	struct	mtx *pipe_mtxp;		/* shared mutex between both pipes */
};

#define PIPE_MTX(pipe)		(pipe)->pipe_mtxp
#define PIPE_LOCK(pipe)		mtx_lock(PIPE_MTX(pipe))
#define PIPE_UNLOCK(pipe)	mtx_unlock(PIPE_MTX(pipe))
#define PIPE_LOCK_ASSERT(pipe, type)  mtx_assert(PIPE_MTX(pipe), (type))


#endif /* !_SYS_PIPE_H_ */
