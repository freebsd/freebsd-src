/* Remote target glue for the Oki op50n based eval board.

   Copyright 1995 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "gdbcore.h"
#include "target.h"
#include "monitor.h"
#include "serial.h"

static void op50n_open PARAMS ((char *args, int from_tty));

/*
 * this array of registers need to match the indexes used by GDB. The
 * whole reason this exists is cause the various ROM monitors use
 * different strings than GDB does, and doesn't support all the
 * registers either. So, typing "info reg sp" becomes a "r30".
 */

static char *op50n_regnames[NUM_REGS] =
{
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
  "cr11", "p", NULL, NULL, NULL, "cr15", "cr19", "cr20",
  "cr21", "cr22", NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, "cr0", "cr8", "cr9", "cr10","cr12",
  "cr13", "cr24", "cr25", "cr26",
};

/*
 * Define the monitor command strings. Since these are passed directly
 * through to a printf style function, we need can include formatting
 * strings. We also need a CR or LF on the end.
 */

static struct target_ops op50n_ops;

static char *op50n_inits[] = {".\r", NULL};

static struct monitor_ops op50n_cmds =
{
  MO_CLR_BREAK_USES_ADDR /*| MO_GETMEM_READ_SINGLE*/, /* flags */
  op50n_inits,			/* Init strings */
  "g\r",			/* continue command */
  "t\r",			/* single step */
  "\003.\r",			/* Interrupt char */
  "b %x\r",			/* set a breakpoint */
  "b %x,0\r",			/* clear breakpoint at addr */
  "bx\r",			/* clear all breakpoints */
  "fx %x s%x %x\r",		/* memory fill cmd (addr, len, val) */
  {
    "sx %x %x\r",		/* setmem.cmdb (addr, value) */
    "sh %x %x\r",		/* setmem.cmdw (addr, value) */
    "s %x %x\r",		/* setmem.cmdl (addr, value) */
    NULL,			/* setmem.cmdll (addr, value) */
    NULL,			/* setmem.resp_delim */
    NULL,			/* setmem.term */
    NULL,			/* setmem.term_cmd */
  },
#if 0
  {
    "sx %x\r",			/* getmem.cmdb (addr, len) */
    "sh %x\r",			/* getmem.cmdw (addr, len) */
    "s %x\r",			/* getmem.cmdl (addr, len) */
    NULL,			/* getmem.cmdll (addr, len) */
    " : ",			/* getmem.resp_delim */
    " ",			/* getmem.term */
    ".\r",			/* getmem.term_cmd */
  },
#else
  {
    "dx %x s%x\r",		/* getmem.cmdb (addr, len) */
    NULL,			/* getmem.cmdw (addr, len) */
    NULL,			/* getmem.cmdl (addr, len) */
    NULL,			/* getmem.cmdll (addr, len) */
    " : ",			/* getmem.resp_delim */
    NULL,			/* getmem.term */
    NULL,			/* getmem.term_cmd */
  },
#endif
  {
    "x %s %x\r",		/* setreg.cmd (name, value) */
    NULL,			/* setreg.resp_delim */
    NULL,			/* setreg.term */
    NULL,			/* setreg.term_cmd */
  },
  {
    "x %s\r",			/* getreg.cmd (name) */
    "=",			/* getreg.resp_delim */
    " ",			/* getreg.term */
    ".\r",			/* getreg.term_cmd */
  },
  NULL,				/* dump_registers */
  NULL,				/* register_pattern */
  NULL,				/* supply_register */
  NULL,				/* load routine */
  "r 0\r",			/* download command */
  NULL,				/* load response */
  "\n#",			/* monitor command prompt */
  "\r",				/* end-of-command delimitor */
  NULL,				/* optional command terminator */
  &op50n_ops,			/* target operations */
  SERIAL_1_STOPBITS,		/* number of stop bits */
  op50n_regnames,		/* register names */
  MONITOR_OPS_MAGIC		/* magic */
  };

static void
op50n_open (args, from_tty)
     char *args;
     int from_tty;
{
  monitor_open (args, &op50n_cmds, from_tty);
}

void
_initialize_op50n ()
{
  init_monitor_ops (&op50n_ops);

  op50n_ops.to_shortname = "op50n";
  op50n_ops.to_longname = "Oki's debug monitor for the Op50n Eval board";
  op50n_ops.to_doc = "Debug on a Oki OP50N eval board.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";
  op50n_ops.to_open = op50n_open;

  add_target (&op50n_ops);
}
