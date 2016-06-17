/* 
 * kgdb.h: Defines and declarations for serial line source level
 *         remote debugging of the Linux kernel using gdb.
 *
 * PPC Mods (C) 1998 Michael Tesch (tesch@cs.wisc.edu)
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _PPC64_KGDB_H
#define _PPC64_KGDB_H

#ifndef __ASSEMBLY__
/* To initialize the serial, first thing called */
extern void zs_kgdb_hook(int tty_num);
/* To init the kgdb engine. (called by serial hook)*/
extern void set_debug_traps(void);

/* To enter the debugger explicitly. */
extern void breakpoint(void);

/* For taking exceptions
 * these are defined in traps.c
 */
extern void (*debugger)(struct pt_regs *regs);
extern int (*debugger_bpt)(struct pt_regs *regs);
extern int (*debugger_sstep)(struct pt_regs *regs);
extern int (*debugger_iabr_match)(struct pt_regs *regs);
extern int (*debugger_dabr_match)(struct pt_regs *regs);
extern void (*debugger_fault_handler)(struct pt_regs *regs);

/* What we bring to the party */
int kgdb_bpt(struct pt_regs *regs);
int kgdb_sstep(struct pt_regs *regs);
void kgdb(struct pt_regs *regs);
int kgdb_iabr_match(struct pt_regs *regs);
int kgdb_dabr_match(struct pt_regs *regs);
static void kgdb_fault_handler(struct pt_regs *regs);
static void handle_exception (struct pt_regs *regs);

/*
 * external low-level support routines (ie macserial.c)
 */
extern void kgdb_interruptible(int); /* control interrupts from serial */
extern void putDebugChar(char);   /* write a single character      */
extern char getDebugChar(void);   /* read and return a single char */

#endif /* !(__ASSEMBLY__) */
#endif /* !(_PPC64_KGDB_H) */
