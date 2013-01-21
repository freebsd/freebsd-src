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

#ifndef _PCI_EMUL_H_
#define _PCI_EMUL_H_

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/kernel.h>

#include <dev/pci/pcireg.h>

#include <assert.h>

#define	PCI_BARMAX	PCIR_MAX_BAR_0	/* BAR registers in a Type 0 header */
#define	PCIY_RESERVED	0x00

struct vmctx;
struct pci_devinst;
struct memory_region;

struct pci_devemu {
	char      *pe_emu;		/* Name of device emulation */

	/* instance creation */
	int       (*pe_init)(struct vmctx *, struct pci_devinst *,
			     char *opts);

	/* config space read/write callbacks */
	int	(*pe_cfgwrite)(struct vmctx *ctx, int vcpu,
			       struct pci_devinst *pi, int offset,
			       int bytes, uint32_t val);
	int	(*pe_cfgread)(struct vmctx *ctx, int vcpu,
			      struct pci_devinst *pi, int offset,
			      int bytes, uint32_t *retval);

	/* BAR read/write callbacks */
	void      (*pe_barwrite)(struct vmctx *ctx, int vcpu,
				 struct pci_devinst *pi, int baridx,
				 uint64_t offset, int size, uint64_t value);
	uint64_t  (*pe_barread)(struct vmctx *ctx, int vcpu,
				struct pci_devinst *pi, int baridx,
				uint64_t offset, int size);
};
#define PCI_EMUL_SET(x)   DATA_SET(pci_devemu_set, x);

enum pcibar_type {
	PCIBAR_NONE,
	PCIBAR_IO,
	PCIBAR_MEM32,
	PCIBAR_MEM64,
	PCIBAR_MEMHI64
};

struct pcibar {
	enum pcibar_type	type;		/* io or memory */
	uint64_t		size;
	uint64_t		addr;
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
#define MSIX_TABLE_ENTRY_SIZE 16
#define MAX_MSIX_TABLE_SIZE 2048

struct pci_devinst {
	struct pci_devemu *pi_d;
	struct vmctx *pi_vmctx;
	uint8_t	  pi_bus, pi_slot, pi_func;
	uint8_t   pi_lintr_pin;
	char	  pi_name[PI_NAMESZ];
	uint16_t  pi_iobase;
	int	  pi_bar_getsize;

	struct {
		int	enabled;
		int	cpu;
		int	vector;
		int	msgnum;
	} pi_msi;

	struct {
		int	enabled;
		int	table_bar;
		int	pba_bar;
		size_t	table_offset;
		int	table_count;
		size_t	pba_offset;
		struct msix_table_entry table[MAX_MSIX_TABLE_SIZE];
	} pi_msix;

	void      *pi_arg;		/* devemu-private data */

	u_char	  pi_cfgdata[PCI_REGMAX + 1];
	struct pcibar pi_bar[PCI_BARMAX + 1];
};

struct msicap {
	uint8_t		capid;
	uint8_t		nextptr;
	uint16_t	msgctrl;
	uint32_t	addrlo;
	uint32_t	addrhi;
	uint16_t	msgdata;
} __packed;

struct msixcap {
	uint8_t		capid;
	uint8_t		nextptr;
	uint16_t	msgctrl;
	uint32_t	table_offset;
	uint32_t	pba_offset;
} __packed;

void	init_pci(struct vmctx *ctx);
void	msicap_cfgwrite(struct pci_devinst *pi, int capoff, int offset,
	    int bytes, uint32_t val);
void	msixcap_cfgwrite(struct pci_devinst *pi, int capoff, int offset,
	    int bytes, uint32_t val);
void	pci_callback(void);
int	pci_emul_alloc_bar(struct pci_devinst *pdi, int idx,
	    enum pcibar_type type, uint64_t size);
int	pci_emul_alloc_pbar(struct pci_devinst *pdi, int idx,
	    uint64_t hostbase, enum pcibar_type type, uint64_t size);
int	pci_emul_add_msicap(struct pci_devinst *pi, int msgnum);
int	pci_is_legacy(struct pci_devinst *pi);
void	pci_generate_msi(struct pci_devinst *pi, int msgnum);
void	pci_generate_msix(struct pci_devinst *pi, int msgnum);
void	pci_lintr_assert(struct pci_devinst *pi);
void	pci_lintr_deassert(struct pci_devinst *pi);
int	pci_lintr_request(struct pci_devinst *pi, int ivec);
int	pci_msi_enabled(struct pci_devinst *pi);
int	pci_msix_enabled(struct pci_devinst *pi);
int	pci_msi_msgnum(struct pci_devinst *pi);
void	pci_parse_slot(char *opt, int legacy);
void	pci_populate_msicap(struct msicap *cap, int msgs, int nextptr);

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
