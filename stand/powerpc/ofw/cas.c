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

#include <sys/endian.h>

/* #define CAS_DEBUG */
#ifdef CAS_DEBUG
#define DPRINTF(fmt, ...)	printf(fmt, ## __VA_ARGS__)
#else
#define DPRINTF(fmt, ...)	do { ; } while (0)
#endif

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
#define OV5_MMU_INDEX		24
#define OV5_MMU_HPT		0
#define OV5_MMU_RADIX		0x40
#define OV5_MMU_EITHER		0x80
#define OV5_MMU_DYNAMIC		0xc0

/* byte 25: HPT MMU Extensions */
#define OV5_HPT_EXT_INDEX	25
#define OV5_HPT_GTSE		0x40

/* byte 26: Radix MMU Extensions */
#define OV5_RADIX_EXT_INDEX	26
#define OV5_RADIX_GTSE		0x40


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
		{ htobe32(PVR_CPU_MASK), htobe32(PVR_CPU_P8) },
		{ htobe32(PVR_CPU_MASK), htobe32(PVR_CPU_P8E) },
		{ htobe32(PVR_CPU_MASK), htobe32(PVR_CPU_P8NVL) },
		{ htobe32(PVR_CPU_MASK), htobe32(PVR_CPU_P9) },
		{ htobe32(PVR_ISA_MASK), htobe32(PVR_ISA_207) },
		{ htobe32(PVR_ISA_MASK), htobe32(PVR_ISA_300) },
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
		0,
		0
	}
};

int
ppc64_cas(void)
{
	phandle_t pkg;
	ihandle_t inst;
	cell_t err;
	uint8_t buf[16], idx, val;
	int i, len, rc, radix_mmu;
	const char *var;
	char *ov5;

	pkg = OF_finddevice("/chosen");
	if (pkg == -1) {
		printf("cas: couldn't find /chosen\n");
		return (-1);
	}

	len = OF_getprop(pkg, "ibm,arch-vec-5-platform-support", buf,
	    sizeof(buf));
	if (len == -1)
		/* CAS not supported */
		return (0);

	radix_mmu = 0;
	ov5 = ibm_arch_vec.vec5.data;
	for (i = 0; i < len; i += 2) {
		idx = buf[i];
		val = buf[i + 1];
		DPRINTF("idx 0x%02x val 0x%02x\n", idx, val);

		switch (idx) {
		case OV5_MMU_INDEX:
			/*
			 * Note that testing for OV5_MMU_RADIX/OV5_MMU_EITHER
			 * also covers OV5_MMU_DYNAMIC.
			 */
			if ((val & OV5_MMU_RADIX) || (val & OV5_MMU_EITHER))
				radix_mmu = 1;
			break;

		case OV5_RADIX_EXT_INDEX:
			if (val & OV5_RADIX_GTSE)
				ov5[idx] = OV5_RADIX_GTSE;
			break;

		case OV5_HPT_EXT_INDEX:
		default:
			break;
		}
	}

	if (!radix_mmu)
		/*
		 * If radix is not supported, set radix_mmu to 0 to avoid
		 * the kernel trying to use it and panic.
		 */
		setenv("radix_mmu", "0", 1);
	else if ((var = getenv("radix_mmu")) != NULL && var[0] == '0')
		radix_mmu = 0;
	else
		ov5[OV5_MMU_INDEX] = OV5_MMU_RADIX;

	inst = OF_open("/");
	if (inst == -1) {
		printf("cas: failed to open / node\n");
		return (-1);
	}

	DPRINTF("MMU 0x%02x RADIX_EXT 0x%02x\n",
	    ov5[OV5_MMU_INDEX], ov5[OV5_RADIX_EXT_INDEX]);
	rc = OF_call_method("ibm,client-architecture-support",
	    inst, 1, 1, &ibm_arch_vec, &err);
	if (rc != 0 || err) {
		printf("cas: CAS method returned an error: rc %d err %jd\n",
		    rc, (intmax_t)err);
		rc = -1;
	}

	OF_close(inst);
	printf("cas: selected %s MMU\n", radix_mmu ? "radix" : "hash");
	return (rc);
}
