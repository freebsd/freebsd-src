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
#include <sys/param.h>
#include <sys/linker_set.h>
#include <sys/mman.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdbool.h>
#include <sysexits.h>

#include <machine/vmm.h>
#include <machine/vmm_snapshot.h>
#include <vmmapi.h>

#include "acpi.h"
#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "inout.h"
#include "ioapic.h"
#include "mem.h"
#include "pci_emul.h"
#include "pci_irq.h"
#include "pci_lpc.h"
#include "pci_passthru.h"
#include "qemu_fwcfg.h"

#define CONF1_ADDR_PORT	   0x0cf8
#define CONF1_DATA_PORT	   0x0cfc

#define CONF1_ENABLE	   0x80000000ul

#define	MAXBUSES	(PCI_BUSMAX + 1)
#define MAXSLOTS	(PCI_SLOTMAX + 1)
#define	MAXFUNCS	(PCI_FUNCMAX + 1)

#define GB		(1024 * 1024 * 1024UL)

struct funcinfo {
	nvlist_t *fi_config;
	struct pci_devemu *fi_pde;
	struct pci_devinst *fi_devi;
};

struct intxinfo {
	int	ii_count;
	int	ii_pirq_pin;
	int	ii_ioapic_irq;
};

struct slotinfo {
	struct intxinfo si_intpins[4];
	struct funcinfo si_funcs[MAXFUNCS];
};

struct businfo {
	uint16_t iobase, iolimit;		/* I/O window */
	uint32_t membase32, memlimit32;		/* mmio window below 4GB */
	uint64_t membase64, memlimit64;		/* mmio window above 4GB */
	struct slotinfo slotinfo[MAXSLOTS];
};

static struct businfo *pci_businfo[MAXBUSES];

SET_DECLARE(pci_devemu_set, struct pci_devemu);

static uint64_t pci_emul_iobase;
static uint8_t *pci_emul_rombase;
static uint64_t pci_emul_romoffset;
static uint8_t *pci_emul_romlim;
static uint64_t pci_emul_membase32;
static uint64_t pci_emul_membase64;
static uint64_t pci_emul_memlim64;

struct pci_bar_allocation {
	TAILQ_ENTRY(pci_bar_allocation) chain;
	struct pci_devinst *pdi;
	int idx;
	enum pcibar_type type;
	uint64_t size;
};

static TAILQ_HEAD(pci_bar_list, pci_bar_allocation) pci_bars =
    TAILQ_HEAD_INITIALIZER(pci_bars);

struct boot_device {
	TAILQ_ENTRY(boot_device) boot_device_chain;
	struct pci_devinst *pdi;
	int bootindex;
};
static TAILQ_HEAD(boot_list, boot_device) boot_devices = TAILQ_HEAD_INITIALIZER(
    boot_devices);

#define	PCI_EMUL_IOBASE		0x2000
#define	PCI_EMUL_IOLIMIT	0x10000

#define PCI_EMUL_ROMSIZE 0x10000000

#define	PCI_EMUL_ECFG_BASE	0xE0000000		    /* 3.5GB */
#define	PCI_EMUL_ECFG_SIZE	(MAXBUSES * 1024 * 1024)    /* 1MB per bus */
SYSRES_MEM(PCI_EMUL_ECFG_BASE, PCI_EMUL_ECFG_SIZE);

/*
 * OVMF always uses 0xC0000000 as base address for 32 bit PCI MMIO. Don't
 * change this address without changing it in OVMF.
 */
#define PCI_EMUL_MEMBASE32 0xC0000000
#define	PCI_EMUL_MEMLIMIT32	PCI_EMUL_ECFG_BASE
#define PCI_EMUL_MEMSIZE64	(32*GB)

static struct pci_devemu *pci_emul_finddev(const char *name);
static void pci_lintr_route(struct pci_devinst *pi);
static void pci_lintr_update(struct pci_devinst *pi);
static void pci_cfgrw(int in, int bus, int slot, int func, int coff,
    int bytes, uint32_t *val);

static __inline void
CFGWRITE(struct pci_devinst *pi, int coff, uint32_t val, int bytes)
{

	if (bytes == 1)
		pci_set_cfgdata8(pi, coff, val);
	else if (bytes == 2)
		pci_set_cfgdata16(pi, coff, val);
	else
		pci_set_cfgdata32(pi, coff, val);
}

static __inline uint32_t
CFGREAD(struct pci_devinst *pi, int coff, int bytes)
{

	if (bytes == 1)
		return (pci_get_cfgdata8(pi, coff));
	else if (bytes == 2)
		return (pci_get_cfgdata16(pi, coff));
	else
		return (pci_get_cfgdata32(pi, coff));
}

static int
is_pcir_bar(int coff)
{
	return (coff >= PCIR_BAR(0) && coff < PCIR_BAR(PCI_BARMAX + 1));
}

static int
is_pcir_bios(int coff)
{
	return (coff >= PCIR_BIOS && coff < PCIR_BIOS + 4);
}

/*
 * I/O access
 */

/*
 * Slot options are in the form:
 *
 *  <bus>:<slot>:<func>,<emul>[,<config>]
 *  <slot>[:<func>],<emul>[,<config>]
 *
 *  slot is 0..31
 *  func is 0..7
 *  emul is a string describing the type of PCI device e.g. virtio-net
 *  config is an optional string, depending on the device, that can be
 *  used for configuration.
 *   Examples are:
 *     1,virtio-net,tap0
 *     3:0,dummy
 */
static void
pci_parse_slot_usage(char *aopt)
{

	EPRINTLN("Invalid PCI slot info field \"%s\"", aopt);
}

/*
 * Helper function to parse a list of comma-separated options where
 * each option is formatted as "name[=value]".  If no value is
 * provided, the option is treated as a boolean and is given a value
 * of true.
 */
int
pci_parse_legacy_config(nvlist_t *nvl, const char *opt)
{
	char *config, *name, *tofree, *value;

	if (opt == NULL)
		return (0);

	config = tofree = strdup(opt);
	while ((name = strsep(&config, ",")) != NULL) {
		value = strchr(name, '=');
		if (value != NULL) {
			*value = '\0';
			value++;
			set_config_value_node(nvl, name, value);
		} else
			set_config_bool_node(nvl, name, true);
	}
	free(tofree);
	return (0);
}

/*
 * PCI device configuration is stored in MIBs that encode the device's
 * location:
 *
 * pci.<bus>.<slot>.<func>
 *
 * Where "bus", "slot", and "func" are all decimal values without
 * leading zeroes.  Each valid device must have a "device" node which
 * identifies the driver model of the device.
 *
 * Device backends can provide a parser for the "config" string.  If
 * a custom parser is not provided, pci_parse_legacy_config() is used
 * to parse the string.
 */
int
pci_parse_slot(char *opt)
{
	char node_name[sizeof("pci.XXX.XX.X")];
	struct pci_devemu *pde;
	char *emul, *config, *str, *cp;
	int error, bnum, snum, fnum;
	nvlist_t *nvl;

	error = -1;
	str = strdup(opt);

	emul = config = NULL;
	if ((cp = strchr(str, ',')) != NULL) {
		*cp = '\0';
		emul = cp + 1;
		if ((cp = strchr(emul, ',')) != NULL) {
			*cp = '\0';
			config = cp + 1;
		}
	} else {
		pci_parse_slot_usage(opt);
		goto done;
	}

	/* <bus>:<slot>:<func> */
	if (sscanf(str, "%d:%d:%d", &bnum, &snum, &fnum) != 3) {
		bnum = 0;
		/* <slot>:<func> */
		if (sscanf(str, "%d:%d", &snum, &fnum) != 2) {
			fnum = 0;
			/* <slot> */
			if (sscanf(str, "%d", &snum) != 1) {
				snum = -1;
			}
		}
	}

	if (bnum < 0 || bnum >= MAXBUSES || snum < 0 || snum >= MAXSLOTS ||
	    fnum < 0 || fnum >= MAXFUNCS) {
		pci_parse_slot_usage(opt);
		goto done;
	}

	pde = pci_emul_finddev(emul);
	if (pde == NULL) {
		EPRINTLN("pci slot %d:%d:%d: unknown device \"%s\"", bnum, snum,
		    fnum, emul);
		goto done;
	}

	snprintf(node_name, sizeof(node_name), "pci.%d.%d.%d", bnum, snum,
	    fnum);
	nvl = find_config_node(node_name);
	if (nvl != NULL) {
		EPRINTLN("pci slot %d:%d:%d already occupied!", bnum, snum,
		    fnum);
		goto done;
	}
	nvl = create_config_node(node_name);
	if (pde->pe_alias != NULL)
		set_config_value_node(nvl, "device", pde->pe_alias);
	else
		set_config_value_node(nvl, "device", pde->pe_emu);

	if (pde->pe_legacy_config != NULL)
		error = pde->pe_legacy_config(nvl, config);
	else
		error = pci_parse_legacy_config(nvl, config);
done:
	free(str);
	return (error);
}

void
pci_print_supported_devices(void)
{
	struct pci_devemu **pdpp, *pdp;

	SET_FOREACH(pdpp, pci_devemu_set) {
		pdp = *pdpp;
		printf("%s\n", pdp->pe_emu);
	}
}

uint32_t
pci_config_read_reg(const struct pcisel *const host_sel, nvlist_t *nvl,
    const uint32_t reg, const uint8_t size, const uint32_t def)
{
	const char *config;
	const nvlist_t *pci_regs;

	assert(size == 1 || size == 2 || size == 4);

	pci_regs = find_relative_config_node(nvl, "pcireg");
	if (pci_regs == NULL) {
		return def;
	}

	switch (reg) {
	case PCIR_DEVICE:
		config = get_config_value_node(pci_regs, "device");
		break;
	case PCIR_VENDOR:
		config = get_config_value_node(pci_regs, "vendor");
		break;
	case PCIR_REVID:
		config = get_config_value_node(pci_regs, "revid");
		break;
	case PCIR_SUBVEND_0:
		config = get_config_value_node(pci_regs, "subvendor");
		break;
	case PCIR_SUBDEV_0:
		config = get_config_value_node(pci_regs, "subdevice");
		break;
	default:
		return (-1);
	}

	if (config == NULL) {
		return def;
	} else if (host_sel != NULL && strcmp(config, "host") == 0) {
		return read_config(host_sel, reg, size);
	} else {
		return strtol(config, NULL, 16);
	}
}

static int
pci_valid_pba_offset(struct pci_devinst *pi, uint64_t offset)
{

	if (offset < pi->pi_msix.pba_offset)
		return (0);

	if (offset >= pi->pi_msix.pba_offset + pi->pi_msix.pba_size) {
		return (0);
	}

	return (1);
}

int
pci_emul_msix_twrite(struct pci_devinst *pi, uint64_t offset, int size,
		     uint64_t value)
{
	int msix_entry_offset;
	int tab_index;
	char *dest;

	/* support only 4 or 8 byte writes */
	if (size != 4 && size != 8)
		return (-1);

	/*
	 * Return if table index is beyond what device supports
	 */
	tab_index = offset / MSIX_TABLE_ENTRY_SIZE;
	if (tab_index >= pi->pi_msix.table_count)
		return (-1);

	msix_entry_offset = offset % MSIX_TABLE_ENTRY_SIZE;

	/* support only aligned writes */
	if ((msix_entry_offset % size) != 0)
		return (-1);

	dest = (char *)(pi->pi_msix.table + tab_index);
	dest += msix_entry_offset;

	if (size == 4)
		*((uint32_t *)dest) = value;
	else
		*((uint64_t *)dest) = value;

	return (0);
}

uint64_t
pci_emul_msix_tread(struct pci_devinst *pi, uint64_t offset, int size)
{
	char *dest;
	int msix_entry_offset;
	int tab_index;
	uint64_t retval = ~0;

	/*
	 * The PCI standard only allows 4 and 8 byte accesses to the MSI-X
	 * table but we also allow 1 byte access to accommodate reads from
	 * ddb.
	 */
	if (size != 1 && size != 4 && size != 8)
		return (retval);

	msix_entry_offset = offset % MSIX_TABLE_ENTRY_SIZE;

	/* support only aligned reads */
	if ((msix_entry_offset % size) != 0) {
		return (retval);
	}

	tab_index = offset / MSIX_TABLE_ENTRY_SIZE;

	if (tab_index < pi->pi_msix.table_count) {
		/* valid MSI-X Table access */
		dest = (char *)(pi->pi_msix.table + tab_index);
		dest += msix_entry_offset;

		if (size == 1)
			retval = *((uint8_t *)dest);
		else if (size == 4)
			retval = *((uint32_t *)dest);
		else
			retval = *((uint64_t *)dest);
	} else if (pci_valid_pba_offset(pi, offset)) {
		/* return 0 for PBA access */
		retval = 0;
	}

	return (retval);
}

int
pci_msix_table_bar(struct pci_devinst *pi)
{

	if (pi->pi_msix.table != NULL)
		return (pi->pi_msix.table_bar);
	else
		return (-1);
}

int
pci_msix_pba_bar(struct pci_devinst *pi)
{

	if (pi->pi_msix.table != NULL)
		return (pi->pi_msix.pba_bar);
	else
		return (-1);
}

static int
pci_emul_io_handler(struct vmctx *ctx __unused, int in, int port,
    int bytes, uint32_t *eax, void *arg)
{
	struct pci_devinst *pdi = arg;
	struct pci_devemu *pe = pdi->pi_d;
	uint64_t offset;
	int i;

	assert(port >= 0);

	for (i = 0; i <= PCI_BARMAX; i++) {
		if (pdi->pi_bar[i].type == PCIBAR_IO &&
		    (uint64_t)port >= pdi->pi_bar[i].addr &&
		    (uint64_t)port + bytes <=
		    pdi->pi_bar[i].addr + pdi->pi_bar[i].size) {
			offset = port - pdi->pi_bar[i].addr;
			if (in)
				*eax = (*pe->pe_barread)(pdi, i,
							 offset, bytes);
			else
				(*pe->pe_barwrite)(pdi, i, offset,
						   bytes, *eax);
			return (0);
		}
	}
	return (-1);
}

static int
pci_emul_mem_handler(struct vcpu *vcpu __unused, int dir,
    uint64_t addr, int size, uint64_t *val, void *arg1, long arg2)
{
	struct pci_devinst *pdi = arg1;
	struct pci_devemu *pe = pdi->pi_d;
	uint64_t offset;
	int bidx = (int) arg2;

	assert(bidx <= PCI_BARMAX);
	assert(pdi->pi_bar[bidx].type == PCIBAR_MEM32 ||
	       pdi->pi_bar[bidx].type == PCIBAR_MEM64);
	assert(addr >= pdi->pi_bar[bidx].addr &&
	       addr + size <= pdi->pi_bar[bidx].addr + pdi->pi_bar[bidx].size);

	offset = addr - pdi->pi_bar[bidx].addr;

	if (dir == MEM_F_WRITE) {
		if (size == 8) {
			(*pe->pe_barwrite)(pdi, bidx, offset,
					   4, *val & 0xffffffff);
			(*pe->pe_barwrite)(pdi, bidx, offset + 4,
					   4, *val >> 32);
		} else {
			(*pe->pe_barwrite)(pdi, bidx, offset,
					   size, *val);
		}
	} else {
		if (size == 8) {
			*val = (*pe->pe_barread)(pdi, bidx,
						 offset, 4);
			*val |= (*pe->pe_barread)(pdi, bidx,
						  offset + 4, 4) << 32;
		} else {
			*val = (*pe->pe_barread)(pdi, bidx,
						 offset, size);
		}
	}

	return (0);
}


static int
pci_emul_alloc_resource(uint64_t *baseptr, uint64_t limit, uint64_t size,
			uint64_t *addr)
{
	uint64_t base;

	assert((size & (size - 1)) == 0);	/* must be a power of 2 */

	base = roundup2(*baseptr, size);

	if (base + size <= limit) {
		*addr = base;
		*baseptr = base + size;
		return (0);
	} else
		return (-1);
}

/*
 * Register (or unregister) the MMIO or I/O region associated with the BAR
 * register 'idx' of an emulated pci device.
 */
static void
modify_bar_registration(struct pci_devinst *pi, int idx, int registration)
{
	struct pci_devemu *pe;
	int error;
	struct inout_port iop;
	struct mem_range mr;

	pe = pi->pi_d;
	switch (pi->pi_bar[idx].type) {
	case PCIBAR_IO:
		bzero(&iop, sizeof(struct inout_port));
		iop.name = pi->pi_name;
		iop.port = pi->pi_bar[idx].addr;
		iop.size = pi->pi_bar[idx].size;
		if (registration) {
			iop.flags = IOPORT_F_INOUT;
			iop.handler = pci_emul_io_handler;
			iop.arg = pi;
			error = register_inout(&iop);
		} else
			error = unregister_inout(&iop);
		break;
	case PCIBAR_MEM32:
	case PCIBAR_MEM64:
		bzero(&mr, sizeof(struct mem_range));
		mr.name = pi->pi_name;
		mr.base = pi->pi_bar[idx].addr;
		mr.size = pi->pi_bar[idx].size;
		if (registration) {
			mr.flags = MEM_F_RW;
			mr.handler = pci_emul_mem_handler;
			mr.arg1 = pi;
			mr.arg2 = idx;
			error = register_mem(&mr);
		} else
			error = unregister_mem(&mr);
		break;
	case PCIBAR_ROM:
		error = 0;
		break;
	default:
		error = EINVAL;
		break;
	}
	assert(error == 0);

	if (pe->pe_baraddr != NULL)
		(*pe->pe_baraddr)(pi, idx, registration, pi->pi_bar[idx].addr);
}

static void
unregister_bar(struct pci_devinst *pi, int idx)
{

	modify_bar_registration(pi, idx, 0);
}

static void
register_bar(struct pci_devinst *pi, int idx)
{

	modify_bar_registration(pi, idx, 1);
}

/* Is the ROM enabled for the emulated pci device? */
static int
romen(struct pci_devinst *pi)
{
	return (pi->pi_bar[PCI_ROM_IDX].lobits & PCIM_BIOS_ENABLE) ==
	    PCIM_BIOS_ENABLE;
}

/* Are we decoding i/o port accesses for the emulated pci device? */
static int
porten(struct pci_devinst *pi)
{
	uint16_t cmd;

	cmd = pci_get_cfgdata16(pi, PCIR_COMMAND);

	return (cmd & PCIM_CMD_PORTEN);
}

/* Are we decoding memory accesses for the emulated pci device? */
static int
memen(struct pci_devinst *pi)
{
	uint16_t cmd;

	cmd = pci_get_cfgdata16(pi, PCIR_COMMAND);

	return (cmd & PCIM_CMD_MEMEN);
}

/*
 * Update the MMIO or I/O address that is decoded by the BAR register.
 *
 * If the pci device has enabled the address space decoding then intercept
 * the address range decoded by the BAR register.
 */
static void
update_bar_address(struct pci_devinst *pi, uint64_t addr, int idx, int type)
{
	int decode;

	if (pi->pi_bar[idx].type == PCIBAR_IO)
		decode = porten(pi);
	else
		decode = memen(pi);

	if (decode)
		unregister_bar(pi, idx);

	switch (type) {
	case PCIBAR_IO:
	case PCIBAR_MEM32:
		pi->pi_bar[idx].addr = addr;
		break;
	case PCIBAR_MEM64:
		pi->pi_bar[idx].addr &= ~0xffffffffUL;
		pi->pi_bar[idx].addr |= addr;
		break;
	case PCIBAR_MEMHI64:
		pi->pi_bar[idx].addr &= 0xffffffff;
		pi->pi_bar[idx].addr |= addr;
		break;
	default:
		assert(0);
	}

	if (decode)
		register_bar(pi, idx);
}

int
pci_emul_alloc_bar(struct pci_devinst *pdi, int idx, enum pcibar_type type,
    uint64_t size)
{
	assert((type == PCIBAR_ROM) || (idx >= 0 && idx <= PCI_BARMAX));
	assert((type != PCIBAR_ROM) || (idx == PCI_ROM_IDX));

	if ((size & (size - 1)) != 0)
		size = 1UL << flsl(size);	/* round up to a power of 2 */

	/* Enforce minimum BAR sizes required by the PCI standard */
	if (type == PCIBAR_IO) {
		if (size < 4)
			size = 4;
	} else if (type == PCIBAR_ROM) {
		if (size < ~PCIM_BIOS_ADDR_MASK + 1)
			size = ~PCIM_BIOS_ADDR_MASK + 1;
	} else {
		if (size < 16)
			size = 16;
	}

	/*
	 * To reduce fragmentation of the MMIO space, we allocate the BARs by
	 * size. Therefore, don't allocate the BAR yet. We create a list of all
	 * BAR allocation which is sorted by BAR size. When all PCI devices are
	 * initialized, we will assign an address to the BARs.
	 */

	/* create a new list entry */
	struct pci_bar_allocation *const new_bar = malloc(sizeof(*new_bar));
	memset(new_bar, 0, sizeof(*new_bar));
	new_bar->pdi = pdi;
	new_bar->idx = idx;
	new_bar->type = type;
	new_bar->size = size;

	/*
	 * Search for a BAR which size is lower than the size of our newly
	 * allocated BAR.
	 */
	struct pci_bar_allocation *bar = NULL;
	TAILQ_FOREACH(bar, &pci_bars, chain) {
		if (bar->size < size) {
			break;
		}
	}

	if (bar == NULL) {
		/*
		 * Either the list is empty or new BAR is the smallest BAR of
		 * the list. Append it to the end of our list.
		 */
		TAILQ_INSERT_TAIL(&pci_bars, new_bar, chain);
	} else {
		/*
		 * The found BAR is smaller than our new BAR. For that reason,
		 * insert our new BAR before the found BAR.
		 */
		TAILQ_INSERT_BEFORE(bar, new_bar, chain);
	}

	/*
	 * pci_passthru devices synchronize their physical and virtual command
	 * register on init. For that reason, the virtual cmd reg should be
	 * updated as early as possible.
	 */
	uint16_t enbit = 0;
	switch (type) {
	case PCIBAR_IO:
		enbit = PCIM_CMD_PORTEN;
		break;
	case PCIBAR_MEM64:
	case PCIBAR_MEM32:
		enbit = PCIM_CMD_MEMEN;
		break;
	default:
		enbit = 0;
		break;
	}

	const uint16_t cmd = pci_get_cfgdata16(pdi, PCIR_COMMAND);
	pci_set_cfgdata16(pdi, PCIR_COMMAND, cmd | enbit);

	return (0);
}

static int
pci_emul_assign_bar(struct pci_devinst *const pdi, const int idx,
    const enum pcibar_type type, const uint64_t size)
{
	int error;
	uint64_t *baseptr, limit, addr, mask, lobits, bar;

	switch (type) {
	case PCIBAR_NONE:
		baseptr = NULL;
		addr = mask = lobits = 0;
		break;
	case PCIBAR_IO:
		baseptr = &pci_emul_iobase;
		limit = PCI_EMUL_IOLIMIT;
		mask = PCIM_BAR_IO_BASE;
		lobits = PCIM_BAR_IO_SPACE;
		break;
	case PCIBAR_MEM64:
		/*
		 * XXX
		 * Some drivers do not work well if the 64-bit BAR is allocated
		 * above 4GB. Allow for this by allocating small requests under
		 * 4GB unless then allocation size is larger than some arbitrary
		 * number (128MB currently).
		 */
		if (size > 128 * 1024 * 1024) {
			baseptr = &pci_emul_membase64;
			limit = pci_emul_memlim64;
			mask = PCIM_BAR_MEM_BASE;
			lobits = PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_64 |
				 PCIM_BAR_MEM_PREFETCH;
		} else {
			baseptr = &pci_emul_membase32;
			limit = PCI_EMUL_MEMLIMIT32;
			mask = PCIM_BAR_MEM_BASE;
			lobits = PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_64;
		}
		break;
	case PCIBAR_MEM32:
		baseptr = &pci_emul_membase32;
		limit = PCI_EMUL_MEMLIMIT32;
		mask = PCIM_BAR_MEM_BASE;
		lobits = PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_32;
		break;
	case PCIBAR_ROM:
		/* do not claim memory for ROM. OVMF will do it for us. */
		baseptr = NULL;
		limit = 0;
		mask = PCIM_BIOS_ADDR_MASK;
		lobits = 0;
		break;
	default:
		printf("pci_emul_alloc_base: invalid bar type %d\n", type);
		assert(0);
	}

	if (baseptr != NULL) {
		error = pci_emul_alloc_resource(baseptr, limit, size, &addr);
		if (error != 0)
			return (error);
	} else {
		addr = 0;
	}

	pdi->pi_bar[idx].type = type;
	pdi->pi_bar[idx].addr = addr;
	pdi->pi_bar[idx].size = size;
	/*
	 * passthru devices are using same lobits as physical device they set
	 * this property
	 */
	if (pdi->pi_bar[idx].lobits != 0) {
		lobits = pdi->pi_bar[idx].lobits;
	} else {
		pdi->pi_bar[idx].lobits = lobits;
	}

	/* Initialize the BAR register in config space */
	bar = (addr & mask) | lobits;
	pci_set_cfgdata32(pdi, PCIR_BAR(idx), bar);

	if (type == PCIBAR_MEM64) {
		assert(idx + 1 <= PCI_BARMAX);
		pdi->pi_bar[idx + 1].type = PCIBAR_MEMHI64;
		pci_set_cfgdata32(pdi, PCIR_BAR(idx + 1), bar >> 32);
	}

	if (type != PCIBAR_ROM) {
		register_bar(pdi, idx);
	}

	return (0);
}

int
pci_emul_alloc_rom(struct pci_devinst *const pdi, const uint64_t size,
    void **const addr)
{
	/* allocate ROM space once on first call */
	if (pci_emul_rombase == 0) {
		pci_emul_rombase = vm_create_devmem(pdi->pi_vmctx, VM_PCIROM,
		    "pcirom", PCI_EMUL_ROMSIZE);
		if (pci_emul_rombase == MAP_FAILED) {
			warnx("%s: failed to create rom segment", __func__);
			return (-1);
		}
		pci_emul_romlim = pci_emul_rombase + PCI_EMUL_ROMSIZE;
		pci_emul_romoffset = 0;
	}

	/* ROM size should be a power of 2 and greater than 2 KB */
	const uint64_t rom_size = MAX(1UL << flsl(size),
	    ~PCIM_BIOS_ADDR_MASK + 1);

	/* check if ROM fits into ROM space */
	if (pci_emul_romoffset + rom_size > PCI_EMUL_ROMSIZE) {
		warnx("%s: no space left in rom segment:", __func__);
		warnx("%16lu bytes left",
		    PCI_EMUL_ROMSIZE - pci_emul_romoffset);
		warnx("%16lu bytes required by %d/%d/%d", rom_size, pdi->pi_bus,
		    pdi->pi_slot, pdi->pi_func);
		return (-1);
	}

	/* allocate ROM BAR */
	const int error = pci_emul_alloc_bar(pdi, PCI_ROM_IDX, PCIBAR_ROM,
	    rom_size);
	if (error)
		return error;

	/* return address */
	*addr = pci_emul_rombase + pci_emul_romoffset;

	/* save offset into ROM Space */
	pdi->pi_romoffset = pci_emul_romoffset;

	/* increase offset for next ROM */
	pci_emul_romoffset += rom_size;

	return (0);
}

int
pci_emul_add_boot_device(struct pci_devinst *pi, int bootindex)
{
	struct boot_device *new_device, *device;

	/* don't permit a negative bootindex */
	if (bootindex < 0) {
		errx(4, "Invalid bootindex %d for %s", bootindex, pi->pi_name);
	}

	/* alloc new boot device */
	new_device = calloc(1, sizeof(struct boot_device));
	if (new_device == NULL) {
		return (ENOMEM);
	}
	new_device->pdi = pi;
	new_device->bootindex = bootindex;

	/* search for boot device with higher boot index */
	TAILQ_FOREACH(device, &boot_devices, boot_device_chain) {
		if (device->bootindex == bootindex) {
			errx(4,
			    "Could not set bootindex %d for %s. Bootindex already occupied by %s",
			    bootindex, pi->pi_name, device->pdi->pi_name);
		} else if (device->bootindex > bootindex) {
			break;
		}
	}

	/* add boot device to queue */
	if (device == NULL) {
		TAILQ_INSERT_TAIL(&boot_devices, new_device, boot_device_chain);
	} else {
		TAILQ_INSERT_BEFORE(device, new_device, boot_device_chain);
	}

	return (0);
}

#define	CAP_START_OFFSET	0x40
static int
pci_emul_add_capability(struct pci_devinst *pi, u_char *capdata, int caplen)
{
	int i, capoff, reallen;
	uint16_t sts;

	assert(caplen > 0);

	reallen = roundup2(caplen, 4);		/* dword aligned */

	sts = pci_get_cfgdata16(pi, PCIR_STATUS);
	if ((sts & PCIM_STATUS_CAPPRESENT) == 0)
		capoff = CAP_START_OFFSET;
	else
		capoff = pi->pi_capend + 1;

	/* Check if we have enough space */
	if (capoff + reallen > PCI_REGMAX + 1)
		return (-1);

	/* Set the previous capability pointer */
	if ((sts & PCIM_STATUS_CAPPRESENT) == 0) {
		pci_set_cfgdata8(pi, PCIR_CAP_PTR, capoff);
		pci_set_cfgdata16(pi, PCIR_STATUS, sts|PCIM_STATUS_CAPPRESENT);
	} else
		pci_set_cfgdata8(pi, pi->pi_prevcap + 1, capoff);

	/* Copy the capability */
	for (i = 0; i < caplen; i++)
		pci_set_cfgdata8(pi, capoff + i, capdata[i]);

	/* Set the next capability pointer */
	pci_set_cfgdata8(pi, capoff + 1, 0);

	pi->pi_prevcap = capoff;
	pi->pi_capend = capoff + reallen - 1;
	return (0);
}

static struct pci_devemu *
pci_emul_finddev(const char *name)
{
	struct pci_devemu **pdpp, *pdp;

	SET_FOREACH(pdpp, pci_devemu_set) {
		pdp = *pdpp;
		if (!strcmp(pdp->pe_emu, name)) {
			return (pdp);
		}
	}

	return (NULL);
}

static int
pci_emul_init(struct vmctx *ctx, struct pci_devemu *pde, int bus, int slot,
    int func, struct funcinfo *fi)
{
	struct pci_devinst *pdi;
	int err;

	pdi = calloc(1, sizeof(struct pci_devinst));

	pdi->pi_vmctx = ctx;
	pdi->pi_bus = bus;
	pdi->pi_slot = slot;
	pdi->pi_func = func;
	pthread_mutex_init(&pdi->pi_lintr.lock, NULL);
	pdi->pi_lintr.pin = 0;
	pdi->pi_lintr.state = IDLE;
	pdi->pi_lintr.pirq_pin = 0;
	pdi->pi_lintr.ioapic_irq = 0;
	pdi->pi_d = pde;
	snprintf(pdi->pi_name, PI_NAMESZ, "%s@pci.%d.%d.%d", pde->pe_emu, bus,
	    slot, func);

	/* Disable legacy interrupts */
	pci_set_cfgdata8(pdi, PCIR_INTLINE, 255);
	pci_set_cfgdata8(pdi, PCIR_INTPIN, 0);

	pci_set_cfgdata8(pdi, PCIR_COMMAND, PCIM_CMD_BUSMASTEREN);

	err = (*pde->pe_init)(pdi, fi->fi_config);
	if (err == 0)
		fi->fi_devi = pdi;
	else
		free(pdi);

	return (err);
}

void
pci_populate_msicap(struct msicap *msicap, int msgnum, int nextptr)
{
	int mmc;

	/* Number of msi messages must be a power of 2 between 1 and 32 */
	assert((msgnum & (msgnum - 1)) == 0 && msgnum >= 1 && msgnum <= 32);
	mmc = ffs(msgnum) - 1;

	bzero(msicap, sizeof(struct msicap));
	msicap->capid = PCIY_MSI;
	msicap->nextptr = nextptr;
	msicap->msgctrl = PCIM_MSICTRL_64BIT | (mmc << 1);
}

int
pci_emul_add_msicap(struct pci_devinst *pi, int msgnum)
{
	struct msicap msicap;

	pci_populate_msicap(&msicap, msgnum, 0);

	return (pci_emul_add_capability(pi, (u_char *)&msicap, sizeof(msicap)));
}

static void
pci_populate_msixcap(struct msixcap *msixcap, int msgnum, int barnum,
		     uint32_t msix_tab_size)
{

	assert(msix_tab_size % 4096 == 0);

	bzero(msixcap, sizeof(struct msixcap));
	msixcap->capid = PCIY_MSIX;

	/*
	 * Message Control Register, all fields set to
	 * zero except for the Table Size.
	 * Note: Table size N is encoded as N-1
	 */
	msixcap->msgctrl = msgnum - 1;

	/*
	 * MSI-X BAR setup:
	 * - MSI-X table start at offset 0
	 * - PBA table starts at a 4K aligned offset after the MSI-X table
	 */
	msixcap->table_info = barnum & PCIM_MSIX_BIR_MASK;
	msixcap->pba_info = msix_tab_size | (barnum & PCIM_MSIX_BIR_MASK);
}

static void
pci_msix_table_init(struct pci_devinst *pi, int table_entries)
{
	int i, table_size;

	assert(table_entries > 0);
	assert(table_entries <= MAX_MSIX_TABLE_ENTRIES);

	table_size = table_entries * MSIX_TABLE_ENTRY_SIZE;
	pi->pi_msix.table = calloc(1, table_size);

	/* set mask bit of vector control register */
	for (i = 0; i < table_entries; i++)
		pi->pi_msix.table[i].vector_control |= PCIM_MSIX_VCTRL_MASK;
}

int
pci_emul_add_msixcap(struct pci_devinst *pi, int msgnum, int barnum)
{
	uint32_t tab_size;
	struct msixcap msixcap;

	assert(msgnum >= 1 && msgnum <= MAX_MSIX_TABLE_ENTRIES);
	assert(barnum >= 0 && barnum <= PCIR_MAX_BAR_0);

	tab_size = msgnum * MSIX_TABLE_ENTRY_SIZE;

	/* Align table size to nearest 4K */
	tab_size = roundup2(tab_size, 4096);

	pi->pi_msix.table_bar = barnum;
	pi->pi_msix.pba_bar   = barnum;
	pi->pi_msix.table_offset = 0;
	pi->pi_msix.table_count = msgnum;
	pi->pi_msix.pba_offset = tab_size;
	pi->pi_msix.pba_size = PBA_SIZE(msgnum);

	pci_msix_table_init(pi, msgnum);

	pci_populate_msixcap(&msixcap, msgnum, barnum, tab_size);

	/* allocate memory for MSI-X Table and PBA */
	pci_emul_alloc_bar(pi, barnum, PCIBAR_MEM32,
				tab_size + pi->pi_msix.pba_size);

	return (pci_emul_add_capability(pi, (u_char *)&msixcap,
					sizeof(msixcap)));
}

static void
msixcap_cfgwrite(struct pci_devinst *pi, int capoff, int offset,
		 int bytes, uint32_t val)
{
	uint16_t msgctrl, rwmask;
	int off;

	off = offset - capoff;
	/* Message Control Register */
	if (off == 2 && bytes == 2) {
		rwmask = PCIM_MSIXCTRL_MSIX_ENABLE | PCIM_MSIXCTRL_FUNCTION_MASK;
		msgctrl = pci_get_cfgdata16(pi, offset);
		msgctrl &= ~rwmask;
		msgctrl |= val & rwmask;
		val = msgctrl;

		pi->pi_msix.enabled = val & PCIM_MSIXCTRL_MSIX_ENABLE;
		pi->pi_msix.function_mask = val & PCIM_MSIXCTRL_FUNCTION_MASK;
		pci_lintr_update(pi);
	}

	CFGWRITE(pi, offset, val, bytes);
}

static void
msicap_cfgwrite(struct pci_devinst *pi, int capoff, int offset,
		int bytes, uint32_t val)
{
	uint16_t msgctrl, rwmask, msgdata, mme;
	uint32_t addrlo;

	/*
	 * If guest is writing to the message control register make sure
	 * we do not overwrite read-only fields.
	 */
	if ((offset - capoff) == 2 && bytes == 2) {
		rwmask = PCIM_MSICTRL_MME_MASK | PCIM_MSICTRL_MSI_ENABLE;
		msgctrl = pci_get_cfgdata16(pi, offset);
		msgctrl &= ~rwmask;
		msgctrl |= val & rwmask;
		val = msgctrl;
	}
	CFGWRITE(pi, offset, val, bytes);

	msgctrl = pci_get_cfgdata16(pi, capoff + 2);
	addrlo = pci_get_cfgdata32(pi, capoff + 4);
	if (msgctrl & PCIM_MSICTRL_64BIT)
		msgdata = pci_get_cfgdata16(pi, capoff + 12);
	else
		msgdata = pci_get_cfgdata16(pi, capoff + 8);

	mme = msgctrl & PCIM_MSICTRL_MME_MASK;
	pi->pi_msi.enabled = msgctrl & PCIM_MSICTRL_MSI_ENABLE ? 1 : 0;
	if (pi->pi_msi.enabled) {
		pi->pi_msi.addr = addrlo;
		pi->pi_msi.msg_data = msgdata;
		pi->pi_msi.maxmsgnum = 1 << (mme >> 4);
	} else {
		pi->pi_msi.maxmsgnum = 0;
	}
	pci_lintr_update(pi);
}

static void
pciecap_cfgwrite(struct pci_devinst *pi, int capoff __unused, int offset,
    int bytes, uint32_t val)
{

	/* XXX don't write to the readonly parts */
	CFGWRITE(pi, offset, val, bytes);
}

#define	PCIECAP_VERSION	0x2
int
pci_emul_add_pciecap(struct pci_devinst *pi, int type)
{
	int err;
	struct pciecap pciecap;

	bzero(&pciecap, sizeof(pciecap));

	/*
	 * Use the integrated endpoint type for endpoints on a root complex bus.
	 *
	 * NB: bhyve currently only supports a single PCI bus that is the root
	 * complex bus, so all endpoints are integrated.
	 */
	if ((type == PCIEM_TYPE_ENDPOINT) && (pi->pi_bus == 0))
		type = PCIEM_TYPE_ROOT_INT_EP;

	pciecap.capid = PCIY_EXPRESS;
	pciecap.pcie_capabilities = PCIECAP_VERSION | type;
	if (type != PCIEM_TYPE_ROOT_INT_EP) {
		pciecap.link_capabilities = 0x411;	/* gen1, x1 */
		pciecap.link_status = 0x11;		/* gen1, x1 */
	}

	err = pci_emul_add_capability(pi, (u_char *)&pciecap, sizeof(pciecap));
	return (err);
}

/*
 * This function assumes that 'coff' is in the capabilities region of the
 * config space. A capoff parameter of zero will force a search for the
 * offset and type.
 */
void
pci_emul_capwrite(struct pci_devinst *pi, int offset, int bytes, uint32_t val,
    uint8_t capoff, int capid)
{
	uint8_t nextoff;

	/* Do not allow un-aligned writes */
	if ((offset & (bytes - 1)) != 0)
		return;

	if (capoff == 0) {
		/* Find the capability that we want to update */
		capoff = CAP_START_OFFSET;
		while (1) {
			nextoff = pci_get_cfgdata8(pi, capoff + 1);
			if (nextoff == 0)
				break;
			if (offset >= capoff && offset < nextoff)
				break;

			capoff = nextoff;
		}
		assert(offset >= capoff);
		capid = pci_get_cfgdata8(pi, capoff);
	}

	/*
	 * Capability ID and Next Capability Pointer are readonly.
	 * However, some o/s's do 4-byte writes that include these.
	 * For this case, trim the write back to 2 bytes and adjust
	 * the data.
	 */
	if (offset == capoff || offset == capoff + 1) {
		if (offset == capoff && bytes == 4) {
			bytes = 2;
			offset += 2;
			val >>= 16;
		} else
			return;
	}

	switch (capid) {
	case PCIY_MSI:
		msicap_cfgwrite(pi, capoff, offset, bytes, val);
		break;
	case PCIY_MSIX:
		msixcap_cfgwrite(pi, capoff, offset, bytes, val);
		break;
	case PCIY_EXPRESS:
		pciecap_cfgwrite(pi, capoff, offset, bytes, val);
		break;
	default:
		break;
	}
}

static int
pci_emul_iscap(struct pci_devinst *pi, int offset)
{
	uint16_t sts;

	sts = pci_get_cfgdata16(pi, PCIR_STATUS);
	if ((sts & PCIM_STATUS_CAPPRESENT) != 0) {
		if (offset >= CAP_START_OFFSET && offset <= pi->pi_capend)
			return (1);
	}
	return (0);
}

static int
pci_emul_fallback_handler(struct vcpu *vcpu __unused, int dir,
    uint64_t addr __unused, int size __unused, uint64_t *val,
    void *arg1 __unused, long arg2 __unused)
{
	/*
	 * Ignore writes; return 0xff's for reads. The mem read code
	 * will take care of truncating to the correct size.
	 */
	if (dir == MEM_F_READ) {
		*val = 0xffffffffffffffff;
	}

	return (0);
}

static int
pci_emul_ecfg_handler(struct vcpu *vcpu __unused, int dir, uint64_t addr,
    int bytes, uint64_t *val, void *arg1 __unused, long arg2 __unused)
{
	int bus, slot, func, coff, in;

	coff = addr & 0xfff;
	func = (addr >> 12) & 0x7;
	slot = (addr >> 15) & 0x1f;
	bus = (addr >> 20) & 0xff;
	in = (dir == MEM_F_READ);
	if (in)
		*val = ~0UL;
	pci_cfgrw(in, bus, slot, func, coff, bytes, (uint32_t *)val);
	return (0);
}

uint64_t
pci_ecfg_base(void)
{

	return (PCI_EMUL_ECFG_BASE);
}

static int
init_bootorder(void)
{
	struct boot_device *device;
	FILE *fp;
	char *bootorder;
	size_t bootorder_len;

	if (TAILQ_EMPTY(&boot_devices))
		return (0);

	fp = open_memstream(&bootorder, &bootorder_len);
	TAILQ_FOREACH(device, &boot_devices, boot_device_chain) {
		fprintf(fp, "/pci@i0cf8/pci@%d,%d\n",
		    device->pdi->pi_slot, device->pdi->pi_func);
	}
	fclose(fp);

	return (qemu_fwcfg_add_file("bootorder", bootorder_len, bootorder));
}

#define	BUSIO_ROUNDUP		32
#define	BUSMEM32_ROUNDUP	(1024 * 1024)
#define	BUSMEM64_ROUNDUP	(512 * 1024 * 1024)

int
init_pci(struct vmctx *ctx)
{
	char node_name[sizeof("pci.XXX.XX.X")];
	struct mem_range mr;
	struct pci_devemu *pde;
	struct businfo *bi;
	struct slotinfo *si;
	struct funcinfo *fi;
	nvlist_t *nvl;
	const char *emul;
	size_t lowmem;
	int bus, slot, func;
	int error;

	if (vm_get_lowmem_limit(ctx) > PCI_EMUL_MEMBASE32)
		errx(EX_OSERR, "Invalid lowmem limit");

	pci_emul_iobase = PCI_EMUL_IOBASE;
	pci_emul_membase32 = PCI_EMUL_MEMBASE32;

	pci_emul_membase64 = 4*GB + vm_get_highmem_size(ctx);
	pci_emul_membase64 = roundup2(pci_emul_membase64, PCI_EMUL_MEMSIZE64);
	pci_emul_memlim64 = pci_emul_membase64 + PCI_EMUL_MEMSIZE64;

	TAILQ_INIT(&boot_devices);

	for (bus = 0; bus < MAXBUSES; bus++) {
		snprintf(node_name, sizeof(node_name), "pci.%d", bus);
		nvl = find_config_node(node_name);
		if (nvl == NULL)
			continue;
		pci_businfo[bus] = calloc(1, sizeof(struct businfo));
		bi = pci_businfo[bus];

		/*
		 * Keep track of the i/o and memory resources allocated to
		 * this bus.
		 */
		bi->iobase = pci_emul_iobase;
		bi->membase32 = pci_emul_membase32;
		bi->membase64 = pci_emul_membase64;

		/* first run: init devices */
		for (slot = 0; slot < MAXSLOTS; slot++) {
			si = &bi->slotinfo[slot];
			for (func = 0; func < MAXFUNCS; func++) {
				fi = &si->si_funcs[func];
				snprintf(node_name, sizeof(node_name),
				    "pci.%d.%d.%d", bus, slot, func);
				nvl = find_config_node(node_name);
				if (nvl == NULL)
					continue;

				fi->fi_config = nvl;
				emul = get_config_value_node(nvl, "device");
				if (emul == NULL) {
					EPRINTLN("pci slot %d:%d:%d: missing "
					    "\"device\" value", bus, slot, func);
					return (EINVAL);
				}
				pde = pci_emul_finddev(emul);
				if (pde == NULL) {
					EPRINTLN("pci slot %d:%d:%d: unknown "
					    "device \"%s\"", bus, slot, func,
					    emul);
					return (EINVAL);
				}
				if (pde->pe_alias != NULL) {
					EPRINTLN("pci slot %d:%d:%d: legacy "
					    "device \"%s\", use \"%s\" instead",
					    bus, slot, func, emul,
					    pde->pe_alias);
					return (EINVAL);
				}
				fi->fi_pde = pde;
				error = pci_emul_init(ctx, pde, bus, slot,
				    func, fi);
				if (error)
					return (error);
			}
		}

		/* second run: assign BARs and free list */
		struct pci_bar_allocation *bar;
		struct pci_bar_allocation *bar_tmp;
		TAILQ_FOREACH_SAFE(bar, &pci_bars, chain, bar_tmp) {
			pci_emul_assign_bar(bar->pdi, bar->idx, bar->type,
			    bar->size);
			free(bar);
		}
		TAILQ_INIT(&pci_bars);

		/*
		 * Add some slop to the I/O and memory resources decoded by
		 * this bus to give a guest some flexibility if it wants to
		 * reprogram the BARs.
		 */
		pci_emul_iobase += BUSIO_ROUNDUP;
		pci_emul_iobase = roundup2(pci_emul_iobase, BUSIO_ROUNDUP);
		bi->iolimit = pci_emul_iobase;

		pci_emul_membase32 += BUSMEM32_ROUNDUP;
		pci_emul_membase32 = roundup2(pci_emul_membase32,
		    BUSMEM32_ROUNDUP);
		bi->memlimit32 = pci_emul_membase32;

		pci_emul_membase64 += BUSMEM64_ROUNDUP;
		pci_emul_membase64 = roundup2(pci_emul_membase64,
		    BUSMEM64_ROUNDUP);
		bi->memlimit64 = pci_emul_membase64;
	}

	/*
	 * PCI backends are initialized before routing INTx interrupts
	 * so that LPC devices are able to reserve ISA IRQs before
	 * routing PIRQ pins.
	 */
	for (bus = 0; bus < MAXBUSES; bus++) {
		if ((bi = pci_businfo[bus]) == NULL)
			continue;

		for (slot = 0; slot < MAXSLOTS; slot++) {
			si = &bi->slotinfo[slot];
			for (func = 0; func < MAXFUNCS; func++) {
				fi = &si->si_funcs[func];
				if (fi->fi_devi == NULL)
					continue;
				pci_lintr_route(fi->fi_devi);
			}
		}
	}
	lpc_pirq_routed();

	if ((error = init_bootorder()) != 0) {
		warnx("%s: Unable to init bootorder", __func__);
		return (error);
	}

	/*
	 * The guest physical memory map looks like the following:
	 * [0,		    lowmem)		guest system memory
	 * [lowmem,	    0xC0000000)		memory hole (may be absent)
	 * [0xC0000000,     0xE0000000)		PCI hole (32-bit BAR allocation)
	 * [0xE0000000,	    0xF0000000)		PCI extended config window
	 * [0xF0000000,	    4GB)		LAPIC, IOAPIC, HPET, firmware
	 * [4GB,	    4GB + highmem)
	 */

	/*
	 * Accesses to memory addresses that are not allocated to system
	 * memory or PCI devices return 0xff's.
	 */
	lowmem = vm_get_lowmem_size(ctx);
	bzero(&mr, sizeof(struct mem_range));
	mr.name = "PCI hole";
	mr.flags = MEM_F_RW | MEM_F_IMMUTABLE;
	mr.base = lowmem;
	mr.size = (4ULL * 1024 * 1024 * 1024) - lowmem;
	mr.handler = pci_emul_fallback_handler;
	error = register_mem_fallback(&mr);
	assert(error == 0);

	/* PCI extended config space */
	bzero(&mr, sizeof(struct mem_range));
	mr.name = "PCI ECFG";
	mr.flags = MEM_F_RW | MEM_F_IMMUTABLE;
	mr.base = PCI_EMUL_ECFG_BASE;
	mr.size = PCI_EMUL_ECFG_SIZE;
	mr.handler = pci_emul_ecfg_handler;
	error = register_mem(&mr);
	assert(error == 0);

	return (0);
}

static void
pci_apic_prt_entry(int bus __unused, int slot, int pin, int pirq_pin __unused,
    int ioapic_irq, void *arg __unused)
{

	dsdt_line("  Package ()");
	dsdt_line("  {");
	dsdt_line("    0x%X,", slot << 16 | 0xffff);
	dsdt_line("    0x%02X,", pin - 1);
	dsdt_line("    Zero,");
	dsdt_line("    0x%X", ioapic_irq);
	dsdt_line("  },");
}

static void
pci_pirq_prt_entry(int bus __unused, int slot, int pin, int pirq_pin,
    int ioapic_irq __unused, void *arg __unused)
{
	char *name;

	name = lpc_pirq_name(pirq_pin);
	if (name == NULL)
		return;
	dsdt_line("  Package ()");
	dsdt_line("  {");
	dsdt_line("    0x%X,", slot << 16 | 0xffff);
	dsdt_line("    0x%02X,", pin - 1);
	dsdt_line("    %s,", name);
	dsdt_line("    0x00");
	dsdt_line("  },");
	free(name);
}

/*
 * A bhyve virtual machine has a flat PCI hierarchy with a root port
 * corresponding to each PCI bus.
 */
static void
pci_bus_write_dsdt(int bus)
{
	struct businfo *bi;
	struct slotinfo *si;
	struct pci_devinst *pi;
	int count, func, slot;

	/*
	 * If there are no devices on this 'bus' then just return.
	 */
	if ((bi = pci_businfo[bus]) == NULL) {
		/*
		 * Bus 0 is special because it decodes the I/O ports used
		 * for PCI config space access even if there are no devices
		 * on it.
		 */
		if (bus != 0)
			return;
	}

	dsdt_line("  Device (PC%02X)", bus);
	dsdt_line("  {");
	dsdt_line("    Name (_HID, EisaId (\"PNP0A03\"))");

	dsdt_line("    Method (_BBN, 0, NotSerialized)");
	dsdt_line("    {");
	dsdt_line("        Return (0x%08X)", bus);
	dsdt_line("    }");
	dsdt_line("    Name (_CRS, ResourceTemplate ()");
	dsdt_line("    {");
	dsdt_line("      WordBusNumber (ResourceProducer, MinFixed, "
	    "MaxFixed, PosDecode,");
	dsdt_line("        0x0000,             // Granularity");
	dsdt_line("        0x%04X,             // Range Minimum", bus);
	dsdt_line("        0x%04X,             // Range Maximum", bus);
	dsdt_line("        0x0000,             // Translation Offset");
	dsdt_line("        0x0001,             // Length");
	dsdt_line("        ,, )");

	if (bus == 0) {
		dsdt_indent(3);
		dsdt_fixed_ioport(0xCF8, 8);
		dsdt_unindent(3);

		dsdt_line("      WordIO (ResourceProducer, MinFixed, MaxFixed, "
		    "PosDecode, EntireRange,");
		dsdt_line("        0x0000,             // Granularity");
		dsdt_line("        0x0000,             // Range Minimum");
		dsdt_line("        0x0CF7,             // Range Maximum");
		dsdt_line("        0x0000,             // Translation Offset");
		dsdt_line("        0x0CF8,             // Length");
		dsdt_line("        ,, , TypeStatic)");

		dsdt_line("      WordIO (ResourceProducer, MinFixed, MaxFixed, "
		    "PosDecode, EntireRange,");
		dsdt_line("        0x0000,             // Granularity");
		dsdt_line("        0x0D00,             // Range Minimum");
		dsdt_line("        0x%04X,             // Range Maximum",
		    PCI_EMUL_IOBASE - 1);
		dsdt_line("        0x0000,             // Translation Offset");
		dsdt_line("        0x%04X,             // Length",
		    PCI_EMUL_IOBASE - 0x0D00);
		dsdt_line("        ,, , TypeStatic)");

		if (bi == NULL) {
			dsdt_line("    })");
			goto done;
		}
	}
	assert(bi != NULL);

	/* i/o window */
	dsdt_line("      WordIO (ResourceProducer, MinFixed, MaxFixed, "
	    "PosDecode, EntireRange,");
	dsdt_line("        0x0000,             // Granularity");
	dsdt_line("        0x%04X,             // Range Minimum", bi->iobase);
	dsdt_line("        0x%04X,             // Range Maximum",
	    bi->iolimit - 1);
	dsdt_line("        0x0000,             // Translation Offset");
	dsdt_line("        0x%04X,             // Length",
	    bi->iolimit - bi->iobase);
	dsdt_line("        ,, , TypeStatic)");

	/* mmio window (32-bit) */
	dsdt_line("      DWordMemory (ResourceProducer, PosDecode, "
	    "MinFixed, MaxFixed, NonCacheable, ReadWrite,");
	dsdt_line("        0x00000000,         // Granularity");
	dsdt_line("        0x%08X,         // Range Minimum\n", bi->membase32);
	dsdt_line("        0x%08X,         // Range Maximum\n",
	    bi->memlimit32 - 1);
	dsdt_line("        0x00000000,         // Translation Offset");
	dsdt_line("        0x%08X,         // Length\n",
	    bi->memlimit32 - bi->membase32);
	dsdt_line("        ,, , AddressRangeMemory, TypeStatic)");

	/* mmio window (64-bit) */
	dsdt_line("      QWordMemory (ResourceProducer, PosDecode, "
	    "MinFixed, MaxFixed, NonCacheable, ReadWrite,");
	dsdt_line("        0x0000000000000000, // Granularity");
	dsdt_line("        0x%016lX, // Range Minimum\n", bi->membase64);
	dsdt_line("        0x%016lX, // Range Maximum\n",
	    bi->memlimit64 - 1);
	dsdt_line("        0x0000000000000000, // Translation Offset");
	dsdt_line("        0x%016lX, // Length\n",
	    bi->memlimit64 - bi->membase64);
	dsdt_line("        ,, , AddressRangeMemory, TypeStatic)");
	dsdt_line("    })");

	count = pci_count_lintr(bus);
	if (count != 0) {
		dsdt_indent(2);
		dsdt_line("Name (PPRT, Package ()");
		dsdt_line("{");
		pci_walk_lintr(bus, pci_pirq_prt_entry, NULL);
		dsdt_line("})");
		dsdt_line("Name (APRT, Package ()");
		dsdt_line("{");
		pci_walk_lintr(bus, pci_apic_prt_entry, NULL);
		dsdt_line("})");
		dsdt_line("Method (_PRT, 0, NotSerialized)");
		dsdt_line("{");
		dsdt_line("  If (PICM)");
		dsdt_line("  {");
		dsdt_line("    Return (APRT)");
		dsdt_line("  }");
		dsdt_line("  Else");
		dsdt_line("  {");
		dsdt_line("    Return (PPRT)");
		dsdt_line("  }");
		dsdt_line("}");
		dsdt_unindent(2);
	}

	dsdt_indent(2);
	for (slot = 0; slot < MAXSLOTS; slot++) {
		si = &bi->slotinfo[slot];
		for (func = 0; func < MAXFUNCS; func++) {
			pi = si->si_funcs[func].fi_devi;
			if (pi != NULL && pi->pi_d->pe_write_dsdt != NULL)
				pi->pi_d->pe_write_dsdt(pi);
		}
	}
	dsdt_unindent(2);
done:
	dsdt_line("  }");
}

void
pci_write_dsdt(void)
{
	int bus;

	dsdt_indent(1);
	dsdt_line("Name (PICM, 0x00)");
	dsdt_line("Method (_PIC, 1, NotSerialized)");
	dsdt_line("{");
	dsdt_line("  Store (Arg0, PICM)");
	dsdt_line("}");
	dsdt_line("");
	dsdt_line("Scope (_SB)");
	dsdt_line("{");
	for (bus = 0; bus < MAXBUSES; bus++)
		pci_bus_write_dsdt(bus);
	dsdt_line("}");
	dsdt_unindent(1);
}

int
pci_bus_configured(int bus)
{
	assert(bus >= 0 && bus < MAXBUSES);
	return (pci_businfo[bus] != NULL);
}

int
pci_msi_enabled(struct pci_devinst *pi)
{
	return (pi->pi_msi.enabled);
}

int
pci_msi_maxmsgnum(struct pci_devinst *pi)
{
	if (pi->pi_msi.enabled)
		return (pi->pi_msi.maxmsgnum);
	else
		return (0);
}

int
pci_msix_enabled(struct pci_devinst *pi)
{

	return (pi->pi_msix.enabled && !pi->pi_msi.enabled);
}

void
pci_generate_msix(struct pci_devinst *pi, int index)
{
	struct msix_table_entry *mte;

	if (!pci_msix_enabled(pi))
		return;

	if (pi->pi_msix.function_mask)
		return;

	if (index >= pi->pi_msix.table_count)
		return;

	mte = &pi->pi_msix.table[index];
	if ((mte->vector_control & PCIM_MSIX_VCTRL_MASK) == 0) {
		/* XXX Set PBA bit if interrupt is disabled */
		vm_lapic_msi(pi->pi_vmctx, mte->addr, mte->msg_data);
	}
}

void
pci_generate_msi(struct pci_devinst *pi, int index)
{

	if (pci_msi_enabled(pi) && index < pci_msi_maxmsgnum(pi)) {
		vm_lapic_msi(pi->pi_vmctx, pi->pi_msi.addr,
			     pi->pi_msi.msg_data + index);
	}
}

static bool
pci_lintr_permitted(struct pci_devinst *pi)
{
	uint16_t cmd;

	cmd = pci_get_cfgdata16(pi, PCIR_COMMAND);
	return (!(pi->pi_msi.enabled || pi->pi_msix.enabled ||
		(cmd & PCIM_CMD_INTxDIS)));
}

void
pci_lintr_request(struct pci_devinst *pi)
{
	struct businfo *bi;
	struct slotinfo *si;
	int bestpin, bestcount, pin;

	bi = pci_businfo[pi->pi_bus];
	assert(bi != NULL);

	/*
	 * Just allocate a pin from our slot.  The pin will be
	 * assigned IRQs later when interrupts are routed.
	 */
	si = &bi->slotinfo[pi->pi_slot];
	bestpin = 0;
	bestcount = si->si_intpins[0].ii_count;
	for (pin = 1; pin < 4; pin++) {
		if (si->si_intpins[pin].ii_count < bestcount) {
			bestpin = pin;
			bestcount = si->si_intpins[pin].ii_count;
		}
	}

	si->si_intpins[bestpin].ii_count++;
	pi->pi_lintr.pin = bestpin + 1;
	pci_set_cfgdata8(pi, PCIR_INTPIN, bestpin + 1);
}

static void
pci_lintr_route(struct pci_devinst *pi)
{
	struct businfo *bi;
	struct intxinfo *ii;

	if (pi->pi_lintr.pin == 0)
		return;

	bi = pci_businfo[pi->pi_bus];
	assert(bi != NULL);
	ii = &bi->slotinfo[pi->pi_slot].si_intpins[pi->pi_lintr.pin - 1];

	/*
	 * Attempt to allocate an I/O APIC pin for this intpin if one
	 * is not yet assigned.
	 */
	if (ii->ii_ioapic_irq == 0)
		ii->ii_ioapic_irq = ioapic_pci_alloc_irq(pi);
	assert(ii->ii_ioapic_irq > 0);

	/*
	 * Attempt to allocate a PIRQ pin for this intpin if one is
	 * not yet assigned.
	 */
	if (ii->ii_pirq_pin == 0)
		ii->ii_pirq_pin = pirq_alloc_pin(pi);
	assert(ii->ii_pirq_pin > 0);

	pi->pi_lintr.ioapic_irq = ii->ii_ioapic_irq;
	pi->pi_lintr.pirq_pin = ii->ii_pirq_pin;
	pci_set_cfgdata8(pi, PCIR_INTLINE, pirq_irq(ii->ii_pirq_pin));
}

void
pci_lintr_assert(struct pci_devinst *pi)
{

	assert(pi->pi_lintr.pin > 0);

	pthread_mutex_lock(&pi->pi_lintr.lock);
	if (pi->pi_lintr.state == IDLE) {
		if (pci_lintr_permitted(pi)) {
			pi->pi_lintr.state = ASSERTED;
			pci_irq_assert(pi);
		} else
			pi->pi_lintr.state = PENDING;
	}
	pthread_mutex_unlock(&pi->pi_lintr.lock);
}

void
pci_lintr_deassert(struct pci_devinst *pi)
{

	assert(pi->pi_lintr.pin > 0);

	pthread_mutex_lock(&pi->pi_lintr.lock);
	if (pi->pi_lintr.state == ASSERTED) {
		pi->pi_lintr.state = IDLE;
		pci_irq_deassert(pi);
	} else if (pi->pi_lintr.state == PENDING)
		pi->pi_lintr.state = IDLE;
	pthread_mutex_unlock(&pi->pi_lintr.lock);
}

static void
pci_lintr_update(struct pci_devinst *pi)
{

	pthread_mutex_lock(&pi->pi_lintr.lock);
	if (pi->pi_lintr.state == ASSERTED && !pci_lintr_permitted(pi)) {
		pci_irq_deassert(pi);
		pi->pi_lintr.state = PENDING;
	} else if (pi->pi_lintr.state == PENDING && pci_lintr_permitted(pi)) {
		pi->pi_lintr.state = ASSERTED;
		pci_irq_assert(pi);
	}
	pthread_mutex_unlock(&pi->pi_lintr.lock);
}

int
pci_count_lintr(int bus)
{
	int count, slot, pin;
	struct slotinfo *slotinfo;

	count = 0;
	if (pci_businfo[bus] != NULL) {
		for (slot = 0; slot < MAXSLOTS; slot++) {
			slotinfo = &pci_businfo[bus]->slotinfo[slot];
			for (pin = 0; pin < 4; pin++) {
				if (slotinfo->si_intpins[pin].ii_count != 0)
					count++;
			}
		}
	}
	return (count);
}

void
pci_walk_lintr(int bus, pci_lintr_cb cb, void *arg)
{
	struct businfo *bi;
	struct slotinfo *si;
	struct intxinfo *ii;
	int slot, pin;

	if ((bi = pci_businfo[bus]) == NULL)
		return;

	for (slot = 0; slot < MAXSLOTS; slot++) {
		si = &bi->slotinfo[slot];
		for (pin = 0; pin < 4; pin++) {
			ii = &si->si_intpins[pin];
			if (ii->ii_count != 0)
				cb(bus, slot, pin + 1, ii->ii_pirq_pin,
				    ii->ii_ioapic_irq, arg);
		}
	}
}

/*
 * Return 1 if the emulated device in 'slot' is a multi-function device.
 * Return 0 otherwise.
 */
static int
pci_emul_is_mfdev(int bus, int slot)
{
	struct businfo *bi;
	struct slotinfo *si;
	int f, numfuncs;

	numfuncs = 0;
	if ((bi = pci_businfo[bus]) != NULL) {
		si = &bi->slotinfo[slot];
		for (f = 0; f < MAXFUNCS; f++) {
			if (si->si_funcs[f].fi_devi != NULL) {
				numfuncs++;
			}
		}
	}
	return (numfuncs > 1);
}

/*
 * Ensure that the PCIM_MFDEV bit is properly set (or unset) depending on
 * whether or not is a multi-function being emulated in the pci 'slot'.
 */
static void
pci_emul_hdrtype_fixup(int bus, int slot, int off, int bytes, uint32_t *rv)
{
	int mfdev;

	if (off <= PCIR_HDRTYPE && off + bytes > PCIR_HDRTYPE) {
		mfdev = pci_emul_is_mfdev(bus, slot);
		switch (bytes) {
		case 1:
		case 2:
			*rv &= ~PCIM_MFDEV;
			if (mfdev) {
				*rv |= PCIM_MFDEV;
			}
			break;
		case 4:
			*rv &= ~(PCIM_MFDEV << 16);
			if (mfdev) {
				*rv |= (PCIM_MFDEV << 16);
			}
			break;
		}
	}
}

/*
 * Update device state in response to changes to the PCI command
 * register.
 */
void
pci_emul_cmd_changed(struct pci_devinst *pi, uint16_t old)
{
	int i;
	uint16_t changed, new;

	new = pci_get_cfgdata16(pi, PCIR_COMMAND);
	changed = old ^ new;

	/*
	 * If the MMIO or I/O address space decoding has changed then
	 * register/unregister all BARs that decode that address space.
	 */
	for (i = 0; i <= PCI_BARMAX_WITH_ROM; i++) {
		switch (pi->pi_bar[i].type) {
			case PCIBAR_NONE:
			case PCIBAR_MEMHI64:
				break;
			case PCIBAR_IO:
				/* I/O address space decoding changed? */
				if (changed & PCIM_CMD_PORTEN) {
					if (new & PCIM_CMD_PORTEN)
						register_bar(pi, i);
					else
						unregister_bar(pi, i);
				}
				break;
			case PCIBAR_ROM:
				/* skip (un-)register of ROM if it disabled */
				if (!romen(pi))
					break;
				/* fallthrough */
			case PCIBAR_MEM32:
			case PCIBAR_MEM64:
				/* MMIO address space decoding changed? */
				if (changed & PCIM_CMD_MEMEN) {
					if (new & PCIM_CMD_MEMEN)
						register_bar(pi, i);
					else
						unregister_bar(pi, i);
				}
				break;
			default:
				assert(0);
		}
	}

	/*
	 * If INTx has been unmasked and is pending, assert the
	 * interrupt.
	 */
	pci_lintr_update(pi);
}

static void
pci_emul_cmdsts_write(struct pci_devinst *pi, int coff, uint32_t new, int bytes)
{
	int rshift;
	uint32_t cmd, old, readonly;

	cmd = pci_get_cfgdata16(pi, PCIR_COMMAND);	/* stash old value */

	/*
	 * From PCI Local Bus Specification 3.0 sections 6.2.2 and 6.2.3.
	 *
	 * XXX Bits 8, 11, 12, 13, 14 and 15 in the status register are
	 * 'write 1 to clear'. However these bits are not set to '1' by
	 * any device emulation so it is simpler to treat them as readonly.
	 */
	rshift = (coff & 0x3) * 8;
	readonly = 0xFFFFF880 >> rshift;

	old = CFGREAD(pi, coff, bytes);
	new &= ~readonly;
	new |= (old & readonly);
	CFGWRITE(pi, coff, new, bytes);			/* update config */

	pci_emul_cmd_changed(pi, cmd);
}

static void
pci_cfgrw(int in, int bus, int slot, int func, int coff, int bytes,
    uint32_t *valp)
{
	struct businfo *bi;
	struct slotinfo *si;
	struct pci_devinst *pi;
	struct pci_devemu *pe;
	int idx, needcfg;
	uint64_t addr, bar, mask;

	if ((bi = pci_businfo[bus]) != NULL) {
		si = &bi->slotinfo[slot];
		pi = si->si_funcs[func].fi_devi;
	} else
		pi = NULL;

	/*
	 * Just return if there is no device at this slot:func or if the
	 * the guest is doing an un-aligned access.
	 */
	if (pi == NULL || (bytes != 1 && bytes != 2 && bytes != 4) ||
	    (coff & (bytes - 1)) != 0) {
		if (in)
			*valp = 0xffffffff;
		return;
	}

	/*
	 * Ignore all writes beyond the standard config space and return all
	 * ones on reads.
	 */
	if (coff >= PCI_REGMAX + 1) {
		if (in) {
			*valp = 0xffffffff;
			/*
			 * Extended capabilities begin at offset 256 in config
			 * space. Absence of extended capabilities is signaled
			 * with all 0s in the extended capability header at
			 * offset 256.
			 */
			if (coff <= PCI_REGMAX + 4)
				*valp = 0x00000000;
		}
		return;
	}

	pe = pi->pi_d;

	/*
	 * Config read
	 */
	if (in) {
		/* Let the device emulation override the default handler */
		if (pe->pe_cfgread != NULL) {
			needcfg = pe->pe_cfgread(pi, coff, bytes, valp);
		} else {
			needcfg = 1;
		}

		if (needcfg)
			*valp = CFGREAD(pi, coff, bytes);

		pci_emul_hdrtype_fixup(bus, slot, coff, bytes, valp);
	} else {
		/* Let the device emulation override the default handler */
		if (pe->pe_cfgwrite != NULL &&
		    (*pe->pe_cfgwrite)(pi, coff, bytes, *valp) == 0)
			return;

		/*
		 * Special handling for write to BAR and ROM registers
		 */
		if (is_pcir_bar(coff) || is_pcir_bios(coff)) {
			/*
			 * Ignore writes to BAR registers that are not
			 * 4-byte aligned.
			 */
			if (bytes != 4 || (coff & 0x3) != 0)
				return;

			if (is_pcir_bar(coff)) {
				idx = (coff - PCIR_BAR(0)) / 4;
			} else if (is_pcir_bios(coff)) {
				idx = PCI_ROM_IDX;
			} else {
				errx(4, "%s: invalid BAR offset %d", __func__,
				    coff);
			}

			mask = ~(pi->pi_bar[idx].size - 1);
			switch (pi->pi_bar[idx].type) {
			case PCIBAR_NONE:
				pi->pi_bar[idx].addr = bar = 0;
				break;
			case PCIBAR_IO:
				addr = *valp & mask;
				addr &= 0xffff;
				bar = addr | pi->pi_bar[idx].lobits;
				/*
				 * Register the new BAR value for interception
				 */
				if (addr != pi->pi_bar[idx].addr) {
					update_bar_address(pi, addr, idx,
							   PCIBAR_IO);
				}
				break;
			case PCIBAR_MEM32:
				addr = bar = *valp & mask;
				bar |= pi->pi_bar[idx].lobits;
				if (addr != pi->pi_bar[idx].addr) {
					update_bar_address(pi, addr, idx,
							   PCIBAR_MEM32);
				}
				break;
			case PCIBAR_MEM64:
				addr = bar = *valp & mask;
				bar |= pi->pi_bar[idx].lobits;
				if (addr != (uint32_t)pi->pi_bar[idx].addr) {
					update_bar_address(pi, addr, idx,
							   PCIBAR_MEM64);
				}
				break;
			case PCIBAR_MEMHI64:
				mask = ~(pi->pi_bar[idx - 1].size - 1);
				addr = ((uint64_t)*valp << 32) & mask;
				bar = addr >> 32;
				if (bar != pi->pi_bar[idx - 1].addr >> 32) {
					update_bar_address(pi, addr, idx - 1,
							   PCIBAR_MEMHI64);
				}
				break;
			case PCIBAR_ROM:
				addr = bar = *valp & mask;
				if (memen(pi) && romen(pi)) {
					unregister_bar(pi, idx);
				}
				pi->pi_bar[idx].addr = addr;
				pi->pi_bar[idx].lobits = *valp &
				    PCIM_BIOS_ENABLE;
				/* romen could have changed it value */
				if (memen(pi) && romen(pi)) {
					register_bar(pi, idx);
				}
				bar |= pi->pi_bar[idx].lobits;
				break;
			default:
				assert(0);
			}
			pci_set_cfgdata32(pi, coff, bar);

		} else if (pci_emul_iscap(pi, coff)) {
			pci_emul_capwrite(pi, coff, bytes, *valp, 0, 0);
		} else if (coff >= PCIR_COMMAND && coff < PCIR_REVID) {
			pci_emul_cmdsts_write(pi, coff, *valp, bytes);
		} else {
			CFGWRITE(pi, coff, *valp, bytes);
		}
	}
}

static int cfgenable, cfgbus, cfgslot, cfgfunc, cfgoff;

static int
pci_emul_cfgaddr(struct vmctx *ctx __unused, int in,
    int port __unused, int bytes, uint32_t *eax, void *arg __unused)
{
	uint32_t x;

	if (bytes != 4) {
		if (in)
			*eax = (bytes == 2) ? 0xffff : 0xff;
		return (0);
	}

	if (in) {
		x = (cfgbus << 16) | (cfgslot << 11) | (cfgfunc << 8) | cfgoff;
		if (cfgenable)
			x |= CONF1_ENABLE;
		*eax = x;
	} else {
		x = *eax;
		cfgenable = (x & CONF1_ENABLE) == CONF1_ENABLE;
		cfgoff = (x & PCI_REGMAX) & ~0x03;
		cfgfunc = (x >> 8) & PCI_FUNCMAX;
		cfgslot = (x >> 11) & PCI_SLOTMAX;
		cfgbus = (x >> 16) & PCI_BUSMAX;
	}

	return (0);
}
INOUT_PORT(pci_cfgaddr, CONF1_ADDR_PORT, IOPORT_F_INOUT, pci_emul_cfgaddr);

static int
pci_emul_cfgdata(struct vmctx *ctx __unused, int in, int port,
    int bytes, uint32_t *eax, void *arg __unused)
{
	int coff;

	assert(bytes == 1 || bytes == 2 || bytes == 4);

	coff = cfgoff + (port - CONF1_DATA_PORT);
	if (cfgenable) {
		pci_cfgrw(in, cfgbus, cfgslot, cfgfunc, coff, bytes, eax);
	} else {
		/* Ignore accesses to cfgdata if not enabled by cfgaddr */
		if (in)
			*eax = 0xffffffff;
	}
	return (0);
}

INOUT_PORT(pci_cfgdata, CONF1_DATA_PORT+0, IOPORT_F_INOUT, pci_emul_cfgdata);
INOUT_PORT(pci_cfgdata, CONF1_DATA_PORT+1, IOPORT_F_INOUT, pci_emul_cfgdata);
INOUT_PORT(pci_cfgdata, CONF1_DATA_PORT+2, IOPORT_F_INOUT, pci_emul_cfgdata);
INOUT_PORT(pci_cfgdata, CONF1_DATA_PORT+3, IOPORT_F_INOUT, pci_emul_cfgdata);

#ifdef BHYVE_SNAPSHOT
/*
 * Saves/restores PCI device emulated state. Returns 0 on success.
 */
static int
pci_snapshot_pci_dev(struct vm_snapshot_meta *meta)
{
	struct pci_devinst *pi;
	int i;
	int ret;

	pi = meta->dev_data;

	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msi.enabled, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msi.addr, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msi.msg_data, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msi.maxmsgnum, meta, ret, done);

	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msix.enabled, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msix.table_bar, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msix.pba_bar, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msix.table_offset, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msix.table_count, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msix.pba_offset, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msix.pba_size, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(pi->pi_msix.function_mask, meta, ret, done);

	SNAPSHOT_BUF_OR_LEAVE(pi->pi_cfgdata, sizeof(pi->pi_cfgdata),
			      meta, ret, done);

	for (i = 0; i < (int)nitems(pi->pi_bar); i++) {
		SNAPSHOT_VAR_OR_LEAVE(pi->pi_bar[i].type, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(pi->pi_bar[i].size, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(pi->pi_bar[i].addr, meta, ret, done);
	}

	/* Restore MSI-X table. */
	for (i = 0; i < pi->pi_msix.table_count; i++) {
		SNAPSHOT_VAR_OR_LEAVE(pi->pi_msix.table[i].addr,
				      meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(pi->pi_msix.table[i].msg_data,
				      meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(pi->pi_msix.table[i].vector_control,
				      meta, ret, done);
	}

done:
	return (ret);
}

int
pci_snapshot(struct vm_snapshot_meta *meta)
{
	struct pci_devemu *pde;
	struct pci_devinst *pdi;
	int ret;

	assert(meta->dev_name != NULL);

	pdi = meta->dev_data;
	pde = pdi->pi_d;

	if (pde->pe_snapshot == NULL)
		return (ENOTSUP);

	ret = pci_snapshot_pci_dev(meta);
	if (ret == 0)
		ret = (*pde->pe_snapshot)(meta);

	return (ret);
}

int
pci_pause(struct pci_devinst *pdi)
{
	struct pci_devemu *pde = pdi->pi_d;

	if (pde->pe_pause == NULL) {
		/* The pause/resume functionality is optional. */
		return (0);
	}

	return (*pde->pe_pause)(pdi);
}

int
pci_resume(struct pci_devinst *pdi)
{
	struct pci_devemu *pde = pdi->pi_d;

	if (pde->pe_resume == NULL) {
		/* The pause/resume functionality is optional. */
		return (0);
	}

	return (*pde->pe_resume)(pdi);
}
#endif

#define PCI_EMUL_TEST
#ifdef PCI_EMUL_TEST
/*
 * Define a dummy test device
 */
#define DIOSZ	8
#define DMEMSZ	4096
struct pci_emul_dsoftc {
	uint8_t   ioregs[DIOSZ];
	uint8_t	  memregs[2][DMEMSZ];
};

#define	PCI_EMUL_MSI_MSGS	 4
#define	PCI_EMUL_MSIX_MSGS	16

static int
pci_emul_dinit(struct pci_devinst *pi, nvlist_t *nvl __unused)
{
	int error;
	struct pci_emul_dsoftc *sc;

	sc = calloc(1, sizeof(struct pci_emul_dsoftc));

	pi->pi_arg = sc;

	pci_set_cfgdata16(pi, PCIR_DEVICE, 0x0001);
	pci_set_cfgdata16(pi, PCIR_VENDOR, 0x10DD);
	pci_set_cfgdata8(pi, PCIR_CLASS, 0x02);

	error = pci_emul_add_msicap(pi, PCI_EMUL_MSI_MSGS);
	assert(error == 0);

	error = pci_emul_alloc_bar(pi, 0, PCIBAR_IO, DIOSZ);
	assert(error == 0);

	error = pci_emul_alloc_bar(pi, 1, PCIBAR_MEM32, DMEMSZ);
	assert(error == 0);

	error = pci_emul_alloc_bar(pi, 2, PCIBAR_MEM32, DMEMSZ);
	assert(error == 0);

	return (0);
}

static void
pci_emul_diow(struct pci_devinst *pi, int baridx, uint64_t offset, int size,
    uint64_t value)
{
	int i;
	struct pci_emul_dsoftc *sc = pi->pi_arg;

	if (baridx == 0) {
		if (offset + size > DIOSZ) {
			printf("diow: iow too large, offset %ld size %d\n",
			       offset, size);
			return;
		}

		if (size == 1) {
			sc->ioregs[offset] = value & 0xff;
		} else if (size == 2) {
			*(uint16_t *)&sc->ioregs[offset] = value & 0xffff;
		} else if (size == 4) {
			*(uint32_t *)&sc->ioregs[offset] = value;
		} else {
			printf("diow: iow unknown size %d\n", size);
		}

		/*
		 * Special magic value to generate an interrupt
		 */
		if (offset == 4 && size == 4 && pci_msi_enabled(pi))
			pci_generate_msi(pi, value % pci_msi_maxmsgnum(pi));

		if (value == 0xabcdef) {
			for (i = 0; i < pci_msi_maxmsgnum(pi); i++)
				pci_generate_msi(pi, i);
		}
	}

	if (baridx == 1 || baridx == 2) {
		if (offset + size > DMEMSZ) {
			printf("diow: memw too large, offset %ld size %d\n",
			       offset, size);
			return;
		}

		i = baridx - 1;		/* 'memregs' index */

		if (size == 1) {
			sc->memregs[i][offset] = value;
		} else if (size == 2) {
			*(uint16_t *)&sc->memregs[i][offset] = value;
		} else if (size == 4) {
			*(uint32_t *)&sc->memregs[i][offset] = value;
		} else if (size == 8) {
			*(uint64_t *)&sc->memregs[i][offset] = value;
		} else {
			printf("diow: memw unknown size %d\n", size);
		}

		/*
		 * magic interrupt ??
		 */
	}

	if (baridx > 2 || baridx < 0) {
		printf("diow: unknown bar idx %d\n", baridx);
	}
}

static uint64_t
pci_emul_dior(struct pci_devinst *pi, int baridx, uint64_t offset, int size)
{
	struct pci_emul_dsoftc *sc = pi->pi_arg;
	uint32_t value;
	int i;

	if (baridx == 0) {
		if (offset + size > DIOSZ) {
			printf("dior: ior too large, offset %ld size %d\n",
			       offset, size);
			return (0);
		}

		value = 0;
		if (size == 1) {
			value = sc->ioregs[offset];
		} else if (size == 2) {
			value = *(uint16_t *) &sc->ioregs[offset];
		} else if (size == 4) {
			value = *(uint32_t *) &sc->ioregs[offset];
		} else {
			printf("dior: ior unknown size %d\n", size);
		}
	}

	if (baridx == 1 || baridx == 2) {
		if (offset + size > DMEMSZ) {
			printf("dior: memr too large, offset %ld size %d\n",
			       offset, size);
			return (0);
		}

		i = baridx - 1;		/* 'memregs' index */

		if (size == 1) {
			value = sc->memregs[i][offset];
		} else if (size == 2) {
			value = *(uint16_t *) &sc->memregs[i][offset];
		} else if (size == 4) {
			value = *(uint32_t *) &sc->memregs[i][offset];
		} else if (size == 8) {
			value = *(uint64_t *) &sc->memregs[i][offset];
		} else {
			printf("dior: ior unknown size %d\n", size);
		}
	}


	if (baridx > 2 || baridx < 0) {
		printf("dior: unknown bar idx %d\n", baridx);
		return (0);
	}

	return (value);
}

#ifdef BHYVE_SNAPSHOT
struct pci_devinst *
pci_next(const struct pci_devinst *cursor)
{
	unsigned bus = 0, slot = 0, func = 0;
	struct businfo *bi;
	struct slotinfo *si;
	struct funcinfo *fi;

	bus = cursor ? cursor->pi_bus : 0;
	slot = cursor ? cursor->pi_slot : 0;
	func = cursor ? (cursor->pi_func + 1) : 0;

	for (; bus < MAXBUSES; bus++) {
		if ((bi = pci_businfo[bus]) == NULL)
			continue;

		if (slot >= MAXSLOTS)
			slot = 0;

		for (; slot < MAXSLOTS; slot++) {
			si = &bi->slotinfo[slot];
			if (func >= MAXFUNCS)
				func = 0;
			for (; func < MAXFUNCS; func++) {
				fi = &si->si_funcs[func];
				if (fi->fi_devi == NULL)
					continue;

				return (fi->fi_devi);
			}
		}
	}

	return (NULL);
}

static int
pci_emul_snapshot(struct vm_snapshot_meta *meta __unused)
{
	return (0);
}
#endif

static const struct pci_devemu pci_dummy = {
	.pe_emu = "dummy",
	.pe_init = pci_emul_dinit,
	.pe_barwrite = pci_emul_diow,
	.pe_barread = pci_emul_dior,
#ifdef BHYVE_SNAPSHOT
	.pe_snapshot = pci_emul_snapshot,
#endif
};
PCI_EMUL_SET(pci_dummy);

#endif /* PCI_EMUL_TEST */
