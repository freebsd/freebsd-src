/*
 * $FreeBSD$
 * From	$NetBSD: OSFpal.c,v 1.5 1998/06/24 01:33:19 ross Exp $ 
 */

/*
 * Copyright (c) 1994, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Keith Bostic
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/types.h>
#include <stand.h>

#include <machine/prom.h>
#include <machine/rpb.h>
#include <machine/alpha_cpu.h>

vm_offset_t ptbr_save;

#include "common.h"

void
OSFpal()
{
	struct rpb *r;
	struct pcs *p;

	r = (struct rpb *)HWRPB_ADDR;
	/*
	 * Note, cpu_number() is a VMS op, can't necessarily call it.
	 * Real fun: PAL_VMS_mfpr_whami == PAL_OSF1_rti...
	 * We might not be rpb_primary_cpu_id, but it is supposed to go
	 * first so the answer should apply to everyone.
	 */
	p = LOCATE_PCS(r, r->rpb_primary_cpu_id);

	printf("VMS PAL rev: 0x%lx\n", p->pcs_palrevisions[PALvar_OpenVMS]);
	printf("OSF PAL rev: 0x%lx\n", p->pcs_palrevisions[PALvar_OSF1]);

	if(p->pcs_pal_type==PAL_TYPE_OSF1) {
		printf("OSF PAL code already running.\n");
		ptbr_save = ((struct alpha_pcb *)p)->apcb_ptbr;
		printf("PTBR is:          0x%lx\n", ptbr_save);
		return;
	}
	switch_palcode();
	bcopy(&p->pcs_palrevisions[PALvar_OSF1], &p->pcs_pal_rev,
	      sizeof(p->pcs_pal_rev));
	printf("Switch to OSF PAL code succeeded.\n");
}

