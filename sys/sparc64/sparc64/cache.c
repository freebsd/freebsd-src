/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
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
 */
/*-
 * Copyright (c) 2001 by Thomas Moestl <tmm@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: @(#)cache.c	8.2 (Berkeley) 10/30/93
 *	from: NetBSD: cache.c,v 1.5 2000/12/06 01:47:50 mrg Exp
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/ofw/openfirm.h>

#include <machine/cache.h>
#include <machine/tlb.h>
#include <machine/ver.h>

struct cacheinfo cache;

cache_enable_t *cache_enable;
cache_flush_t *cache_flush;
dcache_page_inval_t *dcache_page_inval;
icache_page_inval_t *icache_page_inval;

#define	OF_GET(h, n, v)	OF_getprop((h), (n), &(v), sizeof(v))

/*
 * Fill in the cache parameters using the cpu node.
 */
void
cache_init(phandle_t node)
{
	u_long set;

	if (OF_GET(node, "icache-size", cache.ic_size) == -1 ||
	    OF_GET(node, "icache-line-size", cache.ic_linesize) == -1 ||
	    OF_GET(node, "icache-associativity", cache.ic_assoc) == -1 ||
	    OF_GET(node, "dcache-size", cache.dc_size) == -1 ||
	    OF_GET(node, "dcache-line-size", cache.dc_linesize) == -1 ||
	    OF_GET(node, "dcache-associativity", cache.dc_assoc) == -1 ||
	    OF_GET(node, "ecache-size", cache.ec_size) == -1 ||
	    OF_GET(node, "ecache-line-size", cache.ec_linesize) == -1 ||
	    OF_GET(node, "ecache-associativity", cache.ec_assoc) == -1)
		panic("cache_init: could not retrieve cache parameters");

	cache.ic_set = cache.ic_size / cache.ic_assoc;
	cache.ic_l2set = ffs(cache.ic_set) - 1;
	if ((cache.ic_set & ~(1UL << cache.ic_l2set)) != 0)
		panic("cache_init: I$ set size not a power of 2");
	cache.dc_l2size = ffs(cache.dc_size) - 1;
	if ((cache.dc_size & ~(1UL << cache.dc_l2size)) != 0)
		panic("cache_init: D$ size not a power of 2");
	if (((cache.dc_size / cache.dc_assoc) / PAGE_SIZE) != DCACHE_COLORS)
		panic("cache_init: too many D$ colors");
	set = cache.ec_size / cache.ec_assoc;
	cache.ec_l2set = ffs(set) - 1;
	if ((set & ~(1UL << cache.ec_l2set)) != 0)
		panic("cache_init: E$ set size not a power of 2");

	if (cpu_impl >= CPU_IMPL_ULTRASPARCIII) {
		cache_enable = cheetah_cache_enable;
		cache_flush = cheetah_cache_flush;
		dcache_page_inval = cheetah_dcache_page_inval;
		icache_page_inval = cheetah_icache_page_inval;
		tlb_flush_user = cheetah_tlb_flush_user;
	} else {
		cache_enable = spitfire_cache_enable;
		cache_flush = spitfire_cache_flush;
		dcache_page_inval = spitfire_dcache_page_inval;
		icache_page_inval = spitfire_icache_page_inval;
		tlb_flush_user = spitfire_tlb_flush_user;
	}
}
