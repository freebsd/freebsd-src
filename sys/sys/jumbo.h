/*-
 * Copyright (c) 1997, Duke University
 * All rights reserved.
 *
 * Author:
 *         Andrew Gallatin <gallatin@cs.duke.edu>  
 *            
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Duke University may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY DUKE UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DUKE UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITSOR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
 *
 * $FreeBSD: src/sys/sys/jumbo.h,v 1.5 2003/03/25 01:47:29 jake Exp $
 */

#ifndef _SYS_JUMBO_H_
#define _SYS_JUMBO_H_

#ifdef _KERNEL
extern vm_offset_t jumbo_basekva;

static __inline caddr_t	jumbo_phys_to_kva(vm_paddr_t pa);
static __inline caddr_t
jumbo_phys_to_kva(vm_paddr_t pa)
{
	vm_page_t pg;

	pg = PHYS_TO_VM_PAGE(pa);
	pg->flags &= ~PG_BUSY;
	return (caddr_t)(ptoa((vm_offset_t)pg->pindex) + jumbo_basekva);
}

int		jumbo_vm_init(void);
void		jumbo_freem(void *addr, void *args);
vm_page_t	jumbo_pg_alloc(void);
void		jumbo_pg_free(vm_offset_t addr);
void 		jumbo_pg_steal(vm_page_t pg);
#endif /* _KERNEL */

#endif /* !_SYS_JUMBO_H_ */
