/*-
 * Copyright (c) 2010 Marcel Moolenaar
 * Copyright (c) 2001 Doug Rabson
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <machine/cpufunc.h>
#include <machine/pci_cfgreg.h>
#include <machine/sal.h>

static u_long
pci_sal_address(int dom, int bus, int slot, int func, int reg)
{
	u_long addr;

	addr = ~0ul;
	if (dom >= 0 && dom <= 255 && bus >= 0 && bus <= 255 &&
	    slot >= 0 && slot <= 31 && func >= 0 && func <= 7 &&
	    reg >= 0 && reg <= 255) {
		addr = ((u_long)dom << 24) | ((u_long)bus << 16) |
		    ((u_long)slot << 11) | ((u_long)func << 8) | (u_long)reg;
	}
	return (addr);
}

static int
pci_valid_access(int reg, int len)
{
	int ok;

	ok = ((len == 1 || len == 2 || len == 4) && (reg & (len - 1)) == 0)
	    ? 1 : 0;
	return (ok);
}

int
pci_cfgregopen(void)
{
	return (1);
}

uint32_t
pci_cfgregread(int bus, int slot, int func, int reg, int len)
{
	struct ia64_sal_result res;
	register_t is;
	u_long addr;

	addr = pci_sal_address(0, bus, slot, func, reg);
	if (addr == ~0ul)
		return (~0);

	if (!pci_valid_access(reg, len))
		return (~0);

	is = intr_disable();
	res = ia64_sal_entry(SAL_PCI_CONFIG_READ, addr, len, 0, 0, 0, 0, 0);
	intr_restore(is);

	return ((res.sal_status < 0) ? ~0 : res.sal_result[0]);
}

void
pci_cfgregwrite(int bus, int slot, int func, int reg, uint32_t data, int len)
{
	struct ia64_sal_result res;
	register_t is;
	u_long addr;

	addr = pci_sal_address(0, bus, slot, func, reg);
	if (addr == ~0ul)
		return;

	if (!pci_valid_access(reg, len))
		return;

	is = intr_disable();
	res = ia64_sal_entry(SAL_PCI_CONFIG_WRITE, addr, len, data, 0, 0, 0, 0);
	intr_restore(is);
}
