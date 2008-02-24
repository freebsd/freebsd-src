/*-
 * Copyright (c) 2006 Kip Macy
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
 *
 * $FreeBSD: src/sys/sun4v/include/sun4v_cpufunc.h,v 1.2 2006/11/24 05:27:48 kmacy Exp $
 */

#ifndef	_MACHINE_SUN4V_CPUFUNC_H_
#define	_MACHINE_SUN4V_CPUFUNC_H_
#include <machine/hv_api.h>
void set_mmfsa_scratchpad(vm_paddr_t mmfsa);

void set_hash_user_scratchpad(uint64_t);
void set_tsb_user_scratchpad(uint64_t);
void set_hash_kernel_scratchpad(uint64_t);
void set_tsb_kernel_scratchpad(uint64_t);
void init_mondo(uint64_t func, uint64_t arg1, uint64_t arg2, uint64_t arg3);
void init_mondo_queue(void);

static __inline void * 
set_tba(void *ntba)
{
	void *otba;
	otba = (char *)rdpr(tba);
	wrpr(tba, ntba, 0);
	return otba;
}



static __inline void 
set_wstate(u_long nwstate)
{
	wrpr(wstate, nwstate, 0);
}

void invlpg(uint16_t ctx, vm_offset_t va);

void invlctx(uint16_t ctx);

void invltlb(void);

static __inline void 
store_real(vm_paddr_t ra, uint64_t val)
{
	stxa(ra, ASI_REAL, val);
}

static __inline void 
store_real_sync(vm_paddr_t ra, uint64_t val)
{
	stxa_sync(ra, ASI_REAL, val);
}

static __inline uint64_t 
load_real(vm_paddr_t ra)
{
	uint64_t val;
	val = ldxa(ra, ASI_REAL);
	return val;
}


void load_real_dw(vm_paddr_t ra, uint64_t *lo, uint64_t *hi);
void bzerophyspage(vm_paddr_t ra, uint64_t size);
int hwblkclr(void *p, uint64_t size);
int novbcopy(void *src, void *dst, uint64_t size);


#endif /* !_MACHINE_CPUFUNC_H_ */
