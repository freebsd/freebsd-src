/*-
 * Copyright (c) 1994 John Dyson
 * Copyright (c) 2001 Matt Dillon
 *
 * All rights reserved.  Terms for use and redistribution
 * are covered by the BSD Copyright as found in /usr/src/COPYRIGHT.
 *
 *	from: @(#)vm_machdep.c	7.3 (Berkeley) 5/13/91
 *	Utah $Hdr: vm_machdep.c 1.16.1.1 89/06/23$
 * $FreeBSD$
 */

#ifndef	__alpha__
#include "opt_npx.h"
#ifdef PC98
#include "opt_pc98.h"
#endif
#include "opt_reset.h"
#include "opt_isa.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#ifndef	__alpha__
#include <machine/pcb_ext.h>
#include <machine/vm86.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <sys/user.h>

#ifndef	__alpha__
#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif
#endif

SYSCTL_DECL(_vm_stats_misc);

static int cnt_prezero;

SYSCTL_INT(_vm_stats_misc, OID_AUTO,
	cnt_prezero, CTLFLAG_RD, &cnt_prezero, 0, "");

/*
 * Implement the pre-zeroed page mechanism.
 * This routine is called from the idle loop.
 */

#define ZIDLE_LO(v)	((v) * 2 / 3)
#define ZIDLE_HI(v)	((v) * 4 / 5)

int
vm_page_zero_idle(void)
{
	static int free_rover;
	static int zero_state;
	vm_page_t m;

	/*
	 * Attempt to maintain approximately 1/2 of our free pages in a
	 * PG_ZERO'd state.   Add some hysteresis to (attempt to) avoid
	 * generally zeroing a page when the system is near steady-state.
	 * Otherwise we might get 'flutter' during disk I/O / IPC or 
	 * fast sleeps.  We also do not want to be continuously zeroing
	 * pages because doing so may flush our L1 and L2 caches too much.
	 */

	if (zero_state && vm_page_zero_count >= ZIDLE_LO(cnt.v_free_count))
		return(0);
	if (vm_page_zero_count >= ZIDLE_HI(cnt.v_free_count))
		return(0);

	if (mtx_trylock(&Giant)) {
		zero_state = 0;
		m = vm_pageq_find(PQ_FREE, free_rover, FALSE);
		if (m != NULL && (m->flags & PG_ZERO) == 0) {
			vm_page_queues[m->queue].lcnt--;
			TAILQ_REMOVE(&vm_page_queues[m->queue].pl, m, pageq);
			m->queue = PQ_NONE;
			pmap_zero_page(VM_PAGE_TO_PHYS(m));
			vm_page_flag_set(m, PG_ZERO);
			m->queue = PQ_FREE + m->pc;
			vm_page_queues[m->queue].lcnt++;
			TAILQ_INSERT_TAIL(&vm_page_queues[m->queue].pl, m,
			    pageq);
			++vm_page_zero_count;
			++cnt_prezero;
			if (vm_page_zero_count >= ZIDLE_HI(cnt.v_free_count))
				zero_state = 1;
		}
		free_rover = (free_rover + PQ_PRIME2) & PQ_L2_MASK;
		mtx_unlock(&Giant);
		return (1);
	}
	return(0);
}

