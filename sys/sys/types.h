/*-
 * Copyright (c) 1982, 1986, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)types.h	8.6 (Berkeley) 2/19/95
 * $FreeBSD$
 */

#ifndef _SYS_TYPES_H_
#define	_SYS_TYPES_H_

#include <sys/cdefs.h>

/* Machine type dependent parameters. */
#include <sys/stdint.h>			/* includes <machine/ansi.h> */
#include <machine/types.h>

#ifndef _POSIX_SOURCE
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;
typedef	unsigned short	ushort;		/* Sys V compatibility */
typedef	unsigned int	uint;		/* Sys V compatibility */
#endif

typedef __uint8_t	u_int8_t;
typedef __uint16_t	u_int16_t;
typedef __uint32_t	u_int32_t;
typedef __uint64_t	u_int64_t;

typedef	u_int64_t	u_quad_t;	/* quads */
typedef	int64_t		quad_t;
typedef	quad_t *	qaddr_t;

typedef	char *		caddr_t;	/* core address */
typedef	__const char *	c_caddr_t;	/* core address, pointer to const */
typedef	__volatile char *v_caddr_t;	/* core address, pointer to volatile */
typedef	int32_t		daddr_t;	/* disk address */
typedef	u_int32_t	u_daddr_t;	/* unsigned disk address */
typedef	u_int32_t	fixpt_t;	/* fixed point number */
typedef	u_int32_t	gid_t;		/* group id */
typedef	u_int32_t	ino_t;		/* inode number */
typedef	long		key_t;		/* IPC key (for Sys V IPC) */
typedef	u_int16_t	mode_t;		/* permissions */
typedef	u_int16_t	nlink_t;	/* link count */
typedef	_BSD_OFF_T_	off_t;		/* file offset */
typedef	_BSD_PID_T_	pid_t;		/* process id */
typedef	quad_t		rlim_t;		/* resource limit */
#ifdef __alpha__		/* XXX should be in <machine/types.h> */
typedef	int64_t		segsz_t;	/* segment size */
#else
typedef	int32_t		segsz_t;	/* segment size */
#endif
typedef	int32_t		swblk_t;	/* swap offset */
typedef	int32_t		ufs_daddr_t;
typedef	int32_t		ufs_time_t;
typedef	u_int32_t	uid_t;		/* user id */

#ifdef _KERNEL
typedef	int		boolean_t;
typedef	u_int64_t	uoff_t;
typedef	struct vm_page	*vm_page_t;

struct specinfo;

typedef	u_int32_t	udev_t;		/* device number */
typedef struct specinfo	*dev_t;

#define offsetof(type, field) __offsetof(type, field)

#else /* !_KERNEL */

typedef	u_int32_t	dev_t;		/* device number */
#define udev_t dev_t

#ifndef _POSIX_SOURCE

/*
 * minor() gives a cookie instead of an index since we don't want to
 * change the meanings of bits 0-15 or waste time and space shifting
 * bits 16-31 for devices that don't use them.
 */
#define major(x)        ((int)(((u_int)(x) >> 8)&0xff)) /* major number */
#define minor(x)        ((int)((x)&0xffff00ff))         /* minor number */
#define makedev(x,y)    ((dev_t)(((x) << 8) | (y)))     /* create dev_t */

#endif /* _POSIX_SOURCE */

#endif /* !_KERNEL */

/*
 * XXX: Deprecated;
 * byteorder(3) functions now defined in <arpa/inet.h>.
 */
#include <machine/endian.h>

#ifdef	_BSD_CLOCK_T_
typedef	_BSD_CLOCK_T_	clock_t;
#undef	_BSD_CLOCK_T_
#endif

#ifdef	_BSD_CLOCKID_T_
typedef	_BSD_CLOCKID_T_	clockid_t;
#undef	_BSD_CLOCKID_T_
#endif

/* XXX: Deprecated; now defined in <arpa/inet.h>. */
#ifndef _IN_ADDR_T_DECLARED_
typedef	__uint32_t	in_addr_t;
#define	_IN_ADDR_T_DECLARED_
#endif

/* XXX: Deprecated; now defined in <arpa/inet.h>. */
#ifndef _IN_PORT_T_DECLARED_
typedef	__uint16_t	in_port_t;
#define	_IN_PORT_T_DECLARED_
#endif

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#ifdef	_BSD_SSIZE_T_
typedef	_BSD_SSIZE_T_	ssize_t;
#undef	_BSD_SSIZE_T_
#endif

#ifdef	_BSD_TIME_T_
typedef	_BSD_TIME_T_	time_t;
#undef	_BSD_TIME_T_
#endif

#ifdef	_BSD_TIMER_T_
typedef	_BSD_TIMER_T_	timer_t;
#undef	_BSD_TIMER_T_
#endif

#ifndef _POSIX_SOURCE
#define	NBBY	8		/* number of bits in a byte */

/*
 * Select uses bit masks of file descriptors in longs.  These macros
 * manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here should
 * be enough for most uses.
 */
#ifndef	FD_SETSIZE
#define	FD_SETSIZE	1024
#endif

typedef	unsigned long	fd_mask;
#define	NFDBITS	(sizeof(fd_mask) * NBBY)	/* bits per mask */

#ifndef howmany
#define	howmany(x, y)	(((x) + ((y) - 1)) / (y))
#endif

typedef	struct fd_set {
	fd_mask	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;

#define	_fdset_mask(n)	((fd_mask)1 << ((n) % NFDBITS))
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= _fdset_mask(n))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~_fdset_mask(n))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & _fdset_mask(n))
#define	FD_COPY(f, t)	bcopy(f, t, sizeof(*(f)))
#define	FD_ZERO(p)	bzero(p, sizeof(*(p)))

/*
 * These declarations belong elsewhere, but are repeated here and in
 * <stdio.h> to give broken programs a better chance of working with
 * 64-bit off_t's.
 */
#ifndef _KERNEL
__BEGIN_DECLS
#ifndef _FTRUNCATE_DECLARED
#define	_FTRUNCATE_DECLARED
int	 ftruncate __P((int, off_t));
#endif
#ifndef _LSEEK_DECLARED
#define	_LSEEK_DECLARED
off_t	 lseek __P((int, off_t, int));
#endif
#ifndef _MMAP_DECLARED
#define	_MMAP_DECLARED
void *	 mmap __P((void *, size_t, int, int, int, off_t));
#endif
#ifndef _TRUNCATE_DECLARED
#define	_TRUNCATE_DECLARED
int	 truncate __P((const char *, off_t));
#endif
__END_DECLS
#endif /* !_KERNEL */

#endif /* !_POSIX_SOURCE */

#endif /* !_SYS_TYPES_H_ */
