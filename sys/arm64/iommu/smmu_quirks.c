/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ruslan Bukin <br@bsdpad.com>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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
#include <sys/bitstring.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/tree.h>
#include <sys/taskqueue.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/iommu/iommu.h>

#include "smmuvar.h"

struct smmu_quirk_entry {
	uint16_t vendor;
	uint16_t device;
	const char *name;
	uint8_t event_id;
	uintptr_t input_address;
};

/* List of events that are known and will be silenced. */
static const struct smmu_quirk_entry smmu_quirk_table[] = {
	{ 0x10ec, 0x8168, "RealTek 8168/8111", 0x10 /* F_TRANSLATION */, 0x0 },
	{ 0, 0, NULL, 0, 0 },
};

bool
smmu_quirks_check(device_t dev, u_int sid, uint8_t event_id,
    uintptr_t input_addr)
{
	const struct smmu_quirk_entry *q;
	struct smmu_ctx *ctx;
	int i;

	ctx = smmu_ctx_lookup_by_sid(dev, sid);
	if (!ctx)
		return (false);

	for (i = 0; smmu_quirk_table[i].vendor != 0; i++) {
		q = &smmu_quirk_table[i];
		if (ctx->vendor == q->vendor &&
		    ctx->device == q->device &&
		    input_addr == q->input_address &&
		    event_id == q->event_id)
			return (true);
	}

	return (false);
}
