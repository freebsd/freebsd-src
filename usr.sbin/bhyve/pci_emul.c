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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#include <machine/vmm.h>
#include <vmmapi.h>

#include "fbsdrun.h"
#include "inout.h"
#include "pci_emul.h"
#include "instruction_emul.h"
#include "ioapic.h"

#define CONF1_ADDR_PORT    0x0cf8
#define CONF1_DATA_PORT    0x0cfc

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

#define MAXSLOTS	32

static struct slotinfo {
	char	*si_name;
	char	*si_param;
	struct pci_devinst *si_devi;
	int	si_titled;
	int	si_pslot;
	char	si_prefix;
	char	si_suffix;
	int	si_legacy;
} pci_slotinfo[MAXSLOTS];

/*
 * Used to keep track of legacy interrupt owners/requestors
 */
#define NLIRQ		16

static struct lirqinfo {
	int	li_generic;
	int	li_acount;
	struct pci_devinst *li_owner;	/* XXX should be a list */
} lirq[NLIRQ];

/*
 * NetApp specific:
 * struct used to build an in-core OEM table to supply device names
 * to driver instances
 */
static struct mptable_pci_devnames {
#define MPT_HDR_BASE	0
#define MPT_HDR_NAME	2
	uint16_t  md_hdrtype;
	uint16_t  md_entries;
	uint16_t  md_cksum;
	uint16_t  md_pad;
#define MPT_NTAP_SIG	\
	((uint32_t)(('P' << 24) | ('A' << 16) | ('T' << 8) | 'N'))
	uint32_t  md_sig;
	uint32_t  md_rsvd;
	struct mptable_pci_slotinfo {
		uint16_t mds_type;
		uint16_t mds_phys_slot;
		uint8_t  mds_bus;
		uint8_t  mds_slot;
		uint8_t  mds_func;
		uint8_t  mds_pad;
		uint16_t mds_vid;
		uint16_t mds_did;
		uint8_t  mds_suffix[4];
		uint8_t  mds_prefix[4];
		uint32_t mds_rsvd[3];
	} md_slotinfo[MAXSLOTS];
} pci_devnames;

SET_DECLARE(pci_devemu_set, struct pci_devemu);

static uint64_t pci_emul_iobase;
static uint64_t pci_emul_membase32;
static uint64_t pci_emul_membase64;

#define	PCI_EMUL_IOBASE		0x2000
#define	PCI_EMUL_IOLIMIT	0x10000

#define	PCI_EMUL_MEMBASE32	(lomem_sz)
#define	PCI_EMUL_MEMLIMIT32	0xE0000000		/* 3.5GB */

#define	PCI_EMUL_MEMBASE64	0xD000000000UL
#define	PCI_EMUL_MEMLIMIT64	0xFD00000000UL

static int pci_emul_devices;
static int devname_elems;

/*
 * I/O access
 */

/*
 * Slot options are in the form:
 *
 *  <slot>,<emul>[,<config>]
 *
 *  slot is 0..31
 *  emul is a string describing the type of PCI device e.g. virtio-net
 *  config is an optional string, depending on the device, that can be
 *  used for configuration.
 *   Examples are:
 *     1,virtio-net,tap0
 *     3,dummy
 */
static void
pci_parse_slot_usage(char *aopt)
{
	printf("Invalid PCI slot info field \"%s\"\n", aopt);
	free(aopt);
}

void
pci_parse_slot(char *opt, int legacy)
{
	char *slot, *emul, *config;
	char *str, *cpy;
	int snum;

	str = cpy = strdup(opt);
	config = NULL;

	slot = strsep(&str, ",");
	emul = strsep(&str, ",");
	if (str != NULL) {
		config = strsep(&str, ",");
	}

	if (emul == NULL) {
		pci_parse_slot_usage(cpy);
		return;
	}

	snum = 255;
	snum = atoi(slot);
	if (snum < 0 || snum >= MAXSLOTS) {
		pci_parse_slot_usage(cpy);
	} else {
		pci_slotinfo[snum].si_name = emul;
		pci_slotinfo[snum].si_param = config;
		pci_slotinfo[snum].si_legacy = legacy;
	}
}


/*
 *
 * PCI MPTable names are of the form:
 *
 *  <slot>,[prefix]<digit><suffix>
 *
 *  .. with <prefix> an alphabetic char, <digit> a 1 or 2-digit string,
 * and <suffix> a single char.
 *
 *  Examples:
 *    1,e0c
 *    4,e0P
 *    6,43a
 *    7,0f
 *    10,1
 *    12,e0M
 *    2,12a
 *
 *  Note that this is NetApp-specific, but is ignored on other o/s's.
 */
static void
pci_parse_name_usage(char *aopt)
{
	printf("Invalid PCI slot name field \"%s\"\n", aopt);
}

void
pci_parse_name(char *opt)
{
	char csnum[4];
	char *namestr;
	char *slotend;
	char prefix, suffix;
	int i;
	int pslot;
	int snum;

	pslot = -1;
	prefix = suffix = 0;
	slotend = strchr(opt, ',');

	/*
	 * A comma must be present, and can't be the first character
	 * or no slot would be present. Also, the slot number can't be
	 * more than 2 characters.
	 */
	if (slotend == NULL || slotend == opt || (slotend - opt > 2)) {
		pci_parse_name_usage(opt);
		return;
	}

	for (i = 0; i < (slotend - opt); i++) {
		csnum[i] = opt[i];
	}
	csnum[i] = '\0';
	
	snum = 255;
	snum = atoi(csnum);
	if (snum < 0 || snum >= MAXSLOTS) {
		pci_parse_name_usage(opt);
		return;
	}

	namestr = slotend + 1;

	if (strlen(namestr) > 3) {
		pci_parse_name_usage(opt);
		return;
	}

	if (isalpha(*namestr)) {
		prefix = *namestr++;
	}

	if (!isdigit(*namestr)) {
		pci_parse_name_usage(opt);
	} else {
		pslot = *namestr++ - '0';
		if (isnumber(*namestr)) {
			pslot = 10*pslot + *namestr++ - '0';
			
		}
		if (isalpha(*namestr) && *(namestr + 1) == 0) {
			suffix = *namestr;
			pci_slotinfo[snum].si_titled = 1;
			pci_slotinfo[snum].si_pslot = pslot;
			pci_slotinfo[snum].si_prefix = prefix;
			pci_slotinfo[snum].si_suffix = suffix;
			
		} else {
			pci_parse_name_usage(opt);
		}
	}
}

static void
pci_add_mptable_name(struct slotinfo *si)
{
	struct mptable_pci_slotinfo *ms;

	/*
	 * If naming information has been supplied for this slot, populate
	 * the next available mptable OEM entry
	 */
	if (si->si_titled) {
		ms = &pci_devnames.md_slotinfo[devname_elems];

		ms->mds_type = MPT_HDR_NAME;
		ms->mds_phys_slot = si->si_pslot;
		ms->mds_bus = si->si_devi->pi_bus;
		ms->mds_slot = si->si_devi->pi_slot;
		ms->mds_func = si->si_devi->pi_func;
		ms->mds_vid = pci_get_cfgdata16(si->si_devi, PCIR_VENDOR);
		ms->mds_did = pci_get_cfgdata16(si->si_devi, PCIR_DEVICE);
		ms->mds_suffix[0] = si->si_suffix;
		ms->mds_prefix[0] = si->si_prefix;
		
		devname_elems++;
	}
}

static void
pci_finish_mptable_names(void)
{
	int size;

	if (devname_elems) {
		pci_devnames.md_hdrtype = MPT_HDR_BASE;
		pci_devnames.md_entries = devname_elems;
		pci_devnames.md_cksum = 0; /* XXX */
		pci_devnames.md_sig = MPT_NTAP_SIG;

		size = (uintptr_t)&pci_devnames.md_slotinfo[devname_elems] -
			(uintptr_t)&pci_devnames;

		fbsdrun_add_oemtbl(&pci_devnames, size);
	}
}

static int
pci_emul_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		 uint32_t *eax, void *arg)
{
	struct pci_devinst *pdi = arg;
	struct pci_devemu *pe = pdi->pi_d;
	int offset, i;

	for (i = 0; i <= PCI_BARMAX; i++) {
		if (pdi->pi_bar[i].type == PCIBAR_IO &&
		    port >= pdi->pi_bar[i].addr &&
		    port + bytes <= pdi->pi_bar[i].addr + pdi->pi_bar[i].size) {
			offset = port - pdi->pi_bar[i].addr;
			if (in)
				*eax = (*pe->pe_ior)(pdi, i, offset, bytes);
			else
				(*pe->pe_iow)(pdi, i, offset, bytes, *eax);
			return (0);
		}
	}
	return (-1);
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
pci_emul_alloc_bar(struct pci_devinst *pdi, int idx, uint64_t hostbase,
		   enum pcibar_type type, uint64_t size)
{
	int i, error;
	uint64_t *baseptr, limit, addr, mask, lobits, bar;
	struct inout_port iop;

	assert(idx >= 0 && idx <= PCI_BARMAX);

	if ((size & (size - 1)) != 0)
		size = 1UL << flsl(size);	/* round up to a power of 2 */

	switch (type) {
	case PCIBAR_NONE:
		baseptr = NULL;
		addr = mask = lobits = 0;
		break;
	case PCIBAR_IO:
		if (hostbase && pci_slotinfo[pdi->pi_slot].si_legacy) {
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
		}
		/* fallthrough */
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
	
	/* add a handler to intercept accesses to the I/O bar */
	if (type == PCIBAR_IO) {
		iop.name = pdi->pi_name;
		iop.flags = IOPORT_F_INOUT;
		iop.handler = pci_emul_handler;
		iop.arg = pdi;

		for (i = 0; i < size; i++) {
			iop.port = addr + i;
			register_inout(&iop);
		}
	}

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

static void
pci_emul_init(struct vmctx *ctx, struct pci_devemu *pde, int slot, char *params)
{
	struct pci_devinst *pdi;
	pdi = malloc(sizeof(struct pci_devinst));
	bzero(pdi, sizeof(*pdi));

	pdi->pi_vmctx = ctx;
	pdi->pi_bus = 0;
	pdi->pi_slot = slot;
	pdi->pi_func = 0;
	pdi->pi_d = pde;
	snprintf(pdi->pi_name, PI_NAMESZ, "%s-pci-%d", pde->pe_emu, slot);

	/* Disable legacy interrupts */
	pci_set_cfgdata8(pdi, PCIR_INTLINE, 255);
	pci_set_cfgdata8(pdi, PCIR_INTPIN, 0);

	pci_set_cfgdata8(pdi, PCIR_COMMAND,
		    PCIM_CMD_PORTEN | PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);

	if ((*pde->pe_init)(ctx, pdi, params) != 0) {
		free(pdi);
	} else {
		pci_emul_devices++;
		pci_slotinfo[slot].si_devi = pdi;
	}	
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

		/*
		 * XXX check delivery mode, destination mode etc
		 */
		mme = msgctrl & PCIM_MSICTRL_MME_MASK;
		pi->pi_msi.enabled = msgctrl & PCIM_MSICTRL_MSI_ENABLE ? 1 : 0;
		if (pi->pi_msi.enabled) {
			pi->pi_msi.cpu = (addrlo >> 12) & 0xff;
			pi->pi_msi.vector = msgdata & 0xff;
			pi->pi_msi.msgnum = 1 << (mme >> 4);
		} else {
			pi->pi_msi.cpu = 0;
			pi->pi_msi.vector = 0;
			pi->pi_msi.msgnum = 0;
		}
	}

	CFGWRITE(pi, offset, val, bytes);
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
	 * Capability ID and Next Capability Pointer are readonly
	 */
	if (offset == capoff || offset == capoff + 1)
		return;

	switch (capid) {
	case PCIY_MSI:
		msicap_cfgwrite(pi, capoff, offset, bytes, val);
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

void
init_pci(struct vmctx *ctx)
{
	struct pci_devemu *pde;
	struct slotinfo *si;
	int i;

	pci_emul_iobase = PCI_EMUL_IOBASE;
	pci_emul_membase32 = PCI_EMUL_MEMBASE32;
	pci_emul_membase64 = PCI_EMUL_MEMBASE64;

	si = pci_slotinfo;

	for (i = 0; i < MAXSLOTS; i++, si++) {
		if (si->si_name != NULL) {
			pde = pci_emul_finddev(si->si_name);
			if (pde != NULL) {
				pci_emul_init(ctx, pde, i, si->si_param);
				pci_add_mptable_name(si);
			}
		}
	}
	pci_finish_mptable_names();

	/*
	 * Allow ISA IRQs 5,10,11,12, and 15 to be available for
	 * generic use
	 */
	lirq[5].li_generic = 1;
	lirq[10].li_generic = 1;
	lirq[11].li_generic = 1;
	lirq[12].li_generic = 1;
	lirq[15].li_generic = 1;
}

int
pci_msi_enabled(struct pci_devinst *pi)
{
	return (pi->pi_msi.enabled);
}

int
pci_msi_msgnum(struct pci_devinst *pi)
{
	if (pi->pi_msi.enabled)
		return (pi->pi_msi.msgnum);
	else
		return (0);
}

void
pci_generate_msi(struct pci_devinst *pi, int msg)
{

	if (pci_msi_enabled(pi) && msg < pci_msi_msgnum(pi)) {
		vm_lapic_irq(pi->pi_vmctx,
			     pi->pi_msi.cpu,
			     pi->pi_msi.vector + msg);
	}
}

int
pci_is_legacy(struct pci_devinst *pi)
{

	return (pci_slotinfo[pi->pi_slot].si_legacy);
}

static int
pci_lintr_alloc(struct pci_devinst *pi, int vec)
{
	int i;

	assert(vec < NLIRQ);

	if (vec == -1) {
		for (i = 0; i < NLIRQ; i++) {
			if (lirq[i].li_generic &&
			    lirq[i].li_owner == NULL) {
				vec = i;
				break;
			}
		}
	} else {
		if (lirq[vec].li_owner != NULL) {
			vec = -1;
		}
	}
	assert(vec != -1);

	lirq[vec].li_owner = pi;
	pi->pi_lintr_pin = vec;

	return (vec);
}

int
pci_lintr_request(struct pci_devinst *pi, int vec)
{

	vec = pci_lintr_alloc(pi, vec);
	pci_set_cfgdata8(pi, PCIR_INTLINE, vec);
	pci_set_cfgdata8(pi, PCIR_INTPIN, 1);
	return (0);
}

void
pci_lintr_assert(struct pci_devinst *pi)
{

	assert(pi->pi_lintr_pin);
	ioapic_assert_pin(pi->pi_vmctx, pi->pi_lintr_pin);
}

void
pci_lintr_deassert(struct pci_devinst *pi)
{

	assert(pi->pi_lintr_pin);
	ioapic_deassert_pin(pi->pi_vmctx, pi->pi_lintr_pin);
}



static int cfgbus, cfgslot, cfgfunc, cfgoff;

static int
pci_emul_cfgaddr(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		 uint32_t *eax, void *arg)
{
	uint32_t x;

	assert(!in);

	if (bytes != 4)
		return (-1);

	x = *eax;
	cfgoff = x & PCI_REGMAX;
	cfgfunc = (x >> 8) & PCI_FUNCMAX;
	cfgslot = (x >> 11) & PCI_SLOTMAX;
	cfgbus = (x >> 16) & PCI_BUSMAX;

	return (0);
}
INOUT_PORT(pci_cfgaddr, CONF1_ADDR_PORT, IOPORT_F_OUT, pci_emul_cfgaddr);

static int
pci_emul_cfgdata(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		 uint32_t *eax, void *arg)
{
	struct pci_devinst *pi;
	struct pci_devemu *pe;
	int coff, idx;
	uint64_t mask, bar;

	assert(bytes == 1 || bytes == 2 || bytes == 4);
	
	pi = pci_slotinfo[cfgslot].si_devi;
	coff = cfgoff + (port - CONF1_DATA_PORT);

#if 0
	printf("pcicfg-%s from 0x%0x of %d bytes (%d/%d/%d)\n\r",
		in ? "read" : "write", coff, bytes, cfgbus, cfgslot, cfgfunc);
#endif

	if (pi == NULL || cfgfunc != 0) {
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
		if (pe->pe_cfgread != NULL &&
		    (*pe->pe_cfgread)(ctx, vcpu, pi, coff, bytes, eax) == 0)
			return (0);

		if (bytes == 1)
			*eax = pci_get_cfgdata8(pi, coff);
		else if (bytes == 2)
			*eax = pci_get_cfgdata16(pi, coff);
		else
			*eax = pci_get_cfgdata32(pi, coff);
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
			switch (pi->pi_bar[idx].type) {
			case PCIBAR_NONE:
				bar = 0;
				break;
			case PCIBAR_IO:
				mask = ~(pi->pi_bar[idx].size - 1);
				mask &= PCIM_BAR_IO_BASE;
				bar = (*eax & mask) | PCIM_BAR_IO_SPACE;
				break;
			case PCIBAR_MEM32:
				mask = ~(pi->pi_bar[idx].size - 1);
				mask &= PCIM_BAR_MEM_BASE;
				bar = *eax & mask;
				bar |= PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_32;
				break;
			case PCIBAR_MEM64:
				mask = ~(pi->pi_bar[idx].size - 1);
				mask &= PCIM_BAR_MEM_BASE;
				bar = *eax & mask;
				bar |= PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_64 |
				       PCIM_BAR_MEM_PREFETCH;
				break;
			case PCIBAR_MEMHI64:
				mask = ~(pi->pi_bar[idx - 1].size - 1);
				mask &= PCIM_BAR_MEM_BASE;
				bar = ((uint64_t)*eax << 32) & mask;
				bar = bar >> 32;
				break;
			default:
				assert(0);
			}
			pci_set_cfgdata32(pi, coff, bar);

			if (pi->pi_bar[idx].handler) {
				pi->pi_bar[idx].handler(pi, idx, bar);
			}

		} else if (pci_emul_iscap(pi, coff)) {
			pci_emul_capwrite(pi, coff, bytes, *eax);
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
#define DREGSZ	20
struct pci_emul_dsoftc {
	uint8_t   regs[DREGSZ];
};

#define	PCI_EMUL_MSGS	4

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

	error = pci_emul_alloc_bar(pi, 0, 0, PCIBAR_IO, DREGSZ);
	assert(error == 0);

	error = pci_emul_add_msicap(pi, PCI_EMUL_MSGS);
	assert(error == 0);

	return (0);
}

static void
pci_emul_diow(struct pci_devinst *pi, int baridx, int offset, int size,
	      uint32_t value)
{
	int i;
	struct pci_emul_dsoftc *sc = pi->pi_arg;

	if (offset + size > DREGSZ) {
		printf("diow: too large, offset %d size %d\n", offset, size);
		return;
	}

	if (size == 1) {
		sc->regs[offset] = value & 0xff;
	} else if (size == 2) {
		*(uint16_t *)&sc->regs[offset] = value & 0xffff;
	} else {
		*(uint32_t *)&sc->regs[offset] = value;
	}

	/*
	 * Special magic value to generate an interrupt
	 */
	if (offset == 4 && size == 4 && pci_msi_enabled(pi))
		pci_generate_msi(pi, value % pci_msi_msgnum(pi));

	if (value == 0xabcdef) {
		for (i = 0; i < pci_msi_msgnum(pi); i++)
			pci_generate_msi(pi, i);
	}
}

static uint32_t
pci_emul_dior(struct pci_devinst *pi, int baridx, int offset, int size)
{
	struct pci_emul_dsoftc *sc = pi->pi_arg;
	uint32_t value;

	if (offset + size > DREGSZ) {
		printf("dior: too large, offset %d size %d\n", offset, size);
		return (0);
	}
	
	if (size == 1) {
		value = sc->regs[offset];
	} else if (size == 2) {
		value = *(uint16_t *) &sc->regs[offset];
	} else {
		value = *(uint32_t *) &sc->regs[offset];
	}

	return (value);
}

struct pci_devemu pci_dummy = {
	.pe_emu = "dummy",
	.pe_init = pci_emul_dinit,
	.pe_iow = pci_emul_diow,
	.pe_ior = pci_emul_dior
};
PCI_EMUL_SET(pci_dummy);

#endif /* PCI_EMUL_TEST */
