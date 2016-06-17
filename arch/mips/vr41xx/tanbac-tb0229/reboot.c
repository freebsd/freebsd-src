/*
 * FILE NAME
 *	arch/mips/vr41xx/tanbac-tb0229/reboot.c
 *
 * BRIEF MODULE DESCRIPTION
 *	Depending on TANBAC TB0229(VR4131DIMM) of reboot system call.
 *
 * Copyright 2003 Megasolution Inc.
 *                matsu@megasolution.jp
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 */
#include <asm/io.h>
#include <asm/vr41xx/tb0229.h>

#define tb0229_hard_reset()	writew(0, TB0219_RESET_REGS)

void tanbac_tb0229_restart(char *command)
{
#ifdef CONFIG_TANBAC_TB0219
	local_irq_disable();
	tb0229_hard_reset();
	while (1);
#else
	vr41xx_restart(command);
#endif
}
