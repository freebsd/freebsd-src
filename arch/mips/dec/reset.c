/*
 * Reset a DECstation machine.
 *
 * Copyright (C) 199x  the Anonymous
 * Copyright (C) 2001, 2002, 2003  Maciej W. Rozycki
 */

#include <asm/addrspace.h>
#include <asm/ptrace.h>

#define back_to_prom()	(((void (*)(void))KSEG1ADDR(0x1fc00000))())

void dec_machine_restart(char *command)
{
	back_to_prom();
}

void dec_machine_halt(void)
{
	back_to_prom();
}

void dec_machine_power_off(void)
{
    /* DECstations don't have a software power switch */
	back_to_prom();
}

void dec_intr_halt(int irq, void *dev_id, struct pt_regs *regs)
{
	dec_machine_halt();
}
