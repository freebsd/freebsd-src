/* Remote target glue for the Intel 960 MON960 ROM monitor.
   Copyright 1995, 1996, 1997, 1998, 1999, 2000
   Free Software Foundation, Inc.

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
#include "srec.h"
#include "xmodem.h"
#include "symtab.h"
#include "symfile.h"		/* for generic_load */
#include "inferior.h"		/* for write_pc() */

#define USE_GENERIC_LOAD

static struct target_ops mon960_ops;

static void mon960_open (char *args, int from_tty);

#ifdef USE_GENERIC_LOAD

static void
mon960_load_gen (char *filename, int from_tty)
{
  generic_load (filename, from_tty);
  /* Finally, make the PC point at the start address */
  if (exec_bfd)
    write_pc (bfd_get_start_address (exec_bfd));

  inferior_ptid = null_ptid;		/* No process now */
}

#else

static void
mon960_load (struct serial *desc, char *file, int hashmark)
{
  bfd *abfd;
  asection *s;
  char *buffer;
  int i;

  buffer = alloca (XMODEM_PACKETSIZE);
  abfd = bfd_openr (file, 0);
  if (!abfd)
    {
      printf_filtered ("Unable to open file %s\n", file);
      return;
    }
  if (bfd_check_format (abfd, bfd_object) == 0)
    {
      printf_filtered ("File is not an object file\n");
      return;
    }
  for (s = abfd->sections; s; s = s->next)
    if (s->flags & SEC_LOAD)
      {
	bfd_size_type section_size;
	printf_filtered ("%s\t: 0x%4x .. 0x%4x  ", s->name, s->vma,
			 s->vma + s->_raw_size);
	gdb_flush (gdb_stdout);
	monitor_printf (current_monitor->load, s->vma);
	if (current_monitor->loadresp)
	  monitor_expect (current_monitor->loadresp, NULL, 0);
	xmodem_init_xfer (desc);
	section_size = bfd_section_size (abfd, s);
	for (i = 0; i < section_size; i += XMODEM_DATASIZE)
	  {
	    int numbytes;
	    numbytes = min (XMODEM_DATASIZE, section_size - i);
	    bfd_get_section_contents (abfd, s, buffer + XMODEM_DATAOFFSET, i,
				      numbytes);
	    xmodem_send_packet (desc, buffer, numbytes, hashmark);
	    if (hashmark)
	      {
		putchar_unfiltered ('#');
		gdb_flush (gdb_stdout);
	      }
	  }			/* Per-packet (or S-record) loop */
	xmodem_finish_xfer (desc);
	monitor_expect_prompt (NULL, 0);
	putchar_unfiltered ('\n');
      }				/* Loadable sections */
  if (hashmark)
    putchar_unfiltered ('\n');
}

#endif /* USE_GENERIC_LOAD */

/* This array of registers need to match the indexes used by GDB.
   This exists because the various ROM monitors use different strings
   than does GDB, and don't necessarily support all the registers
   either. So, typing "info reg sp" becomes a "r30".  */

/* these correspond to the offsets from tm-* files from config directories */
/* g0-g14, fp, pfp, sp, rip,r3-15, pc, ac, tc, fp0-3 */
/* NOTE: "ip" is documented as "ir" in the Mon960 UG. */
/* NOTE: "ir" can't be accessed... but there's an ip and rip. */
static char *full_regnames[NUM_REGS] =
{
  /*  0 */ "pfp", "sp", "rip", "r3", "r4", "r5", "r6", "r7",
  /*  8 */ "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  /* 16 */ "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
  /* 24 */ "g8", "g9", "g10", "g11", "g12", "g13", "g14", "fp",
  /* 32 */ "pc", "ac", "tc", "ip", "fp0", "fp1", "fp2", "fp3",
};

static char *mon960_regnames[NUM_REGS];

/* Define the monitor command strings. Since these are passed directly
   through to a printf style function, we may include formatting
   strings. We also need a CR or LF on the end.  */

/* need to pause the monitor for timing reasons, so slow it down */

#if 0
/* FIXME: this extremely long init string causes MON960 to return two NAKS
   instead of performing the autobaud recognition, at least when gdb
   is running on GNU/Linux.  The short string below works on Linux, and on
   SunOS using a tcp serial connection.  Must retest on SunOS using a
   direct serial connection; if that works, get rid of the long string. */
static char *mon960_inits[] =
{"\n\r\r\r\r\r\r\r\r\r\r\r\r\r\r\n\r\n\r\n", NULL};
#else
static char *mon960_inits[] =
{"\r", NULL};
#endif

static struct monitor_ops mon960_cmds;

static void
init_mon960_cmds (void)
{
  mon960_cmds.flags = MO_CLR_BREAK_USES_ADDR
    | MO_NO_ECHO_ON_OPEN | MO_SEND_BREAK_ON_STOP | MO_GETMEM_READ_SINGLE;	/* flags */
  mon960_cmds.init = mon960_inits;	/* Init strings */
  mon960_cmds.cont = "go\n\r";	/* continue command */
  mon960_cmds.step = "st\n\r";	/* single step */
  mon960_cmds.stop = NULL;	/* break interrupts the program */
  mon960_cmds.set_break = NULL;	/* set a breakpoint */
  mon960_cmds.clr_break =	/* can't use "br" because only 2 hw bps are supported */
    mon960_cmds.clr_all_break = NULL;	/* clear a breakpoint - "de" is for hw bps */
  NULL,				/* clear all breakpoints */
    mon960_cmds.fill = NULL;	/* fill (start end val) */
  /* can't use "fi" because it takes words, not bytes */
  /* can't use "mb", "md" or "mo" because they require interaction */
  mon960_cmds.setmem.cmdb = NULL;	/* setmem.cmdb (addr, value) */
  mon960_cmds.setmem.cmdw = NULL;	/* setmem.cmdw (addr, value) */
  mon960_cmds.setmem.cmdl = "md %x %x\n\r";	/* setmem.cmdl (addr, value) */
  mon960_cmds.setmem.cmdll = NULL;	/* setmem.cmdll (addr, value) */
  mon960_cmds.setmem.resp_delim = NULL;		/* setmem.resp_delim */
  mon960_cmds.setmem.term = NULL;	/* setmem.term */
  mon960_cmds.setmem.term_cmd = NULL;	/* setmem.term_cmd */
  /* since the parsing of multiple bytes is difficult due to
     interspersed addresses, we'll only read 1 value at a time,
     even tho these can handle a count */
  mon960_cmds.getmem.cmdb = "db %x\n\r";	/* getmem.cmdb (addr, #bytes) */
  mon960_cmds.getmem.cmdw = "ds %x\n\r";	/* getmem.cmdw (addr, #swords) */
  mon960_cmds.getmem.cmdl = "di %x\n\r";	/* getmem.cmdl (addr, #words) */
  mon960_cmds.getmem.cmdll = "dd %x\n\r";	/* getmem.cmdll (addr, #dwords) */
  mon960_cmds.getmem.resp_delim = " : ";	/* getmem.resp_delim */
  mon960_cmds.getmem.term = NULL;	/* getmem.term */
  mon960_cmds.getmem.term_cmd = NULL;	/* getmem.term_cmd */
  mon960_cmds.setreg.cmd = "md %s %x\n\r";	/* setreg.cmd (name, value) */
  mon960_cmds.setreg.resp_delim = NULL;		/* setreg.resp_delim */
  mon960_cmds.setreg.term = NULL;	/* setreg.term */
  mon960_cmds.setreg.term_cmd = NULL,	/* setreg.term_cmd */
    mon960_cmds.getreg.cmd = "di %s\n\r";	/* getreg.cmd (name) */
  mon960_cmds.getreg.resp_delim = " : ";	/* getreg.resp_delim */
  mon960_cmds.getreg.term = NULL;	/* getreg.term */
  mon960_cmds.getreg.term_cmd = NULL;	/* getreg.term_cmd */
  mon960_cmds.dump_registers = "re\n\r";	/* dump_registers */
  mon960_cmds.register_pattern = "\\(\\w+\\)=\\([0-9a-fA-F]+\\)";	/* register_pattern */
  mon960_cmds.supply_register = NULL;	/* supply_register */
#ifdef USE_GENERIC_LOAD
  mon960_cmds.load_routine = NULL;	/* load_routine (defaults to SRECs) */
  mon960_cmds.load = NULL;	/* download command */
  mon960_cmds.loadresp = NULL;	/* load response */
#else
  mon960_cmds.load_routine = mon960_load;	/* load_routine (defaults to SRECs) */
  mon960_cmds.load = "do\n\r";	/* download command */
  mon960_cmds.loadresp = "Downloading\n\r";	/* load response */
#endif
  mon960_cmds.prompt = "=>";	/* monitor command prompt */
  mon960_cmds.line_term = "\n\r";	/* end-of-command delimitor */
  mon960_cmds.cmd_end = NULL;	/* optional command terminator */
  mon960_cmds.target = &mon960_ops;	/* target operations */
  mon960_cmds.stopbits = SERIAL_1_STOPBITS;	/* number of stop bits */
  mon960_cmds.regnames = mon960_regnames;	/* registers names */
  mon960_cmds.magic = MONITOR_OPS_MAGIC;	/* magic */
};

static void
mon960_open (char *args, int from_tty)
{
  char buf[64];

  monitor_open (args, &mon960_cmds, from_tty);

  /* Attempt to fetch the value of the first floating point register (fp0).
     If the monitor returns a string containing the word "Bad" we'll assume
     this processor has no floating point registers, and nullify the 
     regnames entries that refer to FP registers.  */

  monitor_printf (mon960_cmds.getreg.cmd, full_regnames[FP0_REGNUM]);	/* di fp0 */
  if (monitor_expect_prompt (buf, sizeof (buf)) != -1)
    if (strstr (buf, "Bad") != NULL)
      {
	int i;

	for (i = FP0_REGNUM; i < FP0_REGNUM + 4; i++)
	  mon960_regnames[i] = NULL;
      }
}

void
_initialize_mon960 (void)
{
  memcpy (mon960_regnames, full_regnames, sizeof (full_regnames));

  init_mon960_cmds ();

  init_monitor_ops (&mon960_ops);

  mon960_ops.to_shortname = "mon960";	/* for the target command */
  mon960_ops.to_longname = "Intel 960 MON960 monitor";
#ifdef USE_GENERIC_LOAD
  mon960_ops.to_load = mon960_load_gen;		/* FIXME - should go back and try "do" */
#endif
  /* use SW breaks; target only supports 2 HW breakpoints */
  mon960_ops.to_insert_breakpoint = memory_insert_breakpoint;
  mon960_ops.to_remove_breakpoint = memory_remove_breakpoint;

  mon960_ops.to_doc =
    "Use an Intel 960 board running the MON960 debug monitor.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";

  mon960_ops.to_open = mon960_open;
  add_target (&mon960_ops);
}
