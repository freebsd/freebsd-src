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

#ifndef _PCI_EMUL_H_
#define _PCI_EMUL_H_

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/nv.h>
#include <sys/pciio.h>
#include <sys/_pthreadtypes.h>

#include <dev/pci/pcireg.h>

#include <assert.h>

#define	PCI_BARMAX	PCIR_MAX_BAR_0	/* BAR registers in a Type 0 header */
#define PCI_BARMAX_WITH_ROM (PCI_BARMAX + 1)
#define PCI_ROM_IDX (PCI_BARMAX + 1)

struct vmctx;
struct pci_devinst;
struct memory_region;
struct vm_snapshot_meta;

struct pci_devemu {
	const char      *pe_emu;	/* Name of device emulation */

	/* instance creation */
	int       (*pe_init)(struct pci_devinst *, nvlist_t *);
	int	(*pe_legacy_config)(nvlist_t *, const char *);
	const char *pe_alias;

	/* ACPI DSDT enumeration */
	void	(*pe_write_dsdt)(struct pci_devinst *);

	/* config space read/write callbacks */
	int	(*pe_cfgwrite)(struct pci_devinst *pi, int offset,
			       int bytes, uint32_t val);
	int	(*pe_cfgread)(struct pci_devinst *pi, int offset,
			      int bytes, uint32_t *retval);

	/* BAR read/write callbacks */
	void      (*pe_barwrite)(struct pci_devinst *pi, int baridx,
				 uint64_t offset, int size, uint64_t value);
	uint64_t  (*pe_barread)(struct pci_devinst *pi, int baridx,
				uint64_t offset, int size);

	void	(*pe_baraddr)(struct pci_devinst *pi,
			      int baridx, int enabled, uint64_t address);

	/* Save/restore device state */
	int	(*pe_snapshot)(struct vm_snapshot_meta *meta);
	int	(*pe_pause)(struct pci_devinst *pi);
	int	(*pe_resume)(struct pci_devinst *pi);

};
#define PCI_EMUL_SET(x)   DATA_SET(pci_devemu_set, x)

enum pcibar_type {
	PCIBAR_NONE,
	PCIBAR_IO,
	PCIBAR_MEM32,
	PCIBAR_MEM64,
	PCIBAR_MEMHI64,
	PCIBAR_ROM,
};

struct pcibar {
	enum pcibar_type	type;		/* io or memory */
	uint64_t		size;
	uint64_t		addr;
	uint8_t			lobits;
};

#define PI_NAMESZ	40

struct msix_table_entry {
	uint64_t	addr;
	uint32_t	msg_data;
	uint32_t	vector_control;
} __packed;

/*
 * In case the structure is modified to hold extra information, use a define
 * for the size that should be emulated.
 */
#define	MSIX_TABLE_ENTRY_SIZE	16
#define MAX_MSIX_TABLE_ENTRIES	2048
#define	PBA_SIZE(msgnum)	(roundup2((msgnum), 64) / 8)

enum lintr_stat {
	IDLE,
	ASSERTED,
	PENDING
};

struct pci_devinst {
	struct pci_devemu *pi_d;
	struct vmctx *pi_vmctx;
	uint8_t	  pi_bus, pi_slot, pi_func;
	char	  pi_name[PI_NAMESZ];
	int	  pi_bar_getsize;
	int	  pi_prevcap;
	int	  pi_capend;

	struct {
		int8_t    	pin;
		enum lintr_stat	state;
		int		pirq_pin;
		int	  	ioapic_irq;
		pthread_mutex_t	lock;
	} pi_lintr;

	struct {
		int		enabled;
		uint64_t	addr;
		uint64_t	msg_data;
		int		maxmsgnum;
	} pi_msi;

	struct {
		int	enabled;
		int	table_bar;
		int	pba_bar;
		uint32_t table_offset;
		int	table_count;
		uint32_t pba_offset;
		int	pba_size;
		int	function_mask;
		struct msix_table_entry *table;	/* allocated at runtime */
		uint8_t *mapped_addr;
		size_t	mapped_size;
	} pi_msix;

	void      *pi_arg;		/* devemu-private data */

	u_char	  pi_cfgdata[PCI_REGMAX + 1];
	/* ROM is handled like a BAR */
	struct pcibar pi_bar[PCI_BARMAX_WITH_ROM + 1];
	uint64_t pi_romoffset;
};

struct msicap {
	uint8_t		capid;
	uint8_t		nextptr;
	uint16_t	msgctrl;
	uint32_t	addrlo;
	uint32_t	addrhi;
	uint16_t	msgdata;
} __packed;
static_assert(sizeof(struct msicap) == 14, "compile-time assertion failed");

struct msixcap {
	uint8_t		capid;
	uint8_t		nextptr;
	uint16_t	msgctrl;
	uint32_t	table_info;	/* bar index and offset within it */
	uint32_t	pba_info;	/* bar index and offset within it */
} __packed;
static_assert(sizeof(struct msixcap) == 12, "compile-time assertion failed");

struct pciecap {
	uint8_t		capid;
	uint8_t		nextptr;
	uint16_t	pcie_capabilities;

	uint32_t	dev_capabilities;	/* all devices */
	uint16_t	dev_control;
	uint16_t	dev_status;

	uint32_t	link_capabilities;	/* devices with links */
	uint16_t	link_control;
	uint16_t	link_status;

	uint32_t	slot_capabilities;	/* ports with slots */
	uint16_t	slot_control;
	uint16_t	slot_status;

	uint16_t	root_control;		/* root ports */
	uint16_t	root_capabilities;
	uint32_t	root_status;

	uint32_t	dev_capabilities2;	/* all devices */
	uint16_t	dev_control2;
	uint16_t	dev_status2;

	uint32_t	link_capabilities2;	/* devices with links */
	uint16_t	link_control2;
	uint16_t	link_status2;

	uint32_t	slot_capabilities2;	/* ports with slots */
	uint16_t	slot_control2;
	uint16_t	slot_status2;
} __packed;
static_assert(sizeof(struct pciecap) == 60, "compile-time assertion failed");

typedef void (*pci_lintr_cb)(int b, int s, int pin, int pirq_pin,
    int ioapic_irq, void *arg);

int	init_pci(struct vmctx *ctx);
void	pci_callback(void);
uint32_t pci_config_read_reg(const struct pcisel *host_sel, nvlist_t *nvl,
	    uint32_t reg, uint8_t size, uint32_t def);
int	pci_emul_alloc_bar(struct pci_devinst *pdi, int idx,
	    enum pcibar_type type, uint64_t size);
int 	pci_emul_alloc_rom(struct pci_devinst *const pdi, const uint64_t size,
    	    void **const addr);
int 	pci_emul_add_boot_device(struct pci_devinst *const pi,
	    const int bootindex);
int	pci_emul_add_msicap(struct pci_devinst *pi, int msgnum);
int	pci_emul_add_pciecap(struct pci_devinst *pi, int pcie_device_type);
void	pci_emul_capwrite(struct pci_devinst *pi, int offset, int bytes,
	    uint32_t val, uint8_t capoff, int capid);
void	pci_emul_cmd_changed(struct pci_devinst *pi, uint16_t old);
void	pci_generate_msi(struct pci_devinst *pi, int msgnum);
void	pci_generate_msix(struct pci_devinst *pi, int msgnum);
void	pci_lintr_assert(struct pci_devinst *pi);
void	pci_lintr_deassert(struct pci_devinst *pi);
void	pci_lintr_request(struct pci_devinst *pi);
int	pci_msi_enabled(struct pci_devinst *pi);
int	pci_msix_enabled(struct pci_devinst *pi);
int	pci_msix_table_bar(struct pci_devinst *pi);
int	pci_msix_pba_bar(struct pci_devinst *pi);
int	pci_msi_maxmsgnum(struct pci_devinst *pi);
int	pci_parse_legacy_config(nvlist_t *nvl, const char *opt);
int	pci_parse_slot(char *opt);
void    pci_print_supported_devices(void);
void	pci_populate_msicap(struct msicap *cap, int msgs, int nextptr);
int	pci_emul_add_msixcap(struct pci_devinst *pi, int msgnum, int barnum);
int	pci_emul_msix_twrite(struct pci_devinst *pi, uint64_t offset, int size,
			     uint64_t value);
uint64_t pci_emul_msix_tread(struct pci_devinst *pi, uint64_t offset, int size);
int	pci_count_lintr(int bus);
void	pci_walk_lintr(int bus, pci_lintr_cb cb, void *arg);
void	pci_write_dsdt(void);
uint64_t pci_ecfg_base(void);
int	pci_bus_configured(int bus);
#ifdef BHYVE_SNAPSHOT
struct pci_devinst *pci_next(const struct pci_devinst *cursor);
int	pci_snapshot(struct vm_snapshot_meta *meta);
int	pci_pause(struct pci_devinst *pdi);
int	pci_resume(struct pci_devinst *pdi);
#endif

static __inline void
pci_set_cfgdata8(struct pci_devinst *pi, int offset, uint8_t val)
{
	assert(offset <= PCI_REGMAX);
	*(uint8_t *)(pi->pi_cfgdata + offset) = val;
}

static __inline void
pci_set_cfgdata16(struct pci_devinst *pi, int offset, uint16_t val)
{
	assert(offset <= (PCI_REGMAX - 1) && (offset & 1) == 0);
	*(uint16_t *)(pi->pi_cfgdata + offset) = val;
}

static __inline void
pci_set_cfgdata32(struct pci_devinst *pi, int offset, uint32_t val)
{
	assert(offset <= (PCI_REGMAX - 3) && (offset & 3) == 0);
	*(uint32_t *)(pi->pi_cfgdata + offset) = val;
}

static __inline uint8_t
pci_get_cfgdata8(struct pci_devinst *pi, int offset)
{
	assert(offset <= PCI_REGMAX);
	return (*(uint8_t *)(pi->pi_cfgdata + offset));
}

static __inline uint16_t
pci_get_cfgdata16(struct pci_devinst *pi, int offset)
{
	assert(offset <= (PCI_REGMAX - 1) && (offset & 1) == 0);
	return (*(uint16_t *)(pi->pi_cfgdata + offset));
}

static __inline uint32_t
pci_get_cfgdata32(struct pci_devinst *pi, int offset)
{
	assert(offset <= (PCI_REGMAX - 3) && (offset & 3) == 0);
	return (*(uint32_t *)(pi->pi_cfgdata + offset));
}

#endif /* _PCI_EMUL_H_ */
