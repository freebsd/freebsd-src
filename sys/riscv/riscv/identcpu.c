/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 * Copyright (c) 2022 Mitchell Horne <mhorne@FreeBSD.org>
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
 *
 * Portions of this software were developed by Mitchell Horne
 * <mhorne@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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

#include "opt_platform.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/elf.h>
#include <machine/md_var.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

const char machine[] = "riscv";

SYSCTL_CONST_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD | CTLFLAG_CAPRD,
    machine, "Machine class");

/* Hardware implementation info. These values may be empty. */
register_t mvendorid;	/* The CPU's JEDEC vendor ID */
register_t marchid;	/* The architecture ID */
register_t mimpid;	/* The implementation ID */

u_int mmu_caps;

/* Supervisor-mode extension support. */
bool __read_frequently has_sstc;
bool __read_frequently has_sscofpmf;

struct cpu_desc {
	const char	*cpu_mvendor_name;
	const char	*cpu_march_name;
	u_int		isa_extensions;		/* Single-letter extensions. */
	u_int		mmu_caps;
	u_int		smode_extensions;
#define	 SV_SSTC	(1 << 0)
#define	 SV_SVNAPOT	(1 << 1)
#define	 SV_SVPBMT	(1 << 2)
#define	 SV_SVINVAL	(1 << 3)
#define	 SV_SSCOFPMF	(1 << 4)
};

struct cpu_desc cpu_desc[MAXCPU];

/*
 * Micro-architecture tables.
 */
struct marchid_entry {
	register_t	march_id;
	const char	*march_name;
};

#define	MARCHID_END	{ -1ul, NULL }

/* Open-source RISC-V architecture IDs; globally allocated. */
static const struct marchid_entry global_marchids[] = {
	{ MARCHID_UCB_ROCKET,	"UC Berkeley Rocket"		},
	{ MARCHID_UCB_BOOM,	"UC Berkeley Boom"		},
	{ MARCHID_UCB_SPIKE,	"UC Berkeley Spike"		},
	{ MARCHID_UCAM_RVBS,	"University of Cambridge RVBS"	},
	MARCHID_END
};

static const struct marchid_entry sifive_marchids[] = {
	{ MARCHID_SIFIVE_U7,	"6/7/P200/X200-Series Processor" },
	MARCHID_END
};

/*
 * Known CPU vendor/manufacturer table.
 */
static const struct {
	register_t			mvendor_id;
	const char			*mvendor_name;
	const struct marchid_entry	*marchid_table;
} mvendor_ids[] = {
	{ MVENDORID_UNIMPL,	"Unspecified",		NULL		},
	{ MVENDORID_SIFIVE,	"SiFive",		sifive_marchids	},
	{ MVENDORID_THEAD,	"T-Head",		NULL		},
};

/*
 * The ISA string describes the complete set of instructions supported by a
 * RISC-V CPU. The string begins with a small prefix (e.g. rv64) indicating the
 * base ISA. It is followed first by single-letter ISA extensions, and then
 * multi-letter ISA extensions.
 *
 * Underscores are used mainly to separate consecutive multi-letter extensions,
 * but may optionally appear between any two extensions. An extension may be
 * followed by a version number, in the form of 'Mpm', where M is the
 * extension's major version number, and 'm' is the minor version number.
 *
 * The format is described in detail by the "ISA Extension Naming Conventions"
 * chapter of the unprivileged spec.
 */
#define	ISA_PREFIX		("rv" __XSTRING(__riscv_xlen))
#define	ISA_PREFIX_LEN		(sizeof(ISA_PREFIX) - 1)

static __inline int
parse_ext_s(struct cpu_desc *desc, char *isa, int idx, int len)
{
#define	CHECK_S_EXT(str, flag)						\
	do {								\
		if (strncmp(&isa[idx], (str),				\
		    MIN(strlen(str), len - idx)) == 0) {		\
			desc->smode_extensions |= flag;			\
			return (idx + strlen(str));			\
		}							\
	} while (0)

	/* Check for known/supported extensions. */
	CHECK_S_EXT("sstc",	SV_SSTC);
	CHECK_S_EXT("svnapot",	SV_SVNAPOT);
	CHECK_S_EXT("svpbmt",	SV_SVPBMT);
	CHECK_S_EXT("svinval",	SV_SVINVAL);
	CHECK_S_EXT("sscofpmf",	SV_SSCOFPMF);

#undef CHECK_S_EXT

	/*
	 * Proceed to the next multi-letter extension or the end of the
	 * string.
	 */
	while (isa[idx] != '_' && idx < len) {
		idx++;
	}

	return (idx);
}

static __inline int
parse_ext_x(struct cpu_desc *desc __unused, char *isa, int idx, int len)
{
	/*
	 * Proceed to the next multi-letter extension or the end of the
	 * string.
	 */
	while (isa[idx] != '_' && idx < len) {
		idx++;
	}

	return (idx);
}

static __inline int
parse_ext_z(struct cpu_desc *desc __unused, char *isa, int idx, int len)
{
	/*
	 * Proceed to the next multi-letter extension or the end of the
	 * string.
	 *
	 * TODO: parse some of these.
	 */
	while (isa[idx] != '_' && idx < len) {
		idx++;
	}

	return (idx);
}

static __inline int
parse_ext_version(char *isa, int idx, u_int *majorp __unused,
    u_int *minorp __unused)
{
	/* Major version. */
	while (isdigit(isa[idx]))
		idx++;

	if (isa[idx] != 'p')
		return (idx);
	else
		idx++;

	/* Minor version. */
	while (isdigit(isa[idx]))
		idx++;

	return (idx);
}

/*
 * Parse the ISA string, building up the set of HWCAP bits as they are found.
 */
static int
parse_riscv_isa(struct cpu_desc *desc, char *isa, int len)
{
	int i;

	/* Check the string prefix. */
	if (strncmp(isa, ISA_PREFIX, ISA_PREFIX_LEN) != 0) {
		printf("%s: Unrecognized ISA string: %s\n", __func__, isa);
		return (-1);
	}

	i = ISA_PREFIX_LEN;
	while (i < len) {
		switch(isa[i]) {
		case 'a':
		case 'c':
#ifdef FPE
		case 'd':
		case 'f':
#endif
		case 'i':
		case 'm':
			desc->isa_extensions |= HWCAP_ISA_BIT(isa[i]);
			i++;
			break;
		case 'g':
			desc->isa_extensions |= HWCAP_ISA_G;
			i++;
			break;
		case 's':
			/*
			 * XXX: older versions of this string erroneously
			 * indicated supervisor and user mode support as
			 * single-letter extensions. Detect and skip both 's'
			 * and 'u'.
			 */
			if (isa[i - 1] != '_' && isa[i + 1] == 'u') {
				i += 2;
				continue;
			}

			/*
			 * Supervisor-level extension namespace.
			 */
			i = parse_ext_s(desc, isa, i, len);
			break;
		case 'x':
			/*
			 * Custom extension namespace. For now, we ignore
			 * these.
			 */
			i = parse_ext_x(desc, isa, i, len);
			break;
		case 'z':
			/*
			 * Multi-letter standard extension namespace.
			 */
			i = parse_ext_z(desc, isa, i, len);
			break;
		case '_':
			i++;
			continue;
		default:
			/* Unrecognized/unsupported. */
			i++;
			break;
		}

		i = parse_ext_version(isa, i, NULL, NULL);
	}

	return (0);
}

#ifdef FDT
static void
parse_mmu_fdt(struct cpu_desc *desc, phandle_t node)
{
	char mmu[16];

	desc->mmu_caps |= MMU_SV39;
	if (OF_getprop(node, "mmu-type", mmu, sizeof(mmu)) > 0) {
		if (strcmp(mmu, "riscv,sv48") == 0)
			desc->mmu_caps |= MMU_SV48;
		else if (strcmp(mmu, "riscv,sv57") == 0)
			desc->mmu_caps |= MMU_SV48 | MMU_SV57;
	}
}

static void
identify_cpu_features_fdt(u_int cpu, struct cpu_desc *desc)
{
	char isa[1024];
	phandle_t node;
	ssize_t len;
	pcell_t reg;
	u_int hart;

	node = OF_finddevice("/cpus");
	if (node == -1) {
		printf("%s: could not find /cpus node in FDT\n", __func__);
		return;
	}

	hart = pcpu_find(cpu)->pc_hart;

	/*
	 * Locate our current CPU's node in the device-tree, and parse its
	 * contents to detect supported CPU/ISA features and extensions.
	 */
	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		/* Skip any non-CPU nodes, such as cpu-map. */
		if (!ofw_bus_node_is_compatible(node, "riscv"))
			continue;

		/* Find this CPU */
		if (OF_getencprop(node, "reg", &reg, sizeof(reg)) <= 0 ||
		    reg != hart)
			continue;

		len = OF_getprop(node, "riscv,isa", isa, sizeof(isa));
		KASSERT(len <= sizeof(isa), ("ISA string truncated"));
		if (len == -1) {
			printf("%s: could not find 'riscv,isa' property "
			    "for CPU %d, hart %u\n", __func__, cpu, hart);
			return;
		}

		/*
		 * The string is specified to be lowercase, but let's be
		 * certain.
		 */
		for (int i = 0; i < len; i++)
			isa[i] = tolower(isa[i]);
		if (parse_riscv_isa(desc, isa, len) != 0)
			return;

		/* Check MMU features. */
		parse_mmu_fdt(desc, node);

		/* We are done. */
		break;
	}
	if (node <= 0) {
		printf("%s: could not find FDT node for CPU %u, hart %u\n",
		    __func__, cpu, hart);
	}
}
#endif

static void
identify_cpu_features(u_int cpu, struct cpu_desc *desc)
{
#ifdef FDT
	identify_cpu_features_fdt(cpu, desc);
#endif
}

/*
 * Update kernel/user global state based on the feature parsing results, stored
 * in desc.
 *
 * We keep only the subset of values common to all CPUs.
 */
static void
update_global_capabilities(u_int cpu, struct cpu_desc *desc)
{
#define UPDATE_CAP(t, v)				\
	do {						\
		if (cpu == 0) {				\
			(t) = (v);			\
		} else {				\
			(t) &= (v);			\
		}					\
	} while (0)

	/* Update the capabilities exposed to userspace via AT_HWCAP. */
	UPDATE_CAP(elf_hwcap, (u_long)desc->isa_extensions);

	/*
	 * MMU capabilities, e.g. Sv48.
	 */
	UPDATE_CAP(mmu_caps, desc->mmu_caps);

	/* Supervisor-mode extension support. */
	UPDATE_CAP(has_sstc, (desc->smode_extensions & SV_SSTC) != 0);
	UPDATE_CAP(has_sscofpmf, (desc->smode_extensions & SV_SSCOFPMF) != 0);

#undef UPDATE_CAP
}

static void
identify_cpu_ids(struct cpu_desc *desc)
{
	const struct marchid_entry *table = NULL;
	int i;

	desc->cpu_mvendor_name = "Unknown";
	desc->cpu_march_name = "Unknown";

	/*
	 * Search for a recognized vendor, and possibly obtain the secondary
	 * table for marchid lookup.
	 */
	for (i = 0; i < nitems(mvendor_ids); i++) {
		if (mvendorid == mvendor_ids[i].mvendor_id) {
			desc->cpu_mvendor_name = mvendor_ids[i].mvendor_name;
			table = mvendor_ids[i].marchid_table;
			break;
		}
	}

	if (marchid == MARCHID_UNIMPL) {
		desc->cpu_march_name = "Unspecified";
		return;
	}

	if (MARCHID_IS_OPENSOURCE(marchid)) {
		table = global_marchids;
	} else if (table == NULL)
		return;

	for (i = 0; table[i].march_name != NULL; i++) {
		if (marchid == table[i].march_id) {
			desc->cpu_march_name = table[i].march_name;
			break;
		}
	}
}

void
identify_cpu(u_int cpu)
{
	struct cpu_desc *desc = &cpu_desc[cpu];

	identify_cpu_ids(desc);
	identify_cpu_features(cpu, desc);

	update_global_capabilities(cpu, desc);
}

void
printcpuinfo(u_int cpu)
{
	struct cpu_desc *desc;
	u_int hart;

	desc = &cpu_desc[cpu];
	hart = pcpu_find(cpu)->pc_hart;

	/* XXX: check this here so we are guaranteed to have console output. */
	KASSERT(desc->isa_extensions != 0,
	    ("Empty extension set for CPU %u, did parsing fail?", cpu));

	/*
	 * Suppress the output of some fields in the common case of identical
	 * CPU features.
	 */
#define	SHOULD_PRINT(_field)	\
    (cpu == 0 || desc[0]._field != desc[-1]._field)

	/* Always print summary line. */
	printf("CPU %-3u: Vendor=%s Core=%s (Hart %u)\n", cpu,
	    desc->cpu_mvendor_name, desc->cpu_march_name, hart);

	/* These values are global. */
	if (cpu == 0)
		printf("  marchid=%#lx, mimpid=%#lx\n", marchid, mimpid);

	if (SHOULD_PRINT(mmu_caps)) {
		printf("  MMU: %#b\n", desc->mmu_caps,
		    "\020"
		    "\01Sv39"
		    "\02Sv48"
		    "\03Sv57");
	}

	if (SHOULD_PRINT(isa_extensions)) {
		printf("  ISA: %#b\n", desc->isa_extensions,
		    "\020"
		    "\01Atomic"
		    "\03Compressed"
		    "\04Double"
		    "\06Float"
		    "\15Mult/Div");
	}

	if (SHOULD_PRINT(smode_extensions)) {
		printf("  S-mode Extensions: %#b\n", desc->smode_extensions,
		    "\020"
		    "\01Sstc"
		    "\02Svnapot"
		    "\03Svpbmt"
		    "\04Svinval"
		    "\05Sscofpmf");
	}

#undef SHOULD_PRINT
}
