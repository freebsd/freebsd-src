/*
 * linux/arch/arm/boot/compressed/head-at91rm9200.S
 *
 *  Copyright (C) 2003 SAN People
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <asm/mach-types.h>

		.section	".start", "ax"
		mov	r7, #MACH_TYPE_AT91RM9200
