/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Conrad Meyer <cem@FreeBSD.org>.  All rights reserved.
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
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/uuid.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <unistd.h>

#include <machine/vmm.h>
#include <vmmapi.h>

#include "acpi.h"
#include "bootrom.h"
#include "vmgenc.h"

static uint64_t	vmgen_gpa;

void
vmgenc_init(struct vmctx *ctx)
{
	char *region;
	int error;

	error = bootrom_alloc(ctx, PAGE_SIZE, PROT_READ, 0, &region,
	    &vmgen_gpa);
	if (error != 0)
		errx(4, "%s: bootrom_alloc", __func__);

	/*
	 * It is basically harmless to always generate a random ID when
	 * starting a VM.
	 */
	error = getentropy(region, sizeof(struct uuid));
	if (error == -1)
		err(4, "%s: getentropy", __func__);

	/* XXX When we have suspend/resume/rollback. */
#if 0
	acpi_raise_gpe(ctx, GPE_VMGENC);
#endif
}

void
vmgenc_write_dsdt(void)
{
	dsdt_line("");
	dsdt_indent(1);
	dsdt_line("Scope (_SB)");
	dsdt_line("{");

	dsdt_line("  Device (GENC)");
	dsdt_line("  {");

	dsdt_indent(2);
	dsdt_line("Name (_CID, \"VM_Gen_Counter\")");
	dsdt_line("Method (_HID, 0, NotSerialized)");
	dsdt_line("{");
	dsdt_line("  Return (\"Bhyve_V_Gen_Counter_V1\")");
	dsdt_line("}");
	dsdt_line("Name (_UID, 0)");
	dsdt_line("Name (_DDN, \"VM_Gen_Counter\")");
	dsdt_line("Name (ADDR, Package (0x02)");
	dsdt_line("{");
	dsdt_line("  0x%08x,", (uint32_t)vmgen_gpa);
	dsdt_line("  0x%08x", (uint32_t)(vmgen_gpa >> 32));
	dsdt_line("})");

	dsdt_unindent(2);
	dsdt_line("  }");	/* Device (GENC) */

	dsdt_line("}");		/* Scope (_SB) */
	dsdt_line("");

	dsdt_line("Scope (_GPE)");
	dsdt_line("{");
	dsdt_line("  Method (_E%02x, 0, NotSerialized)", GPE_VMGENC);
	dsdt_line("  {");
	dsdt_line("    Notify (\\_SB.GENC, 0x80)");
	dsdt_line("  }");
	dsdt_line("}");
	dsdt_unindent(1);
}
