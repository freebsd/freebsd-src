/*
 * Copyright (c) 1997 John S. Dyson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. John S. Dyson's name may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * DISCLAIMER:  This code isn't warranted to do anything useful.  Anything
 * bad that happens because of using this software isn't the responsibility
 * of the author.  This software is distributed AS-IS.
 *
 * $FreeBSD$
 */

#ifndef _SYS_AIO_H_
#define	_SYS_AIO_H_

#include <sys/time.h>
#include <sys/types.h>
#include <sys/signal.h>

/*
 * Returned by aio_cancel:
 */
#define	AIO_CANCELED		0x1
#define	AIO_NOTCANCELED		0x2
#define	AIO_ALLDONE		0x3

/*
 * LIO opcodes
 */
#define	LIO_NOP			0x0
#define LIO_WRITE		0x1
#define	LIO_READ		0x2

/*
 * LIO modes
 */
#define	LIO_NOWAIT		0x0
#define	LIO_WAIT		0x1

/*
 * Maximum number of allowed LIO operations
 */
#define	AIO_LISTIO_MAX		16

/*
 * Private members for aiocb -- don't access
 * directly.
 */
struct __aiocb_private {
	long	status;
	long	error;
	void	*kernelinfo;
};

/*
 * I/O control block
 */
typedef struct aiocb {
	int	aio_fildes;		/* File descriptor */
	off_t	aio_offset;		/* File offset for I/O */
	volatile void *aio_buf;         /* I/O buffer in process space */
	size_t	aio_nbytes;		/* Number of bytes for I/O */
	struct	sigevent aio_sigevent;	/* Signal to deliver */
	int	aio_lio_opcode;		/* LIO opcode */
	int	aio_reqprio;		/* Request priority -- ignored */
	struct	__aiocb_private	_aiocb_private;
} aiocb_t;

#ifndef _KERNEL

__BEGIN_DECLS
/*
 * Asynchronously read from a file
 */
int	aio_read(struct aiocb *);

/*
 * Asynchronously write to file
 */
int	aio_write(struct aiocb *);

/*
 * List I/O Asynchronously/synchronously read/write to/from file
 *	"lio_mode" specifies whether or not the I/O is synchronous.
 *	"acb_list" is an array of "nacb_listent" I/O control blocks.
 *	when all I/Os are complete, the optional signal "sig" is sent.
 */
int	lio_listio(int, struct aiocb * const [], int, struct sigevent *);

/*
 * Get completion status
 *	returns EINPROGRESS until I/O is complete.
 *	this routine does not block.
 */
int	aio_error(const struct aiocb *);

/*
 * Finish up I/O, releasing I/O resources and returns the value
 *	that would have been associated with a synchronous I/O request.
 *	This routine must be called once and only once for each
 *	I/O control block who has had I/O associated with it.
 */
ssize_t	aio_return(struct aiocb *);

/*
 * Cancel I/O
 */
int	aio_cancel(int, struct aiocb *);

/*
 * Suspend until all specified I/O or timeout is complete.
 */
int	aio_suspend(const struct aiocb * const[], int, const struct timespec *);

int	aio_waitcomplete(struct aiocb **, struct timespec *);

__END_DECLS

#else

/* Forward declarations for prototypes below. */
struct socket;
struct sockbuf;

extern void (*aio_swake)(struct socket *, struct sockbuf *);

#endif

#endif
