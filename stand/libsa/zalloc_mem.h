/*
 * This module derived from code donated to the FreeBSD Project by
 * Matthew Dillon <dillon@backplane.com>
 *
 * Copyright (c) 1998 The FreeBSD Project
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * H/MEM.H
 *
 * Basic memory pool / memory node structures.
 */
#ifndef _ZALLOC_MEM_H
#define	_ZALLOC_MEM_H

typedef struct MemNode {
	struct MemNode	*mr_Next;
	uintptr_t	mr_Bytes;
} MemNode;

typedef struct MemPool {
	void		*mp_Base;
	void		*mp_End;
	MemNode		*mp_First;
	uintptr_t	mp_Size;
	uintptr_t	mp_Used;
} MemPool;

#define	ZNOTE_FREE	0
#define	ZNOTE_REUSE	1

#endif	/* _ZALLOC_MEM_H */
