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

#include <machine/cpuconf.h>
#include <machine/md_var.h>
#include <machine/rpb.h>

#include <alpha/tlsb/tlsbreg.h>
#include <alpha/tlsb/gbusreg.h>
#include <alpha/tlsb/zsvar.h>

void dec_3000_300_init(int);
static void dec_3000_300_cons_init(void);

static const struct alpha_variation_table dec_3000_300_variations[] = {
	{ SV_ST_PELICAN, "DEC 3000/300 (\"Pelican\")" },
	{ SV_ST_PELICA, "DEC 3000/300L (\"Pelica\")" },
	{ SV_ST_PELICANPLUS, "DEC 3000/300X (\"Pelican+\")" },
	{ SV_ST_PELICAPLUS, "DEC 3000/300LX (\"Pelica+\")" },
	{ 0, NULL },
};

void
dec_3000_300_init(int cputype)
{
	u_int64_t variation;

	platform.family = "DEC 3000/300 (\"Pelican\")";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		variation = hwrpb->rpb_variation & SV_ST_MASK;
		if ((platform.model = alpha_variation_name(variation,
		    dec_3000_300_variations)) == NULL)
			platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "tcasic";
	platform.cons_init = dec_3000_300_cons_init;
}

/*
 * dec_3000_300_cons_init- not needed right now.
 *
 */
static void
dec_3000_300_cons_init(void)
{

	return;
}
