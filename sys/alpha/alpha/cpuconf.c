/* $FreeBSD$ */
/*	$NetBSD: cpuconf.c,v 1.10 1998/03/20 21:48:21 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/cpuconf.h>
#include <machine/rpb.h>

#include "opt_cpu.h"

#ifdef DEC_3000_500
extern void dec_3000_500_init __P((int));
#else
#define	dec_3000_500_init	platform_not_configured
#endif

#ifdef DEC_3000_300
extern void dec_3000_300_init __P((int));
#else
#define	dec_3000_300_init	platform_not_configured
#endif

#ifdef DEC_AXPPCI_33
extern void dec_axppci_33_init __P((int));
#else
#define	dec_axppci_33_init	platform_not_configured
#endif

#ifdef DEC_KN8AE
extern void dec_kn8ae_init __P((int));
#else
#define	dec_kn8ae_init		platform_not_configured
#endif

#ifdef DEC_2100_A50
extern void dec_2100_a50_init __P((int));
#else
#define	dec_2100_a50_init	platform_not_configured
#endif

#ifdef DEC_KN20AA
extern void dec_kn20aa_init __P((int));
#else
#define	dec_kn20aa_init		platform_not_configured
#endif

#ifdef DEC_EB64PLUS
extern void dec_eb64plus_init __P((int));
#else
#define	dec_eb64plus_init	platform_not_configured
#endif

#ifdef DEC_EB164
extern void dec_eb164_init __P((int));
#else
#define	dec_eb164_init		platform_not_configured
#endif

#ifdef DEC_KN300
extern void dec_kn300_init __P((int));
#else
#define	dec_kn300_init		platform_not_configured
#endif

#ifdef AVALON_A12
extern void avalon_a12_init __P((int));
#else
#define	avalon_a12_init		platform_not_configured
#endif

#ifdef DEC_ST550
extern void st550_init __P((int));
#else
#define	st550_init		platform_not_configured
#endif

#ifdef DEC_ST6600
extern void st6600_init __P((int));
#else
#define	st6600_init		platform_not_configured
#endif

#ifdef DEC_1000A
extern void dec_1000a_init __P((int));
#else
#define dec_1000a_init          platform_not_configured
#endif

#ifdef DEC_2100_A500
extern void dec_2100_a500_init __P((int));
#else
#define dec_2100_a500_init     platform_not_configured
#endif

#ifdef API_UP1000
extern void api_up1000_init __P((int));
#else
#define     api_up1000_init platform_not_configured
#endif

struct cpuinit cpuinit[] = {
	cpu_notsupp("???"),			     /*  0: ??? */
	cpu_notsupp("ST_ADU"),			     /*  1: ST_ADU */
	cpu_notsupp("ST_DEC_4000"),		     /*  2: ST_DEC_4000 */
	cpu_notsupp("ST_DEC_7000"),		     /*  3: ST_DEC_7000 */
	cpu_init(dec_3000_500_init,"DEC_3000_500"),  /*  4: ST_DEC_3000_500 */
	cpu_notsupp("???"),			     /*  5: ??? */
	cpu_notsupp("ST_DEC_2000_300"),		     /*  6: ST_DEC_2000_300 */
	cpu_init(dec_3000_300_init,"DEC_3000_300"),  /*  7: ST_DEC_3000_300 */
	cpu_init(avalon_a12_init,"ST_AVALON_A12"),   /*  8: ST_AVALON_A12 */
	cpu_init(dec_2100_a500_init, "ST_DEC_2100_A500"),	/*  9: ST_DEC_2100_A500 */
	cpu_notsupp("ST_DEC_APXVME_64"),	     /* 10: ST_DEC_APXVME_64 */
	cpu_init(dec_axppci_33_init,"DEC_AXPPCI_33"),/* 11: ST_DEC_AXPPCI_33 */
	cpu_init(dec_kn8ae_init,"DEC_KN8AE"),	     /* 12: ST_DEC_21000 */
	cpu_init(dec_2100_a50_init,"DEC_2100_A50"),  /* 13: ST_DEC_2100_A50 */
	cpu_notsupp("ST_DEC_MUSTANG"),		     /* 14: ST_DEC_MUSTANG */
	cpu_init(dec_kn20aa_init,"DEC_KN20AA"),	     /* 15: ST_DEC_KN20AA */
	cpu_notsupp("???"),			     /* 16: ??? */
	cpu_init(dec_1000a_init, "ST_DEC_1000"),     /* 17: ST_DEC_1000 */
	cpu_notsupp("???"),			     /* 18: ??? */
	cpu_notsupp("ST_EB66"),			     /* 19: ST_EB66 */
	cpu_init(dec_eb64plus_init,"DEC_EB64PLUS"),  /* 20: ST_EB64P */
	cpu_notsupp("ST_ALPHABOOK1"),		     /* 21: ST_ALPHABOOK1 */
	cpu_init(dec_kn300_init,"DEC_KN300"),	     /* 22: ST_DEC_4100 */
	cpu_notsupp("ST_DEC_EV45_PBP"),		     /* 23: ST_DEC_EV45_PBP */
	cpu_init(dec_2100_a500_init, "ST_DEC_2100A_A500"), /* 24: ST_DEC_2100A_A500 */
	cpu_notsupp("???"),			     /* 25: ??? */
	cpu_init(dec_eb164_init,"DEC_EB164"),	     /* 26: ST_EB164 */
	cpu_init(dec_1000a_init,"ST_DEC_1000A"),     /* 27: ST_DEC_1000A */
	cpu_notsupp("ST_DEC_ALPHAVME_224"),	   /* 28: ST_DEC_ALPHAVME_224 */
	cpu_notsupp("???"),			     /* 29: ??? */
	cpu_init(st550_init,"DEC_ST550"),            /* 30: ST_DEC_550 */
	cpu_notsupp("???"),			     /* 31: ??? */
	cpu_notsupp("ST_DEC_EV56_PBP"),		     /* 32: ST_DEC_EV56_PBP */
	cpu_notsupp("ST_DEC_ALPHAVME_320"),	   /* 33: ST_DEC_ALPHAVME_320 */
	cpu_init(st6600_init, "ST_DEC_ST6600"),	     /* 34: ST_DEC_6600 */
};
int ncpuinit = (sizeof(cpuinit) / sizeof(cpuinit[0]));

struct cpuinit api_cpuinit[] = {
	cpu_notsupp("???"),		       	     /*  0: ??? */
	cpu_init(api_up1000_init,"API_UP1000"),      /*  1: ST_API_UP1000 */
};
int napi_cpuinit = (sizeof(api_cpuinit) / sizeof(api_cpuinit[0]));


void
platform_not_configured(int cputype)
{
	struct cpuinit *cpu;
	int cpuidx;

	cputype = hwrpb->rpb_type;

	if (cputype < 1)
		cputype *= -1;
	
	if (cputype >= API_ST_BASE) {
		cpuidx = cputype - API_ST_BASE;
		cpu = api_cpuinit;
	} else {
		cpuidx = cputype;
		cpu = cpuinit;
	}

	printf("\n");
	printf("Support for system type %d is not present in this kernel.\n",
	    cputype);
	printf("Please build a kernel with \"options %s\" and reboot.\n",
	    cpu[cpuidx].option);
	printf("\n");   
	panic("platform not configured\n");
}

void
platform_not_supported(int cputype)
{
	const char *typestr;

	cputype = hwrpb->rpb_type;

	if (cputype < 1)
		cputype *= -1;

	if (cputype >= ncpuinit)
		typestr = "???";
	else
		typestr = cpuinit[cputype].option;

	printf("\n");
	printf("FreeBSD does not yet support system type %d (%s).\n", cputype,
	     typestr);
	printf("\n");
	panic("platform not supported");
}
