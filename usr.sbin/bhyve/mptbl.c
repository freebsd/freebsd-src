/*-
 * Copyright (c) 2012 NetApp, Inc.
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

#include <sys/types.h>
#include <sys/errno.h>
#include <x86/mptable.h>

#include <stdio.h>
#include <string.h>

#include "bhyverun.h"
#include "mptbl.h"

#define MPTABLE_BASE		0xF0000

/* floating pointer length + maximum length of configuration table */
#define	MPTABLE_MAX_LENGTH	(65536 + 16)

#define LAPIC_PADDR		0xFEE00000
#define LAPIC_VERSION 		16

#define IOAPIC_PADDR		0xFEC00000
#define IOAPIC_VERSION		0x11

#define MP_SPECREV		4
#define MPFP_SIG		"_MP_"

/* Configuration header defines */
#define MPCH_SIG		"PCMP"
#define MPCH_OEMID		"BHyVe   "
#define MPCH_OEMID_LEN          8
#define MPCH_PRODID             "Hypervisor  "
#define MPCH_PRODID_LEN         12

/* Processor entry defines */
#define MPEP_SIG_FAMILY		6	/* XXX bhyve should supply this */
#define MPEP_SIG_MODEL		26
#define MPEP_SIG_STEPPING	5
#define MPEP_SIG		\
	((MPEP_SIG_FAMILY << 8) | \
	 (MPEP_SIG_MODEL << 4)	| \
	 (MPEP_SIG_STEPPING))

#define MPEP_FEATURES           (0xBFEBFBFF) /* XXX Intel i7 */

/* Define processor entry struct since <x86/mptable.h> gets it wrong */
typedef struct BPROCENTRY {
	u_char		type;
	u_char		apic_id;
	u_char		apic_version;
	u_char		cpu_flags;
	uint32_t	cpu_signature;
	uint32_t	feature_flags;
	uint32_t	reserved1;
	uint32_t	reserved2;
}      *bproc_entry_ptr;
CTASSERT(sizeof(struct BPROCENTRY) == 20);

/* Bus entry defines */
#define MPE_NUM_BUSES		2
#define MPE_BUSNAME_LEN		6
#define MPE_BUSNAME_ISA		"ISA   "
#define MPE_BUSNAME_PCI		"PCI   "

static void *oem_tbl_start;
static int oem_tbl_size;

static uint8_t
mpt_compute_checksum(void *base, size_t len)
{
	uint8_t	*bytes;
	uint8_t	sum;

	for(bytes = base, sum = 0; len > 0; len--) {
		sum += *bytes++;
	}

	return (256 - sum);
}

static void
mpt_build_mpfp(mpfps_t mpfp, vm_paddr_t gpa)
{

	memset(mpfp, 0, sizeof(*mpfp));
	memcpy(mpfp->signature, MPFP_SIG, 4);
	mpfp->pap = gpa + sizeof(*mpfp);
	mpfp->length = 1;
	mpfp->spec_rev = MP_SPECREV;
	mpfp->checksum = mpt_compute_checksum(mpfp, sizeof(*mpfp));
}

static void
mpt_build_mpch(mpcth_t mpch)
{

	memset(mpch, 0, sizeof(*mpch));
	memcpy(mpch->signature, MPCH_SIG, 4);
	mpch->spec_rev = MP_SPECREV;
	memcpy(mpch->oem_id, MPCH_OEMID, MPCH_OEMID_LEN);
	memcpy(mpch->product_id, MPCH_PRODID, MPCH_PRODID_LEN);
	mpch->apic_address = LAPIC_PADDR;
}

static void
mpt_build_proc_entries(bproc_entry_ptr mpep, int ncpu)
{
	int i;

	for (i = 0; i < ncpu; i++) {
		memset(mpep, 0, sizeof(*mpep));
		mpep->type = MPCT_ENTRY_PROCESSOR;
		mpep->apic_id = i; // XXX
		mpep->apic_version = LAPIC_VERSION;
		mpep->cpu_flags = PROCENTRY_FLAG_EN;
		if (i == 0)
			mpep->cpu_flags |= PROCENTRY_FLAG_BP;
		mpep->cpu_signature = MPEP_SIG;
		mpep->feature_flags = MPEP_FEATURES;
		mpep++;
	}
}

static void
mpt_build_bus_entries(bus_entry_ptr mpeb)
{

	memset(mpeb, 0, sizeof(*mpeb));
	mpeb->type = MPCT_ENTRY_BUS;
	mpeb->bus_id = ISA;
	memcpy(mpeb->bus_type, MPE_BUSNAME_ISA, MPE_BUSNAME_LEN);
	mpeb++;

	memset(mpeb, 0, sizeof(*mpeb));
	mpeb->type = MPCT_ENTRY_BUS;
	mpeb->bus_id = PCI;
	memcpy(mpeb->bus_type, MPE_BUSNAME_PCI, MPE_BUSNAME_LEN);
}

static void
mpt_build_ioapic_entries(io_apic_entry_ptr mpei, int id)
{

	memset(mpei, 0, sizeof(*mpei));
	mpei->type = MPCT_ENTRY_IOAPIC;
	mpei->apic_id = id;
	mpei->apic_version = IOAPIC_VERSION;
	mpei->apic_flags = IOAPICENTRY_FLAG_EN;
	mpei->apic_address = IOAPIC_PADDR;
}

#ifdef notyet
static void
mpt_build_ioint_entries(struct mpe_ioint *mpeii, int num_pins, int id)
{
	int pin;

	/*
	 * The following config is taken from kernel mptable.c
	 * mptable_parse_default_config_ints(...), for now 
	 * just use the default config, tweek later if needed.
	 */


	/* Run through all 16 pins. */
	for (pin = 0; pin < num_pins; pin++) {
		memset(mpeii, 0, sizeof(*mpeii));
		mpeii->entry_type = MP_ENTRY_IOINT;
		mpeii->src_bus_id = MPE_BUSID_ISA;
		mpeii->dst_apic_id = id;

		/*
		 * All default configs route IRQs from bus 0 to the first 16
		 * pins of the first I/O APIC with an APIC ID of 2.
		 */
		mpeii->dst_apic_intin = pin;
		switch (pin) {
		case 0:
			/* Pin 0 is an ExtINT pin. */
			mpeii->intr_type = MPEII_INTR_EXTINT;
			break;
		case 2:
			/* IRQ 0 is routed to pin 2. */
			mpeii->intr_type = MPEII_INTR_INT;
			mpeii->src_bus_irq = 0;
			break;
		case 5:
		case 10:
		case 11:
			/*
			 * PCI Irqs set to level triggered.
			 */
			mpeii->intr_flags = MPEII_FLAGS_TRIGMODE_LEVEL;
			mpeii->src_bus_id = MPE_BUSID_PCI;
		default:
			/* All other pins are identity mapped. */
			mpeii->intr_type = MPEII_INTR_INT;
			mpeii->src_bus_irq = pin;
			break;
		}
		mpeii++;
	}

}

#define COPYSTR(dest, src, bytes)		\
	memcpy(dest, src, bytes); 		\
	str[bytes] = 0;

static void
mptable_dump(struct mp_floating_pointer *mpfp, struct mp_config_hdr *mpch)
{
	static char 	 str[16];
	int 		 i;
	char 		*cur;

	union mpe {
		struct mpe_proc 	*proc;
		struct mpe_bus  	*bus;
		struct mpe_ioapic 	*ioapic;
		struct mpe_ioint 	*ioint;
		struct mpe_lint 	*lnit;
		char   			*p;
	};

	union mpe mpe;

	printf(" MP Floating Pointer :\n");
	COPYSTR(str, mpfp->signature, 4);
	printf("\tsignature:\t%s\n", str);
	printf("\tmpch paddr:\t%x\n", mpfp->mptable_paddr);
	printf("\tlength:\t%x\n", mpfp->length);
	printf("\tspecrec:\t%x\n", mpfp->specrev);
	printf("\tchecksum:\t%x\n", mpfp->checksum);
	printf("\tfeature1:\t%x\n", mpfp->feature1);
	printf("\tfeature2:\t%x\n", mpfp->feature2);
	printf("\tfeature3:\t%x\n", mpfp->feature3);
	printf("\tfeature4:\t%x\n", mpfp->feature4);

	printf(" MP Configuration Header :\n");
	COPYSTR(str, mpch->signature, 4);
	printf("    signature: 		%s\n", str);
	printf("    length: 		%x\n", mpch->length);
	printf("    specrec: 		%x\n", mpch->specrev);
	printf("    checksum: 		%x\n", mpch->checksum);
	COPYSTR(str, mpch->oemid, MPCH_OEMID_LEN);
	printf("    oemid: 		%s\n", str);
	COPYSTR(str, mpch->prodid, MPCH_PRODID_LEN);
	printf("    prodid: 		%s\n", str);
	printf("    oem_ptr: 		%x\n", mpch->oem_ptr);
	printf("    oem_sz: 		%x\n", mpch->oem_sz);
	printf("    nr_entries: 	%x\n", mpch->nr_entries);
	printf("    apic paddr: 	%x\n", mpch->lapic_paddr);
	printf("    ext_length: 	%x\n", mpch->ext_length);
	printf("    ext_checksum: 	%x\n", mpch->ext_checksum);

	cur = (char *)mpch + sizeof(*mpch);
	for (i = 0; i < mpch->nr_entries; i++) {
		mpe.p = cur;
		switch(*mpe.p) {		
			case MP_ENTRY_PROC:
				printf(" MP Processor Entry :\n");
				printf("	lapic_id: 	%x\n", mpe.proc->lapic_id);
				printf("	lapic_version:	%x\n", mpe.proc->lapic_version);
				printf("	proc_flags: 	%x\n", mpe.proc->proc_flags);
				printf("	proc_signature: %x\n", mpe.proc->proc_signature);
				printf("	feature_flags: 	%x\n", mpe.proc->feature_flags);
				cur += sizeof(struct mpe_proc);
				break;
			case MP_ENTRY_BUS:
				printf(" MP Bus Entry :\n");
				printf("	busid: 		%x\n", mpe.bus->busid);
				COPYSTR(str, mpe.bus->busname, MPE_BUSNAME_LEN);
				printf("	busname: 	%s\n", str);
				cur += sizeof(struct mpe_bus);
				break;
			case MP_ENTRY_IOAPIC:
				printf(" MP IOAPIC Entry :\n");
				printf("	ioapi_id: 		%x\n", mpe.ioapic->ioapic_id);
				printf("	ioapi_version: 		%x\n", mpe.ioapic->ioapic_version);
				printf("	ioapi_flags: 		%x\n", mpe.ioapic->ioapic_flags);
				printf("	ioapi_paddr: 		%x\n", mpe.ioapic->ioapic_paddr);
				cur += sizeof(struct mpe_ioapic);
				break;
			case MP_ENTRY_IOINT:
				printf(" MP IO Interrupt Entry :\n");
				printf("	intr_type: 		%x\n", mpe.ioint->intr_type);
				printf("	intr_flags: 		%x\n", mpe.ioint->intr_flags);
				printf("	src_bus_id: 		%x\n", mpe.ioint->src_bus_id);
				printf("	src_bus_irq: 		%x\n", mpe.ioint->src_bus_irq);
				printf("	dst_apic_id: 		%x\n", mpe.ioint->dst_apic_id);
				printf("	dst_apic_intin:		%x\n", mpe.ioint->dst_apic_intin);
				cur += sizeof(struct mpe_ioint);
				break;
			case MP_ENTRY_LINT:
				printf(" MP Local Interrupt Entry :\n");
				cur += sizeof(struct mpe_lint);
				break;
		}

	}
}
#endif

void
mptable_add_oemtbl(void *tbl, int tblsz)
{

	oem_tbl_start = tbl;
	oem_tbl_size = tblsz;
}

int
mptable_build(struct vmctx *ctx, int ncpu, int ioapic)
{
	mpcth_t			mpch;
	bus_entry_ptr		mpeb;
	io_apic_entry_ptr	mpei;
	bproc_entry_ptr		mpep;
	mpfps_t			mpfp;
	char 			*curraddr;
	char 			*startaddr;

	startaddr = paddr_guest2host(ctx, MPTABLE_BASE, MPTABLE_MAX_LENGTH);
	if (startaddr == NULL) {
		printf("mptable requires mapped mem\n");
		return (ENOMEM);
	}

	curraddr = startaddr;
	mpfp = (mpfps_t)curraddr;
	mpt_build_mpfp(mpfp, MPTABLE_BASE);
	curraddr += sizeof(*mpfp);

	mpch = (mpcth_t)curraddr;
	mpt_build_mpch(mpch);
	curraddr += sizeof(*mpch);

	mpep = (bproc_entry_ptr)curraddr;
	mpt_build_proc_entries(mpep, ncpu);
	curraddr += sizeof(*mpep) * ncpu;
	mpch->entry_count += ncpu;

	mpeb = (bus_entry_ptr) curraddr;
	mpt_build_bus_entries(mpeb);
	curraddr += sizeof(*mpeb) * MPE_NUM_BUSES;
	mpch->entry_count += MPE_NUM_BUSES;

	if (ioapic) {
		mpei = (io_apic_entry_ptr)curraddr;
		mpt_build_ioapic_entries(mpei, ncpu + 1);
		curraddr += sizeof(*mpei);
		mpch->entry_count++;
	}

#ifdef notyet
	mpt_build_ioint_entries((struct mpe_ioint*)curraddr, MPEII_MAX_IRQ,
				ncpu + 1);
	curraddr += sizeof(struct mpe_ioint) * MPEII_MAX_IRQ;
	mpch->entry_count += MPEII_MAX_IRQ;
#endif

	if (oem_tbl_start) {
		mpch->oem_table_pointer = curraddr - startaddr + MPTABLE_BASE;
		mpch->oem_table_size = oem_tbl_size;
		memcpy(curraddr, oem_tbl_start, oem_tbl_size);
	}

	mpch->base_table_length = curraddr - (char *)mpch;
	mpch->checksum = mpt_compute_checksum(mpch, mpch->base_table_length);

	return (0);
}
