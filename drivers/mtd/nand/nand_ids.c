/*
 *  drivers/mtd/nandids.c
 *
 *  Copyright (C) 2002 Thomas Gleixner (tglx@linutronix.de)
 *
 *
 * $Id: nand_ids.c,v 1.1 2002/12/02 22:06:04 gleixner Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/mtd/nand.h>

/*
*	Chip ID list
*/
struct nand_flash_dev nand_flash_ids[] = {
	{"NAND 1MB 5V", 0x6e, 20, 0x1000, 1},	// 1Mb 5V
	{"NAND 2MB 5V", 0x64, 21, 0x1000, 1},	// 2Mb 5V
	{"NAND 4MB 5V", 0x6b, 22, 0x2000, 0},	// 4Mb 5V
	{"NAND 1MB 3,3V", 0xe8, 20, 0x1000, 1},	// 1Mb 3.3V
	{"NAND 1MB 3,3V", 0xec, 20, 0x1000, 1},	// 1Mb 3.3V
	{"NAND 2MB 3,3V", 0xea, 21, 0x1000, 1},	// 2Mb 3.3V
	{"NAND 4MB 3,3V", 0xd5, 22, 0x2000, 0},	// 4Mb 3.3V
	{"NAND 4MB 3,3V", 0xe3, 22, 0x2000, 0},	// 4Mb 3.3V
	{"NAND 4MB 3,3V", 0xe5, 22, 0x2000, 0},	// 4Mb 3.3V
	{"NAND 8MB 3,3V", 0xd6, 23, 0x2000, 0},	// 8Mb 3.3V
	{"NAND 8MB 3,3V", 0xe6, 23, 0x2000, 0},	// 8Mb 3.3V
	{"NAND 16MB 3,3V", 0x73, 24, 0x4000, 0},// 16Mb 3,3V
	{"NAND 32MB 3,3V", 0x75, 25, 0x4000, 0}, // 32Mb 3,3V
	{"NAND 64MB 3,3V", 0x76, 26, 0x4000, 0}, // 64Mb 3,3V
	{"NAND 128MB 3,3V", 0x79, 27, 0x4000, 0}, // 128Mb 3,3V
	{NULL,}
};

/*
*	Manufacturer ID list
*/
struct nand_manufacturers nand_manuf_ids[] = {
	{NAND_MFR_TOSHIBA, "Toshiba"},
	{NAND_MFR_SAMSUNG, "Samsung"},
	{NAND_MFR_FUJITSU, "Fujitsu"},
	{NAND_MFR_NATIONAL, "National"},
	{0x0, "Unknown"}
};


EXPORT_SYMBOL (nand_manuf_ids);
EXPORT_SYMBOL (nand_flash_ids);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Thomas Gleixner <tglx@linutronix.de>");
MODULE_DESCRIPTION ("Nand device & manufacturer ID's");
