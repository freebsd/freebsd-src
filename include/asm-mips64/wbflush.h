/*
 * Header file for using the wbflush routine
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1998 Harald Koerfgen
 * Copyright (C) 2002 Maciej W. Rozycki
 */
#ifndef __ASM_MIPS64_WBFLUSH_H
#define __ASM_MIPS64_WBFLUSH_H

#define wbflush_setup() do { } while (0)

#define wbflush() fast_iob()

#endif /* __ASM_MIPS64_WBFLUSH_H */
