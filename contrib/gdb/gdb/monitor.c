/* Remote debugging interface for boot monitors, for GDB.
   Copyright 1990, 1991, 1992, 1993, 1995, 1996
   Free Software Foundation, Inc.
   Contributed by Cygnus Support.  Written by Rob Savoye for Cygnus.
   Resurrected from the ashes by Stu Grossman.

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

/* This file was derived from various remote-* modules. It is a collection
   of generic support functions so GDB can talk directly to a ROM based
   monitor. This saves use from having to hack an exception based handler
   into existance, and makes for quick porting.

   This module talks to a debug monitor called 'MONITOR', which
   We communicate with MONITOR via either a direct serial line, or a TCP
   (or possibly TELNET) stream to a terminal multiplexor,
   which in turn talks to the target board.  */

#include "defs.h"
#include "gdbcore.h"
#include "target.h"
#include "wait.h"
#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <signal.h>
#include <ctype.h>
#include "gdb_string.h"
#include <sys/types.h>
#include "command.h"
#include "serial.h"
#include "monitor.h"
#include "gdbcmd.h"
#include "inferior.h"
#include "gnu-regex.h"
#include "dcache.h"
#include "srec.h"

static int readchar PARAMS ((int timeout));

static void monitor_command PARAMS ((char *args, int fromtty));

static void monitor_fetch_register PARAMS ((int regno));
static void monitor_store_register PARAMS ((int regno));

static void monitor_detach PARAMS ((char *args, int from_tty));
static void monitor_resume PARAMS ((int pid, int step, enum target_signal sig));
static void monitor_interrupt PARAMS ((int signo));
static void monitor_interrupt_twice PARAMS ((int signo));
static void monitor_interrupt_query PARAMS ((void));
static void monitor_wait_cleanup PARAMS ((int old_timeout));

static int monitor_wait PARAMS ((int pid, struct target_waitstatus *status));
static void monitor_fetch_registers PARAMS ((int regno));
static void monitor_store_registers PARAMS ((int regno));
static void monitor_prepare_to_store PARAMS ((void));
static int monitor_xfer_memory PARAMS ((CORE_ADDR memaddr, char *myaddr, int len, int write, struct target_ops *target));
static void monitor_files_info PARAMS ((struct target_ops *ops));
static int monitor_insert_breakpoint PARAMS ((CORE_ADDR addr, char *shadow));
static int monitor_remove_breakpoint PARAMS ((CORE_ADDR addr, char *shadow));
static void monitor_kill PARAMS ((void));
static void monitor_load PARAMS ((char *file, int from_tty));
static void monitor_mourn_inferior PARAMS ((void));
static void monitor_stop PARAMS ((void));
static void monitor_debug PARAMS ((char *prefix, char *string, char *suffix));

static int monitor_read_memory PARAMS ((CORE_ADDR addr, char *myaddr,int len));
static int monitor_write_memory PARAMS ((CORE_ADDR addr, char *myaddr,int len));

static int monitor_expect_regexp PARAMS ((struct re_pattern_buffer *pat,
					  char *buf, int buflen));
static int from_hex PARAMS ((int a));
static unsigned long get_hex_word PARAMS ((void));

static struct monitor_ops *current_monitor;

static int hashmark;		/* flag set by "set hash" */

static int timeout = 30;

static int in_monitor_wait = 0;	/* Non-zero means we are in monitor_wait() */

static void (*ofunc)();		/* Old SIGINT signal handler */

/* Descriptor for I/O to remote machine.  Initialize it to NULL so
   that monitor_open knows that we don't have a file open when the
   program starts.  */

static serial_t monitor_desc = NULL;

/* Pointer to regexp pattern matching data */

static struct re_pattern_buffer register_pattern;
static char register_fastmap[256];

static struct re_pattern_buffer getmem_resp_delim_pattern;
static char getmem_resp_delim_fastmap[256];

static int dump_reg_flag;	/* Non-zero means do a dump_registers cmd when
				   monitor_wait wakes up.  */

static DCACHE *remote_dcache;

/* monitor_debug is like fputs_unfiltered, except it prints special
   characters in printable fashion.  */

static void
monitor_debug (prefix, string, suffix)
     char *prefix;
     char *string;
     char *suffix;
{
  int ch;

  /* print prefix and suffix after each line */
  static int new_line=1;
  if (new_line==1) {	/* print prefix if last char was a newline */
    fputs_unfiltered (prefix, gdb_stderr);
    new_line=0;
  }
  if (strchr(string,'\n'))	/* save state for next call */
    new_line=1;

  while ((ch = *string++) != '\0')
    {
      switch (ch) {
      default:
	if (isprint (ch))
	  fputc_unfiltered (ch, gdb_stderr);

	else
	  fprintf_unfiltered (gdb_stderr, "\\%03o", ch);

	break;

      case '\\': fputs_unfiltered ("\\\\",  gdb_stderr);	break;	
      case '\b': fputs_unfiltered ("\\b",   gdb_stderr);	break;	
      case '\f': fputs_unfiltered ("\\f",   gdb_stderr);	break;	
      case '\n': fputs_unfiltered ("\\n",   gdb_stderr);	break;	
      case '\r': fputs_unfiltered ("\\r",   gdb_stderr);	break;	
      case '\t': fputs_unfiltered ("\\t",   gdb_stderr);	break;	
      case '\v': fputs_unfiltered ("\\v",   gdb_stderr);	break;	
      }
    }

  if (new_line==1) {	/* print suffix if last char was a newline */
    fputs_unfiltered (suffix, gdb_stderr);
    fputs_unfiltered ("\n", gdb_stderr);
  }
}

/* monitor_printf_noecho -- Send data to monitor, but don't expect an echo.
   Works just like printf.  */

void
#ifdef ANSI_PROTOTYPES
monitor_printf_noecho (char *pattern, ...)
#else
monitor_printf_noecho (va_alist)
     va_dcl
#endif
{
  va_list args;
  char sndbuf[2000];
  int len;

#if ANSI_PROTOTYPES
  va_start (args, pattern);
#else
  char *pattern;
  va_start (args);
  pattern = va_arg (args, char *);
#endif

  vsprintf (sndbuf, pattern, args);

  if (remote_debug > 0)
    monitor_debug ("sent -->", sndbuf, "<--");

  len = strlen (sndbuf);

  if (len + 1 > sizeof sndbuf)
    abort ();

  if (SERIAL_WRITE(monitor_desc, sndbuf, len))
    fprintf_unfiltered (stderr, "SERIAL_WRITE failed: %s\n", safe_strerror (errno));
}

/* monitor_printf -- Send data to monitor and check the echo.  Works just like
   printf.  */

void
#ifdef ANSI_PROTOTYPES
monitor_printf (char *pattern, ...)
#else
monitor_printf (va_alist)
     va_dcl
#endif
{
  va_list args;
  char sndbuf[2000];
  int len;

#ifdef ANSI_PROTOTYPES
  va_start (args, pattern);
#else
  char *pattern;
  va_start (args);
  pattern = va_arg (args, char *);
#endif

  vsprintf (sndbuf, pattern, args);

  if (remote_debug > 0)
    monitor_debug ("sent -->", sndbuf, "<--");

  len = strlen (sndbuf);

  if (len + 1 > sizeof sndbuf)
    abort ();

  if (SERIAL_WRITE(monitor_desc, sndbuf, len))
    fprintf_unfiltered (stderr, "SERIAL_WRITE failed: %s\n", safe_strerror (errno));

  /* We used to expect that the next immediate output was the characters we
     just output, but sometimes some extra junk appeared before the characters
     we expected, like an extra prompt, or a portmaster sending telnet negotiations.
     So, just start searching for what we sent, and skip anything unknown.  */
  monitor_expect (sndbuf, (char *)0, 0);
}

/* Read a character from the remote system, doing all the fancy
   timeout stuff.  */

static int
readchar (timeout)
     int timeout;
{
  int c;
  static enum { last_random, last_nl, last_cr, last_crnl } state = last_random;
  int looping;

  do
    {
      looping = 0;
      c = SERIAL_READCHAR (monitor_desc, timeout);

      if (c >= 0)
	{
	  c &= 0x7f;
	  if (remote_debug > 0)
	    {
	      char buf[2];
	      buf[0] = c;
	      buf[1] = '\0';
	      monitor_debug ("read -->", buf, "<--");
	    }
	}

      /* Canonicialize \n\r combinations into one \r */
      if ((current_monitor->flags & MO_HANDLE_NL) != 0)
	{
	  if ((c == '\r' && state == last_nl)
	      || (c == '\n' && state == last_cr))
	    {
	      state = last_crnl;
	      looping = 1;
	    }
	  else if (c == '\r')
	    state = last_cr;
	  else if (c != '\n')
	    state = last_random;
	  else
	    {
	      state = last_nl;
	      c = '\r';
	    }
	}
    }
  while (looping);

  if (c >= 0)
    return c;

  if (c == SERIAL_TIMEOUT)
#ifdef MAINTENANCE_CMDS
    if (in_monitor_wait)	/* Watchdog went off */
      {
	target_mourn_inferior ();
	error ("Watchdog has expired.  Target detached.\n");
      }
    else
#endif
      error ("Timeout reading from remote system.");

  perror_with_name ("remote-monitor");
}

/* Scan input from the remote system, until STRING is found.  If BUF is non-
   zero, then collect input until we have collected either STRING or BUFLEN-1
   chars.  In either case we terminate BUF with a 0.  If input overflows BUF
   because STRING can't be found, return -1, else return number of chars in BUF
   (minus the terminating NUL).  Note that in the non-overflow case, STRING
   will be at the end of BUF.  */

int
monitor_expect (string, buf, buflen)
     char *string;
     char *buf;
     int buflen;
{
  char *p = string;
  int obuflen = buflen;
  int c;

  immediate_quit = 1;
  while (1)
    {
      if (buf)
	{
	  if (buflen < 2)
	    {
	      *buf = '\000';
	      immediate_quit = 0;
	      return -1;
	    }

	  c = readchar (timeout);
	  if (c == '\000')
	    continue;
	  *buf++ = c;
	  buflen--;
	}
      else
	c = readchar (timeout);

      /* Don't expect any ^C sent to be echoed */
	
      if (*p == '\003' || c == *p)
	{
	  p++;
	  if (*p == '\0')
	    {
	      immediate_quit = 0;

	      if (buf)
		{
		  *buf++ = '\000';
		  return obuflen - buflen;
		}
	      else
		return 0;
	    }
	}
      else
	{
	  p = string;
	  if (c == *p)
	    p++;
	}
    }
}

/* Search for a regexp.  */

static int
monitor_expect_regexp (pat, buf, buflen)
     struct re_pattern_buffer *pat;
     char *buf;
     int buflen;
{
  char *mybuf;
  char *p;

  if (buf)
    mybuf = buf;
  else
    {
      mybuf = alloca (1024);
      buflen = 1024;
    }

  p = mybuf;
  while (1)
    {
      int retval;

      if (p - mybuf >= buflen)
	{			/* Buffer about to overflow */

/* On overflow, we copy the upper half of the buffer to the lower half.  Not
   great, but it usually works... */

	  memcpy (mybuf, mybuf + buflen / 2, buflen / 2);
	  p = mybuf + buflen / 2;
	}

      *p++ = readchar (timeout);

      retval = re_search (pat, mybuf, p - mybuf, 0, p - mybuf, NULL);
      if (retval >= 0)
	return 1;
    }
}

/* Keep discarding input until we see the MONITOR prompt.

   The convention for dealing with the prompt is that you
   o give your command
   o *then* wait for the prompt.

   Thus the last thing that a procedure does with the serial line
   will be an monitor_expect_prompt().  Exception:  monitor_resume does not
   wait for the prompt, because the terminal is being handed over
   to the inferior.  However, the next thing which happens after that
   is a monitor_wait which does wait for the prompt.
   Note that this includes abnormal exit, e.g. error().  This is
   necessary to prevent getting into states from which we can't
   recover.  */

int
monitor_expect_prompt (buf, buflen)
     char *buf;
     int buflen;
{
  return monitor_expect (PROMPT, buf, buflen);
}

/* Get N 32-bit words from remote, each preceded by a space, and put
   them in registers starting at REGNO.  */

static unsigned long
get_hex_word ()
{
  unsigned long val;
  int i;
  int ch;

  do
    ch = readchar (timeout);
  while (isspace(ch));

  val = from_hex (ch);

  for (i = 7; i >= 1; i--)
    {
      ch = readchar (timeout);
      if (!isxdigit (ch))
	break;
      val = (val << 4) | from_hex (ch);
    }

  return val;
}

static void
compile_pattern (pattern, compiled_pattern, fastmap)
     char *pattern;
     struct re_pattern_buffer *compiled_pattern;
     char *fastmap;
{
  int tmp;
  char *val;

  compiled_pattern->fastmap = fastmap;

  tmp = re_set_syntax (RE_SYNTAX_EMACS);
  val = re_compile_pattern (pattern,
			    strlen (pattern),
			    compiled_pattern);
  re_set_syntax (tmp);

  if (val)
    error ("compile_pattern: Can't compile pattern string `%s': %s!", pattern, val);

  if (fastmap)
    re_compile_fastmap (compiled_pattern);
}

/* Open a connection to a remote debugger. NAME is the filename used
   for communication.  */

static char *dev_name;
static struct target_ops *targ_ops;

void
monitor_open (args, mon_ops, from_tty)
     char *args;
     struct monitor_ops *mon_ops;
     int from_tty;
{
  char *name;
  int i;
  char **p;

  if (mon_ops->magic != MONITOR_OPS_MAGIC)
    error ("Magic number of monitor_ops struct wrong.");

  targ_ops = mon_ops->target;
  name = targ_ops->to_shortname;

  if (!args)
    error ("Use `target %s DEVICE-NAME' to use a serial port, or \n\
`target %s HOST-NAME:PORT-NUMBER' to use a network connection.", name, name);

  target_preopen (from_tty);

  /* Setup pattern for register dump */

  if (mon_ops->register_pattern)
    compile_pattern (mon_ops->register_pattern, &register_pattern,
		     register_fastmap);

  if (mon_ops->getmem.resp_delim)
    compile_pattern (mon_ops->getmem.resp_delim, &getmem_resp_delim_pattern,
		     getmem_resp_delim_fastmap);

  unpush_target (targ_ops);

  if (dev_name)
    free (dev_name);
  dev_name = strsave (args);

  monitor_desc = SERIAL_OPEN (dev_name);

  if (!monitor_desc)
    perror_with_name (dev_name);

  if (baud_rate != -1)
    {
      if (SERIAL_SETBAUDRATE (monitor_desc, baud_rate))
	{
	  SERIAL_CLOSE (monitor_desc);
	  perror_with_name (dev_name);
	}
    }
  
  SERIAL_RAW (monitor_desc);

  SERIAL_FLUSH_INPUT (monitor_desc);

  /* some systems only work with 2 stop bits */

  SERIAL_SETSTOPBITS (monitor_desc, mon_ops->stopbits);

  current_monitor = mon_ops;

  /* See if we can wake up the monitor.  First, try sending a stop sequence,
     then send the init strings.  Last, remove all breakpoints.  */

  if (current_monitor->stop)
    {
      monitor_stop ();
      if ((current_monitor->flags & MO_NO_ECHO_ON_OPEN) == 0)
        {
        monitor_expect_prompt (NULL, 0); 
      }
    }

  /* wake up the monitor and see if it's alive */
  for (p = mon_ops->init; *p != NULL; p++)
    {
      /* Some of the characters we send may not be echoed,
	 but we hope to get a prompt at the end of it all. */
	 
      if ((current_monitor->flags & MO_NO_ECHO_ON_OPEN) == 0)
        monitor_printf(*p); 
      else
        monitor_printf_noecho (*p);
      monitor_expect_prompt (NULL, 0);
    }

  SERIAL_FLUSH_INPUT (monitor_desc);

  /* Remove all breakpoints */

  if (mon_ops->clr_all_break)
    {
      monitor_printf (mon_ops->clr_all_break);
      monitor_expect_prompt (NULL, 0);
    }

  if (from_tty)
    printf_unfiltered ("Remote target %s connected to %s\n", name, dev_name);

  push_target (targ_ops);

  inferior_pid = 42000;		/* Make run command think we are busy... */

  /* Give monitor_wait something to read */

  monitor_printf (current_monitor->line_term);

  remote_dcache = dcache_init (monitor_read_memory, monitor_write_memory);

  start_remote ();
}

/* Close out all files and local state before this target loses
   control.  */

void
monitor_close (quitting)
     int quitting;
{
  if (monitor_desc)
    SERIAL_CLOSE (monitor_desc);
  monitor_desc = NULL;
}

/* Terminate the open connection to the remote debugger.  Use this
   when you want to detach and do something else with your gdb.  */

static void
monitor_detach (args, from_tty)
     char *args;
     int from_tty;
{
  pop_target ();		/* calls monitor_close to do the real work */
  if (from_tty)
    printf_unfiltered ("Ending remote %s debugging\n", target_shortname);
}

/* Convert VALSTR into the target byte-ordered value of REGNO and store it.  */

char *
monitor_supply_register (regno, valstr)
     int regno;
     char *valstr;
{
  unsigned LONGEST val;
  unsigned char regbuf[MAX_REGISTER_RAW_SIZE];
  char *p;

  val = strtoul (valstr, &p, 16);

  if (val == 0 && valstr == p)
    error ("monitor_supply_register (%d):  bad value from monitor: %s.",
	   regno, valstr);

  /* supply register stores in target byte order, so swap here */

  store_unsigned_integer (regbuf, REGISTER_RAW_SIZE (regno), val);

  supply_register (regno, regbuf);

  return p;
}

/* Tell the remote machine to resume.  */

static void
monitor_resume (pid, step, sig)
     int pid, step;
     enum target_signal sig;
{
  dcache_flush (remote_dcache);
  if (step)
    monitor_printf (STEP_CMD);
  else
    {
      monitor_printf (CONT_CMD);
      if (current_monitor->flags & MO_NEED_REGDUMP_AFTER_CONT)
	dump_reg_flag = 1;
    }
}

/* Parse the output of a register dump command.  A monitor specific
   regexp is used to extract individual register descriptions of the
   form REG=VAL.  Each description is split up into a name and a value
   string which are passed down to monitor specific code.  */

static char *
parse_register_dump (buf, len)
     char *buf;
     int len;
{
  while (1)
    {
      int regnamelen, vallen;
      char *regname, *val;
      /* Element 0 points to start of register name, and element 1
	 points to the start of the register value.  */
      struct re_registers register_strings;

      if (re_search (&register_pattern, buf, len, 0, len,
		     &register_strings) == -1)
	break;

      regnamelen = register_strings.end[1] - register_strings.start[1];
      regname = buf + register_strings.start[1];
      vallen = register_strings.end[2] - register_strings.start[2];
      val = buf + register_strings.start[2];

      current_monitor->supply_register (regname, regnamelen, val, vallen);

      buf += register_strings.end[0];
      len -= register_strings.end[0];
    }
}

/* Send ^C to target to halt it.  Target will respond, and send us a
   packet.  */

static void
monitor_interrupt (signo)
     int signo;
{
  /* If this doesn't work, try more severe steps.  */
  signal (signo, monitor_interrupt_twice);
  
  if (remote_debug)
    printf_unfiltered ("monitor_interrupt called\n");

  target_stop ();
}

/* The user typed ^C twice.  */

static void
monitor_interrupt_twice (signo)
     int signo;
{
  signal (signo, ofunc);
  
  monitor_interrupt_query ();

  signal (signo, monitor_interrupt);
}

/* Ask the user what to do when an interrupt is received.  */

static void
monitor_interrupt_query ()
{
  target_terminal_ours ();

  if (query ("Interrupted while waiting for the program.\n\
Give up (and stop debugging it)? "))
    {
      target_mourn_inferior ();
      return_to_top_level (RETURN_QUIT);
    }

  target_terminal_inferior ();
}

static void
monitor_wait_cleanup (old_timeout)
     int old_timeout;
{
  timeout = old_timeout;
  signal (SIGINT, ofunc);
  in_monitor_wait = 0;
}

/* Wait until the remote machine stops, then return, storing status in
   status just as `wait' would.  */

static int
monitor_wait (pid, status)
     int pid;
     struct target_waitstatus *status;
{
  int old_timeout = timeout;
  char buf[1024];
  int resp_len;
  struct cleanup *old_chain;

  status->kind = TARGET_WAITKIND_EXITED;
  status->value.integer = 0;

  old_chain = make_cleanup (monitor_wait_cleanup, old_timeout);

#ifdef MAINTENANCE_CMDS
  in_monitor_wait = 1;
  timeout = watchdog > 0 ? watchdog : -1;
#else
  timeout = -1;		/* Don't time out -- user program is running. */
#endif

  ofunc = (void (*)()) signal (SIGINT, monitor_interrupt);

  do
    {
      resp_len = monitor_expect_prompt (buf, sizeof (buf));

      if (resp_len <= 0)
	fprintf_unfiltered (gdb_stderr, "monitor_wait:  excessive response from monitor: %s.", buf);
    }
  while (resp_len < 0);

  signal (SIGINT, ofunc);

  timeout = old_timeout;

  if (dump_reg_flag && current_monitor->dump_registers)
    {
      dump_reg_flag = 0;

      monitor_printf (current_monitor->dump_registers);
      resp_len = monitor_expect_prompt (buf, sizeof (buf));
    }

  if (current_monitor->register_pattern)
    parse_register_dump (buf, resp_len);

  status->kind = TARGET_WAITKIND_STOPPED;
  status->value.sig = TARGET_SIGNAL_TRAP;

  discard_cleanups (old_chain);

  in_monitor_wait = 0;

  return inferior_pid;
}

/* Fetch register REGNO, or all registers if REGNO is -1. Returns
   errno value.  */

static void
monitor_fetch_register (regno)
     int regno;
{
  char *name;
  static char zerobuf[MAX_REGISTER_RAW_SIZE] = {0};
  char regbuf[MAX_REGISTER_RAW_SIZE * 2 + 1];
  int i;

  name = REGNAMES (regno);

  if (!name)
    {
      supply_register (regno, zerobuf);
      return;
    }

  /* send the register examine command */

  monitor_printf (current_monitor->getreg.cmd, name);

  /* If RESP_DELIM is specified, we search for that as a leading
     delimiter for the register value.  Otherwise, we just start
     searching from the start of the buf.  */

  if (current_monitor->getreg.resp_delim)
    monitor_expect (current_monitor->getreg.resp_delim, NULL, 0);

  /* Read upto the maximum number of hex digits for this register, skipping
     spaces, but stop reading if something else is seen.  Some monitors
     like to drop leading zeros.  */

  for (i = 0; i < REGISTER_RAW_SIZE (regno) * 2; i++)
    {
      int c;
      c = readchar (timeout);
      while (c == ' ')
	c = readchar (timeout);

      if (!isxdigit (c))
	break;

      regbuf[i] = c;
    }

  regbuf[i] = '\000';		/* terminate the number */

  /* If TERM is present, we wait for that to show up.  Also, (if TERM
     is present), we will send TERM_CMD if that is present.  In any
     case, we collect all of the output into buf, and then wait for
     the normal prompt.  */

  if (current_monitor->getreg.term)
    {
      monitor_expect (current_monitor->getreg.term, NULL, 0); /* get response */

      if (current_monitor->getreg.term_cmd)
	{
	  monitor_printf (current_monitor->getreg.term_cmd);
	  monitor_expect_prompt (NULL, 0);
	}
    }
  else
    monitor_expect_prompt (NULL, 0); /* get response */

  monitor_supply_register (regno, regbuf);
}

/* Read the remote registers into the block regs.  */

static void monitor_dump_regs ()
{
  if (current_monitor->dump_registers)
    {
      char buf[200];
      int resp_len;

      monitor_printf (current_monitor->dump_registers);
      resp_len = monitor_expect_prompt (buf, sizeof (buf));
      parse_register_dump (buf, resp_len);
    }
  else
    abort(); /* Need some way to read registers */
}

static void
monitor_fetch_registers (regno)
     int regno;
{
  if (current_monitor->getreg.cmd) 
    {
      if (regno >= 0)
	{
	  monitor_fetch_register (regno);
	  return;
	}

      for (regno = 0; regno < NUM_REGS; regno++)
	monitor_fetch_register (regno);
    }
  else {
    monitor_dump_regs ();
  }
}

/* Store register REGNO, or all if REGNO == 0.  Return errno value.  */

static void
monitor_store_register (regno)
     int regno;
{
  char *name;
  unsigned LONGEST val;

  name = REGNAMES (regno);
  if (!name)
    return;

  val = read_register (regno);

 /* send the register deposit command */

  monitor_printf (current_monitor->setreg.cmd, name, val);

/* It's possible that there are actually some monitors out there that
   will prompt you when you set a register.  In that case, you may
   need to add some code here to deal with TERM and TERM_CMD (see
   monitor_fetch_register to get an idea of what's needed...) */

  monitor_expect_prompt (NULL, 0);
}

/* Store the remote registers.  */

static void
monitor_store_registers (regno)
     int regno;
{
  if (regno >= 0)
    {
      monitor_store_register (regno);
      return;
    }

  for (regno = 0; regno < NUM_REGS; regno++)
    monitor_store_register (regno);
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
monitor_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

static void
monitor_files_info (ops)
     struct target_ops *ops;
{
  printf_unfiltered ("\tAttached to %s at %d baud.\n", dev_name, baud_rate);
}

static int
monitor_write_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  unsigned LONGEST val;
  char *cmd;
  int i;

  /* Use memory fill command for leading 0 bytes.  */

  if (current_monitor->fill)
    {
      for (i = 0; i < len; i++)
	if (myaddr[i] != 0)
	  break;

      if (i > 4)		/* More than 4 zeros is worth doing */
	{
	  if (current_monitor->flags & MO_FILL_USES_ADDR)
	    monitor_printf (current_monitor->fill, memaddr, memaddr + i, 0);
	  else
	    monitor_printf (current_monitor->fill, memaddr, i, 0);

	  monitor_expect_prompt (NULL, 0);

	  return i;
	}
    }

  if ((memaddr & 0x7) == 0 && len >= 8 && current_monitor->setmem.cmdll)
    {
      len = 8;
      cmd = current_monitor->setmem.cmdll;
    }
  else if ((memaddr & 0x3) == 0 && len >= 4 && current_monitor->setmem.cmdl)
    {
      len = 4;
      cmd = current_monitor->setmem.cmdl;
    }
  else if ((memaddr & 0x1) == 0 && len >= 2 && current_monitor->setmem.cmdw)
    {
      len = 2;
      cmd = current_monitor->setmem.cmdw;
    }
  else
    {
      len = 1;
      cmd = current_monitor->setmem.cmdb;
    }

  val = extract_unsigned_integer (myaddr, len);

  monitor_printf (cmd, memaddr, val);

  monitor_expect_prompt (NULL, 0);

  return len;
}

/* This is an alternate form of monitor_read_memory which is used for monitors
   which can only read a single byte/word/etc. at a time.  */

static int
monitor_read_memory_single (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  unsigned LONGEST val;
  char membuf[sizeof(LONGEST) * 2 + 1];
  char *p;
  char *cmd;
  int i;

  if ((memaddr & 0x7) == 0 && len >= 8 && current_monitor->getmem.cmdll)
    {
      len = 8;
      cmd = current_monitor->getmem.cmdll;
    }
  else if ((memaddr & 0x3) == 0 && len >= 4 && current_monitor->getmem.cmdl)
    {
      len = 4;
      cmd = current_monitor->getmem.cmdl;
    }
  else if ((memaddr & 0x1) == 0 && len >= 2 && current_monitor->getmem.cmdw)
    {
      len = 2;
      cmd = current_monitor->getmem.cmdw;
    }
  else
    {
      len = 1;
      cmd = current_monitor->getmem.cmdb;
    }

/* Send the examine command.  */

  monitor_printf (cmd, memaddr);

/* If RESP_DELIM is specified, we search for that as a leading delimiter for
   the register value.  Otherwise, we just start searching from the start of
   the buf.  */

  if (current_monitor->getmem.resp_delim)
    monitor_expect_regexp (&getmem_resp_delim_pattern, NULL, 0);

/* Now, read the appropriate number of hex digits for this loc, skipping
   spaces.  */

  for (i = 0; i < len * 2; i++)
    {
      int c;

      while (1)
	{
	  c = readchar (timeout);
	  if (isxdigit (c))
	    break;
	  if (c == ' ')
	    continue;

	  error ("monitor_read_memory_single (0x%x):  bad response from monitor: %.*s%c.",
		 memaddr, i, membuf, c);
	}

      membuf[i] = c;
    }

  membuf[i] = '\000';		/* terminate the number */

/* If TERM is present, we wait for that to show up.  Also, (if TERM is
   present), we will send TERM_CMD if that is present.  In any case, we collect
   all of the output into buf, and then wait for the normal prompt.  */

  if (current_monitor->getmem.term)
    {
      monitor_expect (current_monitor->getmem.term, NULL, 0); /* get response */

      if (current_monitor->getmem.term_cmd)
	{
	  monitor_printf (current_monitor->getmem.term_cmd);
	  monitor_expect_prompt (NULL, 0);
	}
    }
  else
    monitor_expect_prompt (NULL, 0); /* get response */

  p = membuf;
  val = strtoul (membuf, &p, 16);

  if (val == 0 && membuf == p)
    error ("monitor_read_memory_single (0x%x):  bad value from monitor: %s.",
	   memaddr, membuf);

  /* supply register stores in target byte order, so swap here */

  store_unsigned_integer (myaddr, len, val);

  return len;
}

/* Copy LEN bytes of data from debugger memory at MYADDR to inferior's memory
   at MEMADDR.  Returns length moved.  Currently, we only do one byte at a
   time.  */

static int
monitor_read_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  unsigned LONGEST val;
  unsigned char regbuf[MAX_REGISTER_RAW_SIZE];
  char buf[512];
  char *p, *p1;
  char *name;
  int resp_len;
  int i;

  if (current_monitor->flags & MO_GETMEM_READ_SINGLE)
    return monitor_read_memory_single (memaddr, myaddr, len);

  len = min (len, 16);

/* See if xfer would cross a 16 byte boundary.  If so, clip it.  */
  if (((memaddr ^ (memaddr + len - 1)) & ~0xf) != 0)
    len = ((memaddr + len) & ~0xf) - memaddr;

 /* send the memory examine command */

  if (current_monitor->flags & MO_GETMEM_NEEDS_RANGE)
    monitor_printf (current_monitor->getmem.cmdb, memaddr, memaddr + len - 1);
  else
    monitor_printf (current_monitor->getmem.cmdb, memaddr, len);

/* If TERM is present, we wait for that to show up.  Also, (if TERM is
   present), we will send TERM_CMD if that is present.  In any case, we collect
   all of the output into buf, and then wait for the normal prompt.  */

  if (current_monitor->getmem.term)
    {
      resp_len = monitor_expect (current_monitor->getmem.term, buf, sizeof buf); /* get response */

      if (resp_len <= 0)
	error ("monitor_read_memory (0x%x):  excessive response from monitor: %.*s.",
	       memaddr, resp_len, buf);

      if (current_monitor->getmem.term_cmd)
	{
	  SERIAL_WRITE (monitor_desc, current_monitor->getmem.term_cmd,
			strlen (current_monitor->getmem.term_cmd));
	  monitor_expect_prompt (NULL, 0);
	}
    }
  else
    resp_len = monitor_expect_prompt (buf, sizeof buf); /* get response */

  p = buf;

  /* If RESP_DELIM is specified, we search for that as a leading delimiter for
     the values.  Otherwise, we just start searching from the start of the buf.
   */

  if (current_monitor->getmem.resp_delim)
    {
      int retval, tmp;
      struct re_registers resp_strings;

      tmp = strlen (p);
      retval = re_search (&getmem_resp_delim_pattern, p, tmp, 0, tmp,
			  &resp_strings);

      if (retval < 0)
	error ("monitor_read_memory (0x%x):  bad response from monitor: %.*s.",
	       memaddr, resp_len, buf);

      p += resp_strings.end[0];
#if 0
      p = strstr (p, current_monitor->getmem.resp_delim);
      if (!p)
	error ("monitor_read_memory (0x%x):  bad response from monitor: %.*s.",
	       memaddr, resp_len, buf);
      p += strlen (current_monitor->getmem.resp_delim);
#endif
    }

  for (i = len; i > 0; i--)
    {
      /* Skip non-hex chars, but bomb on end of string and newlines */

      while (1)
	{
	  if (isxdigit (*p))
	    break;
	  if (*p == '\000' || *p == '\n' || *p == '\r')
	    error ("monitor_read_memory (0x%x):  badly terminated response from monitor: %.*s", memaddr, resp_len, buf);
	  p++;
	}

      val = strtoul (p, &p1, 16);

      if (val == 0 && p == p1)
	error ("monitor_read_memory (0x%x):  bad value from monitor: %.*s.", memaddr,
	       resp_len, buf);

      *myaddr++ = val;

      if (i == 1)
	break;

      p = p1;
    }

  return len;
}

static int
monitor_xfer_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;		/* ignored */
{
  return dcache_xfer_memory (remote_dcache, memaddr, myaddr, len, write);
}

static void
monitor_kill ()
{
  return;		/* ignore attempts to kill target system */
}

/* All we actually do is set the PC to the start address of exec_bfd, and start
   the program at that point.  */

static void
monitor_create_inferior (exec_file, args, env)
     char *exec_file;
     char *args;
     char **env;
{
  if (args && (*args != '\000'))
    error ("Args are not supported by the monitor.");

  clear_proceed_status ();
  proceed (bfd_get_start_address (exec_bfd), TARGET_SIGNAL_0, 0);
}

/* Clean up when a program exits.
   The program actually lives on in the remote processor's RAM, and may be
   run again without a download.  Don't leave it full of breakpoint
   instructions.  */

static void
monitor_mourn_inferior ()
{
  unpush_target (targ_ops);
  generic_mourn_inferior ();	/* Do all the proper things now */
}

#define NUM_MONITOR_BREAKPOINTS 8

static CORE_ADDR breakaddr[NUM_MONITOR_BREAKPOINTS] = {0};

/* Tell the monitor to add a breakpoint.  */

static int
monitor_insert_breakpoint (addr, shadow)
     CORE_ADDR addr;
     char *shadow;
{
  int i;
  static unsigned char break_insn[] = BREAKPOINT;

  for (i = 0; i < NUM_MONITOR_BREAKPOINTS; i++)
    {
      if (breakaddr[i] == 0)
	{
	  breakaddr[i] = addr;
	  monitor_read_memory (addr, shadow, sizeof (break_insn));
	  monitor_printf (SET_BREAK_CMD, addr);
	  monitor_expect_prompt (NULL, 0);
	  return 0;
	}
    }

  error ("Too many breakpoints (> %d) for monitor.", NUM_MONITOR_BREAKPOINTS);
}

/* Tell the monitor to remove a breakpoint.  */

static int
monitor_remove_breakpoint (addr, shadow)
     CORE_ADDR addr;
     char *shadow;
{
  int i;

  for (i = 0; i < NUM_MONITOR_BREAKPOINTS; i++)
    {
      if (breakaddr[i] == addr)
	{
	  breakaddr[i] = 0;
	  /* some monitors remove breakpoints based on the address */
	  if (current_monitor->flags & MO_CLR_BREAK_USES_ADDR)   
	    monitor_printf (CLR_BREAK_CMD, addr);
	  else
	    monitor_printf (CLR_BREAK_CMD, i);
	  monitor_expect_prompt (NULL, 0);
	  return 0;
	}
    }
  fprintf_unfiltered (stderr, "Can't find breakpoint associated with 0x%x\n", addr);
  return 1;
}

/* monitor_load -- download a file. */

static void
monitor_load (file, from_tty)
    char *file;
    int  from_tty;
{
  dcache_flush (remote_dcache);

  if (current_monitor->load_routine)
    current_monitor->load_routine (monitor_desc, file, hashmark);
  else
    {				/* The default is ascii S-records */
      monitor_printf (LOAD_CMD);	/* tell the monitor to load */
      if (current_monitor->loadresp)
	monitor_expect (current_monitor->loadresp, NULL, 0);

      load_srec (monitor_desc, file, 32, SREC_ALL, hashmark);

      monitor_expect_prompt (NULL, 0);
    }

/* Finally, make the PC point at the start address */

  if (exec_bfd)
    write_pc (bfd_get_start_address (exec_bfd));

  inferior_pid = 0;		/* No process now */

/* This is necessary because many things were based on the PC at the time that
   we attached to the monitor, which is no longer valid now that we have loaded
   new code (and just changed the PC).  Another way to do this might be to call
   normal_stop, except that the stack may not be valid, and things would get
   horribly confused... */

  clear_symtab_users ();
}

static void
monitor_stop ()
{
  if ((current_monitor->flags & MO_SEND_BREAK_ON_STOP) != 0)
    SERIAL_SEND_BREAK (monitor_desc);
  if (current_monitor->stop)
    monitor_printf_noecho (current_monitor->stop);
}

/* Put a command string, in args, out to MONITOR.  Output from MONITOR
   is placed on the users terminal until the prompt is seen. FIXME: We
   read the characters ourseleves here cause of a nasty echo.  */

static void
monitor_command (args, from_tty)
     char *args;
     int from_tty;
{
  char *p;
  int resp_len;
  char buf[1000];

  if (monitor_desc == NULL)
    error ("monitor target not open.");

  p = PROMPT;

  /* Send the command.  Note that if no args were supplied, then we're
     just sending the monitor a newline, which is sometimes useful.  */

  monitor_printf ("%s\r", (args ? args : ""));

  resp_len = monitor_expect_prompt (buf, sizeof buf);

  fputs_unfiltered (buf, gdb_stdout); /* Output the response */
}

/* Convert hex digit A to a number.  */

static int
from_hex (a)
     int a;
{  
  if (a >= '0' && a <= '9')
    return a - '0';
  if (a >= 'a' && a <= 'f')
    return a - 'a' + 10;
  if (a >= 'A' && a <= 'F')
    return a - 'A' + 10;

  error ("Reply contains invalid hex digit 0x%x", a);
}

static struct target_ops monitor_ops =
{
  NULL,				/* to_shortname */
  NULL,				/* to_longname */
  NULL,				/* to_doc */
  NULL,				/* to_open */
  monitor_close,		/* to_close */
  NULL,				/* to_attach */
  monitor_detach,		/* to_detach */
  monitor_resume,		/* to_resume */
  monitor_wait,			/* to_wait */
  monitor_fetch_registers,	/* to_fetch_registers */
  monitor_store_registers,	/* to_store_registers */
  monitor_prepare_to_store,	/* to_prepare_to_store */
  monitor_xfer_memory,		/* to_xfer_memory */
  monitor_files_info,		/* to_files_info */
  monitor_insert_breakpoint,	/* to_insert_breakpoint */
  monitor_remove_breakpoint,	/* to_remove_breakpoint */
  0,				/* to_terminal_init */
  0,				/* to_terminal_inferior */
  0,				/* to_terminal_ours_for_output */
  0,				/* to_terminal_ours */
  0,				/* to_terminal_info */
  monitor_kill,			/* to_kill */
  monitor_load,			/* to_load */
  0,				/* to_lookup_symbol */
  monitor_create_inferior,	/* to_create_inferior */
  monitor_mourn_inferior,	/* to_mourn_inferior */
  0,				/* to_can_run */
  0, 				/* to_notice_signals */
  0,				/* to_thread_alive */
  monitor_stop,			/* to_stop */
  process_stratum,		/* to_stratum */
  0,				/* to_next */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  1,				/* to_has_execution */
  0,				/* sections */
  0,				/* sections_end */
  OPS_MAGIC			/* to_magic */
};

/* Init the target_ops structure pointed at by OPS */

void
init_monitor_ops (ops)
     struct target_ops *ops;
{
  memcpy (ops, &monitor_ops, sizeof monitor_ops);
}

/* Define additional commands that are usually only used by monitors.  */

void
_initialize_remote_monitors ()
{
  add_show_from_set (add_set_cmd ("hash", no_class, var_boolean,
                                  (char *)&hashmark,
				  "Set display of activity while downloading a file.\n\
When enabled, a hashmark \'#\' is displayed.",
                                  &setlist),
		     &showlist);

  add_com ("monitor", class_obscure, monitor_command,
	   "Send a command to the debug monitor."); 
}
