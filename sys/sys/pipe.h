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
 * $Id: pipe.h,v 1.1 1996/01/28 23:38:22 dyson Exp $
 */

#ifndef OLD_PIPE

struct vm_object;

/*
 * pipe buffer information
 * Separate in, out, cnt is used to simplify calculations.
 */
struct pipebuf {
	u_int	cnt;		/* number of chars currently in buffer */
	u_int	in;		/* in pointer */
	u_int	out;		/* out pointer */
	u_int	size;		/* size of buffer */
	caddr_t	buffer;		/* kva of buffer */
	struct	vm_object *object; /* VM object containing buffer */
};

/*
 * pipe_state bits
 */
#define PIPE_NBIO 0x1		/* non-blocking I/O */
#define PIPE_ASYNC 0x4		/* Async? I/O */
#define PIPE_WANTR 0x8		/* Reader wants some characters */
#define PIPE_WANTW 0x10		/* Writer wants space to put characters */
#define PIPE_WANT 0x20		/* Pipe is wanted to be run-down */
#define PIPE_SEL 0x40		/* Pipe has a select active */
#define PIPE_EOF 0x80		/* Pipe is in EOF condition */
#define PIPE_LOCK 0x100		/* Process has exclusive access to pointers/data */
#define PIPE_LWANT 0x200	/* Process wants exclusive access to pointers/data */

/*
 * Per-pipe data structure
 * Two of these are linked together to produce bi-directional
 * pipes.
 */
struct pipe {
	struct	pipebuf pipe_buffer;	/* data storage */
	struct	selinfo pipe_sel;	/* for compat with select */
	struct	timeval pipe_atime;	/* time of last access */
	struct	timeval pipe_mtime;	/* time of last modify */
	struct	timeval pipe_ctime;	/* time of status change */
	int	pipe_pgid;
	struct	pipe	*pipe_peer;	/* link with other direction */
	u_int	pipe_state;		/* pipe status info */
	int	pipe_busy;		/* busy flag, mostly to handle rundown sanely */
};

int pipe_stat __P((struct pipe *pipe, struct stat *ub));

#endif
