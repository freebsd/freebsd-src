/* Remote target glue for the Oki op50n based eval board.

   Copyright 1995, 1998, 1999, 2000 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "gdbcore.h"
#include "target.h"
#include "monitor.h"
#include "serial.h"

static void op50n_open (char *args, int from_tty);

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
  NULL, NULL, NULL, "cr0", "cr8", "cr9", "cr10", "cr12",
  "cr13", "cr24", "cr25", "cr26",
};

/*
 * Define the monitor command strings. Since these are passed directly
 * through to a printf style function, we need can include formatting
 * strings. We also need a CR or LF on the end.
 */

static struct target_ops op50n_ops;

static char *op50n_inits[] =
{".\r", NULL};

static struct monitor_ops op50n_cmds;

static void
init_op50n_cmds (void)
{
  op50n_cmds.flags = MO_CLR_BREAK_USES_ADDR /*| MO_GETMEM_READ_SINGLE */ ;	/* flags */
  op50n_cmds.init = op50n_inits;	/* Init strings */
  op50n_cmds.cont = "g\r";	/* continue command */
  op50n_cmds.step = "t\r";	/* single step */
  op50n_cmds.stop = "\003.\r";	/* Interrupt char */
  op50n_cmds.set_break = "b %x\r";	/* set a breakpoint */
  op50n_cmds.clr_break = "b %x;0\r";	/* clear breakpoint at addr */
  op50n_cmds.clr_all_break = "bx\r";	/* clear all breakpoints */
  op50n_cmds.fill = "fx %x s%x %x\r";	/* memory fill cmd (addr, len, val) */
  op50n_cmds.setmem.cmdb = "sx %x %x\r";	/* setmem.cmdb (addr, value) */
  op50n_cmds.setmem.cmdw = "sh %x %x\r";	/* setmem.cmdw (addr, value) */
  op50n_cmds.setmem.cmdl = "s %x %x\r";		/* setmem.cmdl (addr, value) */
  op50n_cmds.setmem.cmdll = NULL;	/* setmem.cmdll (addr, value) */
  op50n_cmds.setmem.resp_delim = NULL;	/* setmem.resp_delim */
  op50n_cmds.setmem.term = NULL;	/* setmem.term */
  op50n_cmds.setmem.term_cmd = NULL;	/* setmem.term_cmd */
#if 0
  {
    "sx %x\r",			/* getmem.cmdb (addr, len) */
      "sh %x\r",		/* getmem.cmdw (addr, len) */
      "s %x\r",			/* getmem.cmdl (addr, len) */
      NULL,			/* getmem.cmdll (addr, len) */
      " : ",			/* getmem.resp_delim */
      " ",			/* getmem.term */
      ".\r",			/* getmem.term_cmd */
  };
#else
  op50n_cmds.getmem.cmdb = "dx %x s%x\r";	/* getmem.cmdb (addr, len) */
  op50n_cmds.getmem.cmdw = NULL;	/* getmem.cmdw (addr, len) */
  op50n_cmds.getmem.cmdl = NULL;	/* getmem.cmdl (addr, len) */
  op50n_cmds.getmem.cmdll = NULL;	/* getmem.cmdll (addr, len) */
  op50n_cmds.getmem.resp_delim = " : ";		/* getmem.resp_delim */
  op50n_cmds.getmem.term = NULL;	/* getmem.term */
  op50n_cmds.getmem.term_cmd = NULL;	/* getmem.term_cmd */
#endif
  op50n_cmds.setreg.cmd = "x %s %x\r";	/* setreg.cmd (name, value) */
  op50n_cmds.setreg.resp_delim = NULL;	/* setreg.resp_delim */
  op50n_cmds.setreg.term = NULL;	/* setreg.term */
  op50n_cmds.setreg.term_cmd = NULL;	/* setreg.term_cmd */
  op50n_cmds.getreg.cmd = "x %s\r";	/* getreg.cmd (name) */
  op50n_cmds.getreg.resp_delim = "=";	/* getreg.resp_delim */
  op50n_cmds.getreg.term = " ";	/* getreg.term */
  op50n_cmds.getreg.term_cmd = ".\r";	/* getreg.term_cmd */
  op50n_cmds.dump_registers = NULL;	/* dump_registers */
  op50n_cmds.register_pattern = NULL;	/* register_pattern */
  op50n_cmds.supply_register = NULL;	/* supply_register */
  op50n_cmds.load_routine = NULL;	/* load routine */
  op50n_cmds.load = "r 0\r";	/* download command */
  op50n_cmds.loadresp = NULL;	/* load response */
  op50n_cmds.prompt = "\n#";	/* monitor command prompt */
  op50n_cmds.line_term = "\r";	/* end-of-command delimitor */
  op50n_cmds.cmd_end = NULL;	/* optional command terminator */
  op50n_cmds.target = &op50n_ops;	/* target operations */
  op50n_cmds.stopbits = SERIAL_1_STOPBITS;	/* number of stop bits */
  op50n_cmds.regnames = op50n_regnames;		/* register names */
  op50n_cmds.magic = MONITOR_OPS_MAGIC;		/* magic */
};

static void
op50n_open (char *args, int from_tty)
{
  monitor_open (args, &op50n_cmds, from_tty);
}

void
_initialize_op50n (void)
{
  init_op50n_cmds ();
  init_monitor_ops (&op50n_ops);

  op50n_ops.to_shortname = "op50n";
  op50n_ops.to_longname = "Oki's debug monitor for the Op50n Eval board";
  op50n_ops.to_doc = "Debug on a Oki OP50N eval board.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";
  op50n_ops.to_open = op50n_open;

  add_target (&op50n_ops);
}
