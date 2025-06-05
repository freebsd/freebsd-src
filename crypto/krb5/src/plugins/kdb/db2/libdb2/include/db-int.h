/*-
 * Copyright (c) 1991, 1993, 2007
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
 *	@(#)compat.h	8.13 (Berkeley) 2/21/94
 */

#ifndef	_DB_INT_H_
#define	_DB_INT_H_

#include "config.h"
#include "db.h"

/* deal with autoconf-based stuff */

#define DB_LITTLE_ENDIAN 1234
#define DB_BIG_ENDIAN 4321

#include <stdlib.h>
#ifdef HAVE_ENDIAN_H
# include <endian.h>
#endif
#ifdef HAVE_MACHINE_ENDIAN_H
# include <machine/endian.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
/* Handle both BIG and LITTLE defined and BYTE_ORDER matches one, or
   just one defined; both with and without leading underscores.

   Ignore "PDP endian" machines, this code doesn't support them
   anyways.  */
#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN) && !defined(BYTE_ORDER)
# ifdef __LITTLE_ENDIAN__
#  define LITTLE_ENDIAN __LITTLE_ENDIAN__
# endif
# ifdef __BIG_ENDIAN__
#  define BIG_ENDIAN __BIG_ENDIAN__
# endif
#endif
#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN) && !defined(BYTE_ORDER)
# ifdef _LITTLE_ENDIAN
#  define LITTLE_ENDIAN _LITTLE_ENDIAN
# endif
# ifdef _BIG_ENDIAN
#  define BIG_ENDIAN _BIG_ENDIAN
# endif
# ifdef _BYTE_ORDER
#  define BYTE_ORDER _BYTE_ORDER
# endif
#endif
#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN) && !defined(BYTE_ORDER)
# ifdef __LITTLE_ENDIAN
#  define LITTLE_ENDIAN __LITTLE_ENDIAN
# endif
# ifdef __BIG_ENDIAN
#  define BIG_ENDIAN __BIG_ENDIAN
# endif
# ifdef __BYTE_ORDER
#  define BYTE_ORDER __BYTE_ORDER
# endif
#endif

#if defined(_MIPSEL) && !defined(LITTLE_ENDIAN)
# define LITTLE_ENDIAN
#endif
#if defined(_MIPSEB) && !defined(BIG_ENDIAN)
# define BIG_ENDIAN
#endif

#if defined(LITTLE_ENDIAN) && defined(BIG_ENDIAN) && defined(BYTE_ORDER)
# if LITTLE_ENDIAN == BYTE_ORDER
#  define DB_BYTE_ORDER DB_LITTLE_ENDIAN
# elif BIG_ENDIAN == BYTE_ORDER
#  define DB_BYTE_ORDER DB_BIG_ENDIAN
# else
#  error "LITTLE_ENDIAN and BIG_ENDIAN defined, but can't determine byte order"
# endif
#elif defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
# define DB_BYTE_ORDER DB_LITTLE_ENDIAN
#elif defined(BIG_ENDIAN) && !defined(LITTLE_ENDIAN)
# define DB_BYTE_ORDER DB_BIG_ENDIAN
#else
# error "can't determine byte order from included system headers"
#endif

#if 0
#ifdef WORDS_BIGENDIAN
#define DB_BYTE_ORDER DB_BIG_ENDIAN
#else
#define DB_BYTE_ORDER DB_LITTLE_ENDIAN
#endif
#endif

/* end autoconf-based stuff */

/* include necessary system header files */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

/* types and constants used for database structure */

#define	MAX_PAGE_NUMBER	0xffffffff	/* >= # of pages in a file */
typedef u_int32_t	db_pgno_t;
#define	MAX_PAGE_OFFSET	65535		/* >= # of bytes in a page */
typedef u_int16_t	indx_t;
#define	MAX_REC_NUMBER	0xffffffff	/* >= # of records in a tree */
typedef u_int32_t	recno_t;

/*
 * Little endian <==> big endian 32-bit swap macros.
 *	M_32_SWAP	swap a memory location
 *	P_32_SWAP	swap a referenced memory location
 *	P_32_COPY	swap from one location to another
 */
#define	M_32_SWAP(a) {							\
	u_int32_t _tmp = a;						\
	((char *)&a)[0] = ((char *)&_tmp)[3];				\
	((char *)&a)[1] = ((char *)&_tmp)[2];				\
	((char *)&a)[2] = ((char *)&_tmp)[1];				\
	((char *)&a)[3] = ((char *)&_tmp)[0];				\
}
#define	P_32_SWAP(a) {							\
	char _tmp[4];							\
	_tmp[0] = ((char *)a)[0];					\
	_tmp[1] = ((char *)a)[1];					\
	_tmp[2] = ((char *)a)[2];					\
	_tmp[3] = ((char *)a)[3];					\
	((char *)a)[0] = _tmp[3];					\
	((char *)a)[1] = _tmp[2];					\
	((char *)a)[2] = _tmp[1];					\
	((char *)a)[3] = _tmp[0];					\
}
#define	P_32_COPY(a, b) {						\
	((char *)&(b))[0] = ((char *)&(a))[3];				\
	((char *)&(b))[1] = ((char *)&(a))[2];				\
	((char *)&(b))[2] = ((char *)&(a))[1];				\
	((char *)&(b))[3] = ((char *)&(a))[0];				\
}

/*
 * Little endian <==> big endian 16-bit swap macros.
 *	M_16_SWAP	swap a memory location
 *	P_16_SWAP	swap a referenced memory location
 *	P_16_COPY	swap from one location to another
 */
#define	M_16_SWAP(a) {							\
	u_int16_t _tmp = a;						\
	((char *)&a)[0] = ((char *)&_tmp)[1];				\
	((char *)&a)[1] = ((char *)&_tmp)[0];				\
}
#define	P_16_SWAP(a) {							\
	char _tmp[2];							\
	_tmp[0] = ((char *)a)[0];					\
	_tmp[1] = ((char *)a)[1];					\
	((char *)a)[0] = _tmp[1];					\
	((char *)a)[1] = _tmp[0];					\
}
#define	P_16_COPY(a, b) {						\
	((char *)&(b))[0] = ((char *)&(a))[1];				\
	((char *)&(b))[1] = ((char *)&(a))[0];				\
}

/* open functions for each database type, used in dbopen() */

#define __bt_open	__kdb2_bt_open
#define __hash_open	__kdb2_hash_open
#define __rec_open	__kdb2_rec_open
#define __dbpanic	__kdb2_dbpanic

DB	*__bt_open __P((const char *, int, int, const BTREEINFO *, int));
DB	*__hash_open __P((const char *, int, int, const HASHINFO *, int));
DB	*__rec_open __P((const char *, int, int, const RECNOINFO *, int));
void	 __dbpanic __P((DB *dbp));

/*
 * There is no portable way to figure out the maximum value of a file
 * offset, so we put it here.
 */
#ifdef	OFF_T_MAX
#define	DB_OFF_T_MAX	OFF_T_MAX
#else
#define	DB_OFF_T_MAX	LONG_MAX
#endif

#ifndef O_ACCMODE			/* POSIX 1003.1 access mode mask. */
#define	O_ACCMODE	(O_RDONLY|O_WRONLY|O_RDWR)
#endif

/*
 * If you can't provide lock values in the open(2) call.  Note, this
 * allows races to happen.
 */
#ifndef O_EXLOCK			/* 4.4BSD extension. */
#define	O_EXLOCK	0
#endif

#ifndef O_SHLOCK			/* 4.4BSD extension. */
#define	O_SHLOCK	0
#endif

#ifndef EFTYPE
#define	EFTYPE		EINVAL		/* POSIX 1003.1 format errno. */
#endif

#ifndef	STDERR_FILENO
#define	STDIN_FILENO	0		/* ANSI C #defines */
#define	STDOUT_FILENO	1
#define	STDERR_FILENO	2
#endif

#ifndef SEEK_END
#define	SEEK_SET	0		/* POSIX 1003.1 seek values */
#define	SEEK_CUR	1
#define	SEEK_END	2
#endif

#ifndef NULL				/* ANSI C #defines NULL everywhere. */
#define	NULL		0
#endif

#ifndef	MAX				/* Usually found in <sys/param.h>. */
#define	MAX(_a,_b)	((_a)<(_b)?(_b):(_a))
#endif
#ifndef	MIN				/* Usually found in <sys/param.h>. */
#define	MIN(_a,_b)	((_a)<(_b)?(_a):(_b))
#endif

#ifndef S_ISDIR				/* POSIX 1003.1 file type tests. */
#define	S_ISDIR(m)	((m & 0170000) == 0040000)	/* directory */
#define	S_ISCHR(m)	((m & 0170000) == 0020000)	/* char special */
#define	S_ISBLK(m)	((m & 0170000) == 0060000)	/* block special */
#define	S_ISREG(m)	((m & 0170000) == 0100000)	/* regular file */
#define	S_ISFIFO(m)	((m & 0170000) == 0010000)	/* fifo */
#endif
#ifndef S_ISLNK				/* BSD POSIX 1003.1 extensions */
#define	S_ISLNK(m)	((m & 0170000) == 0120000)	/* symbolic link */
#define	S_ISSOCK(m)	((m & 0170000) == 0140000)	/* socket */
#endif

#ifndef O_BINARY
#define O_BINARY	0		/* Needed for Win32 compiles */
#endif
#endif /* _DB_INT_H_ */
