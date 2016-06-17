/*
 * linux/include/asm-arm/arch-tbox/dma.h
 *
 * Architecture DMA routines.  We have to contend with the bizarre DMA
 * machine built into the Tbox hardware.
 *
 * Copyright (C) 1998 Philip Blundell
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

/*
 * DMA channel definitions.  Some of these are physically strange but
 * we sort it out inside dma.c so the user never has to care.  The
 * exception is the double-buffering which we can't really abstract
 * away sensibly.
 */
#define DMA_VIDEO			0
#define DMA_MPEG_B			1
#define DMA_AUDIO_B			2
#define DMA_ASHRX_B			3
#define DMA_ASHTX			4
#define DMA_MPEG			5
#define DMA_AUDIO			6
#define DMA_ASHRX			7

#define MAX_DMA_CHANNELS		0	/* XXX */

/*
 * This is the maximum DMA address that can be DMAd to.
 */
#define MAX_DMA_ADDRESS		0xffffffff
