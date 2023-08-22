/*-
 * Copyright (c) 2004 Tim J. Robbins
 * Copyright (c) 2002 Doug Rabson
 * Copyright (c) 2000 Marcel Moolenaar
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_MMAP_H_
#define	_LINUX_MMAP_H_

/* mmap options */
#define	LINUX_MAP_SHARED	0x0001
#define	LINUX_MAP_PRIVATE	0x0002
#define	LINUX_MAP_FIXED		0x0010
#define	LINUX_MAP_ANON		0x0020
#define	LINUX_MAP_32BIT		0x0040
#define	LINUX_MAP_GROWSDOWN	0x0100

#define	LINUX_PROT_GROWSDOWN	0x01000000
#define	LINUX_PROT_GROWSUP	0x02000000

#define	LINUX_MADV_NORMAL	0
#define	LINUX_MADV_RANDOM	1
#define	LINUX_MADV_SEQUENTIAL	2
#define	LINUX_MADV_WILLNEED	3
#define	LINUX_MADV_DONTNEED	4
#define	LINUX_MADV_FREE		8
#define	LINUX_MADV_REMOVE	9
#define	LINUX_MADV_DONTFORK	10
#define	LINUX_MADV_DOFORK	11
#define	LINUX_MADV_MERGEABLE	12
#define	LINUX_MADV_UNMERGEABLE	13
#define	LINUX_MADV_HUGEPAGE	14
#define	LINUX_MADV_NOHUGEPAGE	15
#define	LINUX_MADV_DONTDUMP	16
#define	LINUX_MADV_DODUMP	17
#define	LINUX_MADV_WIPEONFORK	18
#define	LINUX_MADV_KEEPONFORK	19
#define	LINUX_MADV_HWPOISON	100
#define	LINUX_MADV_SOFT_OFFLINE	101

int linux_mmap_common(struct thread *, uintptr_t, size_t, int, int,
			int, off_t);
int linux_mprotect_common(struct thread *, uintptr_t, size_t, int);
int linux_madvise_common(struct thread *, uintptr_t, size_t, int);

#endif	/* _LINUX_MMAP_H_ */
