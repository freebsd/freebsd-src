/*-
 * Copyright (c) 1996, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef __MACHINE_MPTABLE_H__
#define	__MACHINE_MPTABLE_H__

enum busTypes {
    NOBUS = 0,
    EISA = 3,
    ISA = 6,
    MCA = 9,
    PCI = 13,
    MAX_BUSTYPE = 18,
    UNKNOWN_BUSTYPE = 0xff
};

/* MP Floating Pointer Structure */
typedef struct MPFPS {
	char    signature[4];
	u_int32_t pap;
	u_char  length;
	u_char  spec_rev;
	u_char  checksum;
	u_char  config_type;
	u_char  mpfb2;
	u_char  mpfb3;
	u_char  mpfb4;
	u_char  mpfb5;
}      *mpfps_t;

#define	MPFB2_IMCR_PRESENT	0x80
#define	MPFB2_MUL_CLK_SRCS	0x40

/* MP Configuration Table Header */
typedef struct MPCTH {
	char    signature[4];
	u_short base_table_length;
	u_char  spec_rev;
	u_char  checksum;
	u_char  oem_id[8];
	u_char  product_id[12];
	u_int32_t oem_table_pointer;
	u_short oem_table_size;
	u_short entry_count;
	u_int32_t apic_address;
	u_short extended_table_length;
	u_char  extended_table_checksum;
	u_char  reserved;
}      *mpcth_t;

/* Base table entries */

#define	MPCT_ENTRY_PROCESSOR	0
#define	MPCT_ENTRY_BUS		1
#define	MPCT_ENTRY_IOAPIC	2
#define	MPCT_ENTRY_INT		3
#define	MPCT_ENTRY_LOCAL_INT	4

typedef struct PROCENTRY {
	u_char  type;
	u_char  apic_id;
	u_char  apic_version;
	u_char  cpu_flags;
	u_long  cpu_signature;
	u_long  feature_flags;
	u_long  reserved1;
	u_long  reserved2;
}      *proc_entry_ptr;

#define PROCENTRY_FLAG_EN	0x01
#define PROCENTRY_FLAG_BP	0x02

typedef struct BUSENTRY {
	u_char  type;
	u_char  bus_id;
	char    bus_type[6];
}      *bus_entry_ptr;

typedef struct IOAPICENTRY {
	u_char  type;
	u_char  apic_id;
	u_char  apic_version;
	u_char  apic_flags;
	u_int32_t apic_address;
}      *io_apic_entry_ptr;

#define IOAPICENTRY_FLAG_EN	0x01

typedef struct INTENTRY {
	u_char  type;
	u_char  int_type;
	u_short int_flags;
	u_char  src_bus_id;
	u_char  src_bus_irq;
	u_char  dst_apic_id;
	u_char  dst_apic_int;
}      *int_entry_ptr;

#define	INTENTRY_TYPE_INT  	0
#define	INTENTRY_TYPE_NMI	1
#define	INTENTRY_TYPE_SMI	2
#define	INTENTRY_TYPE_EXTINT	3

#define	INTENTRY_FLAGS_POLARITY			0x3
#define	INTENTRY_FLAGS_POLARITY_CONFORM		0x0
#define	INTENTRY_FLAGS_POLARITY_ACTIVEHI	0x1
#define	INTENTRY_FLAGS_POLARITY_ACTIVELO	0x3
#define	INTENTRY_FLAGS_TRIGGER			0xc
#define	INTENTRY_FLAGS_TRIGGER_CONFORM		0x0
#define	INTENTRY_FLAGS_TRIGGER_EDGE		0x4
#define	INTENTRY_FLAGS_TRIGGER_LEVEL		0xc

/* Extended table entries */

typedef	struct EXTENTRY {
	u_char	type;
	u_char	length;
}      *ext_entry_ptr;

#define	MPCT_EXTENTRY_SAS	0x80
#define	MPCT_EXTENTRY_BHD	0x81
#define	MPCT_EXTENTRY_CBASM	0x82

typedef struct SASENTRY {
	u_char	type;
	u_char	length;
	u_char	bus_id;
	u_char	address_type;
	uint64_t address_base;
	uint64_t address_length;
} __attribute__((__packed__)) *sas_entry_ptr;

#define	SASENTRY_TYPE_IO	0
#define	SASENTRY_TYPE_MEMORY	1
#define	SASENTRY_TYPE_PREFETCH	2

typedef struct BHDENTRY {
	u_char	type;
	u_char	length;
	u_char	bus_id;
	u_char	bus_info;
	u_char	parent_bus;
	u_char	reserved[3];
}      *bhd_entry_ptr;

#define	BHDENTRY_INFO_SUBTRACTIVE_DECODE	0x1

typedef struct CBASMENTRY {
	u_char	type;
	u_char	length;
	u_char	bus_id;
	u_char	address_mod;
	u_int	predefined_range;
}      *cbasm_entry_ptr;

#define	CBASMENTRY_ADDRESS_MOD_ADD		0x0
#define	CBASMENTRY_ADDRESS_MOD_SUBTRACT		0x1

#define	CBASMENTRY_RANGE_ISA_IO		0
#define	CBASMENTRY_RANGE_VGA_IO		1

/* descriptions of MP table entries */
typedef struct BASETABLE_ENTRY {
	u_char  type;
	u_char  length;
	char    name[16];
}       basetable_entry;

#ifdef _KERNEL
struct mptable_hostb_softc {
#ifdef NEW_PCIB
	struct pcib_host_resources sc_host_res;
	int		sc_decodes_vga_io;
	int		sc_decodes_isa_io;
#endif
};

#ifdef NEW_PCIB
void	mptable_pci_host_res_init(device_t pcib);
#endif
int	mptable_pci_probe_table(int bus);
int	mptable_pci_route_interrupt(device_t pcib, device_t dev, int pin);
#endif
#endif /* !__MACHINE_MPTABLE_H__ */
