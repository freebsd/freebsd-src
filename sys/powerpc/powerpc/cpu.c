/*-
 * Copyright (c) 2001 Matt Thomas.
 * Copyright (c) 2001 Tsubai Masanari.
 * Copyright (c) 1998, 1999, 2001 Internet Research Institute, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by
 *	Internet Research Institute, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (C) 2003 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from $NetBSD: cpu_subr.c,v 1.1 2003/02/03 17:10:09 matt Exp $
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <machine/bus.h>
#include <machine/hid.h>
#include <machine/md_var.h>
#include <machine/spr.h>

struct cputab {
	const char	*name;
	uint16_t	version;
	uint16_t	revfmt;
};
#define	REVFMT_MAJMIN	1	/* %u.%u */
#define	REVFMT_HEX	2	/* 0x%04x */
#define	REVFMT_DEC	3	/* %u */
static const struct cputab models[] = {
        { "Motorola PowerPC 601",	MPC601,		REVFMT_DEC },
        { "Motorola PowerPC 602",	MPC602,		REVFMT_DEC },
        { "Motorola PowerPC 603",	MPC603,		REVFMT_MAJMIN },
        { "Motorola PowerPC 603e",	MPC603e,	REVFMT_MAJMIN },
        { "Motorola PowerPC 603ev",	MPC603ev,	REVFMT_MAJMIN },
        { "Motorola PowerPC 604",	MPC604,		REVFMT_MAJMIN },
        { "Motorola PowerPC 604ev",	MPC604ev,	REVFMT_MAJMIN },
        { "Motorola PowerPC 620",	MPC620,		REVFMT_HEX },
        { "Motorola PowerPC 750",	MPC750,		REVFMT_MAJMIN },
        { "IBM PowerPC 750FX",		IBM750FX,	REVFMT_MAJMIN },
        { "Motorola PowerPC 7400",	MPC7400,	REVFMT_MAJMIN },
        { "Motorola PowerPC 7410",	MPC7410,	REVFMT_MAJMIN },
        { "Motorola PowerPC 7450",	MPC7450,	REVFMT_MAJMIN },
        { "Motorola PowerPC 7455",	MPC7455,	REVFMT_MAJMIN },
        { "Motorola PowerPC 8240",	MPC8240,	REVFMT_MAJMIN },
        { "Unknown PowerPC CPU",	0,		REVFMT_HEX }
};

static register_t	l2cr_config = 0;

static void	cpu_print_speed(void);
static void	cpu_config_l2cr(u_int, uint16_t);

void
cpu_setup(u_int cpuid)
{
	u_int		pvr, maj, min, hid0;
	uint16_t	vers, rev, revfmt;
	const struct	cputab *cp;
	const char	*name;
	char		*bitmask;

	pvr = mfpvr();
	vers = pvr >> 16;
	rev = pvr;
	switch (vers) {
	case MPC7410:
		min = (pvr >> 0) & 0xff;
		maj = min <= 4 ? 1 : 2;
		break;
	default:
		maj = (pvr >>  8) & 0xf;
		min = (pvr >>  0) & 0xf;
	}

	for (cp = models; cp->name[0] != '\0'; cp++) {
		if (cp->version == vers)
			break;
	}

	revfmt = cp->revfmt;
	name = cp->name;
	if (rev == MPC750 && pvr == 15) {
		name = "Motorola MPC755";
		revfmt = REVFMT_HEX;
	}

	printf("cpu%d: %s revision ", cpuid, name);

	switch (revfmt) {
	case REVFMT_MAJMIN:
		printf("%u.%u", maj, min);
		break;
	case REVFMT_HEX:
		printf("0x%04x", rev);
		break;
	case REVFMT_DEC:
		printf("%u", rev);
		break;
	}

	hid0 = mfspr(SPR_HID0);

	/*
	 * Configure power-saving mode.
	 */
	switch (vers) {
	case MPC603:
	case MPC603e:
	case MPC603ev:
	case MPC604ev:
	case MPC750:
	case IBM750FX:
	case MPC7400:
	case MPC7410:
	case MPC8240:
	case MPC8245:
		/* Select DOZE mode. */
		hid0 &= ~(HID0_DOZE | HID0_NAP | HID0_SLEEP);
		hid0 |= HID0_DOZE | HID0_DPM;
#ifdef notyet
		powersave = 1;
#endif
		break;

	case MPC7455:
	case MPC7450:
		/* Disable BTIC on 7450 Rev 2.0 or earlier */
		if ((pvr >> 16) == MPC7450 && (pvr & 0xFFFF) <= 0x0200)
			hid0 &= ~HID0_BTIC;
		/* Select NAP mode. */
		hid0 &= ~(HID0_DOZE | HID0_NAP | HID0_SLEEP);
		hid0 |= HID0_NAP | HID0_DPM;
#ifdef notyet
		powersave = 0;		/* but don't use it */
#endif
		break;

	default:
		/* No power-saving mode is available. */ ;
	}

	switch (vers) {
	case IBM750FX:
	case MPC750:
		hid0 &= ~HID0_DBP;		/* XXX correct? */
		hid0 |= HID0_EMCP | HID0_BTIC | HID0_SGE | HID0_BHT;
		break;

	case MPC7400:
	case MPC7410:
		hid0 &= ~HID0_SPD;
		hid0 |= HID0_EMCP | HID0_BTIC | HID0_SGE | HID0_BHT;
		hid0 |= HID0_EIEC;
		break;
	}

	mtspr(SPR_HID0, hid0);

	switch (vers) {
	case MPC7450:
	case MPC7455:
		bitmask = HID0_7450_BITMASK;
		break;
	default:
		bitmask = HID0_BITMASK;
		break;
	}

	switch (vers) {
	case MPC750:
	case IBM750FX:
	case MPC7400:
	case MPC7410:
	case MPC7450:
	case MPC7455:
		cpu_print_speed();
		printf("\n");
		cpu_config_l2cr(cpuid, vers);
		break;

	default:
		printf("\n");
		break;
	}

	printf("cpu%d: HID0 %b\n", cpuid, hid0, bitmask);
}

void
cpu_print_speed(void)
{
	uint64_t	cps;

	mtspr(SPR_MMCR0, SPR_MMCR0_FC);
	mtspr(SPR_PMC1, 0);
	mtspr(SPR_MMCR0, SPR_MMCR0_PMC1SEL(PMCN_CYCLES));
	delay(100000);
	cps = (mfspr(SPR_PMC1) * 10) + 4999;
	printf(", %lld.%02lld MHz", cps / 1000000, (cps / 10000) % 100);
}

void
cpu_config_l2cr(u_int cpuid, uint16_t vers)
{
	u_int l2cr, x, msr;

	l2cr = mfspr(SPR_L2CR);

	/*
	 * For MP systems, the firmware may only configure the L2 cache
	 * on the first CPU.  In this case, assume that the other CPUs
	 * should use the same value for L2CR.
	 */
	if ((l2cr & L2CR_L2E) != 0 && l2cr_config == 0) {
		l2cr_config = l2cr;
	}

	/*
	 * Configure L2 cache if not enabled.
	 */
	if ((l2cr & L2CR_L2E) == 0 && l2cr_config != 0) {
		l2cr = l2cr_config;

		/* Disable interrupts and set the cache config bits. */
		msr = mfmsr();
		mtmsr(msr & ~PSL_EE);
#ifdef ALTIVEC
		if (cpu_altivec)
			__asm __volatile("dssall");
#endif
		__asm __volatile("sync");
		mtspr(SPR_L2CR, l2cr & ~L2CR_L2E);
		__asm __volatile("sync");

		/* Wait for L2 clock to be stable (640 L2 clocks). */
		delay(100);

		/* Invalidate all L2 contents. */
		mtspr(SPR_L2CR, l2cr | L2CR_L2I);
		do {
			x = mfspr(SPR_L2CR);
		} while (x & L2CR_L2IP);

		/* Enable L2 cache. */
		l2cr |= L2CR_L2E;
		mtspr(SPR_L2CR, l2cr);
		mtmsr(msr);
	}

	if (!bootverbose)
		return;

	printf("cpu%d: ", cpuid);

	if (l2cr & L2CR_L2E) {
		if (vers == MPC7450 || vers == MPC7455) {
			u_int l3cr;

			printf("256KB L2 cache");

			l3cr = mfspr(SPR_L3CR);
			if (l3cr & L3CR_L3E)
				printf(", %cMB L3 backside cache",
				   l3cr & L3CR_L3SIZ ? '2' : '1');
			printf("\n");
			return;
		}
		if (vers == IBM750FX) {
			printf("512KB L2 cache\n");
			return;
		}
		switch (l2cr & L2CR_L2SIZ) {
		case L2SIZ_256K:
			printf("256KB");
			break;
		case L2SIZ_512K:
			printf("512KB");
			break;
		case L2SIZ_1M:
			printf("1MB");
			break;
		default:
			printf("unknown size");
		}
		if (l2cr & L2CR_L2WT) {
			printf(" write-through");
		} else {
			printf(" write-back");
		}
		switch (l2cr & L2CR_L2RAM) {
		case L2RAM_FLOWTHRU_BURST:
			printf(" flow-through synchronous burst SRAM");
			break;
		case L2RAM_PIPELINE_BURST:
			printf(" pipelined synchronous burst SRAM");
			break;
		case L2RAM_PIPELINE_LATE:
			printf(" pipelined synchronous late-write SRAM");
			break;
		default:
			printf(" unknown type");
		}

		if (l2cr & L2CR_L2PE)
			printf(" with parity");
		printf(" backside cache");
	} else
		printf("L2 cache not enabled");

	printf("\n");
}
