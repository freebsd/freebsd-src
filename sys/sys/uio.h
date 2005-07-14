/*-
 * Copyright (c) 1982, 1986, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)uio.h	8.5 (Berkeley) 2/22/94
 * $FreeBSD$
 */

#ifndef _SYS_UIO_H_
#define	_SYS_UIO_H_

#include <sys/cdefs.h>
#include <sys/_types.h>
#include <sys/_iovec.h>

#ifndef _SSIZE_T_DECLARED
typedef	__ssize_t	ssize_t;
#define	_SSIZE_T_DECLARED
#endif

#ifndef _OFF_T_DECLARED
typedef	__off_t	off_t;
#define	_OFF_T_DECLARED
#endif

#if __BSD_VISIBLE
enum	uio_rw { UIO_READ, UIO_WRITE };

/* Segment flag values. */
enum uio_seg {
	UIO_USERSPACE,		/* from user data space */
	UIO_SYSSPACE,		/* from system space */
	UIO_NOCOPY		/* don't copy, already in object */
};
#endif

#ifdef _KERNEL

struct uio {
	struct	iovec *uio_iov;
	int	uio_iovcnt;
	off_t	uio_offset;
	int	uio_resid;
	enum	uio_seg uio_segflg;
	enum	uio_rw uio_rw;
	struct	thread *uio_td;
};

/*
 * Limits
 *
 * N.B.: UIO_MAXIOV must be no less than IOV_MAX from <sys/syslimits.h>
 * which in turn must be no less than _XOPEN_IOV_MAX from <limits.h>.  If
 * we ever make this tunable (probably pointless), then IOV_MAX should be
 * removed from <sys/syslimits.h> and applications would be expected to use
 * sysconf(3) to find out the correct value, or else assume the worst
 * (_XOPEN_IOV_MAX).  Perhaps UIO_MAXIOV should be simply defined as
 * IOV_MAX.
 */
#define UIO_MAXIOV	1024		/* max 1K of iov's */

struct vm_object;
struct vm_page;

struct uio *cloneuio(struct uio *uiop);
int	copyinfrom(const void * __restrict src, void * __restrict dst,
	    size_t len, int seg);
int	copyiniov(struct iovec *iovp, u_int iovcnt, struct iovec **iov,
	    int error);
int	copyinstrfrom(const void * __restrict src, void * __restrict dst,
	    size_t len, size_t * __restrict copied, int seg);
int	copyinuio(struct iovec *iovp, u_int iovcnt, struct uio **uiop);
void	uio_yield(void);
int	uiomove(void *cp, int n, struct uio *uio);
int	uiomove_frombuf(void *buf, int buflen, struct uio *uio);
int	uiomove_fromphys(struct vm_page *ma[], vm_offset_t offset, int n,
	    struct uio *uio);
int	uiomoveco(void *cp, int n, struct uio *uio, struct vm_object *obj,
	    int disposable);

#else /* !_KERNEL */

__BEGIN_DECLS
ssize_t	readv(int, const struct iovec *, int);
ssize_t	writev(int, const struct iovec *, int);
ssize_t	preadv(int, const struct iovec *, int, off_t);
ssize_t	pwritev(int, const struct iovec *, int, off_t);
__END_DECLS

#endif /* _KERNEL */

#endif /* !_SYS_UIO_H_ */
