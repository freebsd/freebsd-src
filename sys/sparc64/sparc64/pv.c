/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mutex.h>

#include <vm/vm.h> 
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/asi.h>
#include <machine/frame.h>
#include <machine/pmap.h>
#include <machine/pv.h>
#include <machine/tte.h>
#include <machine/tlb.h>
#include <machine/tsb.h>

/*
 * Physical address of array of physical addresses of stte alias chain heads,
 * and generation count of alias chains.
 */
vm_offset_t pv_table;
u_long pv_generation;

void
pv_insert(pmap_t pm, vm_offset_t pa, vm_offset_t va, struct stte *stp)
{
	vm_offset_t pstp;
	vm_offset_t pvh;

	pstp = tsb_stte_vtophys(pm, stp);
	pvh = pv_lookup(pa);
	PV_LOCK();
	if ((stp->st_next = pv_get_first(pvh)) != 0)
		pv_set_prev(stp->st_next, pstp + ST_NEXT);
	pv_set_first(pvh, pstp);
	stp->st_prev = pvh;
	pv_generation++;
	PV_UNLOCK();
}

void
pv_remove_virt(struct stte *stp)
{
	PV_LOCK();
	if (stp->st_next != 0)
		pv_set_prev(stp->st_next, stp->st_prev);
	stxp(stp->st_prev, stp->st_next);
	pv_generation++;
	PV_UNLOCK();
}

void
pv_dump(vm_offset_t pvh)
{
	vm_offset_t pstp;

	printf("pv_dump: pvh=%#lx first=%#lx\n", pvh, pv_get_first(pvh));
	for (pstp = pv_get_first(pvh); pstp != 0; pstp = pv_get_next(pstp))
		printf("\tpstp=%#lx next=%#lx prev=%#lx\n", pstp,
		    pv_get_next(pstp), pv_get_prev(pstp));
	printf("pv_dump: done\n");
}
