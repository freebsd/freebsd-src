/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Jessica Clarke <jrtc27@FreeBSD.org>
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

#include <vmmapi.h>

#include "pci_emul.h"
#include "pci_irq.h"

static int gic_irqs[4];

void
pci_irq_init(int intrs[static 4])
{
	int i;

	for (i = 0; i < 4; ++i)
		gic_irqs[i] = intrs[i];
}

void
pci_irq_assert(struct pci_devinst *pi)
{
	vm_assert_irq(pi->pi_vmctx, pi->pi_lintr.irq.gic_irq);
}

void
pci_irq_deassert(struct pci_devinst *pi)
{
	vm_deassert_irq(pi->pi_vmctx, pi->pi_lintr.irq.gic_irq);
}

void
pci_irq_route(struct pci_devinst *pi, struct pci_irq *irq)
{
	/*
	 * Assign swizzled IRQ for this INTx if one is not yet assigned. Must
	 * match fdt_add_pcie().
	 */
	if (irq->gic_irq == 0)
		irq->gic_irq =
		    gic_irqs[(pi->pi_slot + pi->pi_lintr.pin - 1) % 4];
}
