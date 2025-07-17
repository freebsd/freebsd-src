/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <err.h>
#include <stdlib.h>

#include "config.h"
#include "pci_emul.h"

static int
pci_hostbridge_init(struct pci_devinst *pi, nvlist_t *nvl)
{
	const char *value;
	u_int vendor, device;

	vendor = 0x1275;	/* NetApp */
	device = 0x1275;	/* NetApp */

	value = get_config_value_node(nvl, "vendor");
	if (value != NULL)
		vendor = strtol(value, NULL, 0);
	else
		vendor = pci_config_read_reg(NULL, nvl, PCIR_VENDOR, 2, vendor);
	value = get_config_value_node(nvl, "devid");
	if (value != NULL)
		device = strtol(value, NULL, 0);
	else
		device = pci_config_read_reg(NULL, nvl, PCIR_DEVICE, 2, device);

	/* config space */
	pci_set_cfgdata16(pi, PCIR_VENDOR, vendor);
	pci_set_cfgdata16(pi, PCIR_DEVICE, device);
	pci_set_cfgdata8(pi, PCIR_HDRTYPE, PCIM_HDRTYPE_NORMAL);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_BRIDGE);
	pci_set_cfgdata8(pi, PCIR_SUBCLASS, PCIS_BRIDGE_HOST);

	pci_emul_add_pciecap(pi, PCIEM_TYPE_ROOT_PORT);

	return (0);
}

static int
pci_amd_hostbridge_legacy_config(nvlist_t *nvl, const char *opts __unused)
{
	nvlist_t *pci_regs;

	pci_regs = create_relative_config_node(nvl, "pcireg");
	if (pci_regs == NULL) {
		warnx("amd_hostbridge: failed to create pciregs node");
		return (-1);
	}

	set_config_value_node(pci_regs, "vendor", "0x1022"); /* AMD */
	set_config_value_node(pci_regs, "device", "0x7432"); /* made up */

	return (0);
}

#ifdef BHYVE_SNAPSHOT
static int
pci_de_snapshot(struct vm_snapshot_meta *meta __unused)
{
	return (0);
}
#endif

static const struct pci_devemu pci_de_amd_hostbridge = {
	.pe_emu = "amd_hostbridge",
	.pe_legacy_config = pci_amd_hostbridge_legacy_config,
	.pe_alias = "hostbridge",
};
PCI_EMUL_SET(pci_de_amd_hostbridge);

static const struct pci_devemu pci_de_hostbridge = {
	.pe_emu = "hostbridge",
	.pe_init = pci_hostbridge_init,
#ifdef BHYVE_SNAPSHOT
	.pe_snapshot =	pci_de_snapshot,
#endif
};
PCI_EMUL_SET(pci_de_hostbridge);
