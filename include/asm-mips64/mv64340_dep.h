/*
 * Copyright 2002 Momentum Computer Inc.
 * Author: Matthew Dharm <mdharm@momenco.com>
 *
 * include/asm-mips/mv64340-dep.h
 *     Board-dependent definitions for MV-64340 chip.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __MV64340_DEP_H__
#define __MV64340_DEP_H__

#include <asm/addrspace.h>		/* for KSEG1ADDR() */
#include <asm/byteorder.h>		/* for cpu_to_le32() */

extern unsigned long mv64340_base;

#define MV64340_BASE       (mv64340_base)

/*
 * Because of an error/peculiarity in the Galileo chip, we need to swap the
 * bytes when running bigendian.
 */

#define MV_WRITE(ofs, data)  \
        *(volatile u32 *)(MV64340_BASE+(ofs)) = cpu_to_le32(data)
#define MV_READ(ofs, data)   \
        *(data) = le32_to_cpu(*(volatile u32 *)(MV64340_BASE+(ofs)))
#define MV_READ_DATA(ofs)    \
        le32_to_cpu(*(volatile u32 *)(MV64340_BASE+(ofs)))

#define MV_WRITE_16(ofs, data)  \
        *(volatile u16 *)(MV64340_BASE+(ofs)) = cpu_to_le16(data)
#define MV_READ_16(ofs, data)   \
        *(data) = le16_to_cpu(*(volatile u16 *)(MV64340_BASE+(ofs)))

#define MV_WRITE_8(ofs, data)  \
        *(volatile u8 *)(MV64340_BASE+(ofs)) = data
#define MV_READ_8(ofs, data)   \
        *(data) = *(volatile u8 *)(MV64340_BASE+(ofs))

#define MV_SET_REG_BITS(ofs,bits) \
	(*((volatile u32 *)(MV64340_BASE+(ofs)))) |= ((u32)cpu_to_le32(bits))
#define MV_RESET_REG_BITS(ofs,bits) \
	(*((volatile u32 *)(MV64340_BASE+(ofs)))) &= ~((u32)cpu_to_le32(bits))

#endif
