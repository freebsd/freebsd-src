/*-
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker_set.h>
#include <sys/errno.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdbool.h>

#include <machine/vmm.h>
#include <vmmapi.h>

#include "bhyverun.h"
#include "inout.h"
#include "legacy_irq.h"
#include "mem.h"
#include "pci_emul.h"

#define CONF1_ADDR_PORT    0x0cf8
#define CONF1_DATA_PORT    0x0cfc

#define CONF1_ENABLE	   0x80000000ul

#define	CFGWRITE(pi,off,val,b)						\
do {									\
	if ((b) == 1) {							\
		pci_set_cfgdata8((pi),(off),(val));			\
	} else if ((b) == 2) {						\
		pci_set_cfgdata16((pi),(off),(val));			\
	} else {							\
		pci_set_cfgdata32((pi),(off),(val));			\
	}								\
} while (0)

#define MAXSLOTS	(PCI_SLOTMAX + 1)
#define	MAXFUNCS	(PCI_FUNCMAX + 1)

static struct slotinfo {
	char	*si_name;
	char	*si_param;
	struct pci_devinst *si_devi;
	int	si_legacy;
} pci_slotinfo[MAXSLOTS][MAXFUNCS];

SET_DECLARE(pci_devemu_set, struct pci_devemu);

static uint64_t pci_emul_iobase;
static uint64_t pci_emul_membase32;
static uint64_t pci_emul_membase64;

#define	PCI_EMUL_IOBASE		0x2000
#define	PCI_EMUL_IOLIMIT	0x10000

#define	PCI_EMUL_MEMLIMIT32	0xE0000000		/* 3.5GB */

#define	PCI_EMUL_MEMBASE64	0xD000000000UL
#define	PCI_EMUL_MEMLIMIT64	0xFD00000000UL

static struct pci_devemu *pci_emul_finddev(char *name);

static int pci_emul_devices;

/*
 * I/O access
 */

/*
 * Slot options are in the form:
 *
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

	fprintf(stderr, "Invalid PCI slot info field \"%s\"\n", aopt);
}

int
pci_parse_slot(char *opt, int legacy)
{
	char *slot, *func, *emul, *config;
	char *str, *cpy;
	int error, snum, fnum;

	error = -1;
	str = cpy = strdup(opt);

        slot = strsep(&str, ",");
        func = NULL;
        if (strchr(slot, ':') != NULL) {
		func = cpy;
		(void) strsep(&func, ":");
        }
	
	emul = strsep(&str, ",");
	config = str;

	if (emul == NULL) {
		pci_parse_slot_usage(opt);
		goto done;
	}

	snum = atoi(slot);
	fnum = func ? atoi(func) : 0;

	if (snum < 0 || snum >= MAXSLOTS || fnum < 0 || fnum >= MAXFUNCS) {
		pci_parse_slot_usage(opt);
		goto done;
	}

	if (pci_slotinfo[snum][fnum].si_name != NULL) {
		fprintf(stderr, "pci slot %d:%d already occupied!\n",
			snum, fnum);
		goto done;
	}

	if (pci_emul_finddev(emul) == NULL) {
		fprintf(stderr, "pci slot %d:%d: unknown device \"%s\"\n",
			snum, fnum, emul);
		goto done;
	}

	error = 0;
	pci_slotinfo[snum][fnum].si_name = emul;
	pci_slotinfo[snum][fnum].si_param = config;
	pci_slotinfo[snum][fnum].si_legacy = legacy;

done:
	if (error)
		free(cpy);

	return (error);
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
	 * table but we also allow 1 byte access to accomodate reads from
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
pci_emul_io_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		    uint32_t *eax, void *arg)
{
	struct pci_devinst *pdi = arg;
	struct pci_devemu *pe = pdi->pi_d;
	uint64_t offset;
	int i;

	for (i = 0; i <= PCI_BARMAX; i++) {
		if (pdi->pi_bar[i].type == PCIBAR_IO &&
		    port >= pdi->pi_bar[i].addr &&
		    port + bytes <= pdi->pi_bar[i].addr + pdi->pi_bar[i].size) {
			offset = port - pdi->pi_bar[i].addr;
			if (in)
				*eax = (*pe->pe_barread)(ctx, vcpu, pdi, i,
							 offset, bytes);
			else
				(*pe->pe_barwrite)(ctx, vcpu, pdi, i, offset,
						   bytes, *eax);
			return (0);
		}
	}
	return (-1);
}

static int
pci_emul_mem_handler(struct vmctx *ctx, int vcpu, int dir, uint64_t addr,
		     int size, uint64_t *val, void *arg1, long arg2)
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

	if (dir == MEM_F_WRITE)
		(*pe->pe_barwrite)(ctx, vcpu, pdi, bidx, offset, size, *val);
	else
		*val = (*pe->pe_barread)(ctx, vcpu, pdi, bidx, offset, size);

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

int
pci_emul_alloc_bar(struct pci_devinst *pdi, int idx, enum pcibar_type type,
		   uint64_t size)
{

	return (pci_emul_alloc_pbar(pdi, idx, 0, type, size));
}

/*
 * Register (or unregister) the MMIO or I/O region associated with the BAR
 * register 'idx' of an emulated pci device.
 */
static void
modify_bar_registration(struct pci_devinst *pi, int idx, int registration)
{
	int error;
	struct inout_port iop;
	struct mem_range mr;

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
	default:
		error = EINVAL;
		break;
	}
	assert(error == 0);
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
update_bar_address(struct  pci_devinst *pi, uint64_t addr, int idx, int type)
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
pci_emul_alloc_pbar(struct pci_devinst *pdi, int idx, uint64_t hostbase,
		    enum pcibar_type type, uint64_t size)
{
	int error;
	uint64_t *baseptr, limit, addr, mask, lobits, bar;

	assert(idx >= 0 && idx <= PCI_BARMAX);

	if ((size & (size - 1)) != 0)
		size = 1UL << flsl(size);	/* round up to a power of 2 */

	/* Enforce minimum BAR sizes required by the PCI standard */
	if (type == PCIBAR_IO) {
		if (size < 4)
			size = 4;
	} else {
		if (size < 16)
			size = 16;
	}

	switch (type) {
	case PCIBAR_NONE:
		baseptr = NULL;
		addr = mask = lobits = 0;
		break;
	case PCIBAR_IO:
		if (hostbase &&
		    pci_slotinfo[pdi->pi_slot][pdi->pi_func].si_legacy) {
			assert(hostbase < PCI_EMUL_IOBASE);
			baseptr = &hostbase;
		} else {
			baseptr = &pci_emul_iobase;
		}
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
		 * number (32MB currently).
		 */
		if (size > 32 * 1024 * 1024) {
			/*
			 * XXX special case for device requiring peer-peer DMA
			 */
			if (size == 0x100000000UL)
				baseptr = &hostbase;
			else
				baseptr = &pci_emul_membase64;
			limit = PCI_EMUL_MEMLIMIT64;
			mask = PCIM_BAR_MEM_BASE;
			lobits = PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_64 |
				 PCIM_BAR_MEM_PREFETCH;
			break;
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
	default:
		printf("pci_emul_alloc_base: invalid bar type %d\n", type);
		assert(0);
	}

	if (baseptr != NULL) {
		error = pci_emul_alloc_resource(baseptr, limit, size, &addr);
		if (error != 0)
			return (error);
	}

	pdi->pi_bar[idx].type = type;
	pdi->pi_bar[idx].addr = addr;
	pdi->pi_bar[idx].size = size;

	/* Initialize the BAR register in config space */
	bar = (addr & mask) | lobits;
	pci_set_cfgdata32(pdi, PCIR_BAR(idx), bar);

	if (type == PCIBAR_MEM64) {
		assert(idx + 1 <= PCI_BARMAX);
		pdi->pi_bar[idx + 1].type = PCIBAR_MEMHI64;
		pci_set_cfgdata32(pdi, PCIR_BAR(idx + 1), bar >> 32);
	}
	
	register_bar(pdi, idx);

	return (0);
}

#define	CAP_START_OFFSET	0x40
static int
pci_emul_add_capability(struct pci_devinst *pi, u_char *capdata, int caplen)
{
	int i, capoff, capid, reallen;
	uint16_t sts;

	static u_char endofcap[4] = {
		PCIY_RESERVED, 0, 0, 0
	};

	assert(caplen > 0 && capdata[0] != PCIY_RESERVED);

	reallen = roundup2(caplen, 4);		/* dword aligned */

	sts = pci_get_cfgdata16(pi, PCIR_STATUS);
	if ((sts & PCIM_STATUS_CAPPRESENT) == 0) {
		capoff = CAP_START_OFFSET;
		pci_set_cfgdata8(pi, PCIR_CAP_PTR, capoff);
		pci_set_cfgdata16(pi, PCIR_STATUS, sts|PCIM_STATUS_CAPPRESENT);
	} else {
		capoff = pci_get_cfgdata8(pi, PCIR_CAP_PTR);
		while (1) {
			assert((capoff & 0x3) == 0);
			capid = pci_get_cfgdata8(pi, capoff);
			if (capid == PCIY_RESERVED)
				break;
			capoff = pci_get_cfgdata8(pi, capoff + 1);
		}
	}

	/* Check if we have enough space */
	if (capoff + reallen + sizeof(endofcap) > PCI_REGMAX + 1)
		return (-1);

	/* Copy the capability */
	for (i = 0; i < caplen; i++)
		pci_set_cfgdata8(pi, capoff + i, capdata[i]);

	/* Set the next capability pointer */
	pci_set_cfgdata8(pi, capoff + 1, capoff + reallen);

	/* Copy of the reserved capability which serves as the end marker */
	for (i = 0; i < sizeof(endofcap); i++)
		pci_set_cfgdata8(pi, capoff + reallen + i, endofcap[i]);

	return (0);
}

static struct pci_devemu *
pci_emul_finddev(char *name)
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
pci_emul_init(struct vmctx *ctx, struct pci_devemu *pde, int slot, int func,
	      char *params)
{
	struct pci_devinst *pdi;
	int err;

	pdi = malloc(sizeof(struct pci_devinst));
	bzero(pdi, sizeof(*pdi));

	pdi->pi_vmctx = ctx;
	pdi->pi_bus = 0;
	pdi->pi_slot = slot;
	pdi->pi_func = func;
	pdi->pi_lintr_pin = -1;
	pdi->pi_d = pde;
	snprintf(pdi->pi_name, PI_NAMESZ, "%s-pci-%d", pde->pe_emu, slot);

	/* Disable legacy interrupts */
	pci_set_cfgdata8(pdi, PCIR_INTLINE, 255);
	pci_set_cfgdata8(pdi, PCIR_INTPIN, 0);

	pci_set_cfgdata8(pdi, PCIR_COMMAND,
		    PCIM_CMD_PORTEN | PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);

	err = (*pde->pe_init)(ctx, pdi, params);
	if (err != 0) {
		free(pdi);
	} else {
		pci_emul_devices++;
		pci_slotinfo[slot][func].si_devi = pdi;
	}

	return (err);
}

void
pci_populate_msicap(struct msicap *msicap, int msgnum, int nextptr)
{
	int mmc;

	CTASSERT(sizeof(struct msicap) == 14);

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
		     uint32_t msix_tab_size, int nextptr)
{
	CTASSERT(sizeof(struct msixcap) == 12);

	assert(msix_tab_size % 4096 == 0);

	bzero(msixcap, sizeof(struct msixcap));
	msixcap->capid = PCIY_MSIX;
	msixcap->nextptr = nextptr;

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
	pi->pi_msix.table = malloc(table_size);
	bzero(pi->pi_msix.table, table_size);

	/* set mask bit of vector control register */
	for (i = 0; i < table_entries; i++)
		pi->pi_msix.table[i].vector_control |= PCIM_MSIX_VCTRL_MASK;
}

int
pci_emul_add_msixcap(struct pci_devinst *pi, int msgnum, int barnum)
{
	uint16_t pba_index;
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

	/* calculate the MMIO size required for MSI-X PBA */
	pba_index = (msgnum - 1) / (PBA_TABLE_ENTRY_SIZE * 8);
	pi->pi_msix.pba_size = (pba_index + 1) * PBA_TABLE_ENTRY_SIZE;

	pci_msix_table_init(pi, msgnum);

	pci_populate_msixcap(&msixcap, msgnum, barnum, tab_size, 0);

	/* allocate memory for MSI-X Table and PBA */
	pci_emul_alloc_bar(pi, barnum, PCIBAR_MEM32,
				tab_size + pi->pi_msix.pba_size);

	return (pci_emul_add_capability(pi, (u_char *)&msixcap,
					sizeof(msixcap)));
}

void
msixcap_cfgwrite(struct pci_devinst *pi, int capoff, int offset,
		 int bytes, uint32_t val)
{
	uint16_t msgctrl, rwmask;
	int off, table_bar;
	
	off = offset - capoff;
	table_bar = pi->pi_msix.table_bar;
	/* Message Control Register */
	if (off == 2 && bytes == 2) {
		rwmask = PCIM_MSIXCTRL_MSIX_ENABLE | PCIM_MSIXCTRL_FUNCTION_MASK;
		msgctrl = pci_get_cfgdata16(pi, offset);
		msgctrl &= ~rwmask;
		msgctrl |= val & rwmask;
		val = msgctrl;

		pi->pi_msix.enabled = val & PCIM_MSIXCTRL_MSIX_ENABLE;
		pi->pi_msix.function_mask = val & PCIM_MSIXCTRL_FUNCTION_MASK;
	} 
	
	CFGWRITE(pi, offset, val, bytes);
}

void
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
	}

	CFGWRITE(pi, offset, val, bytes);
}

void
pciecap_cfgwrite(struct pci_devinst *pi, int capoff, int offset,
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

	CTASSERT(sizeof(struct pciecap) == 60);

	if (type != PCIEM_TYPE_ROOT_PORT)
		return (-1);

	bzero(&pciecap, sizeof(pciecap));

	pciecap.capid = PCIY_EXPRESS;
	pciecap.pcie_capabilities = PCIECAP_VERSION | PCIEM_TYPE_ROOT_PORT;
	pciecap.link_capabilities = 0x411;	/* gen1, x1 */
	pciecap.link_status = 0x11;		/* gen1, x1 */

	err = pci_emul_add_capability(pi, (u_char *)&pciecap, sizeof(pciecap));
	return (err);
}

/*
 * This function assumes that 'coff' is in the capabilities region of the
 * config space.
 */
static void
pci_emul_capwrite(struct pci_devinst *pi, int offset, int bytes, uint32_t val)
{
	int capid;
	uint8_t capoff, nextoff;

	/* Do not allow un-aligned writes */
	if ((offset & (bytes - 1)) != 0)
		return;

	/* Find the capability that we want to update */
	capoff = CAP_START_OFFSET;
	while (1) {
		capid = pci_get_cfgdata8(pi, capoff);
		if (capid == PCIY_RESERVED)
			break;

		nextoff = pci_get_cfgdata8(pi, capoff + 1);
		if (offset >= capoff && offset < nextoff)
			break;

		capoff = nextoff;
	}
	assert(offset >= capoff);

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
	int found;
	uint16_t sts;
	uint8_t capid, lastoff;

	found = 0;
	sts = pci_get_cfgdata16(pi, PCIR_STATUS);
	if ((sts & PCIM_STATUS_CAPPRESENT) != 0) {
		lastoff = pci_get_cfgdata8(pi, PCIR_CAP_PTR);
		while (1) {
			assert((lastoff & 0x3) == 0);
			capid = pci_get_cfgdata8(pi, lastoff);
			if (capid == PCIY_RESERVED)
				break;
			lastoff = pci_get_cfgdata8(pi, lastoff + 1);
		}
		if (offset >= CAP_START_OFFSET && offset <= lastoff)
			found = 1;
	}
	return (found);
}

static int
pci_emul_fallback_handler(struct vmctx *ctx, int vcpu, int dir, uint64_t addr,
			  int size, uint64_t *val, void *arg1, long arg2)
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

int
init_pci(struct vmctx *ctx)
{
	struct mem_range memp;
	struct pci_devemu *pde;
	struct slotinfo *si;
	size_t lowmem;
	int slot, func;
	int error;

	pci_emul_iobase = PCI_EMUL_IOBASE;
	pci_emul_membase32 = vm_get_lowmem_limit(ctx);
	pci_emul_membase64 = PCI_EMUL_MEMBASE64;

	for (slot = 0; slot < MAXSLOTS; slot++) {
		for (func = 0; func < MAXFUNCS; func++) {
			si = &pci_slotinfo[slot][func];
			if (si->si_name != NULL) {
				pde = pci_emul_finddev(si->si_name);
				assert(pde != NULL);
				error = pci_emul_init(ctx, pde, slot, func,
					    si->si_param);
				if (error)
					return (error);
			}
		}
	}

	/*
	 * The guest physical memory map looks like the following:
	 * [0,		    lowmem)		guest system memory
	 * [lowmem,	    lowmem_limit)	memory hole (may be absent)
	 * [lowmem_limit,   4GB)		PCI hole (32-bit BAR allocation)
	 * [4GB,	    4GB + highmem)
	 *
	 * Accesses to memory addresses that are not allocated to system
	 * memory or PCI devices return 0xff's.
	 */
	error = vm_get_memory_seg(ctx, 0, &lowmem, NULL);
	assert(error == 0);

	memset(&memp, 0, sizeof(struct mem_range));
	memp.name = "PCI hole";
	memp.flags = MEM_F_RW;
	memp.base = lowmem;
	memp.size = (4ULL * 1024 * 1024 * 1024) - lowmem;
	memp.handler = pci_emul_fallback_handler;

	error = register_mem_fallback(&memp);
	assert(error == 0);

	return (0);
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

int
pci_is_legacy(struct pci_devinst *pi)
{

	return (pci_slotinfo[pi->pi_slot][pi->pi_func].si_legacy);
}

int
pci_lintr_request(struct pci_devinst *pi, int req)
{
	int irq;

	irq = legacy_irq_alloc(req);
	if (irq < 0)
		return (-1);

	pi->pi_lintr_pin = irq;
	pci_set_cfgdata8(pi, PCIR_INTLINE, irq);
	pci_set_cfgdata8(pi, PCIR_INTPIN, 1);
	return (0);
}

void
pci_lintr_assert(struct pci_devinst *pi)
{

	assert(pi->pi_lintr_pin >= 0);

	if (pi->pi_lintr_state == 0) {
		pi->pi_lintr_state = 1;
		vm_ioapic_assert_irq(pi->pi_vmctx, pi->pi_lintr_pin);
	}
}

void
pci_lintr_deassert(struct pci_devinst *pi)
{

	assert(pi->pi_lintr_pin >= 0);

	if (pi->pi_lintr_state == 1) {
		pi->pi_lintr_state = 0;
		vm_ioapic_deassert_irq(pi->pi_vmctx, pi->pi_lintr_pin);
	}
}

/*
 * Return 1 if the emulated device in 'slot' is a multi-function device.
 * Return 0 otherwise.
 */
static int
pci_emul_is_mfdev(int slot)
{
	int f, numfuncs;

	numfuncs = 0;
	for (f = 0; f < MAXFUNCS; f++) {
		if (pci_slotinfo[slot][f].si_devi != NULL) {
			numfuncs++;
		}
	}
	return (numfuncs > 1);
}

/*
 * Ensure that the PCIM_MFDEV bit is properly set (or unset) depending on
 * whether or not is a multi-function being emulated in the pci 'slot'.
 */
static void
pci_emul_hdrtype_fixup(int slot, int off, int bytes, uint32_t *rv)
{
	int mfdev;

	if (off <= PCIR_HDRTYPE && off + bytes > PCIR_HDRTYPE) {
		mfdev = pci_emul_is_mfdev(slot);
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

static int cfgbus, cfgslot, cfgfunc, cfgoff;

static int
pci_emul_cfgaddr(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		 uint32_t *eax, void *arg)
{
	uint32_t x;

	if (bytes != 4) {
		if (in)
			*eax = (bytes == 2) ? 0xffff : 0xff;
		return (0);
	}

	if (in) {
		x = (cfgbus << 16) |
		    (cfgslot << 11) |
		    (cfgfunc << 8) |
		    cfgoff;
		*eax = x | CONF1_ENABLE;
	} else {
		x = *eax;
		cfgoff = x & PCI_REGMAX;
		cfgfunc = (x >> 8) & PCI_FUNCMAX;
		cfgslot = (x >> 11) & PCI_SLOTMAX;
		cfgbus = (x >> 16) & PCI_BUSMAX;
	}

	return (0);
}
INOUT_PORT(pci_cfgaddr, CONF1_ADDR_PORT, IOPORT_F_INOUT, pci_emul_cfgaddr);

static uint32_t
bits_changed(uint32_t old, uint32_t new, uint32_t mask)
{

	return ((old ^ new) & mask);
}

static void
pci_emul_cmdwrite(struct pci_devinst *pi, uint32_t new, int bytes)
{
	int i;
	uint16_t old;

	/*
	 * The command register is at an offset of 4 bytes and thus the
	 * guest could write 1, 2 or 4 bytes starting at this offset.
	 */

	old = pci_get_cfgdata16(pi, PCIR_COMMAND);	/* stash old value */
	CFGWRITE(pi, PCIR_COMMAND, new, bytes);		/* update config */
	new = pci_get_cfgdata16(pi, PCIR_COMMAND);	/* get updated value */

	/*
	 * If the MMIO or I/O address space decoding has changed then
	 * register/unregister all BARs that decode that address space.
	 */
	for (i = 0; i <= PCI_BARMAX; i++) {
		switch (pi->pi_bar[i].type) {
			case PCIBAR_NONE:
			case PCIBAR_MEMHI64:
				break;
			case PCIBAR_IO:
				/* I/O address space decoding changed? */
				if (bits_changed(old, new, PCIM_CMD_PORTEN)) {
					if (porten(pi))
						register_bar(pi, i);
					else
						unregister_bar(pi, i);
				}
				break;
			case PCIBAR_MEM32:
			case PCIBAR_MEM64:
				/* MMIO address space decoding changed? */
				if (bits_changed(old, new, PCIM_CMD_MEMEN)) {
					if (memen(pi))
						register_bar(pi, i);
					else
						unregister_bar(pi, i);
				}
				break; 
			default:
				assert(0); 
		}
	}
}	

static int
pci_emul_cfgdata(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		 uint32_t *eax, void *arg)
{
	struct pci_devinst *pi;
	struct pci_devemu *pe;
	int coff, idx, needcfg;
	uint64_t addr, bar, mask;

	assert(bytes == 1 || bytes == 2 || bytes == 4);
	
	if (cfgbus == 0)
		pi = pci_slotinfo[cfgslot][cfgfunc].si_devi;
	else
		pi = NULL;

	coff = cfgoff + (port - CONF1_DATA_PORT);

#if 0
	printf("pcicfg-%s from 0x%0x of %d bytes (%d/%d/%d)\n\r",
		in ? "read" : "write", coff, bytes, cfgbus, cfgslot, cfgfunc);
#endif

	/*
	 * Just return if there is no device at this cfgslot:cfgfunc or
	 * if the guest is doing an un-aligned access
	 */
	if (pi == NULL || (coff & (bytes - 1)) != 0) {
		if (in)
			*eax = 0xffffffff;
		return (0);
	}

	pe = pi->pi_d;

	/*
	 * Config read
	 */
	if (in) {
		/* Let the device emulation override the default handler */
		if (pe->pe_cfgread != NULL) {
			needcfg = pe->pe_cfgread(ctx, vcpu, pi,
						    coff, bytes, eax);
		} else {
			needcfg = 1;
		}

		if (needcfg) {
			if (bytes == 1)
				*eax = pci_get_cfgdata8(pi, coff);
			else if (bytes == 2)
				*eax = pci_get_cfgdata16(pi, coff);
			else
				*eax = pci_get_cfgdata32(pi, coff);
		}

		pci_emul_hdrtype_fixup(cfgslot, coff, bytes, eax);
	} else {
		/* Let the device emulation override the default handler */
		if (pe->pe_cfgwrite != NULL &&
		    (*pe->pe_cfgwrite)(ctx, vcpu, pi, coff, bytes, *eax) == 0)
			return (0);

		/*
		 * Special handling for write to BAR registers
		 */
		if (coff >= PCIR_BAR(0) && coff < PCIR_BAR(PCI_BARMAX + 1)) {
			/*
			 * Ignore writes to BAR registers that are not
			 * 4-byte aligned.
			 */
			if (bytes != 4 || (coff & 0x3) != 0)
				return (0);
			idx = (coff - PCIR_BAR(0)) / 4;
			mask = ~(pi->pi_bar[idx].size - 1);
			switch (pi->pi_bar[idx].type) {
			case PCIBAR_NONE:
				pi->pi_bar[idx].addr = bar = 0;
				break;
			case PCIBAR_IO:
				addr = *eax & mask;
				addr &= 0xffff;
				bar = addr | PCIM_BAR_IO_SPACE;
				/*
				 * Register the new BAR value for interception
				 */
				if (addr != pi->pi_bar[idx].addr) {
					update_bar_address(pi, addr, idx,
							   PCIBAR_IO);
				}
				break;
			case PCIBAR_MEM32:
				addr = bar = *eax & mask;
				bar |= PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_32;
				if (addr != pi->pi_bar[idx].addr) {
					update_bar_address(pi, addr, idx,
							   PCIBAR_MEM32);
				}
				break;
			case PCIBAR_MEM64:
				addr = bar = *eax & mask;
				bar |= PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_64 |
				       PCIM_BAR_MEM_PREFETCH;
				if (addr != (uint32_t)pi->pi_bar[idx].addr) {
					update_bar_address(pi, addr, idx,
							   PCIBAR_MEM64);
				}
				break;
			case PCIBAR_MEMHI64:
				mask = ~(pi->pi_bar[idx - 1].size - 1);
				addr = ((uint64_t)*eax << 32) & mask;
				bar = addr >> 32;
				if (bar != pi->pi_bar[idx - 1].addr >> 32) {
					update_bar_address(pi, addr, idx - 1,
							   PCIBAR_MEMHI64);
				}
				break;
			default:
				assert(0);
			}
			pci_set_cfgdata32(pi, coff, bar);

		} else if (pci_emul_iscap(pi, coff)) {
			pci_emul_capwrite(pi, coff, bytes, *eax);
		} else if (coff == PCIR_COMMAND) {
			pci_emul_cmdwrite(pi, *eax, bytes);
		} else {
			CFGWRITE(pi, coff, *eax, bytes);
		}
	}

	return (0);
}

INOUT_PORT(pci_cfgdata, CONF1_DATA_PORT+0, IOPORT_F_INOUT, pci_emul_cfgdata);
INOUT_PORT(pci_cfgdata, CONF1_DATA_PORT+1, IOPORT_F_INOUT, pci_emul_cfgdata);
INOUT_PORT(pci_cfgdata, CONF1_DATA_PORT+2, IOPORT_F_INOUT, pci_emul_cfgdata);
INOUT_PORT(pci_cfgdata, CONF1_DATA_PORT+3, IOPORT_F_INOUT, pci_emul_cfgdata);

/*
 * I/O ports to configure PCI IRQ routing. We ignore all writes to it.
 */
static int
pci_irq_port_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		     uint32_t *eax, void *arg)
{
	assert(in == 0);
	return (0);
}
INOUT_PORT(pci_irq, 0xC00, IOPORT_F_OUT, pci_irq_port_handler);
INOUT_PORT(pci_irq, 0xC01, IOPORT_F_OUT, pci_irq_port_handler);

#define PCI_EMUL_TEST
#ifdef PCI_EMUL_TEST
/*
 * Define a dummy test device
 */
#define DIOSZ	20
#define DMEMSZ	4096
struct pci_emul_dsoftc {
	uint8_t   ioregs[DIOSZ];
	uint8_t	  memregs[DMEMSZ];
};

#define	PCI_EMUL_MSI_MSGS	 4
#define	PCI_EMUL_MSIX_MSGS	16

static int
pci_emul_dinit(struct vmctx *ctx, struct pci_devinst *pi, char *opts)
{
	int error;
	struct pci_emul_dsoftc *sc;

	sc = malloc(sizeof(struct pci_emul_dsoftc));
	memset(sc, 0, sizeof(struct pci_emul_dsoftc));

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

	return (0);
}

static void
pci_emul_diow(struct vmctx *ctx, int vcpu, struct pci_devinst *pi, int baridx,
	      uint64_t offset, int size, uint64_t value)
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

	if (baridx == 1) {
		if (offset + size > DMEMSZ) {
			printf("diow: memw too large, offset %ld size %d\n",
			       offset, size);
			return;
		}

		if (size == 1) {
			sc->memregs[offset] = value;
		} else if (size == 2) {
			*(uint16_t *)&sc->memregs[offset] = value;
		} else if (size == 4) {
			*(uint32_t *)&sc->memregs[offset] = value;
		} else if (size == 8) {
			*(uint64_t *)&sc->memregs[offset] = value;
		} else {
			printf("diow: memw unknown size %d\n", size);
		}
		
		/*
		 * magic interrupt ??
		 */
	}

	if (baridx > 1) {
		printf("diow: unknown bar idx %d\n", baridx);
	}
}

static uint64_t
pci_emul_dior(struct vmctx *ctx, int vcpu, struct pci_devinst *pi, int baridx,
	      uint64_t offset, int size)
{
	struct pci_emul_dsoftc *sc = pi->pi_arg;
	uint32_t value;

	if (baridx == 0) {
		if (offset + size > DIOSZ) {
			printf("dior: ior too large, offset %ld size %d\n",
			       offset, size);
			return (0);
		}
	
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
	
	if (baridx == 1) {
		if (offset + size > DMEMSZ) {
			printf("dior: memr too large, offset %ld size %d\n",
			       offset, size);
			return (0);
		}
	
		if (size == 1) {
			value = sc->memregs[offset];
		} else if (size == 2) {
			value = *(uint16_t *) &sc->memregs[offset];
		} else if (size == 4) {
			value = *(uint32_t *) &sc->memregs[offset];
		} else if (size == 8) {
			value = *(uint64_t *) &sc->memregs[offset];
		} else {
			printf("dior: ior unknown size %d\n", size);
		}
	}


	if (baridx > 1) {
		printf("dior: unknown bar idx %d\n", baridx);
		return (0);
	}

	return (value);
}

struct pci_devemu pci_dummy = {
	.pe_emu = "dummy",
	.pe_init = pci_emul_dinit,
	.pe_barwrite = pci_emul_diow,
	.pe_barread = pci_emul_dior
};
PCI_EMUL_SET(pci_dummy);

#endif /* PCI_EMUL_TEST */
