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
#include <machine/endian.h>
#include <sys/_types.h>

#ifndef _POSIX_SOURCE
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;
typedef	unsigned short	ushort;		/* Sys V compatibility */
typedef	unsigned int	uint;		/* Sys V compatibility */
#endif

/*
 * XXX POSIX sized integrals that should appear only in <sys/stdint.h>.
 */
#ifndef _INT8_T_DECLARED
typedef	__int8_t	int8_t;
#define	_INT8_T_DECLARED
#endif

#ifndef _INT16_T_DECLARED
typedef	__int16_t	int16_t;
#define	_INT16_T_DECLARED
#endif

#ifndef _INT32_T_DECLARED
typedef	__int32_t	int32_t;
#define	_INT32_T_DECLARED
#endif

#ifndef _INT64_T_DECLARED
typedef	__int64_t	int64_t;
#define	_INT64_T_DECLARED
#endif

#ifndef _UINT8_T_DECLARED
typedef	__uint8_t	uint8_t;
#define	_UINT8_T_DECLARED
#endif

#ifndef _UINT16_T_DECLARED
typedef	__uint16_t	uint16_t;
#define	_UINT16_T_DECLARED
#endif

#ifndef _UINT32_T_DECLARED
typedef	__uint32_t	uint32_t;
#define	_UINT32_T_DECLARED
#endif

#ifndef _UINT64_T_DECLARED
typedef	__uint64_t	uint64_t;
#define	_UINT64_T_DECLARED
#endif

#ifndef _INTPTR_T_DECLARED
typedef	__intptr_t	intptr_t;
typedef	__uintptr_t	uintptr_t;
#define	_INTPTR_T_DECLARED
#endif

/*
 * Deprecated BSD unsigned integrals.
 */
typedef __uint8_t	u_int8_t;
typedef __uint16_t	u_int16_t;
typedef __uint32_t	u_int32_t;
typedef __uint64_t	u_int64_t;

/*
 * Deprecated BSD 64-bit integrals.
 */
typedef	u_int64_t	u_quad_t;	/* quads */
typedef	int64_t		quad_t;
typedef	quad_t *	qaddr_t;

typedef	char *		caddr_t;	/* core address */
typedef	__const char *	c_caddr_t;	/* core address, pointer to const */
typedef	__volatile char *v_caddr_t;	/* core address, pointer to volatile */
typedef	__critical_t	critical_t;	/* Critical section value */
typedef	int64_t		daddr_t;	/* disk address */
typedef	u_int32_t	fixpt_t;	/* fixed point number */

#ifndef _GID_T_DECLARED
typedef	__gid_t		gid_t;		/* group id */
#define	_GID_T_DECLARED
#endif

typedef	u_int32_t	ino_t;		/* inode number */
typedef	long		key_t;		/* IPC key (for Sys V IPC) */

#ifndef _MODE_T_DECLARED
typedef	__mode_t	mode_t;		/* permissions */
#define	_MODE_T_DECLARED
#endif

typedef	u_int16_t	nlink_t;	/* link count */

#ifndef _OFF_T_DECLARED
typedef	__off_t		off_t;		/* file offset */
#define	_OFF_T_DECLARED
#endif

#ifndef _PID_T_DECLARED
typedef	__pid_t		pid_t;		/* process id */
#define	_PID_T_DECLARED
#endif

typedef	__register_t	register_t;
typedef	quad_t		rlim_t;		/* resource limit */
typedef	__segsz_t	segsz_t;	/* segment size (in pages) */
typedef	int32_t		swblk_t;	/* swap offset */
typedef	__u_register_t	u_register_t;

#ifndef _UID_T_DECLARED
typedef	__uid_t		uid_t;		/* user id */
#define	_UID_T_DECLARED
#endif

typedef	__vm_offset_t	vm_offset_t;
typedef	__vm_ooffset_t	vm_ooffset_t;
typedef	__vm_pindex_t	vm_pindex_t;
typedef	__vm_size_t	vm_size_t;

#ifdef _KERNEL
typedef	int		boolean_t;
typedef	__intfptr_t	intfptr_t;

/*-
 * XXX this is fixed width for historical reasons.  It should have had type
 * __int_fast32_t.  Fixed-width types should not be used unless binary
 * compatibility is essential.  Least-width types should be used even less
 * since they provide smaller benefits.
 * XXX should be MD.
 * XXX this is bogus in -current, but still used for spl*().
 */
typedef	__uint32_t	intrmask_t;	/* Interrupt mask (spl, xxx_imask...) */

typedef	__uintfptr_t	uintfptr_t;
typedef	u_int64_t	uoff_t;
typedef	struct vm_page	*vm_page_t;

struct specinfo;

typedef	u_int32_t	udev_t;		/* device number */
typedef struct specinfo	*dev_t;

#define offsetof(type, field) __offsetof(type, field)

#else /* !_KERNEL */

typedef	u_int32_t	dev_t;		/* device number */
#define udev_t dev_t

#if __BSD_VISIBLE

/*
 * minor() gives a cookie instead of an index since we don't want to
 * change the meanings of bits 0-15 or waste time and space shifting
 * bits 16-31 for devices that don't use them.
 */
#define major(x)        ((int)(((u_int)(x) >> 8)&0xff)) /* major number */
#define minor(x)        ((int)((x)&0xffff00ff))         /* minor number */
#define makedev(x,y)    ((dev_t)(((x) << 8) | (y)))     /* create dev_t */

#endif /* __BSD_VISIBLE */

#endif /* !_KERNEL */

#ifndef _CLOCK_T_DECLARED
typedef	__clock_t	clock_t;
#define	_CLOCK_T_DECLARED
#endif

#ifndef _CLOCKID_T_DECLARED
typedef	__clockid_t	clockid_t;
#define	_CLOCKID_T_DECLARED
#endif

#ifndef _FFLAGS_T_DECLARED
typedef	__fflags_t	fflags_t;	/* file flags */
#define	_FFLAGS_T_DECLARED
#endif

#ifndef _FSBLKCNT_T_DECLARED		/* for statvfs() */
typedef	__fsblkcnt_t	fsblkcnt_t;
typedef	__fsfilcnt_t	fsfilcnt_t;
#define	_FSBLKCNT_T_DECLARED
#endif

#ifndef _SIZE_T_DECLARED
typedef	__size_t	size_t;
#define	_SIZE_T_DECLARED
#endif

#ifndef _SSIZE_T_DECLARED
typedef	__ssize_t	ssize_t;
#define	_SSIZE_T_DECLARED
#endif

#ifndef _TIME_T_DECLARED
typedef	__time_t	time_t;
#define	_TIME_T_DECLARED
#endif

#ifndef _TIMER_T_DECLARED
typedef	__timer_t	timer_t;
#define	_TIMER_T_DECLARED
#endif

#if __BSD_VISIBLE
#define	NBBY	8		/* number of bits in a byte */

/*
 * Select uses bit masks of file descriptors in longs.  These macros
 * manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here should
 * be enough for most uses.
 */
#ifndef	FD_SETSIZE
#define	FD_SETSIZE	1024U
#endif

typedef	unsigned long	fd_mask;
#define	NFDBITS	(sizeof(fd_mask) * NBBY)	/* bits per mask */

#ifndef howmany
#define	howmany(x, y)	(((x) + ((y) - 1U)) / (y))
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
int	 ftruncate(int, off_t);
#endif
#ifndef _LSEEK_DECLARED
#define	_LSEEK_DECLARED
off_t	 lseek(int, off_t, int);
#endif
#ifndef _MMAP_DECLARED
#define	_MMAP_DECLARED
void *	 mmap(void *, size_t, int, int, int, off_t);
#endif
#ifndef _TRUNCATE_DECLARED
#define	_TRUNCATE_DECLARED
int	 truncate(const char *, off_t);
#endif
__END_DECLS
#endif /* !_KERNEL */

#endif /* __BSD_VISIBLE */

#endif /* !_SYS_TYPES_H_ */
