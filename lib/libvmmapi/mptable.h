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

#ifndef _MPTABLE_h_
#define	_MPTABLE_h_

#define MP_SPECREV		(4)		// MP spec revision 1.1

/*
 * MP Floating Pointer Structure
 */ 
#define MPFP_SIGNATURE		"_MP_"
#define MPFP_SIGNATURE_LEN	(4)
#define MPFP_FEATURE2		(0x80)	// IMCR is present
struct mp_floating_pointer {
	uint8_t 	signature[MPFP_SIGNATURE_LEN];
	uint32_t 	mptable_paddr;
	uint8_t 	length;
	uint8_t 	specrev;
	uint8_t 	checksum;
	uint8_t 	feature1;
	uint8_t 	feature2;
	uint8_t 	feature3;
	uint8_t 	feature4;
	uint8_t 	feature5;
};


/*
 * MP Configuration Table Header
 */
#define MPCH_SIGNATURE		"PCMP"
#define MPCH_SIGNATURE_LEN	(4)

#define MPCH_OEMID		"NETAPP  "
#define MPCH_OEMID_LEN		(8)
#define MPCH_PRODID		"vFiler      "
#define MPCH_PRODID_LEN		(12)

struct mp_config_hdr {
	uint8_t 	signature[MPCH_SIGNATURE_LEN];
	uint16_t 	length;
	uint8_t 	specrev;
	uint8_t 	checksum;
	uint8_t 	oemid[MPCH_OEMID_LEN];
	uint8_t 	prodid[MPCH_PRODID_LEN];
	uint32_t 	oem_ptr;
	uint16_t 	oem_sz;
	uint16_t 	nr_entries;
	uint32_t 	lapic_paddr;
	uint16_t 	ext_length;
	uint8_t 	ext_checksum;
	uint8_t 	reserved;
};

#define MP_ENTRY_PROC	(0)
#define MP_ENTRY_BUS	(1)
#define MP_ENTRY_IOAPIC	(2)
#define MP_ENTRY_IOINT	(3)
#define MP_ENTRY_LINT	(4)

/*
 * MP Processor Entry
 */

#define MPEP_FLAGS_EN		(0x1)
#define MPEP_FLAGS_BSP		(0x2)

#define MPEP_SIG_FAMILY         (6)
#define MPEP_SIG_MODEL          (26)
#define MPEP_SIG_STEPPING       (5)
#define MPEP_SIGNATURE  	((MPEP_SIG_FAMILY << 8) | (MPEP_SIG_MODEL << 4)  \
				| (MPEP_SIG_STEPPING))

#define MPEP_FEATURES		(0xBFEBFBFF) // Value from Intel i7 CPUID

struct mpe_proc {
	uint8_t 	entry_type;
	uint8_t 	lapic_id;
	uint8_t 	lapic_version;
	uint8_t 	proc_flags;
	uint32_t 	proc_signature;
	uint32_t 	feature_flags;
	uint8_t 	reserved[8];
};

/*
 * MP Bus Entry
 */

#define MPE_NUM_BUSES	(2)
#define MPE_BUSNAME_LEN	(6)
#define MPE_BUSID_ISA	(0)
#define MPE_BUSID_PCI	(1)
#define MPE_BUSNAME_ISA	"ISA   "
#define MPE_BUSNAME_PCI	"PCI   "
struct mpe_bus {
	uint8_t 	entry_type;
	uint8_t		busid;
	uint8_t		busname[MPE_BUSNAME_LEN];
};

/*
 * MP IO APIC Entry
 */
#define MPE_IOAPIC_FLAG_EN	(1)
struct mpe_ioapic {
	uint8_t 	entry_type;
	uint8_t		ioapic_id;
	uint8_t		ioapic_version;
	uint8_t		ioapic_flags;
	uint32_t	ioapic_paddr;

};

/*
 * MP IO Interrupt Assignment Entry
 */
#define MPEII_INTR_INT		(0)
#define MPEII_INTR_NMI		(1)
#define MPEII_INTR_SMI		(2)
#define MPEII_INTR_EXTINT	(3)
#define MPEII_PCI_IRQ_MASK    	(0x0c20U) /* IRQ 5,10,11 are PCI connected */
#define MPEII_MAX_IRQ		(16)
#define MPEII_FLAGS_TRIGMODE_LEVEL	(0x3)
struct mpe_ioint {
	uint8_t 	entry_type;
	uint8_t		intr_type;
	uint16_t	intr_flags;
	uint8_t		src_bus_id;
	uint8_t		src_bus_irq;
	uint8_t		dst_apic_id;
	uint8_t		dst_apic_intin;
};

/*
 * MP Local Interrupt Assignment Entry
 */
struct mpe_lint {
	uint8_t 	entry_type;
};

int	vm_build_mptable(struct vmctx *ctxt, vm_paddr_t gpa, int len,
			 int ncpu, int ioapic, void *oemp, int oemsz);
#endif	/* _MPTABLE_h_ */
