/*-
 * Copyright (C) 1995-1997, 1999 Wolfgang Solfrank.
 * Copyright (C) 1995-1997, 1999 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: syncicache.c,v 1.2 1999/05/05 12:36:40 tsubai Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/powerpc/powerpc/syncicache.c,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#if	defined(_KERNEL) || defined(_STANDALONE)
#include <sys/time.h>
#include <sys/proc.h>
#include <vm/vm.h>
#endif
#include <sys/sysctl.h>

#include <machine/md_var.h>

#if	defined(_KERNEL) || defined(_STANDALONE)
#ifndef	CACHELINESIZE
#error "Must know the size of a cache line"
#endif
#else
static void getcachelinesize(void);

static int _cachelinesize;
#define	CACHELINESIZE	_cachelinesize

static void
getcachelinesize()
{
	static int	cachemib[] = { CTL_MACHDEP, CPU_CACHELINE };
	int		clen;

	clen = sizeof(_cachelinesize);

	if (sysctl(cachemib, sizeof(cachemib) / sizeof(cachemib[0]),
	    &_cachelinesize, &clen, NULL, 0) < 0 || !_cachelinesize) {
		abort();
	}
}
#endif

void
__syncicache(void *from, int len)
{
	int	l, off;
	char	*p;

#if	!defined(_KERNEL) && !defined(_STANDALONE)
	if (!_cachelinesize)
		getcachelinesize();
#endif	
	off = (u_int)from & (CACHELINESIZE - 1);
	l = len += off;
	p = (char *)from - off;
	do {
		__asm __volatile ("dcbst 0,%0" :: "r"(p));
		p += CACHELINESIZE;
	} while ((l -= CACHELINESIZE) > 0);
	__asm __volatile ("sync");
	p = (char *)from - off;
	do {
		__asm __volatile ("icbi 0,%0" :: "r"(p));
		p += CACHELINESIZE;
	} while ((len -= CACHELINESIZE) > 0);
	__asm __volatile ("sync; isync");
}
