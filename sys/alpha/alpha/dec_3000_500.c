/*
 * Copyright (c) 1997 by Matthew Jacob
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

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/termios.h>
#include <sys/bus.h>

#include <machine/rpb.h>
#include <machine/cpuconf.h>

#include <alpha/tlsb/tlsbreg.h>
#include <alpha/tlsb/gbusreg.h>
#include <alpha/tlsb/zsvar.h>

void dec_3000_500_init(int);
static void dec_3000_500_cons_init(void);

static const char dec_3000_500_sp[] = "DEC 3000/400 (\"Sandpiper\")";
static const char dec_3000_500_sf[] = "DEC 3000/500 (\"Flamingo\")";

const struct alpha_variation_table dec_3000_500_variations[] = {
	{ SV_ST_SANDPIPER, dec_3000_500_sp },
	{ SV_ST_FLAMINGO, dec_3000_500_sf },
	{ SV_ST_HOTPINK, "DEC 3000/500X (\"Hot Pink\")" },
	{ SV_ST_FLAMINGOPLUS, "DEC 3000/800 (\"Flamingo+\")" },
	{ SV_ST_SANDPLUS, "DEC 3000/600 (\"Sandpiper+\")" },
	{ SV_ST_SANDPIPER45, "DEC 3000/700 (\"Sandpiper45\")" },
	{ SV_ST_FLAMINGO45, "DEC 3000/900 (\"Flamingo45\")" },
	{ 0, NULL },
};

void
dec_3000_500_init(int cputype)
{
	u_int64_t variation;

	platform.family = "DEC 3000/500 (\"Flamingo\")";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		variation = hwrpb->rpb_variation & SV_ST_MASK;
		if (variation == SV_ST_ULTRA) {
			/* These are really the same. */
			variation = SV_ST_FLAMINGOPLUS;
		}
		if ((platform.model = alpha_variation_name(variation,
		    dec_3000_500_variations)) == NULL) {
			/*
			 * This is how things used to be done.
			 */
			if (variation == SV_ST_RESERVED) {
				if (hwrpb->rpb_variation & SV_GRAPHICS)
					platform.model = dec_3000_500_sf;
				else
					platform.model = dec_3000_500_sp;
			} else
				platform.model = alpha_unknown_sysname();
		}
	}

	platform.iobus = "tcasic";
	platform.cons_init = dec_3000_500_cons_init;
}

/*
 * dec_3000_500_cons_init- not needed right now.
 *
 */
static void
dec_3000_500_cons_init(void)
{

	return;
}
