/* Remote target glue for the ROM68K ROM monitor.
   Copyright 1988, 1991, 1992, 1993, 1994, 1995 Free Software Foundation, Inc.

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

static void rom68k_open PARAMS ((char *args, int from_tty));

static void
rom68k_supply_register (regname, regnamelen, val, vallen)
     char *regname;
     int regnamelen;
     char *val;
     int vallen;
{
  int numregs;
  int regno;

  numregs = 1;
  regno = -1;

  if (regnamelen == 2)
    switch (regname[0])
      {
      case 'S':
	if (regname[1] == 'R')
	  regno = PS_REGNUM;
	break;
      case 'P':
	if (regname[1] == 'C')
	  regno = PC_REGNUM;
	break;
      case 'D':
	if (regname[1] != 'R')
	  break;
	regno = D0_REGNUM;
	numregs = 8;
	break;
      case 'A':
	if (regname[1] != 'R')
	  break;
	regno = A0_REGNUM;
	numregs = 7;
	break;
      }
  else if (regnamelen == 3)
    switch (regname[0])
      {
      case 'I':
	if (regname[1] == 'S' && regname[2] == 'P')
	  regno = SP_REGNUM;
      }

  if (regno >= 0)
    while (numregs-- > 0)
      val = monitor_supply_register (regno++, val);
}

/* This array of registers need to match the indexes used by GDB.
   This exists because the various ROM monitors use different strings
   than does GDB, and don't necessarily support all the registers
   either. So, typing "info reg sp" becomes a "r30".  */

static char *rom68k_regnames[NUM_REGS] = {
  "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
  "A0", "A1", "A2", "A3", "A4", "A5", "A6", "ISP",
  "SR", "PC" };

/* Define the monitor command strings. Since these are passed directly
   through to a printf style function, we may include formatting
   strings. We also need a CR or LF on the end.  */

static struct target_ops rom68k_ops;

static char *rom68k_inits[] = {".\r\r", NULL}; /* Exits pm/pr & download cmds */

static struct monitor_ops rom68k_cmds =
{
  0,				/* flags */
  rom68k_inits,			/* monitor init string */
  "go\r",			/* continue command */
  "st\r",			/* single step */
  NULL,				/* No way to interrupt program */
  "db %x\r",			/* set a breakpoint */
  "cb %x\r",			/* clear a breakpoint */
  "cb *\r",			/* clear all breakpoints */
  "fm %x %x %x\r",		/* fill (start len val) */
  {
    "pm %x %x\r",		/* setmem.cmdb (addr, value) */
    "pm.w %x %x\r",		/* setmem.cmdw (addr, value) */
    "pm.l %x %x\r",		/* setmem.cmdl (addr, value) */
    NULL,			/* setmem.cmdll (addr, value) */
    NULL,			/* setreg.resp_delim */
    NULL,			/* setreg.term */
    NULL,			/* setreg.term_cmd */
  },
  {
    "dm %x %x\r",		/* getmem.cmdb (addr, len) */
    "dm.w %x %x\r",		/* getmem.cmdw (addr, len) */
    "dm.l %x %x\r",		/* getmem.cmdl (addr, len) */
    NULL,			/* getmem.cmdll (addr, len) */
    "  ",			/* getmem.resp_delim */
    NULL,			/* getmem.term */
    NULL,			/* getmem.term_cmd */
  },
  {
    "pr %s %x\r",		/* setreg.cmd (name, value) */
    NULL,			/* setreg.resp_delim */
    NULL,			/* setreg.term */
    NULL			/* setreg.term_cmd */
  },
  {
    "pr %s\r",			/* getreg.cmd (name) */
    ":  ",			/* getreg.resp_delim */
    "= ",			/* getreg.term */
    ".\r"			/* getreg.term_cmd */
  },
  "dr\r",			/* dump_registers */
				/* register_pattern */
  "\\(\\w+\\)=\\([0-9a-fA-F]+\\( +[0-9a-fA-F]+\\b\\)*\\)",
  rom68k_supply_register,	/* supply_register */
  NULL,				/* load_routine (defaults to SRECs) */
  "dc\r",			/* download command */
  "Waiting for S-records from host... ", /* Load response */
  "ROM68K :-> ",		/* monitor command prompt */
  "\r",				/* end-of-line terminator */
  ".\r",			/* optional command terminator */
  &rom68k_ops,			/* target operations */
  SERIAL_1_STOPBITS,		/* number of stop bits */
  rom68k_regnames,		/* registers names */
  MONITOR_OPS_MAGIC		/* magic */
};

static void
rom68k_open (args, from_tty)
     char *args;
     int from_tty;
{
  monitor_open (args, &rom68k_cmds, from_tty);
}

void
_initialize_rom68k ()
{
  init_monitor_ops (&rom68k_ops);

  rom68k_ops.to_shortname = "rom68k";
  rom68k_ops.to_longname = "Rom68k debug monitor for the IDP Eval board";
  rom68k_ops.to_doc = "Debug on a Motorola IDP eval board running the ROM68K monitor.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";
  rom68k_ops.to_open = rom68k_open;

  add_target (&rom68k_ops);
}
