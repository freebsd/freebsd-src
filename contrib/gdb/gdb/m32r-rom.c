/* Remote debugging interface to m32r and mon2000 ROM monitors for GDB, 
   the GNU debugger.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.

   Adapted by Michael Snyder of Cygnus Support.

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

/* This module defines communication with the Mitsubishi m32r monitor */

#include "defs.h"
#include "gdbcore.h"
#include "target.h"
#include "monitor.h"
#include "serial.h"
#include "symtab.h"
#include "command.h"
#include "gdbcmd.h"
#include "symfile.h"		/* for generic load */
#include <time.h>		/* for time_t */
#include "gdb_string.h"
#include "objfiles.h"		/* for ALL_OBJFILES etc. */
#include "inferior.h"		/* for write_pc() */
#include <ctype.h>
#include "regcache.h"

extern void report_transfer_performance (unsigned long, time_t, time_t);

/*
 * All this stuff just to get my host computer's IP address!
 */
#include <sys/types.h>
#include <netdb.h>		/* for hostent */
#include <netinet/in.h>		/* for struct in_addr */
#if 1
#include <arpa/inet.h>		/* for inet_ntoa */
#endif

static char *board_addr;	/* user-settable IP address for M32R-EVA */
static char *server_addr;	/* user-settable IP address for gdb host */
static char *download_path;	/* user-settable path for SREC files     */


/* 
 * Function: m32r_load_1 (helper function)
 */

static void
m32r_load_section (bfd *abfd, asection *s, void *obj)
{
  unsigned int *data_count = obj;
  if (s->flags & SEC_LOAD)
    {
      bfd_size_type section_size = bfd_section_size (abfd, s);
      bfd_vma section_base = bfd_section_lma (abfd, s);
      unsigned int buffer, i;

      *data_count += section_size;

      printf_filtered ("Loading section %s, size 0x%lx lma ",
		       bfd_section_name (abfd, s), section_size);
      print_address_numeric (section_base, 1, gdb_stdout);
      printf_filtered ("\n");
      gdb_flush (gdb_stdout);
      monitor_printf ("%s mw\r", paddr_nz (section_base));
      for (i = 0; i < section_size; i += 4)
	{
	  QUIT;
	  monitor_expect (" -> ", NULL, 0);
	  bfd_get_section_contents (abfd, s, (char *) &buffer, i, 4);
	  monitor_printf ("%x\n", buffer);
	}
      monitor_expect (" -> ", NULL, 0);
      monitor_printf ("q\n");
      monitor_expect_prompt (NULL, 0);
    }
}

static int
m32r_load_1 (void *dummy)
{
  int data_count = 0;

  bfd_map_over_sections ((bfd *) dummy, m32r_load_section, &data_count);
  return data_count;
}

/* 
 * Function: m32r_load (an alternate way to load) 
 */

static void
m32r_load (char *filename, int from_tty)
{
  bfd *abfd;
  asection *s;
  unsigned int i, data_count = 0;
  time_t start_time, end_time;	/* for timing of download */

  if (filename == NULL || filename[0] == 0)
    filename = get_exec_file (1);

  abfd = bfd_openr (filename, 0);
  if (!abfd)
    error ("Unable to open file %s\n", filename);
  if (bfd_check_format (abfd, bfd_object) == 0)
    error ("File is not an object file\n");
  start_time = time (NULL);
#if 0
  for (s = abfd->sections; s; s = s->next)
    if (s->flags & SEC_LOAD)
      {
	bfd_size_type section_size = bfd_section_size (abfd, s);
	bfd_vma section_base = bfd_section_vma (abfd, s);
	unsigned int buffer;

	data_count += section_size;

	printf_filtered ("Loading section %s, size 0x%lx vma ",
			 bfd_section_name (abfd, s), section_size);
	print_address_numeric (section_base, 1, gdb_stdout);
	printf_filtered ("\n");
	gdb_flush (gdb_stdout);
	monitor_printf ("%x mw\r", section_base);
	for (i = 0; i < section_size; i += 4)
	  {
	    monitor_expect (" -> ", NULL, 0);
	    bfd_get_section_contents (abfd, s, (char *) &buffer, i, 4);
	    monitor_printf ("%x\n", buffer);
	  }
	monitor_expect (" -> ", NULL, 0);
	monitor_printf ("q\n");
	monitor_expect_prompt (NULL, 0);
      }
#else
  if (!(catch_errors (m32r_load_1, abfd, "Load aborted!\n", RETURN_MASK_ALL)))
    {
      monitor_printf ("q\n");
      return;
    }
#endif
  end_time = time (NULL);
  printf_filtered ("Start address 0x%lx\n", bfd_get_start_address (abfd));
  report_transfer_performance (data_count, start_time, end_time);

  /* Finally, make the PC point at the start address */
  if (exec_bfd)
    write_pc (bfd_get_start_address (exec_bfd));

  inferior_ptid = null_ptid;	/* No process now */

  /* This is necessary because many things were based on the PC at the
     time that we attached to the monitor, which is no longer valid
     now that we have loaded new code (and just changed the PC).
     Another way to do this might be to call normal_stop, except that
     the stack may not be valid, and things would get horribly
     confused... */

  clear_symtab_users ();
}

static void
m32r_load_gen (char *filename, int from_tty)
{
  generic_load (filename, from_tty);
}

static void m32r_open (char *args, int from_tty);
static void mon2000_open (char *args, int from_tty);

/* This array of registers needs to match the indexes used by GDB. The
   whole reason this exists is because the various ROM monitors use
   different names than GDB does, and don't support all the registers
   either. So, typing "info reg sp" becomes an "A7". */

static char *m32r_regnames[] =
{"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
 "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
 "psw", "cbr", "spi", "spu", "bpc", "pc", "accl", "acch",
};

static void
m32r_supply_register (char *regname, int regnamelen, char *val, int vallen)
{
  int regno;
  int num_regs = sizeof (m32r_regnames) / sizeof (m32r_regnames[0]);

  for (regno = 0; regno < num_regs; regno++)
    if (strncmp (regname, m32r_regnames[regno], regnamelen) == 0)
      break;

  if (regno >= num_regs)
    return;			/* no match */

  if (regno == ACCL_REGNUM)
    {				/* special handling for 64-bit acc reg */
      monitor_supply_register (ACCH_REGNUM, val);
      val = strchr (val, ':');	/* skip past ':' to get 2nd word */
      if (val != NULL)
	monitor_supply_register (ACCL_REGNUM, val + 1);
    }
  else
    {
      monitor_supply_register (regno, val);
      if (regno == PSW_REGNUM)
	{
	  unsigned long psw = strtoul (val, NULL, 16);
	  char *zero = "00000000", *one = "00000001";

#ifdef SM_REGNUM
	  /* Stack mode bit */
	  monitor_supply_register (SM_REGNUM, (psw & 0x80) ? one : zero);
#endif
#ifdef BSM_REGNUM
	  /* Backup stack mode bit */
	  monitor_supply_register (BSM_REGNUM, (psw & 0x8000) ? one : zero);
#endif
#ifdef IE_REGNUM
	  /* Interrupt enable bit */
	  monitor_supply_register (IE_REGNUM, (psw & 0x40) ? one : zero);
#endif
#ifdef BIE_REGNUM
	  /* Backup interrupt enable bit */
	  monitor_supply_register (BIE_REGNUM, (psw & 0x4000) ? one : zero);
#endif
#ifdef COND_REGNUM
	  /* Condition bit (carry etc.) */
	  monitor_supply_register (COND_REGNUM, (psw & 0x1) ? one : zero);
#endif
#ifdef CBR_REGNUM
	  monitor_supply_register (CBR_REGNUM, (psw & 0x1) ? one : zero);
#endif
#ifdef BPC_REGNUM
	  monitor_supply_register (BPC_REGNUM, zero);	/* KLUDGE:   (???????) */
#endif
#ifdef BCARRY_REGNUM
	  monitor_supply_register (BCARRY_REGNUM, zero);	/* KLUDGE: (??????) */
#endif
	}

      if (regno == SPI_REGNUM || regno == SPU_REGNUM)
	{			/* special handling for stack pointer (spu or spi) */
	  unsigned long stackmode = read_register (PSW_REGNUM) & 0x80;

	  if (regno == SPI_REGNUM && !stackmode)	/* SP == SPI */
	    monitor_supply_register (SP_REGNUM, val);
	  else if (regno == SPU_REGNUM && stackmode)	/* SP == SPU */
	    monitor_supply_register (SP_REGNUM, val);
	}
    }
}

/* m32r RevC board monitor */

static struct target_ops m32r_ops;

static char *m32r_inits[] =
{"\r", NULL};

static struct monitor_ops m32r_cmds;

static void
init_m32r_cmds (void)
{
  m32r_cmds.flags = MO_CLR_BREAK_USES_ADDR | MO_REGISTER_VALUE_FIRST;
  m32r_cmds.init = m32r_inits;	/* Init strings */
  m32r_cmds.cont = "go\r";	/* continue command */
  m32r_cmds.step = "step\r";	/* single step */
  m32r_cmds.stop = NULL;	/* interrupt command */
  m32r_cmds.set_break = "%x +bp\r";	/* set a breakpoint */
  m32r_cmds.clr_break = "%x -bp\r";	/* clear a breakpoint */
  m32r_cmds.clr_all_break = "bpoff\r";	/* clear all breakpoints */
  m32r_cmds.fill = "%x %x %x fill\r";	/* fill (start length val) */
  m32r_cmds.setmem.cmdb = "%x 1 %x fill\r";	/* setmem.cmdb (addr, value) */
  m32r_cmds.setmem.cmdw = "%x 1 %x fillh\r";	/* setmem.cmdw (addr, value) */
  m32r_cmds.setmem.cmdl = "%x 1 %x fillw\r";	/* setmem.cmdl (addr, value) */
  m32r_cmds.setmem.cmdll = NULL;	/* setmem.cmdll (addr, value) */
  m32r_cmds.setmem.resp_delim = NULL;	/* setmem.resp_delim */
  m32r_cmds.setmem.term = NULL;	/* setmem.term */
  m32r_cmds.setmem.term_cmd = NULL;	/* setmem.term_cmd */
  m32r_cmds.getmem.cmdb = "%x %x dump\r";	/* getmem.cmdb (addr, len) */
  m32r_cmds.getmem.cmdw = NULL;	/* getmem.cmdw (addr, len) */
  m32r_cmds.getmem.cmdl = NULL;	/* getmem.cmdl (addr, len) */
  m32r_cmds.getmem.cmdll = NULL;	/* getmem.cmdll (addr, len) */
  m32r_cmds.getmem.resp_delim = ": ";	/* getmem.resp_delim */
  m32r_cmds.getmem.term = NULL;	/* getmem.term */
  m32r_cmds.getmem.term_cmd = NULL;	/* getmem.term_cmd */
  m32r_cmds.setreg.cmd = "%x to %%%s\r";	/* setreg.cmd (name, value) */
  m32r_cmds.setreg.resp_delim = NULL;	/* setreg.resp_delim */
  m32r_cmds.setreg.term = NULL;	/* setreg.term */
  m32r_cmds.setreg.term_cmd = NULL;	/* setreg.term_cmd */
  m32r_cmds.getreg.cmd = NULL;	/* getreg.cmd (name) */
  m32r_cmds.getreg.resp_delim = NULL;	/* getreg.resp_delim */
  m32r_cmds.getreg.term = NULL;	/* getreg.term */
  m32r_cmds.getreg.term_cmd = NULL;	/* getreg.term_cmd */
  m32r_cmds.dump_registers = ".reg\r";	/* dump_registers */
  m32r_cmds.register_pattern = "\\(\\w+\\) += \\([0-9a-fA-F]+\\b\\)";	/* register_pattern */
  m32r_cmds.supply_register = m32r_supply_register;	/* supply_register */
  m32r_cmds.load_routine = NULL;	/* load_routine (defaults to SRECs) */
  m32r_cmds.load = NULL;	/* download command */
  m32r_cmds.loadresp = NULL;	/* load response */
  m32r_cmds.prompt = "ok ";	/* monitor command prompt */
  m32r_cmds.line_term = "\r";	/* end-of-line terminator */
  m32r_cmds.cmd_end = NULL;	/* optional command terminator */
  m32r_cmds.target = &m32r_ops;	/* target operations */
  m32r_cmds.stopbits = SERIAL_1_STOPBITS;	/* number of stop bits */
  m32r_cmds.regnames = m32r_regnames;	/* registers names */
  m32r_cmds.magic = MONITOR_OPS_MAGIC;	/* magic */
}				/* init_m32r_cmds */

static void
m32r_open (char *args, int from_tty)
{
  monitor_open (args, &m32r_cmds, from_tty);
}

/* Mon2000 monitor (MSA2000 board) */

static struct target_ops mon2000_ops;
static struct monitor_ops mon2000_cmds;

static void
init_mon2000_cmds (void)
{
  mon2000_cmds.flags = MO_CLR_BREAK_USES_ADDR | MO_REGISTER_VALUE_FIRST;
  mon2000_cmds.init = m32r_inits;	/* Init strings */
  mon2000_cmds.cont = "go\r";	/* continue command */
  mon2000_cmds.step = "step\r";	/* single step */
  mon2000_cmds.stop = NULL;	/* interrupt command */
  mon2000_cmds.set_break = "%x +bp\r";	/* set a breakpoint */
  mon2000_cmds.clr_break = "%x -bp\r";	/* clear a breakpoint */
  mon2000_cmds.clr_all_break = "bpoff\r";	/* clear all breakpoints */
  mon2000_cmds.fill = "%x %x %x fill\r";	/* fill (start length val) */
  mon2000_cmds.setmem.cmdb = "%x 1 %x fill\r";	/* setmem.cmdb (addr, value) */
  mon2000_cmds.setmem.cmdw = "%x 1 %x fillh\r";		/* setmem.cmdw (addr, value) */
  mon2000_cmds.setmem.cmdl = "%x 1 %x fillw\r";		/* setmem.cmdl (addr, value) */
  mon2000_cmds.setmem.cmdll = NULL;	/* setmem.cmdll (addr, value) */
  mon2000_cmds.setmem.resp_delim = NULL;	/* setmem.resp_delim */
  mon2000_cmds.setmem.term = NULL;	/* setmem.term */
  mon2000_cmds.setmem.term_cmd = NULL;	/* setmem.term_cmd */
  mon2000_cmds.getmem.cmdb = "%x %x dump\r";	/* getmem.cmdb (addr, len) */
  mon2000_cmds.getmem.cmdw = NULL;	/* getmem.cmdw (addr, len) */
  mon2000_cmds.getmem.cmdl = NULL;	/* getmem.cmdl (addr, len) */
  mon2000_cmds.getmem.cmdll = NULL;	/* getmem.cmdll (addr, len) */
  mon2000_cmds.getmem.resp_delim = ": ";	/* getmem.resp_delim */
  mon2000_cmds.getmem.term = NULL;	/* getmem.term */
  mon2000_cmds.getmem.term_cmd = NULL;	/* getmem.term_cmd */
  mon2000_cmds.setreg.cmd = "%x to %%%s\r";	/* setreg.cmd (name, value) */
  mon2000_cmds.setreg.resp_delim = NULL;	/* setreg.resp_delim */
  mon2000_cmds.setreg.term = NULL;	/* setreg.term */
  mon2000_cmds.setreg.term_cmd = NULL;	/* setreg.term_cmd */
  mon2000_cmds.getreg.cmd = NULL;	/* getreg.cmd (name) */
  mon2000_cmds.getreg.resp_delim = NULL;	/* getreg.resp_delim */
  mon2000_cmds.getreg.term = NULL;	/* getreg.term */
  mon2000_cmds.getreg.term_cmd = NULL;	/* getreg.term_cmd */
  mon2000_cmds.dump_registers = ".reg\r";	/* dump_registers */
  mon2000_cmds.register_pattern = "\\(\\w+\\) += \\([0-9a-fA-F]+\\b\\)";	/* register_pattern */
  mon2000_cmds.supply_register = m32r_supply_register;	/* supply_register */
  mon2000_cmds.load_routine = NULL;	/* load_routine (defaults to SRECs) */
  mon2000_cmds.load = NULL;	/* download command */
  mon2000_cmds.loadresp = NULL;	/* load response */
  mon2000_cmds.prompt = "Mon2000>";	/* monitor command prompt */
  mon2000_cmds.line_term = "\r";	/* end-of-line terminator */
  mon2000_cmds.cmd_end = NULL;	/* optional command terminator */
  mon2000_cmds.target = &mon2000_ops;	/* target operations */
  mon2000_cmds.stopbits = SERIAL_1_STOPBITS;	/* number of stop bits */
  mon2000_cmds.regnames = m32r_regnames;	/* registers names */
  mon2000_cmds.magic = MONITOR_OPS_MAGIC;	/* magic */
}				/* init_mon2000_cmds */

static void
mon2000_open (char *args, int from_tty)
{
  monitor_open (args, &mon2000_cmds, from_tty);
}

/* Function: set_board_address
   Tell the BootOne monitor what it's ethernet IP address is. */

static void
m32r_set_board_address (char *args, int from_tty)
{
  int resp_len;
  char buf[1024];

  if (args && *args)
    {
      monitor_printf ("ulip %s\n", args);
      resp_len = monitor_expect_prompt (buf, sizeof (buf));
      /* now parse the result for success */
    }
  else
    error ("Requires argument (IP address for M32R-EVA board)");
}

/* Function: set_server_address
   Tell the BootOne monitor what gdb's ethernet IP address is. */

static void
m32r_set_server_address (char *args, int from_tty)
{
  int resp_len;
  char buf[1024];

  if (args && *args)
    {
      monitor_printf ("uhip %s\n", args);
      resp_len = monitor_expect_prompt (buf, sizeof (buf));
      /* now parse the result for success */
    }
  else
    error ("Requires argument (IP address of GDB's host computer)");
}

/* Function: set_download_path
   Tell the BootOne monitor the default path for downloadable SREC files. */

static void
m32r_set_download_path (char *args, int from_tty)
{
  int resp_len;
  char buf[1024];

  if (args && *args)
    {
      monitor_printf ("up %s\n", args);
      resp_len = monitor_expect_prompt (buf, sizeof (buf));
      /* now parse the result for success */
    }
  else
    error ("Requires argument (default path for downloadable SREC files)");
}

static void
m32r_upload_command (char *args, int from_tty)
{
  bfd *abfd;
  asection *s;
  time_t start_time, end_time;	/* for timing of download */
  int resp_len, data_count = 0;
  char buf[1024];
  struct hostent *hostent;
  struct in_addr inet_addr;

  /* first check to see if there's an ethernet port! */
  monitor_printf ("ust\r");
  resp_len = monitor_expect_prompt (buf, sizeof (buf));
  if (!strchr (buf, ':'))
    error ("No ethernet connection!");

  if (board_addr == 0)
    {
      /* scan second colon in the output from the "ust" command */
      char *myIPaddress = strchr (strchr (buf, ':') + 1, ':') + 1;

      while (isspace (*myIPaddress))
	myIPaddress++;

      if (!strncmp (myIPaddress, "0.0.", 4))	/* empty */
	error ("Please use 'set board-address' to set the M32R-EVA board's IP address.");
      if (strchr (myIPaddress, '('))
	*(strchr (myIPaddress, '(')) = '\0';	/* delete trailing junk */
      board_addr = xstrdup (myIPaddress);
    }
  if (server_addr == 0)
    {
      buf[0] = 0;
      gethostname (buf, sizeof (buf));
      if (buf[0] != 0)
	hostent = gethostbyname (buf);
      if (hostent != 0)
	{
#if 1
	  memcpy (&inet_addr.s_addr, hostent->h_addr,
		  sizeof (inet_addr.s_addr));
	  server_addr = (char *) inet_ntoa (inet_addr);
#else
	  server_addr = (char *) inet_ntoa (hostent->h_addr);
#endif
	}
      if (server_addr == 0)	/* failed? */
	error ("Need to know gdb host computer's IP address (use 'set server-address')");
    }

  if (args == 0 || args[0] == 0)	/* no args: upload the current file */
    args = get_exec_file (1);

  if (args[0] != '/' && download_path == 0)
    {
      if (current_directory)
	download_path = xstrdup (current_directory);
      else
	error ("Need to know default download path (use 'set download-path')");
    }

  start_time = time (NULL);
  monitor_printf ("uhip %s\r", server_addr);
  resp_len = monitor_expect_prompt (buf, sizeof (buf));		/* parse result? */
  monitor_printf ("ulip %s\r", board_addr);
  resp_len = monitor_expect_prompt (buf, sizeof (buf));		/* parse result? */
  if (args[0] != '/')
    monitor_printf ("up %s\r", download_path);	/* use default path */
  else
    monitor_printf ("up\r");	/* rooted filename/path */
  resp_len = monitor_expect_prompt (buf, sizeof (buf));		/* parse result? */

  if (strrchr (args, '.') && !strcmp (strrchr (args, '.'), ".srec"))
    monitor_printf ("ul %s\r", args);
  else				/* add ".srec" suffix */
    monitor_printf ("ul %s.srec\r", args);
  resp_len = monitor_expect_prompt (buf, sizeof (buf));		/* parse result? */

  if (buf[0] == 0 || strstr (buf, "complete") == 0)
    error ("Upload file not found: %s.srec\nCheck IP addresses and download path.", args);
  else
    printf_filtered (" -- Ethernet load complete.\n");

  end_time = time (NULL);
  abfd = bfd_openr (args, 0);
  if (abfd != NULL)
    {				/* Download is done -- print section statistics */
      if (bfd_check_format (abfd, bfd_object) == 0)
	{
	  printf_filtered ("File is not an object file\n");
	}
      for (s = abfd->sections; s; s = s->next)
	if (s->flags & SEC_LOAD)
	  {
	    bfd_size_type section_size = bfd_section_size (abfd, s);
	    bfd_vma section_base = bfd_section_lma (abfd, s);
	    unsigned int buffer;

	    data_count += section_size;

	    printf_filtered ("Loading section %s, size 0x%lx lma ",
			     bfd_section_name (abfd, s), section_size);
	    print_address_numeric (section_base, 1, gdb_stdout);
	    printf_filtered ("\n");
	    gdb_flush (gdb_stdout);
	  }
      /* Finally, make the PC point at the start address */
      write_pc (bfd_get_start_address (abfd));
      report_transfer_performance (data_count, start_time, end_time);
      printf_filtered ("Start address 0x%lx\n", bfd_get_start_address (abfd));
    }
  inferior_ptid = null_ptid;	/* No process now */

  /* This is necessary because many things were based on the PC at the
     time that we attached to the monitor, which is no longer valid
     now that we have loaded new code (and just changed the PC).
     Another way to do this might be to call normal_stop, except that
     the stack may not be valid, and things would get horribly
     confused... */

  clear_symtab_users ();
}

void
_initialize_m32r_rom (void)
{
  /* Initialize m32r RevC monitor target */
  init_m32r_cmds ();
  init_monitor_ops (&m32r_ops);

  m32r_ops.to_shortname = "m32r";
  m32r_ops.to_longname = "m32r monitor";
  m32r_ops.to_load = m32r_load_gen;	/* monitor lacks a download command */
  m32r_ops.to_doc = "Debug via the m32r monitor.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";
  m32r_ops.to_open = m32r_open;
  add_target (&m32r_ops);

  /* Initialize mon2000 monitor target */
  init_mon2000_cmds ();
  init_monitor_ops (&mon2000_ops);

  mon2000_ops.to_shortname = "mon2000";
  mon2000_ops.to_longname = "Mon2000 monitor";
  mon2000_ops.to_load = m32r_load_gen;	/* monitor lacks a download command */
  mon2000_ops.to_doc = "Debug via the Mon2000 monitor.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";
  mon2000_ops.to_open = mon2000_open;
  add_target (&mon2000_ops);

  add_show_from_set
    (add_set_cmd ("download-path", class_obscure, var_string,
		  (char *) &download_path,
		  "Set the default path for downloadable SREC files.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("board-address", class_obscure, var_string,
		  (char *) &board_addr,
		  "Set IP address for M32R-EVA target board.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("server-address", class_obscure, var_string,
		  (char *) &server_addr,
		"Set IP address for download server (GDB's host computer).",
		  &setlist),
     &showlist);

  add_com ("upload", class_obscure, m32r_upload_command,
      "Upload the srec file via the monitor's Ethernet upload capability.");

  add_com ("tload", class_obscure, m32r_load, "test upload command.");
}
