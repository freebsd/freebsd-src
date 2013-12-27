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

/* Number of local intr entries */
#define	MPEII_NUM_LOCAL_IRQ	2

/* Number of i/o intr entries */
#define	MPEII_MAX_IRQ		24

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
mpt_build_proc_entries(proc_entry_ptr mpep, int ncpu)
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
mpt_build_localint_entries(int_entry_ptr mpie)
{

	/* Hardcode LINT0 as ExtINT on all CPUs. */
	memset(mpie, 0, sizeof(*mpie));
	mpie->type = MPCT_ENTRY_LOCAL_INT;
	mpie->int_type = INTENTRY_TYPE_EXTINT;
	mpie->int_flags = INTENTRY_FLAGS_POLARITY_CONFORM |
	    INTENTRY_FLAGS_TRIGGER_CONFORM;
	mpie->dst_apic_id = 0xff;
	mpie->dst_apic_int = 0;
	mpie++;

	/* Hardcode LINT1 as NMI on all CPUs. */
	memset(mpie, 0, sizeof(*mpie));
	mpie->type = MPCT_ENTRY_LOCAL_INT;
	mpie->int_type = INTENTRY_TYPE_NMI;
	mpie->int_flags = INTENTRY_FLAGS_POLARITY_CONFORM |
	    INTENTRY_FLAGS_TRIGGER_CONFORM;
	mpie->dst_apic_id = 0xff;
	mpie->dst_apic_int = 1;
}

static void
mpt_build_bus_entries(bus_entry_ptr mpeb)
{

	memset(mpeb, 0, sizeof(*mpeb));
	mpeb->type = MPCT_ENTRY_BUS;
	mpeb->bus_id = 0;
	memcpy(mpeb->bus_type, MPE_BUSNAME_PCI, MPE_BUSNAME_LEN);
	mpeb++;

	memset(mpeb, 0, sizeof(*mpeb));
	mpeb->type = MPCT_ENTRY_BUS;
	mpeb->bus_id = 1;	
	memcpy(mpeb->bus_type, MPE_BUSNAME_ISA, MPE_BUSNAME_LEN);
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

static void
mpt_build_ioint_entries(int_entry_ptr mpie, int num_pins, int id)
{
	int pin;

	/*
	 * The following config is taken from kernel mptable.c
	 * mptable_parse_default_config_ints(...), for now 
	 * just use the default config, tweek later if needed.
	 */

	/* Run through all 16 pins. */
	for (pin = 0; pin < num_pins; pin++) {
		memset(mpie, 0, sizeof(*mpie));
		mpie->type = MPCT_ENTRY_INT;
		mpie->src_bus_id = 1;
		mpie->dst_apic_id = id;

		/*
		 * All default configs route IRQs from bus 0 to the first 16
		 * pins of the first I/O APIC with an APIC ID of 2.
		 */
		mpie->dst_apic_int = pin;
		switch (pin) {
		case 0:
			/* Pin 0 is an ExtINT pin. */
			mpie->int_type = INTENTRY_TYPE_EXTINT;
			break;
		case 2:
			/* IRQ 0 is routed to pin 2. */
			mpie->int_type = INTENTRY_TYPE_INT;
			mpie->src_bus_irq = 0;
			break;
		case 5:
		case 10:
		case 11:
			/*
			 * PCI Irqs set to level triggered.
			 */
			mpie->int_flags = INTENTRY_FLAGS_TRIGGER_LEVEL;
			mpie->src_bus_id = 0;
			/* fall through.. */
		default:
			/* All other pins are identity mapped. */
			mpie->int_type = INTENTRY_TYPE_INT;
			mpie->src_bus_irq = pin;
			break;
		}
		mpie++;
	}

}

void
mptable_add_oemtbl(void *tbl, int tblsz)
{

	oem_tbl_start = tbl;
	oem_tbl_size = tblsz;
}

int
mptable_build(struct vmctx *ctx, int ncpu)
{
	mpcth_t			mpch;
	bus_entry_ptr		mpeb;
	io_apic_entry_ptr	mpei;
	proc_entry_ptr		mpep;
	mpfps_t			mpfp;
	int_entry_ptr		mpie;
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

	mpep = (proc_entry_ptr)curraddr;
	mpt_build_proc_entries(mpep, ncpu);
	curraddr += sizeof(*mpep) * ncpu;
	mpch->entry_count += ncpu;

	mpeb = (bus_entry_ptr) curraddr;
	mpt_build_bus_entries(mpeb);
	curraddr += sizeof(*mpeb) * MPE_NUM_BUSES;
	mpch->entry_count += MPE_NUM_BUSES;

	mpei = (io_apic_entry_ptr)curraddr;
	mpt_build_ioapic_entries(mpei, 0);
	curraddr += sizeof(*mpei);
	mpch->entry_count++;

	mpie = (int_entry_ptr) curraddr;
	mpt_build_ioint_entries(mpie, MPEII_MAX_IRQ, 0);
	curraddr += sizeof(*mpie) * MPEII_MAX_IRQ;
	mpch->entry_count += MPEII_MAX_IRQ;

	mpie = (int_entry_ptr)curraddr;
	mpt_build_localint_entries(mpie);
	curraddr += sizeof(*mpie) * MPEII_NUM_LOCAL_IRQ;
	mpch->entry_count += MPEII_NUM_LOCAL_IRQ;

	if (oem_tbl_start) {
		mpch->oem_table_pointer = curraddr - startaddr + MPTABLE_BASE;
		mpch->oem_table_size = oem_tbl_size;
		memcpy(curraddr, oem_tbl_start, oem_tbl_size);
	}

	mpch->base_table_length = curraddr - (char *)mpch;
	mpch->checksum = mpt_compute_checksum(mpch, mpch->base_table_length);

	return (0);
}
