/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	@(#)a.out.h	5.6 (Berkeley) 4/30/91
 */

#ifndef	_AOUT_H_
#define	_AOUT_H_

#include <sys/exec.h>

#if defined(hp300) || defined(i386)
#define	__LDPGSZ	4096
#endif
#if defined(tahoe) || defined(vax)
#define	__LDPGSZ	1024
#endif

#define N_ALIGN(ex,x) \
	((ex).a_magic == ZMAGIC || (ex).a_magic == QMAGIC ? \
	 ((x) + __LDPGSZ - 1) & ~(__LDPGSZ - 1) : (x))

/* Valid magic number check. */
#define	N_BADMAG(ex) \
	((ex).a_magic != NMAGIC && (ex).a_magic != OMAGIC && \
	    (ex).a_magic != ZMAGIC && (ex).a_magic != QMAGIC)

/* Address of the bottom of the text segment. */
#define N_TXTADDR(ex)	((ex).a_magic == QMAGIC ? __LDPGSZ : 0)

/* Address of the bottom of the data segment. */
#define N_DATADDR(ex) \
	N_ALIGN(ex, N_TXTADDR(ex) + (ex).a_text)

/* Text segment offset. */
#define	N_TXTOFF(ex) \
	((ex).a_magic == ZMAGIC ? __LDPGSZ \
	 : ((ex).a_magic == QMAGIC ? 0 : sizeof(struct exec)))

/* Data segment offset. */
#define	N_DATOFF(ex) \
	N_ALIGN(ex, N_TXTOFF(ex) + (ex).a_text)

/* Relocation table offset. */
#define N_RELOFF(ex) \
	N_ALIGN(ex, N_DATOFF(ex) + (ex).a_data)

/* Symbol table offset. */
#define N_SYMOFF(ex) \
	(N_RELOFF(ex) + (ex).a_trsize + (ex).a_drsize)

/* String table offset. */
#define	N_STROFF(ex) 	(N_SYMOFF(ex) + (ex).a_syms)

/* Relocation format. */
struct relocation_info {
	int r_address;			/* offset in text or data segment */
	unsigned int r_symbolnum : 24,	/* ordinal number of add symbol */
			 r_pcrel :  1,	/* 1 if value should be pc-relative */
			r_length :  2,	/* log base 2 of value's width */
			r_extern :  1,	/* 1 if need to add symbol to value */
				 :  4;	/* reserved */
};

#define _AOUT_INCLUDE_
#include <nlist.h>

#endif /* !_AOUT_H_ */
