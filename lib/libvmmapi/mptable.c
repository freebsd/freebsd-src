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

#include <sys/types.h>
#include <sys/mman.h>

#include <stdio.h>
#include <string.h>
#include <machine/vmm.h>
#include <machine/vmm_dev.h>

#include "vmmapi.h"
#include "mptable.h"

#define LAPIC_PADDR		(0xFEE00000)
#define LAPIC_VERSION 		(16)

#define IOAPIC_PADDR		(0xFEC00000)
#define IOAPIC_VERSION		(0x11)

extern int errno;

static uint8_t
mp_compute_checksum(void *base, size_t len)
{
	uint8_t	*bytes = base;
	uint8_t	 sum = 0;
	for(; len > 0; len--) {
		sum += *bytes++;
	}
	return 256 - sum;
}

static void
mp_build_mpfp(struct mp_floating_pointer *mpfp, vm_paddr_t mpfp_gpa)
{
	memset(mpfp, 0, sizeof(*mpfp));
	memcpy(mpfp->signature, MPFP_SIGNATURE, MPFP_SIGNATURE_LEN);
	mpfp->mptable_paddr = mpfp_gpa + sizeof(*mpfp);
	mpfp->specrev = MP_SPECREV;
	mpfp->feature2 = 0;
	mpfp->checksum = mp_compute_checksum(mpfp, sizeof(*mpfp));
}

static void
mp_build_mpch(struct mp_config_hdr *mpch)
{
	memset(mpch, 0, sizeof(*mpch));
	mpch->specrev = MP_SPECREV;
	memcpy(mpch->signature, MPCH_SIGNATURE, MPCH_SIGNATURE_LEN);
	memcpy(mpch->oemid, MPCH_OEMID, MPCH_OEMID_LEN);
	memcpy(mpch->prodid, MPCH_PRODID, MPCH_PRODID_LEN);
	mpch->lapic_paddr = LAPIC_PADDR;


}

static void
mp_build_proc_entries(struct mpe_proc *mpep, int num_proc)
{
	int i;

	for (i = 0; i < num_proc; i++) {
		memset(mpep, 0, sizeof(*mpep));
		mpep->entry_type = MP_ENTRY_PROC;
		mpep->lapic_id = i; // XXX
		mpep->lapic_version = LAPIC_VERSION;
		mpep->proc_flags = (i == 0)?MPEP_FLAGS_BSP:0;
		mpep->proc_flags |= MPEP_FLAGS_EN;
		mpep->proc_signature = MPEP_SIGNATURE;
		mpep->feature_flags = MPEP_FEATURES;
		mpep++;
	}
	
}

static void
mp_build_bus_entries(struct mpe_bus *mpeb)
{
	memset(mpeb, 0, sizeof(*mpeb));
	mpeb->entry_type = MP_ENTRY_BUS;
	mpeb->busid = MPE_BUSID_ISA;
	memcpy(mpeb->busname, MPE_BUSNAME_ISA, MPE_BUSNAME_LEN);
	mpeb++;

	memset(mpeb, 0, sizeof(*mpeb));
	mpeb->entry_type = MP_ENTRY_BUS;
	mpeb->busid = MPE_BUSID_PCI;
	memcpy(mpeb->busname, MPE_BUSNAME_PCI, MPE_BUSNAME_LEN);

}

static void
mp_build_ioapic_entries(struct mpe_ioapic *mpei, int id)
{
	memset(mpei, 0, sizeof(*mpei));
	mpei->entry_type = MP_ENTRY_IOAPIC;
	mpei->ioapic_id = id;
	mpei->ioapic_version = IOAPIC_VERSION;
	mpei->ioapic_flags = MPE_IOAPIC_FLAG_EN;
	mpei->ioapic_paddr = IOAPIC_PADDR;
}

#ifdef notyet
static void
mp_build_ioint_entries(struct mpe_ioint *mpeii, int num_pins, int id)
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
		 * All default configs route IRQs from bus 0 to the first 16 pins
		 * of the first I/O APIC with an APIC ID of 2.
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
	printf("    signature: 	%s\n", str);
	printf("    mpch paddr: %x\n", mpfp->mptable_paddr);
	printf("    length: 	%x\n", mpfp->length);
	printf("    specrec: 	%x\n", mpfp->specrev);
	printf("    checksum: 	%x\n", mpfp->checksum);
	printf("    feature1: 	%x\n", mpfp->feature1);
	printf("    feature2: 	%x\n", mpfp->feature2);
	printf("    feature3: 	%x\n", mpfp->feature3);
	printf("    feature4: 	%x\n", mpfp->feature4);

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

int
vm_build_mptable(struct vmctx *ctx, vm_paddr_t gpa, int len, int ncpu,
		 int ioapic, void *oemp, int oemsz)
{
	struct mp_config_hdr	*mpch;
	char 			*mapaddr;
	char 			*startaddr;
	int 	 		 error;

	mapaddr = vm_map_memory(ctx, gpa, len);
	if (mapaddr == MAP_FAILED) {
		printf("%s\n", strerror(errno));
		goto err;
	}
	startaddr = mapaddr;

	mp_build_mpfp((struct mp_floating_pointer*) mapaddr, gpa);
	mapaddr += sizeof(struct mp_floating_pointer);

	mpch = (struct mp_config_hdr*)mapaddr;
	mp_build_mpch(mpch);
	mapaddr += sizeof(struct mp_config_hdr);

	mp_build_proc_entries((struct mpe_proc*) mapaddr, ncpu);
	mapaddr += (sizeof(struct mpe_proc)*ncpu);
	mpch->nr_entries += ncpu;

	mp_build_bus_entries((struct mpe_bus*)mapaddr);
	mapaddr += (sizeof(struct mpe_bus)*MPE_NUM_BUSES);
	mpch->nr_entries += MPE_NUM_BUSES;

	if (ioapic) {
		mp_build_ioapic_entries((struct mpe_ioapic*)mapaddr, ncpu + 1);
		mapaddr += sizeof(struct mpe_ioapic);
		mpch->nr_entries++;
	}

#ifdef notyet
	mp_build_ioint_entries((struct mpe_ioint*)mapaddr, MPEII_MAX_IRQ,
				ncpu + 1);
	mapaddr += sizeof(struct mpe_ioint)*MPEII_MAX_IRQ;
	mpch->nr_entries += MPEII_MAX_IRQ;

#endif
	if (oemp) {
		mpch->oem_ptr = mapaddr - startaddr + gpa;
		mpch->oem_sz = oemsz;
		memcpy(mapaddr, oemp, oemsz);
	}
	mpch->length = (mapaddr) - ((char*) mpch);
	mpch->checksum = mp_compute_checksum(mpch, sizeof(*mpch));


	// mptable_dump((struct mp_floating_pointer*)startaddr, mpch);
err:
	return (error);
}
