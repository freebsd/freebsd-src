/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)mman.h	7.5 (Berkeley) 6/27/91
 *	$Id: mman.h,v 1.3 1993/11/07 17:52:49 wollman Exp $
 */

#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_ 1

/*
 * Protections are chosen from these bits, or-ed together
 */
#define	PROT_READ	0x04	/* pages can be read */
#define	PROT_WRITE	0x02	/* pages can be written */
#define	PROT_EXEC	0x01	/* pages can be executed */

/*
 * Flags contain mapping type, sharing type and options.
 * Mapping type; choose one
 */
#define	MAP_FILE	0x0001	/* mapped from a file or device */
#define	MAP_ANON	0x0002	/* allocated from memory, swap space */
#define	MAP_TYPE	0x000f	/* mask for type field */

/*
 * Sharing types; choose one
 */
#define	MAP_COPY	0x0020	/* "copy" region at mmap time */
#define	MAP_SHARED	0x0010	/* share changes */
#define	MAP_PRIVATE	0x0000	/* changes are private */

/*
 * Other flags
 */
#define	MAP_FIXED	0x0100	/* map addr must be exactly as requested */
#define	MAP_NOEXTEND	0x0200	/* for MAP_FILE, don't change file size */
#define	MAP_HASSEMPHORE	0x0400	/* region may contain semaphores */
#define	MAP_INHERIT	0x0800	/* region is retained after exec */

/*
 * Advice to madvise
 */
#define	MADV_NORMAL	0	/* no further special treatment */
#define	MADV_RANDOM	1	/* expect random page references */
#define	MADV_SEQUENTIAL	2	/* expect sequential page references */
#define	MADV_WILLNEED	3	/* will need these pages */
#define	MADV_DONTNEED	4	/* dont need these pages */

#ifndef KERNEL

#include <sys/cdefs.h>

__BEGIN_DECLS
/* Some of these int's should probably be size_t's */
caddr_t	mmap __P((caddr_t, size_t, int, int, int, off_t));
int	mprotect __P((caddr_t, int, int));
int	munmap __P((caddr_t, int));
int	msync __P((caddr_t, int));
__END_DECLS

#endif /* !KERNEL */
#endif /* _SYS_MMAN_H_ */
