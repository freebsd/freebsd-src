/*
 *  linux/include/asm-arm/arch-arc/dma.h
 *
 *  Copyright (C) 1996-1998 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Acorn Archimedes/A5000 architecture virtual DMA
 *  implementation
 *
 *  Modifications:
 *   04-04-1998	RMK	Merged arc and a5k versions
 */
#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

#define MAX_DMA_ADDRESS		0x03000000
#define MAX_DMA_CHANNELS	3

/* ARC */
#define DMA_VIRTUAL_FLOPPY0	0
#define DMA_VIRTUAL_FLOPPY1	1
#define DMA_VIRTUAL_SOUND	2

/* A5K */
#define DMA_FLOPPY		DMA_VIRTUAL_FLOPPY0

#endif /* _ASM_ARCH_DMA_H */
