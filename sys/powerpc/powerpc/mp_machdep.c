/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <sys/dkstat.h>

#include <machine/atomic.h>
#include <machine/pmap.h>
#include <machine/clock.h>

int			boot_cpu_id;

/*
 * XXX: needs to move to machdep.c
 *
 * Initialise a struct globaldata.
 */
void
globaldata_init(struct globaldata *globaldata, int cpuno, size_t sz)
{

	bzero(globaldata, sz);
	globaldata->gd_idlepcbphys = vtophys((vm_offset_t) &globaldata->gd_idlepcb);
	globaldata->gd_idlepcb.apcb_ksp = (u_int64_t)
		((caddr_t) globaldata + sz - sizeof(struct trapframe));
	globaldata->gd_idlepcb.apcb_ptbr = proc0.p_addr->u_pcb.pcb_hw.apcb_ptbr;
	globaldata->gd_cpuno = cpuno;
	globaldata->gd_other_cpus = all_cpus & ~(1 << cpuno);
	globaldata->gd_next_asn = 0;
	globaldata->gd_current_asngen = 1;
	globaldata->gd_cpuid = cpuno;
	globaldata_register(globaldata);
}

int
cpu_mp_probe(void)
{
	return 0;
}

void
cpu_mp_start(void)
{
}

void
cpu_mp_announce(void)
{
}
