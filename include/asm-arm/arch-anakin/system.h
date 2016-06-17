/*
 *  linux/include/asm-arm/arch-anakin/system.h
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   11-Apr-2001 TTC	Created
 *   04-May-2001 W/PB	Removed cpu_do_idle()
 */

#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

static inline void arch_idle(void)
{
}

static inline void
arch_reset(char mode)
{
	cpu_reset(0);
}

#endif
