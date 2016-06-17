/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 1998, 2000, 2001, 2003 by Ralf Baechle
 */
#include <linux/module.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/jazzdma.h>

EXPORT_SYMBOL(vdma_alloc);
EXPORT_SYMBOL(vdma_free);
EXPORT_SYMBOL(vdma_log2phys);
