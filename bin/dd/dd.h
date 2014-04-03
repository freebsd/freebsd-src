/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
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
 *	@(#)dd.h	8.3 (Berkeley) 4/2/94
 * $FreeBSD$
 */

/* Input/output stream state. */
typedef struct {
	u_char		*db;		/* buffer address */
	u_char		*dbp;		/* current buffer I/O address */
	/* XXX ssize_t? */
	size_t		dbcnt;		/* current buffer byte count */
	size_t		dbrcnt;		/* last read byte count */
	size_t		dbsz;		/* buffer size */

#define	ISCHR		0x01		/* character device (warn on short) */
#define	ISPIPE		0x02		/* pipe-like (see position.c) */
#define	ISTAPE		0x04		/* tape */
#define	ISSEEK		0x08		/* valid to seek on */
#define	NOREAD		0x10		/* not readable */
#define	ISTRUNC		0x20		/* valid to ftruncate() */
	u_int		flags;

	const char	*name;		/* name */
	int		fd;		/* file descriptor */
	off_t		offset;		/* # of blocks to skip */
} IO;

typedef struct {
	uintmax_t	in_full;	/* # of full input blocks */
	uintmax_t	in_part;	/* # of partial input blocks */
	uintmax_t	out_full;	/* # of full output blocks */
	uintmax_t	out_part;	/* # of partial output blocks */
	uintmax_t	trunc;		/* # of truncated records */
	uintmax_t	swab;		/* # of odd-length swab blocks */
	uintmax_t	bytes;		/* # of bytes written */
	double		start;		/* start time of dd */
} STAT;

/* Flags (in ddflags). */
#define	C_ASCII		0x000000001
#define	C_BLOCK		0x000000002
#define	C_BS		0x000000004
#define	C_CBS		0x000000008
#define	C_COUNT		0x000000010
#define	C_EBCDIC	0x000000020
#define	C_FILES		0x000000040
#define	C_IBS		0x000000080
#define	C_IF		0x000000100
#define	C_LCASE		0x000000200
#define	C_NOERROR	0x000000400
#define	C_NOTRUNC	0x000000800
#define	C_OBS		0x000001000
#define	C_OF		0x000002000
#define	C_OSYNC		0x000004000
#define	C_PAREVEN	0x000008000
#define	C_PARNONE	0x000100000
#define	C_PARODD	0x000200000
#define	C_PARSET	0x000400000
#define	C_SEEK		0x000800000
#define	C_SKIP		0x001000000
#define	C_SPARSE	0x002000000
#define	C_SWAB		0x004000000
#define	C_SYNC		0x008000000
#define	C_UCASE		0x010000000
#define	C_UNBLOCK	0x020000000
#define	C_FILL		0x040000000
#define	C_STATUS	0x080000000
#define	C_NOXFER	0x100000000
#define	C_NOINFO	0x200000000

#define	C_PARITY	(C_PAREVEN | C_PARODD | C_PARNONE | C_PARSET)
