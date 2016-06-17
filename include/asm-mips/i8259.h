/*
 *	include/asm-mips/i8259.h
 *
 *	i8259A interrupt definitions.
 *
 *	Copyright (C) 2003  Maciej W. Rozycki
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
#ifndef __ASM_MIPS_I8259_H
#define __ASM_MIPS_I8259_H

#include <linux/spinlock.h>

#include <asm/io.h>
#include <asm/system.h>

extern void init_i8259_irqs(void);

#endif /* __ASM_MIPS_I8259_H */
