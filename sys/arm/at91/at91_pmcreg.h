/*-
 * Copyright (c) 2005 M. Warner Losh.  All rights reserved.
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

#ifndef ARM_AT91_AT91_PMCREG_H
#define ARM_AT91_AT91_PMCREG_H

/* Registers */
#define	PMC_SCER	0x00		/* System Clock Enable Register */
#define	PMC_SCDR	0x04		/* System Clock Disable Register */
#define	PMC_SCSR	0x08		/* System Clock Status Register */
		/*	0x0c		   reserved */
#define	PMC_PCER	0x10		/* Peripheral Clock Enable Register */
#define	PMC_PCDR	0x14		/* Peripheral Clock Disable Register */
#define	PMC_PCSR	0x18		/* Peripheral Clock Status Register */
		/*	0x1c		   reserved */
#define CKGR_MOR	0x20		/* Main Oscillator Register */
#define CKGR_MCFR	0x24		/* Main Clock Frequency Register */
#define CKGR_PLLAR	0x28		/* PLL A Register */
#define CKGR_PLLBR	0x2c		/* PLL B Register */
#define PMC_MCKR	0x30		/* Master Clock Register */
		/*	0x34		   reserved */
		/*	0x38		   reserved */
		/*	0x3c		   reserved */
#define PMC_PCK0	0x40		/* Programmable Clock 0 Register */
#define PMC_PCK1	0x44		/* Programmable Clock 1 Register */
#define PMC_PCK2	0x48		/* Programmable Clock 2 Register */
#define PMC_PCK3	0x4c		/* Programmable Clock 3 Register */
		/*	0x50		   reserved */
		/*	0x54		   reserved */
		/*	0x58		   reserved */
		/*	0x5c		   reserved */
#define PMC_IER		0x60		/* Interrupt Enable Register */
#define PMC_IDR		0x64		/* Interrupt Disable Register */
#define PMC_SR		0x68		/* Status Register */
#define PMC_IMR		0x6c		/* Interrupt Mask Register */

/* PMC System Clock Enable Register */
/* PMC System Clock Disable Register */
/* PMC System Clock StatusRegister */
#define PMC_SCER_PCK	(1UL << 0)	/* PCK: Processor Clock Enable */
#define PMC_SCER_UDP	(1UL << 1)	/* UDP: USB Device Port Clock Enable */
#define PMC_SCER_MCKUDP	(1UL << 2)	/* MCKUDP: Master disable susp/res */
#define PMC_SCER_UHP	(1UL << 4)	/* UHP: USB Host Port Clock Enable */
#define PMC_SCER_PCK0	(1UL << 8)	/* PCK0: Programmable Clock out en */
#define PMC_SCER_PCK1	(1UL << 10)	/* PCK1: Programmable Clock out en */
#define PMC_SCER_PCK2	(1UL << 11)	/* PCK2: Programmable Clock out en */
#define PMC_SCER_PCK3	(1UL << 12)	/* PCK3: Programmable Clock out en */

/* PMC Peripheral Clock Enable Register */
/* PMC Peripheral Clock Disable Register */
/* PMC Peripheral Clock Status Register */
/* Each bit here is 1 << peripheral number  to enable/disable/status */

/* PMC Clock Generator Main Oscillator Register */
#define CKGR_MOR_MOSCEN	(1UL << 0)	/* MOSCEN: Main Oscillator Enable */
#define CKGR_MOR_OSCBYPASS (1UL << 1)	/* Oscillator Bypass */
#define CKGR_MOR_OSCOUNT(x) (x << 8)	/* Main Oscillator Start-up Time */

/* PMC Clock Generator Main Clock Frequency Register */
#define CKGR_MCFR_MAINRDY	(1UL << 16)	/* Main Clock Ready */
#define CKGR_MCFR_MAINF_MASK	0xfffful	/* Main Clock Frequency */

/* PMC Interrupt Enable Register */
/* PMC Interrupt Disable Register */
/* PMC Status Register */
/* PMC Interrupt Mask Register */
#define PMC_IER_MOSCS	(1UL << 0)	/* Main Oscillator Status */
#define PMC_IER_LOCKA	(1UL << 1)	/* PLL A Locked */
#define PMC_IER_LOCKB	(1UL << 2)	/* PLL B Locked */
#define PMC_IER_MCKRDY	(1UL << 3)	/* Master Clock Status */
#define PMC_IER_PCK0RDY	(1UL << 8)	/* Programmable Clock 0 Ready */
#define PMC_IER_PCK1RDY	(1UL << 9)	/* Programmable Clock 1 Ready */
#define PMC_IER_PCK2RDY	(1UL << 10)	/* Programmable Clock 2 Ready */
#define PMC_IER_PCK3RDY	(1UL << 11)	/* Programmable Clock 3 Ready */

/*
 * PLL input frequency spec sheet says it must be between 1MHz and 32MHz,
 * but it works down as low as 100kHz, a frequency necessary for some
 * output frequencies to work.
 */
#define PMC_PLL_MIN_IN_FREQ	100000
#define PMC_PLL_MAX_IN_FREQ	32000000

/*
 * PLL Max output frequency is 240MHz.  The errata says 180MHz is the max
 * for some revisions of this part.  Be more permissive and optimistic.
 */
#define PMC_PLL_MAX_OUT_FREQ	240000000

#define PMC_PLL_MULT_MIN	2
#define PMC_PLL_MULT_MAX	2048

#define PMC_PLL_SHIFT_TOL	5	/* Allow errors 1 part in 32 */

#define PMC_PLL_FAST_THRESH	155000000

#endif /* ARM_AT91_AT91_PMCREG_H */
