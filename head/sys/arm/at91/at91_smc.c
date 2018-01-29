/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2014 M. Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <arm/at91/at91reg.h>
#include <arm/at91/at91_smc.h>
#include <arm/at91/at91sam9260reg.h>

/*
 * RD4HW()/WR4HW() read and write at91 hardware register space directly. They
 * serve the same purpose as the RD4()/WR4() idiom you see in many drivers,
 * except that those translate to bus_space calls, but in this code we need to
 * access some devices before bus_space is ready to use.  Of course for this to
 * work the appropriate static device mappings need to be made in machdep.c.
 */
static inline uint32_t 
RD4HW(uint32_t devbase, uint32_t regoff)
{
	
	return *(volatile uint32_t *)(AT91_BASE + devbase + regoff);
}


static inline void
WR4HW(uint32_t devbase, uint32_t regoff, uint32_t val)
{
	
	*(volatile uint32_t *)(AT91_BASE + devbase + regoff) = val;
}


void
at91_smc_setup(int id, int cs, const struct at91_smc_init *smc)
{
	// Need a generic way to get this address for all SoCs... Assume 9260 for now...
	uint32_t base = AT91SAM9260_SMC_BASE + SMC_CS_OFF(cs);

	WR4HW(base, SMC_SETUP, SMC_SETUP_NCS_RD_SETUP(smc->ncs_rd_setup) |
	      SMC_SETUP_NRD_SETUP(smc->nrd_setup) |
	      SMC_SETUP_NCS_WR_SETUP(smc->ncs_wr_setup) |
	      SMC_SETUP_NWE_SETUP(smc->nwe_setup));
	WR4HW(base, SMC_PULSE, SMC_PULSE_NCS_RD_PULSE(smc->ncs_rd_pulse) |
	      SMC_PULSE_NRD_PULSE(smc->nrd_pulse) |
	      SMC_PULSE_NCS_WR_PULSE(smc->ncs_wr_pulse) |
	      SMC_PULSE_NWE_PULSE(smc->nwe_pulse));
	WR4HW(base, SMC_CYCLE, SMC_CYCLE_NRD_CYCLE(smc->nrd_cycle) |
	      SMC_CYCLE_NWE_CYCLE(smc->nwe_cycle));
	WR4HW(base, SMC_MODE, smc->mode | SMC_MODE_TDF_CYCLES(smc->tdf_cycles));
}

void
at91_ebi_enable(int bank)
{

	WR4HW(AT91SAM9260_MATRIX_BASE, AT91SAM9260_EBICSA, (1 << bank) |
	      RD4HW(AT91SAM9260_MATRIX_BASE, AT91SAM9260_EBICSA));
}

void
at91_ebi_disable(int bank)
{

	WR4HW(AT91SAM9260_MATRIX_BASE, AT91SAM9260_EBICSA, ~(1 << bank) &
	      RD4HW(AT91SAM9260_MATRIX_BASE, AT91SAM9260_EBICSA));
}
