/* XXX Conflicts with John's - not installed.
 */
/*-
 * Copyright (c) 1996, 1997
 *	HD Associates, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by HD Associates, Inc
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY HD ASSOCIATES AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL HD ASSOCIATES OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: aio.h,v 1.3 1998/03/23 14:05:25 bde Exp $
 */

/* aio.h: P1003.1B-1993 Asynchronous I/O */

#ifndef _P1003_1B_AIO_H_
#define _P1003_1B_AIO_H_

#include <sys/_posix.h>
#include <sys/types.h>

/* For struct sigevent:
 */
#ifdef KERNEL
#include <sys/signal.h>
#else
#include <signal.h>

#ifdef _P1003_1B_INCLUDE_MAYBES
#include <time.h>
#include <fcntl.h>
#else
struct timespec;
#endif
#endif

/* Return values: */

#define AIO_CANCELED		0x01	/* All operations cancelled */
#define AIO_NOTCANCELLED	0x02	/* Some not cancelled */
#define AIO_ALLDONE		0x04	/* None were cancelled */

/* lio_listio synchronization options */

#define LIO_WAIT		0x08	/* Suspend until complete */
#define LIO_NOWAIT		0x10	/* Continue operation */

/* lio_listio element operations */

#define LIO_READ		0x20
#define LIO_WRITE		0x40
#define LIO_NOP			0x80

typedef struct aiocb * const aio_listio_ctl;
typedef const struct aiocb * const caio_listio_ctl;

struct aiocb {
	int		aio_fildes;	/* File descriptor */
	off_t		aio_offset;	/* File offset */
	volatile void *	aio_buf;	/* Location of buffer */
	size_t		aio_nbytes;	/* Length of transfer */
	int		aio_reqprio;	/* Request priority offset */
	struct sigevent	aio_sigevent;	/* Signal number and value */
	int		aio_lio_opcode;	/* Operation to be performed */
};

#ifndef KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int aio_read __P((struct aiocb *));
int aio_write __P((struct aiocb *));

int lio_listio __P((int, aio_listio_ctl[], int, struct sigevent *));

int aio_error __P((const struct aiocb *));
ssize_t aio_return __P((struct aiocb *));
int aio_cancel __P((int, struct aiocb *));

int aio_suspend __P((caio_listio_ctl[], int, const struct timespec *));

int aio_fsync __P((int, struct aiocb *));
__END_DECLS

#endif /* KERNEL */

#endif /* _P1003_1B_AIO_H_ */
