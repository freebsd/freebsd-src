/*
 * Copyright (c) 2000 Andrew Gallatin
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
 * $FreeBSD$
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/termios.h>

#include <machine/rpb.h>
#include <machine/cpuconf.h>
#include <machine/clock.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <alpha/pci/t2var.h>
#include <alpha/pci/t2reg.h>

#include "sio.h"
#include "sc.h"
#ifndef	CONSPEED
#define	CONSPEED TTYDEF_SPEED
#endif
static int comcnrate = CONSPEED;

void dec_2100_a500_init __P((int));
static void dec_2100_a500_cons_init __P((void));
static void dec_2100_a500_intr_init  __P((void ));

extern int siocnattach __P((int, int));
extern int siogdbattach __P((int, int));
extern int sccnattach __P((void));

extern vm_offset_t t2_csr_base;

void
dec_2100_a500_init(cputype)
{
	/*
	 * See if we're a `Sable' or a `Lynx'.
	 */
	if (cputype == ST_DEC_2100_A500) {
		sable_lynx_base = SABLE_BASE;
		platform.family = "DEC AlphaServer 2100";
	} else if (cputype == ST_DEC_2100A_A500) {
		sable_lynx_base = LYNX_BASE;
		platform.family = "DEC AlphaServer 2100A";
	} else {
		sable_lynx_base = SABLE_BASE;
		platform.family = "DEC AlphaServer 2100?????";
	}

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "t2";
	platform.cons_init = dec_2100_a500_cons_init;
	platform.pci_intr_init = dec_2100_a500_intr_init;

	t2_init();
}

/* XXX for forcing comconsole when srm serial console is used */
extern int comconsole; 

static void
dec_2100_a500_cons_init()
{
	struct ctb *ctb;
	t2_init();

#ifdef DDB
	siogdbattach(0x2f8, 9600);
#endif
	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);

	switch (ctb->ctb_term_type) {
	case 2:
		/* serial console ... */
		/* XXX */
		/*
		 * Delay to allow PROM putchars to complete.
		 * FIFO depth * character time,
		 * character time = (1000000 / (defaultrate / 10))
		 */
		DELAY(160000000 / comcnrate);
		/*
		 * force a comconsole on com1 if the SRM has a serial console
		 */
		comconsole = 0;
		if (siocnattach(0x3f8, comcnrate))
			panic("can't init serial console");

		boothowto |= RB_SERIAL;
		break;

	case 3:
		/* display console ... */
		/* XXX */
#if NSC > 0
		sccnattach();
#else
		panic("not configured to use display && keyboard console");
#endif
		break;

	default:
		printf("ctb->ctb_term_type = 0x%lx\n", ctb->ctb_term_type);
		panic("consinit: unknown console type");
	}
}

void
dec_2100_a500_intr_init(void )
{

	outb(SLAVE0_ICU, 0);
	outb(SLAVE1_ICU, 0);
	outb(SLAVE2_ICU, 0);
	outb(MASTER_ICU, 0x44);
}
