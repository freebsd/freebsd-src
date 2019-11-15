/*-
 * Copyright (c) 2015-2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/trap.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

char machine[] = "riscv";

SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0,
    "Machine class");

/* Hardware implementation info. These values may be empty. */
register_t mvendorid;	/* The CPU's JEDEC vendor ID */
register_t marchid;	/* The architecture ID */
register_t mimpid;	/* The implementation ID */

struct cpu_desc {
	u_int		cpu_impl;
	u_int		cpu_part_num;
	const char	*cpu_impl_name;
	const char	*cpu_part_name;
};

struct cpu_desc cpu_desc[MAXCPU];

struct cpu_parts {
	u_int		part_id;
	const char	*part_name;
};
#define	CPU_PART_NONE	{ -1, "Unknown Processor" }

struct cpu_implementers {
	u_int			impl_id;
	const char		*impl_name;
};
#define	CPU_IMPLEMENTER_NONE	{ 0, "Unknown Implementer" }

/*
 * CPU base
 */
static const struct cpu_parts cpu_parts_std[] = {
	{ CPU_PART_RV32,	"RV32" },
	{ CPU_PART_RV64,	"RV64" },
	{ CPU_PART_RV128,	"RV128" },
	CPU_PART_NONE,
};

/*
 * Implementers table.
 */
const struct cpu_implementers cpu_implementers[] = {
	{ CPU_IMPL_UCB_ROCKET,	"UC Berkeley Rocket" },
	CPU_IMPLEMENTER_NONE,
};

#ifdef FDT
/*
 * The ISA string is made up of a small prefix (e.g. rv64) and up to 26 letters
 * indicating the presence of the 26 possible standard extensions. Therefore 32
 * characters will be sufficient.
 */
#define	ISA_NAME_MAXLEN		32
#define	ISA_PREFIX		("rv" __XSTRING(__riscv_xlen))
#define	ISA_PREFIX_LEN		(sizeof(ISA_PREFIX) - 1)

static void
fill_elf_hwcap(void *dummy __unused)
{
	u_long caps[256] = {0};
	char isa[ISA_NAME_MAXLEN];
	u_long hwcap;
	phandle_t node;
	ssize_t len;
	int i;

	caps['i'] = caps['I'] = HWCAP_ISA_I;
	caps['m'] = caps['M'] = HWCAP_ISA_M;
	caps['a'] = caps['A'] = HWCAP_ISA_A;
#ifdef FPE
	caps['f'] = caps['F'] = HWCAP_ISA_F;
	caps['d'] = caps['D'] = HWCAP_ISA_D;
#endif
	caps['c'] = caps['C'] = HWCAP_ISA_C;

	node = OF_finddevice("/cpus");
	if (node == -1) {
		if (bootverbose)
			printf("fill_elf_hwcap: Can't find cpus node\n");
		return;
	}

	/*
	 * Iterate through the CPUs and examine their ISA string. While we
	 * could assign elf_hwcap to be whatever the boot CPU supports, to
	 * handle the (unusual) case of running a system with hetergeneous
	 * ISAs, keep only the extension bits that are common to all harts.
	 */
	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		/* Skip any non-CPU nodes, such as cpu-map. */
		if (!ofw_bus_node_is_compatible(node, "riscv"))
			continue;

		len = OF_getprop(node, "riscv,isa", isa, sizeof(isa));
		KASSERT(len <= ISA_NAME_MAXLEN, ("ISA string truncated"));
		if (len == -1) {
			if (bootverbose)
				printf("fill_elf_hwcap: "
				    "Can't find riscv,isa property\n");
			return;
		} else if (strncmp(isa, ISA_PREFIX, ISA_PREFIX_LEN) != 0) {
			if (bootverbose)
				printf("fill_elf_hwcap: "
				    "Unsupported ISA string: %s\n", isa);
			return;
		}

		hwcap = 0;
		for (i = ISA_PREFIX_LEN; i < len; i++)
			hwcap |= caps[(unsigned char)isa[i]];

		if (elf_hwcap != 0)
			elf_hwcap &= hwcap;
		else
			elf_hwcap = hwcap;

	}
}

SYSINIT(identcpu, SI_SUB_CPU, SI_ORDER_ANY, fill_elf_hwcap, NULL);
#endif

void
identify_cpu(void)
{
	const struct cpu_parts *cpu_partsp;
	uint32_t part_id;
	uint32_t impl_id;
	uint64_t mimpid;
	uint64_t misa;
	u_int cpu;
	size_t i;

	cpu_partsp = NULL;

	/* TODO: can we get mimpid and misa somewhere ? */
	mimpid = 0;
	misa = 0;

	cpu = PCPU_GET(cpuid);

	impl_id	= CPU_IMPL(mimpid);
	for (i = 0; i < nitems(cpu_implementers); i++) {
		if (impl_id == cpu_implementers[i].impl_id ||
		    cpu_implementers[i].impl_id == 0) {
			cpu_desc[cpu].cpu_impl = impl_id;
			cpu_desc[cpu].cpu_impl_name = cpu_implementers[i].impl_name;
			cpu_partsp = cpu_parts_std;
			break;
		}
	}

	part_id = CPU_PART(misa);
	for (i = 0; &cpu_partsp[i] != NULL; i++) {
		if (part_id == cpu_partsp[i].part_id ||
		    cpu_partsp[i].part_id == -1) {
			cpu_desc[cpu].cpu_part_num = part_id;
			cpu_desc[cpu].cpu_part_name = cpu_partsp[i].part_name;
			break;
		}
	}

	/* Print details for boot CPU or if we want verbose output */
	if (cpu == 0 || bootverbose) {
		printf("CPU(%d): %s %s\n", cpu,
		    cpu_desc[cpu].cpu_impl_name,
		    cpu_desc[cpu].cpu_part_name);
	}
}
