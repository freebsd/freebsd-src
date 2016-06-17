/*
 *  linux/include/asm-arm/arch-omaha/timex.h
 *
 *  Omaha architecture timex specifications
 *
 *  Copyright (C) 1999-2002 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Always runs at (almost) 1Mhz.
 *
 * Time is derived as follows
 *
 * CLOCK_TICK_RATE = ((FCLK / 4) / (Prescaler + 1)) / 2
 *
 * Where FCLK = CPU Clock Rate = 133 MHz
 * Prescaler = 16
 *
 * Therefore:
 * CLOCK_TICK_RATE = (33.25 MHz / 17) / 2
 *                 = 977941 Hz
 */
#define CLOCK_TICK_RATE		(977941)
