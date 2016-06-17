#ifndef _LINUX_EFI_H
#define _LINUX_EFI_H

/*
 * Extensible Firmware Interface
 * Based on 'Extensible Firmware Interface Specification' version 0.9, April 30, 1999
 *
 * Copyright (C) 1999 VA Linux Systems
 * Copyright (C) 1999 Walt Drummond <drummond@valinux.com>
 * Copyright (C) 1999, 2002 Hewlett-Packard Co.
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *	Stephane Eranian <eranian@hpl.hp.com>
 */
#include <linux/init.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/proc_fs.h>

#include <asm/page.h>
#include <asm/system.h>

#define EFI_SUCCESS		0
#define EFI_LOAD_ERROR          (1L | (1L << 63))
#define EFI_INVALID_PARAMETER	(2L | (1L << 63))
#define EFI_UNSUPPORTED		(3L | (1L << 63))
#define EFI_BAD_BUFFER_SIZE     (4L | (1L << 63))
#define EFI_BUFFER_TOO_SMALL	(5L | (1L << 63))
#define EFI_NOT_FOUND          (14L | (1L << 63))

typedef unsigned long efi_status_t;
typedef u8 efi_bool_t;
typedef u16 efi_char16_t;		/* UNICODE character */


typedef struct {
	u8 b[16];
} efi_guid_t;

#define EFI_GUID(a,b,c,d0,d1,d2,d3,d4,d5,d6,d7) \
((efi_guid_t) \
{{ (a) & 0xff, ((a) >> 8) & 0xff, ((a) >> 16) & 0xff, ((a) >> 24) & 0xff, \
  (b) & 0xff, ((b) >> 8) & 0xff, \
  (c) & 0xff, ((c) >> 8) & 0xff, \
  (d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7) }})

/*
 * Generic EFI table header
 */
typedef	struct {
	u64 signature;
	u32 revision;
	u32 headersize;
	u32 crc32;
	u32 reserved;
} efi_table_hdr_t;

/*
 * Memory map descriptor:
 */

/* Memory types: */
#define EFI_RESERVED_TYPE		 0
#define EFI_LOADER_CODE			 1
#define EFI_LOADER_DATA			 2
#define EFI_BOOT_SERVICES_CODE		 3
#define EFI_BOOT_SERVICES_DATA		 4
#define EFI_RUNTIME_SERVICES_CODE	 5
#define EFI_RUNTIME_SERVICES_DATA	 6
#define EFI_CONVENTIONAL_MEMORY		 7
#define EFI_UNUSABLE_MEMORY		 8
#define EFI_ACPI_RECLAIM_MEMORY		 9
#define EFI_ACPI_MEMORY_NVS		10
#define EFI_MEMORY_MAPPED_IO		11
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE	12
#define EFI_PAL_CODE			13
#define EFI_MAX_MEMORY_TYPE		14

/* Attribute values: */
#define EFI_MEMORY_UC		0x0000000000000001	/* uncached */
#define EFI_MEMORY_WC		0x0000000000000002	/* write-coalescing */
#define EFI_MEMORY_WT		0x0000000000000004	/* write-through */
#define EFI_MEMORY_WB		0x0000000000000008	/* write-back */
#define EFI_MEMORY_WP		0x0000000000001000	/* write-protect */
#define EFI_MEMORY_RP		0x0000000000002000	/* read-protect */
#define EFI_MEMORY_XP		0x0000000000004000	/* execute-protect */
#define EFI_MEMORY_RUNTIME	0x8000000000000000	/* range requires runtime mapping */
#define EFI_MEMORY_DESCRIPTOR_VERSION	1

#define EFI_PAGE_SHIFT		12

typedef struct {
	u32 type;
	u32 pad;
	u64 phys_addr;
	u64 virt_addr;
	u64 num_pages;
	u64 attribute;
} efi_memory_desc_t;

typedef int efi_freemem_callback_t (u64 start, u64 end, void *arg);

/*
 * Types and defines for Time Services
 */
#define EFI_TIME_ADJUST_DAYLIGHT 0x1
#define EFI_TIME_IN_DAYLIGHT     0x2
#define EFI_UNSPECIFIED_TIMEZONE 0x07ff

typedef struct {
	u16 year;
	u8 month;
	u8 day;
	u8 hour;
	u8 minute;
	u8 second;
	u8 pad1;
	u32 nanosecond;
	s16 timezone;
	u8 daylight;
	u8 pad2;
} efi_time_t;

typedef struct {
	u32 resolution;
	u32 accuracy;
	u8 sets_to_zero;
} efi_time_cap_t;

/*
 * Types and defines for EFI ResetSystem
 */
#define EFI_RESET_COLD 0
#define EFI_RESET_WARM 1

/*
 * EFI Runtime Services table
 */
#define EFI_RUNTIME_SERVICES_SIGNATURE 0x5652453544e5552
#define EFI_RUNTIME_SERVICES_REVISION  0x00010000

typedef struct {
	efi_table_hdr_t hdr;
	u64 get_time;
	u64 set_time;
	u64 get_wakeup_time;
	u64 set_wakeup_time;
	u64 set_virtual_address_map;
	u64 convert_pointer;
	u64 get_variable;
	u64 get_next_variable;
	u64 set_variable;
	u64 get_next_high_mono_count;
	u64 reset_system;
} efi_runtime_services_t;

typedef efi_status_t efi_get_time_t (efi_time_t *tm, efi_time_cap_t *tc);
typedef efi_status_t efi_set_time_t (efi_time_t *tm);
typedef efi_status_t efi_get_wakeup_time_t (efi_bool_t *enabled, efi_bool_t *pending,
					    efi_time_t *tm);
typedef efi_status_t efi_set_wakeup_time_t (efi_bool_t enabled, efi_time_t *tm);
typedef efi_status_t efi_get_variable_t (efi_char16_t *name, efi_guid_t *vendor, u32 *attr,
					 unsigned long *data_size, void *data);
typedef efi_status_t efi_get_next_variable_t (unsigned long *name_size, efi_char16_t *name,
					      efi_guid_t *vendor);
typedef efi_status_t efi_set_variable_t (efi_char16_t *name, efi_guid_t *vendor, u32 attr,
					 unsigned long data_size, void *data);
typedef efi_status_t efi_get_next_high_mono_count_t (u64 *count);
typedef void efi_reset_system_t (int reset_type, efi_status_t status,
				 unsigned long data_size, efi_char16_t *data);

/*
 *  EFI Configuration Table and GUID definitions
 */
#define NULL_GUID \
    EFI_GUID(  0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 )

#define MPS_TABLE_GUID    \
    EFI_GUID(  0xeb9d2d2f, 0x2d88, 0x11d3, 0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d )

#define ACPI_TABLE_GUID    \
    EFI_GUID(  0xeb9d2d30, 0x2d88, 0x11d3, 0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d )

#define ACPI_20_TABLE_GUID    \
    EFI_GUID(  0x8868e871, 0xe4f1, 0x11d3, 0xbc, 0x22, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 )

#define SMBIOS_TABLE_GUID    \
    EFI_GUID(  0xeb9d2d31, 0x2d88, 0x11d3, 0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d )

#define SAL_SYSTEM_TABLE_GUID    \
    EFI_GUID(  0xeb9d2d32, 0x2d88, 0x11d3, 0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d )

#define HCDP_TABLE_GUID	\
    EFI_GUID(  0xf951938d, 0x620b, 0x42ef, 0x82, 0x79, 0xa8, 0x4b, 0x79, 0x61, 0x78, 0x98 )

typedef struct {
	efi_guid_t guid;
	u64 table;
} efi_config_table_t;

#define EFI_SYSTEM_TABLE_SIGNATURE 0x5453595320494249
#define EFI_SYSTEM_TABLE_REVISION  ((1 << 16) | 00)

typedef struct {
	efi_table_hdr_t hdr;
	u64 fw_vendor;		/* physical addr of CHAR16 vendor string */
	u32 fw_revision;
	u64 con_in_handle;
	u64 con_in;
	u64 con_out_handle;
	u64 con_out;
	u64 stderr_handle;
	u64 stderr;
	u64 runtime;
	u64 boottime;
	u64 nr_tables;
	u64 tables;
} efi_system_table_t;

/*
 * All runtime access to EFI goes through this structure:
 */
extern struct efi {
	efi_system_table_t *systab;	/* EFI system table */
	void *mps;			/* MPS table */
	void *acpi;			/* ACPI table  (IA64 ext 0.71) */
	void *acpi20;			/* ACPI table  (ACPI 2.0) */
	void *smbios;			/* SM BIOS table */
	void *sal_systab;		/* SAL system table */
	void *hcdp;			/* HCDP table */
	void *boot_info;		/* boot info table */
	efi_get_time_t *get_time;
	efi_set_time_t *set_time;
	efi_get_wakeup_time_t *get_wakeup_time;
	efi_set_wakeup_time_t *set_wakeup_time;
	efi_get_variable_t *get_variable;
	efi_get_next_variable_t *get_next_variable;
	efi_set_variable_t *set_variable;
	efi_get_next_high_mono_count_t *get_next_high_mono_count;
	efi_reset_system_t *reset_system;
} efi;

static inline int
efi_guidcmp (efi_guid_t left, efi_guid_t right)
{
	return memcmp(&left, &right, sizeof (efi_guid_t));
}

static inline char *
efi_guid_unparse(efi_guid_t *guid, char *out)
{
	sprintf(out, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		guid->b[3], guid->b[2], guid->b[1], guid->b[0],
		guid->b[5], guid->b[4], guid->b[7], guid->b[6],
		guid->b[8], guid->b[9], guid->b[10], guid->b[11],
		guid->b[12], guid->b[13], guid->b[14], guid->b[15]);
        return out;
}

extern void efi_init (void);
extern void efi_map_pal_code (void);
extern void efi_memmap_walk (efi_freemem_callback_t callback, void *arg);
extern void efi_gettimeofday (struct timeval *tv);
extern void efi_enter_virtual_mode (void);	/* switch EFI to virtual mode, if possible */
extern u64 efi_get_iobase (void);
extern u32 efi_mem_type (unsigned long phys_addr);
extern u64 efi_mem_attributes (unsigned long phys_addr);

/*
 * Variable Attributes
 */
#define EFI_VARIABLE_NON_VOLATILE       0x0000000000000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 0x0000000000000002
#define EFI_VARIABLE_RUNTIME_ACCESS     0x0000000000000004


/*
 * efi_dir is allocated in arch/ia64/kernel/efi.c.
 */
#ifdef CONFIG_PROC_FS
extern struct proc_dir_entry *efi_dir;
#endif

#endif /* _LINUX_EFI_H */
