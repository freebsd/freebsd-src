/*
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 *
 *	from: @(#)cache.h	8.1 (Berkeley) 6/11/93
 *	from: NetBSD: cache.h,v 1.3 2000/08/01 00:28:02 eeh Exp
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_CACHE_H_
#define _MACHINE_CACHE_H_

#include <dev/ofw/openfirm.h>

#define	DCACHE_COLOR_BITS	(1)
#define	DCACHE_COLORS		(1 << DCACHE_COLOR_BITS)
#define	DCACHE_COLOR_MASK	(DCACHE_COLORS - 1)
#define	DCACHE_COLOR(va)	(((va) >> PAGE_SHIFT) & DCACHE_COLOR_MASK)
#define	DCACHE_OTHER_COLOR(color) \
	((color) ^ DCACHE_COLOR_BITS)

#define	DC_TAG_SHIFT	2
#define	DC_VALID_SHIFT	0

#define	DC_TAG_BITS	28
#define	DC_VALID_BITS	2

#define	DC_TAG_MASK	((1 << DC_TAG_BITS) - 1)
#define	DC_VALID_MASK	((1 << DC_VALID_BITS) - 1)

#define	IC_TAG_SHIFT	7
#define	IC_VALID_SHIFT	36

#define	IC_TAG_BITS	28
#define	IC_VALID_BITS	1

#define	IC_TAG_MASK	((1 << IC_TAG_BITS) - 1)
#define	IC_VALID_MASK	((1 << IC_VALID_BITS) - 1)

/*
 * Cache control information.
 */
struct cacheinfo {
	u_int	c_enabled;		/* true => cache is enabled */
	u_int 	ic_size;		/* instruction cache */
	u_int	ic_set;
	u_int	ic_l2set;
	u_int 	ic_assoc;
	u_int 	ic_linesize;
	u_int 	dc_size;		/* data cache */
	u_int	dc_l2size;
	u_int 	dc_assoc;
	u_int 	dc_linesize;
	u_int	ec_size;		/* external cache info */
	u_int 	ec_assoc;
	u_int	ec_l2set;
	u_int	ec_linesize;
	u_int	ec_l2linesize;
};

#ifdef _KERNEL

typedef void dcache_page_inval_t(vm_paddr_t pa);
typedef void icache_page_inval_t(vm_paddr_t pa);

void	cache_init(phandle_t node);

dcache_page_inval_t cheetah_dcache_page_inval;
icache_page_inval_t cheetah_icache_page_inval;
dcache_page_inval_t spitfire_dcache_page_inval;
icache_page_inval_t spitfire_icache_page_inval;

extern dcache_page_inval_t *dcache_page_inval;
extern icache_page_inval_t *icache_page_inval;

extern struct cacheinfo cache;

#endif

#endif	/* !_MACHINE_CACHE_H_ */
