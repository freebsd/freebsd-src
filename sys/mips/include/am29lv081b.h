/*
 * Copyright (c) 2000-2003, 2005, Juniper Networks, Inc.
 * All rights reserved.
 * JNPR: am29lv081b.h,v 1.1 2006/08/07 05:38:57 katta
 *
 * am29lv081b.h -- am29lv081b definitions 
 *
 * Chan Lee, May 2000
 */
// $FreeBSD$

#ifndef __AM29LV081B_H__
#define	__AM29LV081B_H__

/*
 * Identifiers for the am29lv081b chip
 */
#define	AM29L_MAN_ID 0x01
#define	AM29L_DEV_ID 0x38

#define	AM29L_DEV_ID_OFFSET  0x01

#define	AM29L_TIMEOUT        3000	/* 3 seconds in ms */
#define	AM29L_ERASE_TIME     30000	/* 30 seconds in ms */

/*
 * This is defined for human consumption.
 */
#define	AM29L_BANNER "AMD29L081B 8Mb flash"

/*
 * Sector definitions.
 */

#define	AM29L_SA0	0x00000
#define	AM29L_SA1	0x10000
#define	AM29L_SA2	0x20000
#define	AM29L_SA3	0x30000
#define	AM29L_SA4	0x40000
#define	AM29L_SA5	0x50000
#define	AM29L_SA6	0x60000
#define	AM29L_SA7	0x70000
#define	AM29L_SA8	0x80000
#define	AM29L_SA9	0x90000
#define	AM29L_SA10	0xA0000
#define	AM29L_SA11	0xB0000
#define	AM29L_SA12	0xC0000
#define	AM29L_SA13	0xD0000
#define	AM29L_SA14	0xE0000
#define	AM29L_SA15	0xF0000

#define	AM29L_BANK_MASK		0xFFF00000
#define	AM29L_SECTOR_MASK	0xFFFF0000
#define	AM29L_SECTOR_SIZE	0x10000
#define	AM29L_SECTOR_PER_BLK	4
#define	AM29L_TOTAL_SECTORS	16
#define	AM29L_PROTECT_OFFSET	0x2

/*
 * Definitions for the unlock sequence, both
 * the address offset and the data definition.
 */
#define	AM29L_ULCK_ADDR1 0x555
#define	AM29L_ULCK_ADDR2 0x2AA

#define	AM29L_ULCK_DATA1 0xAA
#define	AM29L_ULCK_DATA2 0x55

/*
 * Command definitions for the am29lv081b. Most
 * of the following command can only be issue
 * after the unlock command sequence.
 */

#define	AM29L_CMD_AUTO		0x90
#define	AM29L_CMD_BYTE_PROGRAM	0xA0
#define	AM29L_CMD_ERASE		0x80
#define	AM29L_CMD_ERASE_CHIP	0x10
#define	AM29L_CMD_ERASE_SECT	0x30
#define	AM29L_CMD_RESET		0xF0

/*
 * Masks for get the DQ3, DQ5, DQ6, DQ7 bits.
 * All these bits signals the status of the
 * command operations.
 */

#define	AM29L_DQ2_MASK 0x04
#define	AM29L_DQ3_MASK 0x08
#define	AM29L_DQ5_MASK 0x20
#define	AM29L_DQ6_MASK 0x40
#define	AM29L_DQ7_MASK 0x80

#define	AM29L_GET_DQ2(data) ((data & AM29L_DQ2_MASK) >> 2)
#define	AM29L_GET_DQ3(data) ((data & AM29L_DQ3_MASK) >> 3)
#define	AM29L_GET_DQ5(data) ((data & AM29L_DQ5_MASK) >> 5)
#define	AM29L_GET_DQ6(data) ((data & AM29L_DQ6_MASK) >> 6)
#define	AM29L_GET_DQ7(data) ((data & AM29L_DQ7_MASK) >> 7)

extern void flash_add_amd29l081b (flash_device_t *dev);

static inline u_int32_t 
am29f_start_addr_flash(u_int8_t *ptr) 
{

	return((u_int32_t)ptr & AM29L_SECTOR_MASK);
}

#endif /* __AM29LV081B_H_ */

/* End of file */
