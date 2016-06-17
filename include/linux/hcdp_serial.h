/*
 *  linux/include/linux/hcdp_serial.h
 *
 *  Copyright (C) 2002  Hewlett-Packard Co.
 *  Copyright (C) 2002  Khalid Aziz <khalid_aziz@hp.com>
 *
 *  Definitions for HCDP defined serial ports (Serial console and 
 *  debug ports)
 *
 */
#ifndef _LINUX_HCDP_SERIAL_H
#define _LINUX_HCDP_SERIAL_H

/* ACPI table signatures */
#define HCDP_SIG_LEN		4
#define HCDP_SIGNATURE		"HCDP"

/* Space ID as defined in ACPI generic address structure */
#define ACPI_MEM_SPACE		0
#define ACPI_IO_SPACE		1
#define ACPI_PCICONF_SPACE	2

/* 
 * Maximum number of HCDP devices we want to read in
 */
#define MAX_HCDP_DEVICES	6

/*
 * Default base baud rate if clock rate is 0 in HCDP table.
 */
#define DEFAULT_BAUD_BASE	115200

/* 
 * ACPI Generic Address Structure 
 */
typedef struct {
	u8  space_id;
	u8  bit_width;
	u8  bit_offset;
	u8  resv;
	u32 addrlo;
	u32 addrhi;
} acpi_gen_addr;

/* HCDP Device descriptor entry types */
#define HCDP_DEV_CONSOLE	0
#define HCDP_DEV_DEBUG		1

/* HCDP Device descriptor type */
typedef struct {
	u8	type;
	u8	bits;
	u8	parity;
	u8	stop_bits;
	u8	pci_seg;
	u8	pci_bus;
	u8	pci_dev;
	u8	pci_func;
	u64	baud;
	acpi_gen_addr	base_addr;
	u16	pci_dev_id;
	u16	pci_vendor_id;
	u32	global_int;
	u32	clock_rate;
	u8	pci_prog_intfc;
	u8	resv;
} hcdp_dev_t;

/* HCDP Table format */
typedef struct {
	u8	signature[4];
	u32	len;
	u8	rev;
	u8	chksum;
	u8	oemid[6];
	u8	oem_tabid[8];
	u32	oem_rev;
	u8	creator_id[4];
	u32	creator_rev;
	u32	num_entries;
	hcdp_dev_t	hcdp_dev[MAX_HCDP_DEVICES];
} hcdp_t;

#endif	/* _LINUX_HCDP_SERIAL_H */
