/*
 * Copyright 2003 PMC-Sierra
 * Author: Manish Lachwani (lachwani@pmc-sierra.com)
 *
 * Board specific definititions for the PMC-Sierra Yosemite
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __TITAN_DEP_H__
#define __TITAN_DEP_H__

#include <asm/addrspace.h>              /* for KSEG1ADDR() */
#include <asm/byteorder.h>              /* for cpu_to_le32() */

/* Turn on serial */
#define	CONFIG_TITAN_SERIAL

/* PCI */
#define	TITAN_PCI_BASE			0xbb000000

#define TITAN_WRITE(ofs, data)  \
        *(volatile u32 *)(TITAN_PCI_BASE+(ofs)) = cpu_to_le32(data)
#define TITAN_READ(ofs, data)   \
        *(data) = le32_to_cpu(*(volatile u32 *)(TITAN_PCI_BASE+(ofs)))
#define TITAN_READ_DATA(ofs)    \
        le32_to_cpu(*(volatile u32 *)(TITAN_PCI_BASE+(ofs)))

#define TITAN_WRITE_16(ofs, data)  \
        *(volatile u16 *)(TITAN_PCI_BASE+(ofs)) = cpu_to_le16(data)
#define TITAN_READ_16(ofs, data)   \
        *(data) = le16_to_cpu(*(volatile u16 *)(TITAN_PCI_BASE+(ofs)))

#define TITAN_WRITE_8(ofs, data)  \
        *(volatile u8 *)(TITAN_PCI_BASE+(ofs)) = data
#define TITAN_READ_8(ofs, data)   \
        *(data) = *(volatile u8 *)(TITAN_PCI_BASE+(ofs))

/*
 * PCI specific defines
 */
#define	TITAN_PCI_0_CONFIG_ADDRESS	0x780
#define	TITAN_PCI_0_CONFIG_DATA		0x784

/*
 * HT specific defines
 */
#define RM9000x2_HTLINK_REG     0xbb000644
#define RM9000x2_BASE_ADDR      0xbb000000
#define RM9000x2_OCD_HTCFGA     0x06f8
#define RM9000x2_OCD_HTCFGD     0x06fc

/*
 * Hypertransport specific macros
 */
#define RM9K_WRITE(ofs, data)   *(volatile u_int32_t *)(RM9000x2_BASE_ADDR+ofs) = data
#define RM9K_WRITE_8(ofs, data) *(volatile u8 *)(RM9000x2_BASE_ADDR+ofs) = data
#define RM9K_WRITE_16(ofs, data) *(volatile u16 *)(RM9000x2_BASE_ADDR+ofs) = data

#define RM9K_READ(ofs, val)     *(val) = *(volatile u_int32_t *)(RM9000x2_BASE_ADDR+ofs)
#define RM9K_READ_8(ofs, val)   *(val) = *(volatile u8 *)(RM9000x2_BASE_ADDR+ofs)
#define RM9K_READ_16(ofs, val)  *(val) = *(volatile u16 *)(RM9000x2_BASE_ADDR+ofs)

#endif 

