/* Remote target glue for the SPARC Sparclet ROM monitor.

   Copyright 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002 Free
   Software Foundation, Inc.

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
#include "symtab.h"
#include "symfile.h"		/* for generic_load */
#include "regcache.h"
#include <time.h>

extern void report_transfer_performance (unsigned long, time_t, time_t);

static struct target_ops sparclet_ops;

static void sparclet_open (char *args, int from_tty);

/* This array of registers need to match the indexes used by GDB.
   This exists because the various ROM monitors use different strings
   than does GDB, and don't necessarily support all the registers
   either. So, typing "info reg sp" becomes a "r30".  */

/*PSR 0x00000080  impl ver icc AW LE EE EC EF PIL S PS ET CWP  WIM
   0x0  0x0 0x0  0  0  0  0  0 0x0 1  0  0 0x00 0x2
   0000010
   INS        LOCALS       OUTS      GLOBALS
   0  0x00000000  0x00000000  0x00000000  0x00000000
   1  0x00000000  0x00000000  0x00000000  0x00000000
   2  0x00000000  0x00000000  0x00000000  0x00000000
   3  0x00000000  0x00000000  0x00000000  0x00000000
   4  0x00000000  0x00000000  0x00000000  0x00000000
   5  0x00000000  0x00001000  0x00000000  0x00000000
   6  0x00000000  0x00000000  0x123f0000  0x00000000
   7  0x00000000  0x00000000  0x00000000  0x00000000
   pc:  0x12010000 0x00000000    unimp
   npc: 0x12010004 0x00001000    unimp     0x1000
   tbr: 0x00000000
   y:   0x00000000
 */
/* these correspond to the offsets from tm-* files from config directories */

/* is wim part of psr?? */
/* monitor wants lower case */
static char *sparclet_regnames[] = {
  "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7", 
  "o0", "o1", "o2", "o3", "o4", "o5", "o6", "o7", 
  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7", 
  "i0", "i1", "i2", "i3", "i4", "i5", "i6", "i7", 

  "", "", "", "", "", "", "", "", /* no FPU regs */
  "", "", "", "", "", "", "", "", 
  "", "", "", "", "", "", "", "", 
  "", "", "", "", "", "", "", "", 
				  /* no CPSR, FPSR */
  "y", "psr", "wim", "tbr", "pc", "npc", "", "", 

  "ccsr", "ccpr", "cccrcr", "ccor", "ccobr", "ccibr", "ccir", "", 

  /*       ASR15                 ASR19 (don't display them) */  
  "asr1",  "", "asr17", "asr18", "", "asr20", "asr21", "asr22", 
/*
  "awr0",  "awr1",  "awr2",  "awr3",  "awr4",  "awr5",  "awr6",  "awr7",  
  "awr8",  "awr9",  "awr10", "awr11", "awr12", "awr13", "awr14", "awr15", 
  "awr16", "awr17", "awr18", "awr19", "awr20", "awr21", "awr22", "awr23", 
  "awr24", "awr25", "awr26", "awr27", "awr28", "awr29", "awr30", "awr31", 
  "apsr",
 */
};



/* Function: sparclet_supply_register
   Just returns with no action.
   This function is required, because parse_register_dump (monitor.c)
   expects to be able to call it.  If we don't supply something, it will
   call a null pointer and core-dump.  Since this function does not 
   actually do anything, GDB will request the registers individually.  */

static void
sparclet_supply_register (char *regname, int regnamelen, char *val, int vallen)
{
  return;
}

static void
sparclet_load (struct serial *desc, char *file, int hashmark)
{
  bfd *abfd;
  asection *s;
  int i;
  CORE_ADDR load_offset;
  time_t start_time, end_time;
  unsigned long data_count = 0;

  /* enable user to specify address for downloading as 2nd arg to load */

  i = sscanf (file, "%*s 0x%lx", &load_offset);
  if (i >= 1)
    {
      char *p;

      for (p = file; *p != '\000' && !isspace (*p); p++);

      *p = '\000';
    }
  else
    load_offset = 0;

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

  start_time = time (NULL);

  for (s = abfd->sections; s; s = s->next)
    if (s->flags & SEC_LOAD)
      {
	bfd_size_type section_size;
	bfd_vma vma;

	vma = bfd_get_section_vma (abfd, s) + load_offset;
	section_size = bfd_section_size (abfd, s);

	data_count += section_size;

	printf_filtered ("%s\t: 0x%4x .. 0x%4x  ",
			 bfd_get_section_name (abfd, s), vma,
			 vma + section_size);
	gdb_flush (gdb_stdout);

	monitor_printf ("load c r %x %x\r", vma, section_size);

	monitor_expect ("load: loading ", NULL, 0);
	monitor_expect ("\r", NULL, 0);

	for (i = 0; i < section_size; i += 2048)
	  {
	    int numbytes;
	    char buf[2048];

	    numbytes = min (sizeof buf, section_size - i);

	    bfd_get_section_contents (abfd, s, buf, i, numbytes);

	    serial_write (desc, buf, numbytes);

	    if (hashmark)
	      {
		putchar_unfiltered ('#');
		gdb_flush (gdb_stdout);
	      }
	  }			/* Per-packet (or S-record) loop */

	monitor_expect_prompt (NULL, 0);

	putchar_unfiltered ('\n');
      }				/* Loadable sections */

  monitor_printf ("reg pc %x\r", bfd_get_start_address (abfd));
  monitor_expect_prompt (NULL, 0);
  monitor_printf ("reg npc %x\r", bfd_get_start_address (abfd) + 4);
  monitor_expect_prompt (NULL, 0);

  monitor_printf ("run\r");

  end_time = time (NULL);

  if (hashmark)
    putchar_unfiltered ('\n');

  report_transfer_performance (data_count, start_time, end_time);

  pop_target ();
  push_remote_target (monitor_get_dev_name (), 1);

  throw_exception (RETURN_QUIT);
}

/* Define the monitor command strings. Since these are passed directly
   through to a printf style function, we may include formatting
   strings. We also need a CR or LF on the end.  */

/* need to pause the monitor for timing reasons, so slow it down */

static char *sparclet_inits[] =
{"\n\r\r\n", NULL};

static struct monitor_ops sparclet_cmds;

static void
init_sparclet_cmds (void)
{
  sparclet_cmds.flags = MO_CLR_BREAK_USES_ADDR |
    MO_HEX_PREFIX |
    MO_NO_ECHO_ON_OPEN |
    MO_NO_ECHO_ON_SETMEM |
    MO_RUN_FIRST_TIME |
    MO_GETMEM_READ_SINGLE;	/* flags */
  sparclet_cmds.init = sparclet_inits;	/* Init strings */
  sparclet_cmds.cont = "cont\r";	/* continue command */
  sparclet_cmds.step = "step\r";	/* single step */
  sparclet_cmds.stop = "\r";	/* break interrupts the program */
  sparclet_cmds.set_break = "+bp %x\r";		/* set a breakpoint */
  sparclet_cmds.clr_break = "-bp %x\r";		/* can't use "br" because only 2 hw bps are supported */
  sparclet_cmds.clr_all_break = "-bp %x\r";	/* clear a breakpoint */
  "-bp\r";			/* clear all breakpoints */
  sparclet_cmds.fill = "fill %x -n %x -v %x -b\r";	/* fill (start length val) */
  /* can't use "fi" because it takes words, not bytes */
  /* ex [addr] [-n count] [-b|-s|-l]          default: ex cur -n 1 -b */
  sparclet_cmds.setmem.cmdb = "ex %x -b\r%x\rq\r";	/* setmem.cmdb (addr, value) */
  sparclet_cmds.setmem.cmdw = "ex %x -s\r%x\rq\r";	/* setmem.cmdw (addr, value) */
  sparclet_cmds.setmem.cmdl = "ex %x -l\r%x\rq\r";	/* setmem.cmdl (addr, value) */
  sparclet_cmds.setmem.cmdll = NULL;	/* setmem.cmdll (addr, value) */
  sparclet_cmds.setmem.resp_delim = NULL;	/*": " *//* setmem.resp_delim */
  sparclet_cmds.setmem.term = NULL;	/*"? " *//* setmem.term */
  sparclet_cmds.setmem.term_cmd = NULL;		/*"q\r" *//* setmem.term_cmd */
  /* since the parsing of multiple bytes is difficult due to
     interspersed addresses, we'll only read 1 value at a time,
     even tho these can handle a count */
  /* we can use -n to set count to read, but may have to parse? */
  sparclet_cmds.getmem.cmdb = "ex %x -n 1 -b\r";	/* getmem.cmdb (addr, #bytes) */
  sparclet_cmds.getmem.cmdw = "ex %x -n 1 -s\r";	/* getmem.cmdw (addr, #swords) */
  sparclet_cmds.getmem.cmdl = "ex %x -n 1 -l\r";	/* getmem.cmdl (addr, #words) */
  sparclet_cmds.getmem.cmdll = NULL;	/* getmem.cmdll (addr, #dwords) */
  sparclet_cmds.getmem.resp_delim = ": ";	/* getmem.resp_delim */
  sparclet_cmds.getmem.term = NULL;	/* getmem.term */
  sparclet_cmds.getmem.term_cmd = NULL;		/* getmem.term_cmd */
  sparclet_cmds.setreg.cmd = "reg %s 0x%x\r";	/* setreg.cmd (name, value) */
  sparclet_cmds.setreg.resp_delim = NULL;	/* setreg.resp_delim */
  sparclet_cmds.setreg.term = NULL;	/* setreg.term */
  sparclet_cmds.setreg.term_cmd = NULL;		/* setreg.term_cmd */
  sparclet_cmds.getreg.cmd = "reg %s\r";	/* getreg.cmd (name) */
  sparclet_cmds.getreg.resp_delim = " ";	/* getreg.resp_delim */
  sparclet_cmds.getreg.term = NULL;	/* getreg.term */
  sparclet_cmds.getreg.term_cmd = NULL;		/* getreg.term_cmd */
  sparclet_cmds.dump_registers = "reg\r";	/* dump_registers */
  sparclet_cmds.register_pattern = "\\(\\w+\\)=\\([0-9a-fA-F]+\\)";	/* register_pattern */
  sparclet_cmds.supply_register = sparclet_supply_register;	/* supply_register */
  sparclet_cmds.load_routine = sparclet_load;	/* load_routine */
  sparclet_cmds.load = NULL;	/* download command (srecs on console) */
  sparclet_cmds.loadresp = NULL;	/* load response */
  sparclet_cmds.prompt = "monitor>";	/* monitor command prompt */
  /* yikes!  gdb core dumps without this delimitor!! */
  sparclet_cmds.line_term = "\r";	/* end-of-command delimitor */
  sparclet_cmds.cmd_end = NULL;	/* optional command terminator */
  sparclet_cmds.target = &sparclet_ops;		/* target operations */
  sparclet_cmds.stopbits = SERIAL_1_STOPBITS;	/* number of stop bits */
  sparclet_cmds.regnames = sparclet_regnames;	/* registers names */
  sparclet_cmds.magic = MONITOR_OPS_MAGIC;	/* magic */
};

static void
sparclet_open (char *args, int from_tty)
{
  monitor_open (args, &sparclet_cmds, from_tty);
}

void
_initialize_sparclet (void)
{
  int i;
  init_sparclet_cmds ();

  for (i = 0; i < NUM_REGS; i++)
    if (sparclet_regnames[i][0] == 'c' ||
	sparclet_regnames[i][0] == 'a')
      sparclet_regnames[i] = 0;	/* mon can't report c* or a* regs */

  sparclet_regnames[0] = 0;	/* mon won't report %G0 */

  init_monitor_ops (&sparclet_ops);
  sparclet_ops.to_shortname = "sparclet";	/* for the target command */
  sparclet_ops.to_longname = "SPARC Sparclet monitor";
  /* use SW breaks; target only supports 2 HW breakpoints */
  sparclet_ops.to_insert_breakpoint = memory_insert_breakpoint;
  sparclet_ops.to_remove_breakpoint = memory_remove_breakpoint;

  sparclet_ops.to_doc =
    "Use a board running the Sparclet debug monitor.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";

  sparclet_ops.to_open = sparclet_open;
  add_target (&sparclet_ops);
}
