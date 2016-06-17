/*
 *  linux/include/asm-arm/serial.h
 *
 *  Copyright (C) 1996 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   15-10-1996	RMK	Created
 */

#ifndef __ASM_SERIAL_H
#define __ASM_SERIAL_H

#include <asm/arch/serial.h>

#define SERIAL_PORT_DFNS		\
	STD_SERIAL_PORT_DEFNS		\
	EXTRA_SERIAL_PORT_DEFNS

#endif
