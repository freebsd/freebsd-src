/*-
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

/* $FreeBSD$ */

#ifndef ARM_AT91_AT91_SMC_H
#define ARM_AT91_AT91_SMC_H

/* Registers */
#define	SMC_SETUP	0x00
#define SMC_PULSE	0x04
#define SMC_CYCLE	0x08
#define	SMC_MODE	0x0C

#define	SMC_CS_OFF(cs)	(0x10 * (cs))

/* Setup */
#define	SMC_SETUP_NCS_RD_SETUP(x)	((x) << 24)
#define	SMC_SETUP_NRD_SETUP(x)		((x) << 16)
#define	SMC_SETUP_NCS_WR_SETUP(x)	((x) << 8)
#define	SMC_SETUP_NWE_SETUP(x)		(x)

/* Pulse */
#define	SMC_PULSE_NCS_RD_PULSE(x)	((x) << 24)
#define	SMC_PULSE_NRD_PULSE(x)		((x) << 16)
#define	SMC_PULSE_NCS_WR_PULSE(x)	((x) << 8)
#define	SMC_PULSE_NWE_PULSE(x)		(x)

/* Cycle */
#define	SMC_CYCLE_NRD_CYCLE(x)		((x) << 16)
#define	SMC_CYCLE_NWE_CYCLE(x)		(x)

/* Mode */
#define	SMC_MODE_READ			(1 << 0)
#define	SMC_MODE_WRITE			(1 << 1)
#define	SMC_MODE_EXNW_DISABLED		(0 << 4)
#define	SMC_MODE_EXNW_FROZEN_MODE	(2 << 4)
#define	SMC_MODE_EXNW_READY_MODE	(3 << 4)
#define	SMC_MODE_BAT			(1 << 8)
#define	SMC_MODE_DBW_8BIT		(0 << 12)
#define	SMC_MODE_DBW_16BIT		(1 << 12)
#define	SMC_MODE_DBW_32_BIT		(2 << 12)
#define	SMC_MODE_TDF_CYCLES(x)		((x) << 16)
#define	SMC_MODE_TDF_MODE		(1 << 20)
#define	SMC_MODE_PMEN			(1 << 24)
#define SMC_PS_4BYTE			(0 << 28)
#define SMC_PS_8BYTE			(1 << 28)
#define SMC_PS_16BYTE			(2 << 28)
#define SMC_PS_32BYTE			(3 << 28)

/*
 * structure to ease init. See the SMC chapter in the datasheet for
 * the appropriate SoC you are using for details.
 */
struct at91_smc_init
{
	/* Setup register */
	uint8_t	ncs_rd_setup;
	uint8_t nrd_setup;
	uint8_t ncs_wr_setup;
	uint8_t nwe_setup;

	/* Pulse register */
	uint8_t	ncs_rd_pulse;
	uint8_t nrd_pulse;
	uint8_t ncs_wr_pulse;
	uint8_t nwe_pulse;

	/* Cycle register */
	uint16_t nrd_cycle;
	uint16_t nwe_cycle;

	/* Mode register */
	uint8_t mode;		/* Combo of READ/WRITE/EXNW fields */
	uint8_t bat;
	uint8_t dwb;
	uint8_t tdf_cycles;
	uint8_t tdf_mode;
	uint8_t pmen;
	uint8_t ps;
};

/*
 * Convenience routine to fill in SMC registers for a given chip select.
 */
void at91_smc_setup(int id, int cs, const struct at91_smc_init *smc);

/*
 * Disable/Enable different External Bus Interfaces (EBI)
 */
void at91_ebi_enable(int cs);
void at91_ebi_disable(int cs);

#endif /* ARM_AT91_AT91_SMC_H */
