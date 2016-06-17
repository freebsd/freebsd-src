/*
 *  acpitable.h - x86-64-specific ACPI boot-time initialization
 *
 *  Copyright (C) 1999 Andrew Henroid
 *  Copyright (C) 2001 Richard Schaal
 *  Copyright (C) 2001 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (C) 2001 Jun Nakajima <jun.nakajima@intel.com>
 *  Copyright (C) 2001 Arjan van de Ven <arjanv@redhat.com>
 *  Copyright (C) 2002 Vojtech Pavlik <vojtech@suse.cz>
 */

/*
 * The following codes are cut&pasted from drivers/acpi. Part of the code
 * there can be not updated or delivered yet.
 * To avoid conflicts when CONFIG_ACPI is defined, the following codes are
 * modified so that they are self-contained in this file.
 * -- jun
 */

#ifndef _HEADER_ACPITABLE_H_
#define _HEADER_ACPITABLE_H_

struct acpi_table_header {		/* ACPI common table header */
	char signature[4];		/* identifies type of table */
	u32 length;			/* length of table,
					   in bytes, * including header */
	u8 revision;			/* specification minor version # */
	u8 checksum;			/* to make sum of entire table == 0 */
	char oem_id[6];			/* OEM identification */
	char oem_table_id[8];		/* OEM table identification */
	u32 oem_revision;		/* OEM revision number */
	char asl_compiler_id[4];	/* ASL compiler vendor ID */
	u32 asl_compiler_revision;	/* ASL compiler revision number */
} __attribute__ ((packed));;

enum {
	ACPI_APIC = 0,
	ACPI_FACP,
	ACPI_HPET,
	ACPI_TABLE_COUNT
};

static char *acpi_table_signatures[ACPI_TABLE_COUNT] = {
	"APIC",
	"FACP",
	"HPET"
};

struct acpi_table_madt {
	struct acpi_table_header header;
	u32 lapic_address;
	struct {
		u32 pcat_compat:1;
		u32 reserved:31;
	} flags __attribute__ ((packed));
} __attribute__ ((packed));;

enum {
	ACPI_MADT_LAPIC = 0,
	ACPI_MADT_IOAPIC,
	ACPI_MADT_INT_SRC_OVR,
	ACPI_MADT_NMI_SRC,
	ACPI_MADT_LAPIC_NMI,
	ACPI_MADT_LAPIC_ADDR_OVR,
	ACPI_MADT_IOSAPIC,
	ACPI_MADT_LSAPIC,
	ACPI_MADT_PLAT_INT_SRC,

};

#define LO_RSDP_WINDOW_BASE		0	/* Physical Address */
#define HI_RSDP_WINDOW_BASE		0xE0000	/* Physical Address */
#define LO_RSDP_WINDOW_SIZE		0x400
#define HI_RSDP_WINDOW_SIZE		0x20000
#define RSDP_SCAN_STEP			16
#define RSDP_CHECKSUM_LENGTH		20
#define RSDP2_CHECKSUM_LENGTH		36

typedef int (*acpi_table_handler) (struct acpi_table_header *header, unsigned long);

struct acpi_table_rsdp {
	char signature[8];
	u8 checksum;
	char oem_id[6];
	u8 revision;
	u32 rsdt_address;
	u32 length;
	u64 xsdt_address;
	u8 checksum2;
	u8 reserved[3];
} __attribute__ ((packed));

struct acpi_table_rsdt {
	struct acpi_table_header header;
	u32 entry[0];
} __attribute__ ((packed));

struct acpi_table_xsdt {
	struct acpi_table_header header;
	u64 entry[0];
} __attribute__ ((packed));

struct acpi_madt_entry_header {
	u8 type;
	u8 length;
}  __attribute__ ((packed));

struct acpi_madt_int_flags {
	u16 polarity:2;
	u16 trigger:2;
	u16 reserved:12;
} __attribute__ ((packed));

struct acpi_table_lapic {
	struct acpi_madt_entry_header header;
	u8 acpi_id;
	u8 id;
	struct {
		u32 enabled:1;
		u32 reserved:31;
	} flags __attribute__ ((packed));
} __attribute__ ((packed));

struct acpi_table_ioapic {
	struct acpi_madt_entry_header header;
	u8 id;
	u8 reserved;
	u32 address;
	u32 global_irq_base;
} __attribute__ ((packed));

struct acpi_table_int_src_ovr {
	struct acpi_madt_entry_header header;
	u8 bus;
	u8 bus_irq;
	u32 global_irq;
	struct acpi_madt_int_flags flags;
} __attribute__ ((packed));

struct acpi_table_nmi_src {
	struct acpi_madt_entry_header header;
	struct acpi_madt_int_flags flags;
	u32 global_irq;
} __attribute__ ((packed));

struct acpi_table_lapic_nmi {
	struct acpi_madt_entry_header header;
	u8 acpi_id;
	struct acpi_madt_int_flags flags;
	u8 lint;
} __attribute__ ((packed));

struct acpi_table_lapic_addr_ovr {
	struct acpi_madt_entry_header header;
	u8 reserved[2];
	u64 address;
} __attribute__ ((packed));

struct acpi_table_iosapic {
	struct acpi_madt_entry_header header;
	u8 id;
	u8 reserved;
	u32 global_irq_base;
	u64 address;
} __attribute__ ((packed));

struct acpi_table_lsapic {
	struct acpi_madt_entry_header header;
	u8 acpi_id;
	u8 id;
	u8 eid;
	u8 reserved[3];
	struct {
		u32 enabled:1;
		u32 reserved:31;
	} flags;
} __attribute__ ((packed));

struct acpi_table_plat_int_src {
	struct acpi_madt_entry_header header;
	struct acpi_madt_int_flags flags;
	u8 type;
	u8 id;
	u8 eid;
	u8 iosapic_vector;
	u32 global_irq;
	u32 reserved;
} __attribute__ ((packed));

#define ACPI_SPACE_MEM		0
#define ACPI_SPACE_IO		1
#define ACPI_SPACE_PCICONF	2

struct acpi_gen_regaddr {
	u8  space_id;
	u8  bit_width;
	u8  bit_offset;
	u8  resv;
	u32 addrl;
	u32 addrh;
} __attribute__ ((packed));

struct acpi_table_hpet {
	struct acpi_table_header header;
	u32 id;
	struct acpi_gen_regaddr addr;
	u8 number;
	u16 min_tick;
	u8 page_protect;
} __attribute__ ((packed));

#endif
