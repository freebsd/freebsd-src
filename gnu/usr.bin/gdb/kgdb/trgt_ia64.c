/*
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <err.h>
#include <kvm.h>
#include <string.h>

#include "kgdb.h"

#include <defs.h>
#include <target.h>
#include <gdbthread.h>
#include <inferior.h>
#include <regcache.h>

void
kgdb_trgt_fetch_registers(int regno __unused)
{
	struct kthr *kt;
	struct pcb pcb;
	uint64_t r;

	kt = kgdb_thr_lookup_tid(ptid_get_tid(inferior_ptid));
	if (kt == NULL)
		return;
	if (kvm_read(kvm, kt->pcb, &pcb, sizeof(pcb)) != sizeof(pcb)) {
		warnx("kvm_read: %s", kvm_geterr(kvm));
		memset(&pcb, 0, sizeof(pcb));
	}

	/* Registers 0-127: general registers. */
	supply_register(1, (char *)&pcb.pcb_special.gp);
	supply_register(4, (char *)&pcb.pcb_preserved.gr4);
	supply_register(5, (char *)&pcb.pcb_preserved.gr5);
	supply_register(6, (char *)&pcb.pcb_preserved.gr6);
	supply_register(7, (char *)&pcb.pcb_preserved.gr7);
	supply_register(12, (char *)&pcb.pcb_special.sp);
	supply_register(13, (char *)&pcb.pcb_special.tp);

	/* Registers 128-255: floating-point registers. */
	supply_register(130, (char *)&pcb.pcb_preserved_fp.fr2);
	supply_register(131, (char *)&pcb.pcb_preserved_fp.fr3);
	supply_register(132, (char *)&pcb.pcb_preserved_fp.fr4);
	supply_register(133, (char *)&pcb.pcb_preserved_fp.fr5);
	supply_register(144, (char *)&pcb.pcb_preserved_fp.fr16);
	supply_register(145, (char *)&pcb.pcb_preserved_fp.fr17);
	supply_register(146, (char *)&pcb.pcb_preserved_fp.fr18);
	supply_register(147, (char *)&pcb.pcb_preserved_fp.fr19);
	supply_register(148, (char *)&pcb.pcb_preserved_fp.fr20);
	supply_register(149, (char *)&pcb.pcb_preserved_fp.fr21);
	supply_register(150, (char *)&pcb.pcb_preserved_fp.fr22);
	supply_register(151, (char *)&pcb.pcb_preserved_fp.fr23);
	supply_register(152, (char *)&pcb.pcb_preserved_fp.fr24);
	supply_register(153, (char *)&pcb.pcb_preserved_fp.fr25);
	supply_register(154, (char *)&pcb.pcb_preserved_fp.fr26);
	supply_register(155, (char *)&pcb.pcb_preserved_fp.fr27);
	supply_register(156, (char *)&pcb.pcb_preserved_fp.fr28);
	supply_register(157, (char *)&pcb.pcb_preserved_fp.fr29);
	supply_register(158, (char *)&pcb.pcb_preserved_fp.fr30);
	supply_register(159, (char *)&pcb.pcb_preserved_fp.fr31);

	/* Registers 320-327: branch registers. */
	if (pcb.pcb_special.__spare == ~0UL)
		supply_register(320, (char *)&pcb.pcb_special.rp);
	supply_register(321, (char *)&pcb.pcb_preserved.br1);
	supply_register(322, (char *)&pcb.pcb_preserved.br2);
	supply_register(323, (char *)&pcb.pcb_preserved.br3);
	supply_register(324, (char *)&pcb.pcb_preserved.br4);
	supply_register(325, (char *)&pcb.pcb_preserved.br5);

	/* Registers 328-333: misc. other registers. */
	supply_register(330, (char *)&pcb.pcb_special.pr);
	if (pcb.pcb_special.__spare == ~0UL) {
		r = pcb.pcb_special.iip + ((pcb.pcb_special.psr >> 41) & 3);
		supply_register(331, (char *)&r);
		supply_register(333, (char *)&pcb.pcb_special.cfm);
	} else {
		supply_register(331, (char *)&pcb.pcb_special.rp);
		supply_register(333, (char *)&pcb.pcb_special.pfs);
	}

	/* Registers 334-461: application registers. */
	supply_register(350, (char *)&pcb.pcb_special.rsc);
	r = pcb.pcb_special.bspstore;
	if (pcb.pcb_special.__spare == ~0UL)
		r += pcb.pcb_special.ndirty;
	else
		r = ia64_bsp_adjust(r, IA64_CFM_SOF(pcb.pcb_special.pfs) -
		    IA64_CFM_SOL(pcb.pcb_special.pfs));
	supply_register(351, (char *)&r);	/* bsp */
	supply_register(352, (char *)&r);	/* bspstore */
	supply_register(353, (char *)&pcb.pcb_special.rnat);
	supply_register(370, (char *)&pcb.pcb_special.unat);
	supply_register(374, (char *)&pcb.pcb_special.fpsr);
	if (pcb.pcb_special.__spare == ~0UL)
		supply_register(398, (char *)&pcb.pcb_special.pfs);
	supply_register(399, (char *)&pcb.pcb_preserved.lc);
}

void
kgdb_trgt_store_registers(int regno __unused)
{
	fprintf_unfiltered(gdb_stderr, "XXX: %s\n", __func__);
}
