/*
 * Copyright (c) 2000 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_dev_sc.h"

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/termios.h>

#include <machine/clock.h>
#include <machine/cpuconf.h>
#include <machine/intr.h>
#include <machine/md_var.h>
#include <machine/rpb.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/mcbus/mcbusreg.h>
#include <alpha/mcbus/mcbusvar.h>

#ifndef	CONSPEED
#define	CONSPEED	TTYDEF_SPEED
#endif
static int comcnrate = CONSPEED;

void dec_kn300_init(void);
void dec_kn300_cons_init(void);

#define	ALPHASERVER_4100	"AlphaServer 4100"

const struct alpha_variation_table dec_kn300_variations[] = {
	{ 0, ALPHASERVER_4100 },
	{ 0, NULL },
};


extern int siocnattach(int, int);
extern int siogdbattach(int, int);
extern int sccnattach(void);

void
dec_kn300_init()
{
	u_int64_t variation;

	platform.family = ALPHASERVER_4100;

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		variation = hwrpb->rpb_variation & SV_ST_MASK;
		if ((platform.model = alpha_variation_name(variation,
		    dec_kn300_variations)) == NULL)
			platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "mcbus";
	platform.cons_init = dec_kn300_cons_init;
}

extern int comconsole;

void
dec_kn300_cons_init()
{
	struct ctb *ctb;

	mcbus_init();
#ifdef	DDB
	siogdbattach(0x2f8, 57600);
#endif

	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);

	switch (ctb->ctb_term_type) {
	case 2:
		/* serial console ... */
		/*
		 * Delay to allow PROM putchars to complete.
		 * FIFO depth * character time,
		 * character time = (1000000 / (defaultrate / 10))
		 */
		DELAY(160000000 / comcnrate);
		comconsole = 0;
		if (siocnattach(0x3f8, comcnrate))
			panic("can't init serial console");

		boothowto |= RB_SERIAL;
		break;

	case 3:
#ifdef DEV_SC
		sccnattach();
#else
		panic("not configured to use display && keyboard console");
#endif
		break;

	default:
		printf("ctb->ctb_term_type = 0x%lx\n", ctb->ctb_term_type);
		printf("ctb->ctb_turboslot = 0x%lx\n", ctb->ctb_turboslot);
		panic("consinit: unknown cons type %ld\n", ctb->ctb_term_type);
	}
}
