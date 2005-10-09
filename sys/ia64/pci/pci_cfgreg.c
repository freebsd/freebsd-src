/*-
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
#include <machine/pci_cfgreg.h>
#include <machine/sal.h>

#define SAL_PCI_ADDRESS(bus, slot, func, reg) \
	(((bus) << 16) | ((slot) << 11) | ((func) << 8) | (reg))

int
pci_cfgregopen(void)
{
	return 1;
}

u_int32_t
pci_cfgregread(int bus, int slot, int func, int reg, int bytes)
{
	struct ia64_sal_result res;

	res = ia64_sal_entry(SAL_PCI_CONFIG_READ,
			     SAL_PCI_ADDRESS(bus, slot, func, reg),
			     bytes, 0, 0, 0, 0, 0);
	if (res.sal_status < 0)
		return (~0);
	else
		return (res.sal_result[0]);
}

void
pci_cfgregwrite(int bus, int slot, int func, int reg, u_int32_t data, int bytes)
{
	struct ia64_sal_result res;

	res = ia64_sal_entry(SAL_PCI_CONFIG_WRITE,
			     SAL_PCI_ADDRESS(bus, slot, func, reg),
			     bytes, data, 0, 0, 0, 0);
}
