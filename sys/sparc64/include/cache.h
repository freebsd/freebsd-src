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

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/ofw/openfirm.h>

/*
 * Cache diagnostic access definitions.
 */
/* ASI offsets for I$ diagnostic access */
#define	ICDA_SET_SHIFT		13
#define	ICDA_SET_MASK		(1UL << ICDA_SET_SHIFT)
#define	ICDA_SET(a)		(((a) << ICDA_SET_SHIFT) & ICDA_SET_MASK)
/* I$ tag/valid format */
#define	ICDT_TAG_SHIFT		8
#define	ICDT_TAG_BITS		28
#define	ICDT_TAG_MASK		(((1UL << ICDT_TAG_BITS) - 1) << ICDT_TAG_SHIFT)
#define	ICDT_TAG(x)		(((x) & ICDT_TAG_MASK) >> ICDT_TAG_SHIFT)
#define	ICDT_VALID		(1UL << 36)
/* D$ tag/valid format */
#define	DCDT_TAG_SHIFT		2
#define	DCDT_TAG_BITS		28
#define	DCDT_TAG_MASK		(((1UL << DCDT_TAG_BITS) - 1) << DCDT_TAG_SHIFT)
#define	DCDT_TAG(x)		(((x) & DCDT_TAG_MASK) >> DCDT_TAG_SHIFT)
#define	DCDT_VALID_BITS		2
#define	DCDT_VALID_MASK		((1UL << DCDT_VALID_BITS) - 1)
/* E$ ASI_ECACHE_W/ASI_ECACHE_R address flags */
#define	ECDA_DATA		(1UL << 39)
#define	ECDA_TAG		(1UL << 40)
/* E$ tag/state/parity format */
#define	ECDT_TAG_BITS		13
#define	ECDT_TAG_SIZE		(1UL << ECDT_TAG_BITS)
#define	ECDT_TAG_MASK		(ECDT_TAG_SIZE - 1)

/*
 * Do two virtual addresses (at which the same page is mapped) form and illegal
 * alias in D$? XXX: should use cache.dc_size here.
 */
#define	DCACHE_BOUNDARY		0x4000
#define	DCACHE_BMASK		(DCACHE_BOUNDARY - 1)
#define	CACHE_BADALIAS(v1, v2) \
	(((v1) & DCACHE_BMASK) != ((v2) & DCACHE_BMASK))

/*
 * Routines for dealing with the cache.
 */
void	cache_init __P((phandle_t));		/* turn it on */
void	icache_flush __P((vm_offset_t, vm_offset_t));
void	icache_inval_phys __P((vm_offset_t, vm_offset_t));
void	dcache_flush __P((vm_offset_t, vm_offset_t));
void	dcache_inval __P((pmap_t, vm_offset_t, vm_offset_t));
void	dcache_inval_phys __P((vm_offset_t, vm_offset_t));
void	dcache_blast __P((void));
void	ecache_flush __P((vm_offset_t, vm_offset_t));
#if 0
void	ecache_inval_phys __P((vm_offset_t, vm_offset_t));
#endif

/*
 * Cache control information.
 */
struct cacheinfo {
	int	c_enabled;		/* true => cache is enabled */
	int 	ic_size;		/* instruction cache */
	int	ic_set;
	int	ic_l2set;
	int 	ic_assoc;
	int 	ic_linesize;
	int 	dc_size;		/* data cache */
	int	dc_l2size;
	int 	dc_assoc;
	int 	dc_linesize;
	int	ec_size;		/* external cache info */
	int 	ec_assoc;
	int	ec_l2set;
	int	ec_linesize;
	int	ec_l2linesize;
};

#endif	/* !_MACHINE_CACHE_H_ */
