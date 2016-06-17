/*
 *  linux/include/asm-arm/arch-ebsa285/timex.h
 *
 *  Copyright (C) 1998 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  EBSA285 architecture timex specifications
 */

/*
 * On EBSA285 boards, the clock runs at 50MHz and is
 * divided by a 4-bit prescaler.  Other boards use an
 * ISA derived timer, and this is unused.
 */
#define CLOCK_TICK_RATE		(mem_fclk_21285 / 16)
