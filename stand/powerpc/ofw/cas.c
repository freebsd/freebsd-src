/*-
 * Copyright (c) 2019 Leandro Lupori
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <openfirm.h>
#include <stand.h>

/* PVR */
#define PVR_CPU_P8E		0x004b0000
#define PVR_CPU_P8NVL		0x004c0000
#define PVR_CPU_P8		0x004d0000
#define PVR_CPU_P9		0x004e0000
#define PVR_CPU_MASK		0xffff0000

#define PVR_ISA_207		0x0f000004
#define PVR_ISA_300		0x0f000005
#define PVR_ISA_MASK		0xffffffff

/* loader version of kernel's CPU_MAXSIZE */
#define MAX_CPUS		((uint32_t)256u)

/* Option Vectors' settings */

/* length of ignored OV */
#define OV_IGN_LEN		0

/* byte 1 (of any OV) */
#define OV_IGN			0x80

/* Option Vector 5 */

/* byte 2 */
#define OV5_LPAR		0x80
#define OV5_SPLPAR		0x40
#define OV5_DRMEM		0x20
#define OV5_LP			0x10
#define OV5_ALPHA_PART		0x08
#define OV5_DMA_DELAY		0x04
#define OV5_DONATE_CPU		0x02
#define OV5_MSI			0x01

/* 9-12: max cpus */
#define OV5_MAX_CPUS(n)		((MAX_CPUS >> (3*8 - (n)*8)) & 0xff)

/* 13-14: LoPAPR Level */
#define LOPAPR_LEVEL		0x0101	/* 1.1 */
#define OV5_LOPAPR_LEVEL(n)	((LOPAPR_LEVEL >> (8 - (n)*8)) & 0xff)

/* byte 17: Platform Facilities */
#define OV5_RNG			0x80
#define OV5_COMP_ENG		0x40
#define OV5_ENC_ENG		0x20

/* byte 21: Sub-Processors */
#define OV5_NO_SUBPROCS		0
#define OV5_SUBPROCS		1

/* byte 23: interrupt controller */
#define OV5_INTC_XICS		0

/* byte 24: MMU */
#define OV5_MMU_HPT		0

/* byte 25: HPT MMU Extensions */
#define OV5_HPT_EXT_NONE	0

/* byte 26: Radix MMU Extensions */
#define OV5_RPT_EXT_NONE	0


struct pvr {
	uint32_t	mask;
	uint32_t	val;
};

struct opt_vec_ignore {
	char	data[2];
} __packed;

struct opt_vec4 {
	char data[3];
} __packed;

struct opt_vec5 {
	char data[27];
} __packed;

static struct ibm_arch_vec {
	struct pvr		pvr_list[7];
	uint8_t			num_opts;
	struct opt_vec_ignore	vec1;
	struct opt_vec_ignore	vec2;
	struct opt_vec_ignore	vec3;
	struct opt_vec4		vec4;
	struct opt_vec5		vec5;
} __packed ibm_arch_vec = {
	/* pvr_list */ {
		{ PVR_CPU_MASK, PVR_CPU_P8 },		/* POWER8 */
		{ PVR_CPU_MASK, PVR_CPU_P8E },		/* POWER8E */
		{ PVR_CPU_MASK, PVR_CPU_P8NVL },	/* POWER8NVL */
		{ PVR_CPU_MASK, PVR_CPU_P9 },		/* POWER9 */
		{ PVR_ISA_MASK, PVR_ISA_207 },		/* All ISA 2.07 */
		{ PVR_ISA_MASK, PVR_ISA_300 },		/* All ISA 3.00 */
		{ 0, 0xffffffffu }			/* terminator */
	},
	4,	/* num_opts (4 actually means 5 option vectors) */
	{ OV_IGN_LEN, OV_IGN },		/* OV1 */
	{ OV_IGN_LEN, OV_IGN },		/* OV2 */
	{ OV_IGN_LEN, OV_IGN },		/* OV3 */
	/* OV4 (can't be ignored) */ {
		sizeof(struct opt_vec4) - 2,	/* length (n-2) */
		0,
		10 /* Minimum VP entitled capacity percentage * 100
		    * (if absent assume 10%) */
	},
	/* OV5 */ {
		sizeof(struct opt_vec5) - 2,	/* length (n-2) */
		0,				/* don't ignore */
		OV5_LPAR | OV5_SPLPAR | OV5_LP | OV5_MSI,
		0,
		0,	/* Cooperative Memory Over-commitment */
		0,	/* Associativity Information Option */
		0,	/* Binary Option Controls */
		0,	/* Reserved */
		0,	/* Reserved */
		OV5_MAX_CPUS(0),
		OV5_MAX_CPUS(1),		/* 10 */
		OV5_MAX_CPUS(2),
		OV5_MAX_CPUS(3),
		OV5_LOPAPR_LEVEL(0),
		OV5_LOPAPR_LEVEL(1),
		0,	/* Reserved */
		0,	/* Reserved */
		0,	/* Platform Facilities */
		0,	/* Reserved */
		0,	/* Reserved */
		0,	/* Reserved */		/* 20 */
		OV5_NO_SUBPROCS,
		0,	/* DRMEM_V2 */
		OV5_INTC_XICS,
		OV5_MMU_HPT,
		OV5_HPT_EXT_NONE,
		OV5_RPT_EXT_NONE
	}
};

static __inline register_t
mfpvr(void)
{
	register_t value;

	__asm __volatile ("mfpvr %0" : "=r"(value));

	return (value);
}

static __inline int
ppc64_hv(void)
{
	int hv;

	/* PSL_HV is bit 3 of 64-bit MSR */
	__asm __volatile ("mfmsr %0\n\t"
		"rldicl %0,%0,4,63" : "=r"(hv));

	return (hv);
}

int
ppc64_cas(void)
{
	int rc;
	ihandle_t ihandle;
	cell_t err;

	/* Perform CAS only for POWER8 and later cores */
	switch (mfpvr() & PVR_CPU_MASK) {
		case PVR_CPU_P8:
		case PVR_CPU_P8E:
		case PVR_CPU_P8NVL:
		case PVR_CPU_P9:
			break;
		default:
			return (0);
	}

	/* Skip CAS when running on PowerNV */
	if (ppc64_hv())
		return (0);

	ihandle = OF_open("/");
	if (ihandle == -1) {
		printf("cas: failed to open / node\n");
		return (-1);
	}

	if (rc = OF_call_method("ibm,client-architecture-support",
	    ihandle, 1, 1, &ibm_arch_vec, &err))
		printf("cas: failed to call CAS method\n");
	else if (err) {
		printf("cas: error: 0x%08lX\n", err);
		rc = -1;
	}

	OF_close(ihandle);
	return (rc);
}
