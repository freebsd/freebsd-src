/* Remote target glue for the Intel 960 ROM monitor.
   Copyright 1995, 1996 Free Software Foundation, Inc.

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
#include "srec.h"
#include "xmodem.h"

#if !defined (HAVE_TERMIOS) && !defined (HAVE_TERMIO) && !defined (HAVE_SGTTY)
#define HAVE_SGTTY
#endif

#ifdef HAVE_SGTTY
#include <sys/ioctl.h>
#endif

#include <sys/types.h>	/* Needed by file.h on Sys V */
#include <sys/file.h>
#include <signal.h>
#include <sys/stat.h>

#define USE_GENERIC_LOAD

int quiet = 0;	/* 1 => stifle unnecessary messages */
serial_t mon960_serial;
char *mon960_ttyname;   /* name of tty to talk to mon960 on, or null */
static struct monitor_ops mon960_cmds;

#ifdef USE_GENERIC_LOAD
extern void generic_load PARAMS ((char* filename, int from_tty));
#endif
static void mon960_open PARAMS ((char *args, int from_tty));

#ifdef USE_GENERIC_LOAD
static void
mon960_load_gen (filename, from_tty) 
    char *filename;
    int from_tty;
{
  extern int inferior_pid;
  generic_load (filename, from_tty);
  /* Finally, make the PC point at the start address */
  if (exec_bfd)
    write_pc (bfd_get_start_address (exec_bfd));

  inferior_pid = 0;             /* No process now */
}

#else
static void
mon960_load (desc, file, hashmark)
     serial_t desc;
     char *file;
     int hashmark;
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
	monitor_printf (mon960_cmds.load, s->vma);
	if (mon960_cmds.loadresp)
	  monitor_expect (mon960_cmds.loadresp, NULL, 0);
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
#endif

/* This array of registers need to match the indexes used by GDB.
   This exists because the various ROM monitors use different strings
   than does GDB, and don't necessarily support all the registers
   either. So, typing "info reg sp" becomes a "r30".  */

/* these correspond to the offsets from tm-* files from config directories */
/* g0-g14, fp, pfp, sp, rip,r3-15, pc, ac, tc, fp0-3 */ 
/* NOTE: "ip" is documented as "ir" in the Mon960 UG. */
/* NOTE: "ir" can't be accessed... but there's an ip and rip. */
static char *mon960_regnames[NUM_REGS] = {
  /*  0 */ "pfp", "sp",  "rip", "r3",  "r4",  "r5",  "r6",  "r7", \
  /*  8 */ "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",\
  /* 16 */ "g0",  "g1",  "g2",  "g3",  "g4",  "g5",  "g6",  "g7", \
  /* 24 */ "g8",  "g9",  "g10", "g11", "g12", "g13", "g14", "fp", \
  /* 32 */ "pc",  "ac",  "tc",  "ip",  "fp0", "fp1", "fp2", "fp3",\
  };

/* Define the monitor command strings. Since these are passed directly
   through to a printf style function, we may include formatting
   strings. We also need a CR or LF on the end.  */

static struct target_ops mon960_ops;

/* need to pause the monitor for timing reasons, so slow it down */
static char *mon960_inits[] = {"\n\r\r\r\r\r\r\r\r\r\r\r\r\r\r\n\r\n\r\n", NULL}; /* Exits sub-command mode & download cmds */

static struct monitor_ops mon960_cmds =
{
  MO_CLR_BREAK_USES_ADDR
    | MO_NO_ECHO_ON_OPEN
    | MO_SEND_BREAK_ON_STOP
    | MO_GETMEM_READ_SINGLE,    /* flags */
  mon960_inits,			/* Init strings */
  "go\n\r",			/* continue command */
  "st\n\r",			/* single step */
  "\n\r",			/* break interrupts the program */
  NULL,				/* set a breakpoint */
				/* can't use "br" because only 2 hw bps are supported */
  NULL,				/* clear a breakpoint - "de" is for hw bps */
  NULL,				/* clear all breakpoints */
  NULL,				/* fill (start end val) */
				/* can't use "fi" because it takes words, not bytes */
  {
    /* can't use "mb", "md" or "mo" because they require interaction */
    NULL,			/* setmem.cmdb (addr, value) */
    "md %x %x\n\r",		/* setmem.cmdw (addr, value) */
    NULL,			/* setmem.cmdl (addr, value) */
    NULL,			/* setmem.cmdll (addr, value) */
    NULL,			/* setmem.resp_delim */
    NULL,			/* setmem.term */
    NULL,			/* setmem.term_cmd */
  },
  {
    /* since the parsing of multiple bytes is difficult due to
       interspersed addresses, we'll only read 1 value at a time, 
       even tho these can handle a count */
    "db %x\n\r",		/* getmem.cmdb (addr, #bytes) */
    "ds %x\n\r",		/* getmem.cmdw (addr, #swords) */
    "di %x\n\r",		/* getmem.cmdl (addr, #words) */
    "dd %x\n\r",		/* getmem.cmdll (addr, #dwords) */
    " : ",			/* getmem.resp_delim */
    NULL,			/* getmem.term */
    NULL,			/* getmem.term_cmd */
  },
  {
    "md %s %x\n\r",		/* setreg.cmd (name, value) */
    NULL,			/* setreg.resp_delim */
    NULL,			/* setreg.term */
    NULL			/* setreg.term_cmd */
  },
  {
    "di %s\n\r",		/* getreg.cmd (name) */
    " : ",			/* getreg.resp_delim */
    NULL,			/* getreg.term */
    NULL,			/* getreg.term_cmd */
  },
  "re\n\r",			/* dump_registers */
  "\\(\\w+\\)=\\([0-9a-fA-F]+\\)",	/* register_pattern */
  NULL,				/* supply_register */
#ifdef USE_GENERIC_LOAD
  NULL,				/* load_routine (defaults to SRECs) */
  NULL,				/* download command */
  NULL,				/* load response */
#else
  mon960_load,			/* load_routine (defaults to SRECs) */
  "do\n\r",			/* download command */
  "Downloading\n\r",		/* load response */
#endif
  "=>",				/* monitor command prompt */
  "\n\r",			/* end-of-command delimitor */
  NULL,				/* optional command terminator */
  &mon960_ops,			/* target operations */
  SERIAL_1_STOPBITS,		/* number of stop bits */
  mon960_regnames,		/* registers names */
  MONITOR_OPS_MAGIC		/* magic */
};

/* invoked from monitor.c - opens the serial port */
static void
mon960_open (args, from_tty)
     char *args;
     int from_tty;
{
  char *serial_port_name = args;
  if (args) 
    {
      char *cursor =  serial_port_name = strsave (args);

      while (*cursor && *cursor != ' ')
 	cursor++;

      if (*cursor)
	*cursor++ = 0;

      while (*cursor == ' ')
	cursor++;

    }
  monitor_open (serial_port_name, &mon960_cmds, from_tty);
}


void
_initialize_mon960 ()
{
  init_monitor_ops (&mon960_ops);

  mon960_ops.to_shortname = "mon960"; /* for the target command */
  mon960_ops.to_longname = "Intel 960 rom monitor";
#ifdef USE_GENERIC_LOAD
  mon960_ops.to_load = mon960_load_gen; /* FIXME - should go back and try "do" */
#endif
  /* use SW breaks; target only supports 2 HW breakpoints */
  mon960_ops.to_insert_breakpoint = memory_insert_breakpoint; 
  mon960_ops.to_remove_breakpoint = memory_remove_breakpoint; 

  mon960_ops.to_doc = 
    "Debug on an Intel 960 eval board running the Mon960 rom monitor.\n"
    "Specify the serial device it is connected to (e.g. /dev/ttya).";

  mon960_ops.to_open = mon960_open;
  add_target (&mon960_ops);
}

