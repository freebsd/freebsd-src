/*
 *  linux/include/asm-arm/arch-omaha/serial.h
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
#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

#include <asm/arch/platform.h>
#include <asm/irq.h>

/*
 * Baud rate is a function of the cpu PCLK... assume 10 MHz for now.
 */
#define BASE_BAUD (10000000 / 16)

#define _SER_IRQ0	OMAHA_INT_URXD0

#define RS_TABLE_SIZE	1

#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

     /* UART CLK        PORT  IRQ     FLAGS        */
#define STD_SERIAL_PORT_DEFNS \
	{ 0, BASE_BAUD, 0x3F8, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS0 */	\

#define EXTRA_SERIAL_PORT_DEFNS

#endif
