/* Remote debugging interface for Hitachi HMS Monitor Version 1.0
   Copyright 1995 Free Software Foundation, Inc.
   Contributed by Cygnus Support.  Written by Steve Chamberlain
   (sac@cygnus.com).

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

static void hms_open PARAMS ((char *args, int from_tty));

static void
hms_supply_register (regname, regnamelen, val, vallen)
     char *regname;
     int regnamelen;
     char *val;
     int vallen;
{
  int regno;

  if (regnamelen != 2)
    return;
  if (regname[0] != 'P')
    return;
  /* We scan off all the registers in one go */

  val = monitor_supply_register (PC_REGNUM, val);
  /* Skip the ccr string */
  while (*val != '=' && *val)
    val++;

  val = monitor_supply_register (CCR_REGNUM, val + 1);

  /* Skip up to rest of regs */
  while (*val != '=' && *val)
    val++;

  for (regno = 0; regno < 7; regno++)
    {
      val = monitor_supply_register (regno, val + 1);
    }
}

/*
 * This array of registers needs to match the indexes used by GDB. The
 * whole reason this exists is because the various ROM monitors use
 * different names than GDB does, and don't support all the
 * registers either. So, typing "info reg sp" becomes a "r30".
 */

static char *hms_regnames[NUM_REGS] =
{
  "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "CCR", "PC"
};

/*
 * Define the monitor command strings. Since these are passed directly
 * through to a printf style function, we need can include formatting
 * strings. We also need a CR or LF on the end.
 */

static struct target_ops hms_ops;

static char *hms_inits[] =
{"\003",			/* Resets the prompt, and clears repeated cmds */
 NULL};

static struct monitor_ops hms_cmds =
{
  MO_CLR_BREAK_USES_ADDR | MO_FILL_USES_ADDR | MO_GETMEM_NEEDS_RANGE,
  hms_inits,			/* Init strings */
  "g\r",			/* continue command */
  "s\r",			/* single step */
  "\003",			/* ^C interrupts the program */
  "b %x\r",			/* set a breakpoint */
  "b - %x\r",			/* clear a breakpoint */
  "b -\r",			/* clear all breakpoints */
  "f %x %x %x\r",		/* fill (start end val) */
  {
    "m.b %x=%x\r",		/* setmem.cmdb (addr, value) */
    "m.w %x=%x\r",		/* setmem.cmdw (addr, value) */
    NULL,			/* setmem.cmdl (addr, value) */
    NULL,			/* setmem.cmdll (addr, value) */
    NULL,			/* setreg.resp_delim */
    NULL,			/* setreg.term */
    NULL,			/* setreg.term_cmd */
  },
  {
    "m.b %x %x\r",		/* getmem.cmdb (addr, addr) */
    "m.w %x %x\r",		/* getmem.cmdw (addr, addr) */
    NULL,			/* getmem.cmdl (addr, addr) */
    NULL,			/* getmem.cmdll (addr, addr) */
    ": ",			/* getmem.resp_delim */
    ">",			/* getmem.term */
    "\003",			/* getmem.term_cmd */
  },
  {
    "r %s=%x\r",		/* setreg.cmd (name, value) */
    NULL,			/* setreg.resp_delim */
    NULL,			/* setreg.term */
    NULL			/* setreg.term_cmd */
  },
  {
    "r %s\r",			/* getreg.cmd (name) */
    " (",			/* getreg.resp_delim */
    ":",			/* getreg.term */
    "\003",			/* getreg.term_cmd */
  },
  "r\r",			/* dump_registers */
  "\\(\\w+\\)=\\([0-9a-fA-F]+\\)",	/* register_pattern */
  hms_supply_register,		/* supply_register */
  NULL,				/* load_routine (defaults to SRECs) */
  "tl\r",			/* download command */
  NULL,				/* load response */
  ">",				/* monitor command prompt */
  "\r",				/* end-of-command delimitor */
  NULL,				/* optional command terminator */
  &hms_ops,			/* target operations */
  SERIAL_1_STOPBITS,		/* number of stop bits */
  hms_regnames,			/* registers names */
  MONITOR_OPS_MAGIC		/* magic */
};

static void
hms_open (args, from_tty)
     char *args;
     int from_tty;
{
  monitor_open (args, &hms_cmds, from_tty);
}

int write_dos_tick_delay;

void
_initialize_remote_hms ()
{
  init_monitor_ops (&hms_ops);

  hms_ops.to_shortname = "hms";
  hms_ops.to_longname = "Hitachi Microsystems H8/300 debug monitor";
  hms_ops.to_doc = "Debug via the HMS monitor.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";
  hms_ops.to_open = hms_open;
  /* By trial and error I've found that this delay doesn't break things */
  write_dos_tick_delay = 1;
  add_target (&hms_ops);
}


#if 0
/* This is kept here because we used to support the H8/500 in this module,
   and I haven't done the H8/500 yet */
#include "defs.h"
#include "inferior.h"
#include "wait.h"
#include "value.h"
#include "gdb_string.h"
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include "terminal.h"
#include "target.h"
#include "gdbcore.h"
#include "serial.h"
#include "remote-utils.h"
/* External data declarations */
extern int stop_soon_quietly;	/* for wait_for_inferior */

/* Forward data declarations */
extern struct target_ops hms_ops;	/* Forward declaration */

/* Forward function declarations */
static void hms_fetch_registers ();
static int hms_store_registers ();
static void hms_close ();
static int hms_clear_breakpoints ();

extern struct target_ops hms_ops;
static void hms_drain ();
static void add_commands ();
static void remove_commands ();

static int quiet = 1;		/* FIXME - can be removed after Dec '94 */



/***********************************************************************
 * I/O stuff stolen from remote-eb.c
 ***********************************************************************/

static int timeout = 2;

static const char *dev_name;

/* Descriptor for I/O to remote machine.  Initialize it to -1 so that
   hms_open knows that we don't have a file open when the program
   starts.  */

static int before = 0xdead;
static int is_open = 0;
static int after = 0xdead;
int
check_open ()
{
  if (before != 0xdead
      || after != 0xdead)
    printf ("OUTCH! \n");
  if (!is_open)
    {
      error ("remote device not open");
    }
}

#define ON	1
#define OFF	0

/* Read a character from the remote system, doing all the fancy
   timeout stuff.  */
static int
readchar ()
{
  int buf;

  buf = SERIAL_READCHAR (desc, timeout);

  if (buf == SERIAL_TIMEOUT)
    {
      hms_write (".\r\n", 3);
      error ("Timeout reading from remote system.");
    }
  if (buf == SERIAL_ERROR)
    {
      error ("Serial port error!");
    }

  if (!quiet || remote_debug)
    printf_unfiltered ("%c", buf);

  return buf & 0x7f;
}

static void
flush ()
{
  while (1)
    {
      int b = SERIAL_READCHAR (desc, 0);
      if (b == SERIAL_TIMEOUT)
	return;
    }
}

static int
readchar_nofail ()
{
  int buf;

  buf = SERIAL_READCHAR (desc, timeout);
  if (buf == SERIAL_TIMEOUT)
    buf = 0;
  if (!quiet || remote_debug)
    printf_unfiltered ("%c", buf);

  return buf & 0x7f;

}

/* Keep discarding input from the remote system, until STRING is found.
   Let the user break out immediately.  */
static void
expect (string)
     char *string;
{
  char *p = string;
  char c;
  immediate_quit = 1;
  while (1)
    {
      c = readchar ();
      if (c == *p)
	{
	  p++;
	  if (*p == '\0')
	    {
	      immediate_quit = 0;
	      return;
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

/* Keep discarding input until we see the hms prompt.

   The convention for dealing with the prompt is that you
   o give your command
   o *then* wait for the prompt.

   Thus the last thing that a procedure does with the serial line
   will be an expect_prompt().  Exception:  hms_resume does not
   wait for the prompt, because the terminal is being handed over
   to the inferior.  However, the next thing which happens after that
   is a hms_wait which does wait for the prompt.
   Note that this includes abnormal exit, e.g. error().  This is
   necessary to prevent getting into states from which we can't
   recover.  */
static void
expect_prompt ()
{
  expect ("HMS>");
}

/* Get a hex digit from the remote system & return its value.
   If ignore_space is nonzero, ignore spaces (not newline, tab, etc).  */
static int
get_hex_digit (ignore_space)
     int ignore_space;
{
  int ch;

  while (1)
    {
      ch = readchar ();
      if (ch >= '0' && ch <= '9')
	return ch - '0';
      else if (ch >= 'A' && ch <= 'F')
	return ch - 'A' + 10;
      else if (ch >= 'a' && ch <= 'f')
	return ch - 'a' + 10;
      else if (ch == ' ' && ignore_space)
	;
      else
	{
	  expect_prompt ();
	  error ("Invalid hex digit from remote system.");
	}
    }
}

/* Get a byte from hms_desc and put it in *BYT.  Accept any number
   leading spaces.  */
static void
get_hex_byte (byt)
     char *byt;
{
  int val;

  val = get_hex_digit (1) << 4;
  val |= get_hex_digit (0);
  *byt = val;
}

/* Read a 32-bit hex word from the hms, preceded by a space  */
static long
get_hex_word ()
{
  long val;
  int j;

  val = 0;
  for (j = 0; j < 8; j++)
    val = (val << 4) + get_hex_digit (j == 0);
  return val;
}

/* Called when SIGALRM signal sent due to alarm() timeout.  */

/* Number of SIGTRAPs we need to simulate.  That is, the next
   NEED_ARTIFICIAL_TRAP calls to hms_wait should just return
   SIGTRAP without actually waiting for anything.  */

static int need_artificial_trap = 0;

void
hms_kill (arg, from_tty)
     char *arg;
     int from_tty;
{

}

/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */
void
hms_create_inferior (execfile, args, env)
     char *execfile;
     char *args;
     char **env;
{
  int entry_pt;
  char buffer[100];

  if (args && *args)
    error ("Can't pass arguments to remote hms process.");

  if (execfile == 0 || exec_bfd == 0)
    error ("No exec file specified");

  entry_pt = (int) bfd_get_start_address (exec_bfd);
  check_open ();

  hms_kill (NULL, NULL);
  hms_clear_breakpoints ();
  init_wait_for_inferior ();
  hms_write_cr ("");
  expect_prompt ();

  insert_breakpoints ();	/* Needed to get correct instruction in cache */
  proceed (entry_pt, TARGET_SIGNAL_DEFAULT, 0);
}

/* Open a connection to a remote debugger.
   NAME is the filename used for communication, then a space,
   then the baud rate.
 */

static char *
find_end_of_word (s)
     char *s;
{
  while (*s && !isspace (*s))
    s++;
  return s;
}

static char *
get_word (p)
     char **p;
{
  char *s = *p;
  char *word;
  char *copy;
  size_t len;

  while (isspace (*s))
    s++;

  word = s;

  len = 0;

  while (*s && !isspace (*s))
    {
      s++;
      len++;

    }
  copy = xmalloc (len + 1);
  memcpy (copy, word, len);
  copy[len] = 0;
  *p = s;
  return copy;
}

static int baudrate = 9600;

static int
is_baudrate_right ()
{
  int ok;

  /* Put this port into NORMAL mode, send the 'normal' character */

  hms_write ("\001", 1);	/* Control A */
  hms_write ("\r\n", 2);	/* Cr */

  while (1)
    {
      ok = SERIAL_READCHAR (desc, timeout);
      if (ok < 0)
	break;
    }

  hms_write ("r", 1);

  if (readchar_nofail () == 'r')
    return 1;

  /* Not the right baudrate, or the board's not on */
  return 0;
}
static void
set_rate ()
{
  if (!SERIAL_SETBAUDRATE (desc, baudrate))
    error ("Can't set baudrate");
}



/* Close out all files and local state before this target loses control. */

static void
hms_close (quitting)
     int quitting;
{
  /* Clear any break points */
  remove_commands ();
  hms_clear_breakpoints ();
  sleep (1);			/* Let any output make it all the way back */
  if (is_open)
    {
      SERIAL_WRITE (desc, "R\r\n", 3);
      SERIAL_CLOSE (desc);
    }
  is_open = 0;
}

/* Terminate the open connection to the remote debugger.  Use this
   when you want to detach and do something else with your gdb.  */ void
hms_detach (args, from_tty)
     char *args;
     int from_tty;
{
  if (is_open)
    {
      hms_clear_breakpoints ();
    }

  pop_target ();		/* calls hms_close to do the real work
				 */
  if (from_tty)
    printf_filtered ("Ending remote %s debugging\n",
		     target_shortname);
}

/* Tell the remote machine to resume.  */

void
hms_resume (pid, step, sig)
     int pid, step;
     enum target_signal
       sig;
{
  if (step)
    {
      hms_write_cr ("s");
      expect ("Step>");

      /* Force the next hms_wait to return a trap.  Not doing anything
         about I/O from the target means that the user has to type "continue"
         to see any.  FIXME, this should be fixed.  */
      need_artificial_trap = 1;
    }
  else
    {
      hms_write_cr ("g");
      expect ("g");
    }
}

/* Wait until the remote machine stops, then return, storing status in
   STATUS just as `wait' would.  */

int
hms_wait (pid, status)
     int pid;
     struct target_waitstatus *status;
{
  /* Strings to look for.  '?' means match any single character.  Note
     that with the algorithm we use, the initial character of the string
     cannot recur in the string, or we will not find some cases of the
     string in the input.  */

  static char bpt[] = "At breakpoint:";

  /* It would be tempting to look for "\n[__exit + 0x8]\n" but that
     requires loading symbols with "yc i" and even if we did do that we
     don't know that the file has symbols.  */
  static char exitmsg[] = "HMS>";
  char *bp = bpt;
  char *ep = exitmsg;

  /* Large enough for either sizeof (bpt) or sizeof (exitmsg) chars.
   */
  char swallowed[50];

  /* Current position in swallowed.  */
  char *swallowed_p = swallowed;

  int ch;
  int ch_handled;
  int old_timeout = timeout;
  int
    old_immediate_quit = immediate_quit;
  int swallowed_cr = 0;

  status->kind = TARGET_WAITKIND_EXITED;
  status->value.integer = 0;

  if (need_artificial_trap != 0)
    {
      status->kind =
	TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_TRAP;
      need_artificial_trap--;
      return 0;
    }

  timeout = 5;			/* Don't time out for a while - user program is running.
				 */
  immediate_quit = 1;		/* Helps ability to QUIT */
  while (1)
    {
      QUIT;			/* Let user quit and leave process running */
      ch_handled = 0;
      ch = readchar ();
      if (ch == *bp)
	{
	  bp++;
	  if (*bp == '\0')
	    break;
	  ch_handled = 1;

	  *swallowed_p++ = ch;
	}
      else
	{
	  bp = bpt;
	}
      if
	(ch == *ep || *ep == '?')
	{
	  ep++;
	  if (*ep == '\0')
	    break;

	  if (!ch_handled)
	    *swallowed_p++ = ch;
	  ch_handled =
	    1;
	}
      else
	{
	  ep = exitmsg;
	}

      if (!ch_handled)
	{
	  char *p;

	  /* Print out any characters which have been swallowed.  */
	  for (p = swallowed; p < swallowed_p; ++p)
	    putchar_unfiltered (*p);
	  swallowed_p = swallowed;

	  if ((ch != '\r' && ch != '\n') || swallowed_cr > 10)
	    {
	      putchar_unfiltered (ch);
	      swallowed_cr = 10;
	    }
	  swallowed_cr++;

	}
    }
  if (*bp == '\0')
    {
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_TRAP;
      expect_prompt ();
    }
  else
    {
      status->kind = TARGET_WAITKIND_EXITED;
      status->value.integer =
	TARGET_SIGNAL_STOP;
    }

  timeout = old_timeout;
  immediate_quit = old_immediate_quit;
  return
    0;
}

/* Return the name of register number REGNO in the form input and
   output by hms.

   Returns a pointer to a static buffer containing the answer.  */
static char *
get_reg_name (regno)
     int regno;
{
  static char *rn[] =
  REGISTER_NAMES;

  return rn[regno];
}

/* Read the remote registers.  */

static int
gethex (length, start, ok)
     unsigned int length;
     char *start;
     int *ok;
{
  int result = 0;

  while (length--)
    {
      result <<= 4;
      if (*start >= 'a' && *start <= 'f')
	{
	  result += *start - 'a' + 10;
	}
      else if (*start >= 'A' &&
	       *start <= 'F')
	{
	  result += *start - 'A' + 10;
	}
      else if
	(*start >= '0' && *start <= '9')
	{
	  result += *start - '0';
	}
      else
	*ok = 0;
      start++;

    }
  return result;
}
static int
timed_read (buf, n, timeout)
     char
      *buf;

{
  int i;
  char c;

  i = 0;
  while (i < n)
    {
      c = readchar ();

      if (c == 0)
	return i;
      buf[i] = c;
      i++;

    }
  return i;
}

hms_write (a, l)
     char *a;
{
  int i;

  SERIAL_WRITE (desc, a, l);

  if (!quiet || remote_debug)
    {
      printf_unfiltered ("<");
      for (i = 0; i < l; i++)
	{
	  printf_unfiltered ("%c", a[i]);
	}
      printf_unfiltered (">");
    }
}

hms_write_cr (s)
     char *s;
{
  hms_write (s, strlen (s));
  hms_write ("\r\n", 2);
}

#ifdef GDB_TARGET_IS_H8500

/* H8/500 monitor reg dump looks like:

   HMS>r
   PC:8000 SR:070C .7NZ.. CP:00 DP:00 EP:00 TP:00 BR:00
   R0-R7: FF5A 0001 F4FE F500 0000 F528 F528 F4EE
   HMS>


 */

supply_val (n, size, ptr, segptr)
     int n;
     int size;
     char *ptr;
     char *segptr;
{
  int ok;
  char raw[4];
  switch (size)
    {
    case 2:
      raw[0] = gethex (2, ptr, &ok);
      raw[1] = gethex (2, ptr + 2, &ok);
      supply_register (n, raw);
      break;
    case 1:
      raw[0] = gethex (2, ptr, &ok);
      supply_register (n, raw);
      break;
    case 4:
      {
	int v = gethex (4, ptr, &ok);
	v |= gethex (2, segptr, &ok) << 16;
	raw[0] = 0;
	raw[1] = (v >> 16) & 0xff;
	raw[2] = (v >> 8) & 0xff;
	raw[3] = (v >> 0) & 0xff;
	supply_register (n, raw);
      }
    }

}
static void
hms_fetch_register (dummy)
     int dummy;
{
#define REGREPLY_SIZE 108
  char linebuf[REGREPLY_SIZE + 1];
  int i;
  int s;
  int gottok;

  LONGEST reg[NUM_REGS];
  check_open ();

  do
    {

      hms_write_cr ("r");
      expect ("r");
      s = timed_read (linebuf + 1, REGREPLY_SIZE, 1);

      linebuf[REGREPLY_SIZE] = 0;
      gottok = 0;
      if (linebuf[3] == 'P' &&
	  linebuf[4] == 'C' &&
	  linebuf[5] == ':' &&
	  linebuf[105] == 'H' &&
	  linebuf[106] == 'M' &&
	  linebuf[107] == 'S')
	{

	  /*
	     012
	     r**
	     -------1---------2---------3---------4---------5-----
	     345678901234567890123456789012345678901234567890123456
	     PC:8000 SR:070C .7NZ.. CP:00 DP:00 EP:00 TP:00 BR:00**
	     ---6---------7---------8---------9--------10----
	     789012345678901234567890123456789012345678901234
	     R0-R7: FF5A 0001 F4FE F500 0000 F528 F528 F4EE**

	     56789
	     HMS>
	   */
	  gottok = 1;


	  supply_val (PC_REGNUM, 4, linebuf + 6, linebuf + 29);

	  supply_val (CCR_REGNUM, 2, linebuf + 14);
	  supply_val (SEG_C_REGNUM, 1, linebuf + 29);
	  supply_val (SEG_D_REGNUM, 1, linebuf + 35);
	  supply_val (SEG_E_REGNUM, 1, linebuf + 41);
	  supply_val (SEG_T_REGNUM, 1, linebuf + 47);
	  for (i = 0; i < 8; i++)
	    {
	      static int sr[8] =
	      {35, 35, 35, 35,
	       41, 41, 47, 47};

	      char raw[4];
	      char *src = linebuf + 64 + 5 * i;
	      char *segsrc = linebuf + sr[i];
	      supply_val (R0_REGNUM + i, 2, src);
	      supply_val (PR0_REGNUM + i, 4, src, segsrc);
	    }
	}
      if (!gottok)
	{
	  hms_write_cr ("");
	  expect ("HMS>");
	}
    }
  while (!gottok);
}
#endif

#ifdef GDB_TARGET_IS_H8300
static void
hms_fetch_register (dummy)
     int dummy;
{
#define REGREPLY_SIZE 79
  char linebuf[REGREPLY_SIZE + 1];
  int i;
  int s;
  int gottok;

  unsigned LONGEST reg[NUM_REGS];

  check_open ();

  do
    {
      hms_write_cr ("r");

      s = timed_read (linebuf, 1, 1);

      while (linebuf[0] != 'r')
	s = timed_read (linebuf, 1, 1);

      s = timed_read (linebuf + 1, REGREPLY_SIZE - 1, 1);

      linebuf[REGREPLY_SIZE] = 0;
      gottok = 0;
      if (linebuf[0] == 'r' &&
	  linebuf[3] == 'P' &&
	  linebuf[4] == 'C' &&
	  linebuf[5] == '=' &&
	  linebuf[75] == 'H' &&
	  linebuf[76] == 'M' &&
	  linebuf[77] == 'S')
	{
	  /*
	     PC=XXXX CCR=XX:XXXXXXXX R0-R7= XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX
	     5436789012345678901234567890123456789012345678901234567890123456789012
	     0      1         2         3         4         5         6
	   */
	  gottok = 1;

	  reg[PC_REGNUM] = gethex (4, linebuf + 6, &gottok);
	  reg[CCR_REGNUM] = gethex (2, linebuf + 15, &gottok);
	  for (i = 0; i < 8; i++)
	    {
	      reg[i] = gethex (4, linebuf + 34 + 5 * i, &gottok);
	    }
	}
    }
  while (!gottok);
  for (i = 0; i < NUM_REGS; i++)
    {
      char swapped[2];

      swapped[1] = reg[i];
      swapped[0] = (reg[i]) >> 8;

      supply_register (i, swapped);
    }
}
#endif
/* Store register REGNO, or all if REGNO == -1.
   Return errno value.  */
static void
hms_store_register (regno)
     int regno;
{
  if (regno == -1)
    {
      for (regno = 0; regno < NUM_REGS; regno++)
	{
	  hms_store_register (regno);
	}
    }
  else
    {
      char *name = get_reg_name (regno);
      char buffer[100];
      /* Some regs dont really exist */
      if (!(name[0] == 'p' && name[1] == 'r')
	  && !(name[0] == 'c' && name[1] == 'y')
	  && !(name[0] == 't' && name[1] == 'i')
	  && !(name[0] == 'i' && name[1] == 'n'))
	{
	  sprintf (buffer, "r %s=%x", name, read_register (regno));
	  hms_write_cr (buffer);
	  expect_prompt ();
	}
    }
}


/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

void
hms_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

static CORE_ADDR
translate_addr (addr)
     CORE_ADDR addr;
{

  return (addr);

}


int
hms_xfer_inferior_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;	/* ignored */
{

  return len;
}

int
hms_write_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     unsigned char *myaddr;
     int len;
{
  bfd_vma addr;
  int done;
  int todo;
  char buffer[100];
  done = 0;
  hms_write_cr (".");
  expect_prompt ();
  while (done < len)
    {
      char *ptr = buffer;
      int thisgo;
      int idx;

      thisgo = len - done;
      if (thisgo > 20)
	thisgo = 20;

      sprintf (ptr, "M.B %4x =", memaddr + done);
      ptr += 10;
      for (idx = 0; idx < thisgo; idx++)
	{
	  sprintf (ptr, "%2x ", myaddr[idx + done]);
	  ptr += 3;
	}
      hms_write_cr (buffer);
      expect_prompt ();
      done += thisgo;
    }
}

void
hms_files_info ()
{
  char *file = "nothing";

  if (exec_bfd)
    file = bfd_get_filename (exec_bfd);

  if (exec_bfd)
#ifdef __GO32__
    printf_filtered ("\tAttached to DOS asynctsr and running program %s\n", file);
#else
    printf_filtered ("\tAttached to %s at %d baud and running program %s\n", dev_name, baudrate, file);
#endif
  printf_filtered ("\ton an H8/300 processor.\n");
}

/* Copy LEN bytes of data from debugger memory at MYADDR
   to inferior's memory at MEMADDR.  Returns errno value.
   * sb/sh instructions don't work on unaligned addresses, when TU=1.
 */

/* Read LEN bytes from inferior memory at MEMADDR.  Put the result
   at debugger address MYADDR.  Returns errno value.  */
int
hms_read_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  /* Align to nearest low 16 bits */
  int i;

  CORE_ADDR start = memaddr;
  CORE_ADDR end = memaddr + len - 1;

  int ok = 1;

  /*
     AAAA: XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX '................'
     012345678901234567890123456789012345678901234567890123456789012345
     0         1         2         3         4         5         6
   */
  char buffer[66];

  if (memaddr & 0xf)
    abort ();
  if (len != 16)
    abort ();

  sprintf (buffer, "m %4x %4x", start & 0xffff, end & 0xffff);

  flush ();
  hms_write_cr (buffer);
  /* drop the echo and newline */
  for (i = 0; i < 13; i++)
    readchar ();

  /* Grab the lines as they come out and fill the area */
  /* Skip over cr */
  while (1)
    {
      int p;
      int i;
      int addr;
      size_t idx;

      char byte[16];

      buffer[0] = readchar ();
      while (buffer[0] == '\r'
	     || buffer[0] == '\n')
	buffer[0] = readchar ();

      if (buffer[0] == 'M')
	break;

      for (i = 1; i < 50; i++)
	{
	  buffer[i] = readchar ();
	}
      /* sometimes we loose characters in the ascii representation of the 
         data.  I don't know where.  So just scan for the end of line */
      i = readchar ();
      while (i != '\n' && i != '\r')
	i = readchar ();

      /* Now parse the line */

      addr = gethex (4, buffer, &ok);
      idx = 6;
      for (p = 0; p < 16; p += 2)
	{
	  byte[p] = gethex (2, buffer + idx, &ok);
	  byte[p + 1] = gethex (2, buffer + idx + 2, &ok);
	  idx += 5;
	}

      for (p = 0; p < 16; p++)
	{
	  if (addr + p >= memaddr &&
	      addr + p < memaddr + len)
	    {
	      myaddr[(addr + p) - memaddr] = byte[p];

	    }

	}
    }
#ifdef GDB_TARGET_IS_H8500
  expect ("ore>");
#endif
#ifdef GDB_TARGET_IS_H8300
  expect ("emory>");
#endif
  hms_write_cr (".");

  expect_prompt ();
  return len;
}



#define MAX_BREAKS	16
static int num_brkpts = 0;
static int
hms_insert_breakpoint (addr, save)
     CORE_ADDR addr;
     char *save;		/* Throw away, let hms save instructions */
{
  check_open ();

  if (num_brkpts < MAX_BREAKS)
    {
      char buffer[100];

      num_brkpts++;
      sprintf (buffer, "b %x", addr & 0xffff);
      hms_write_cr (buffer);
      expect_prompt ();
      return (0);
    }
  else
    {
      fprintf_filtered (gdb_stderr,
		      "Too many break points, break point not installed\n");
      return (1);
    }

}
static int
hms_remove_breakpoint (addr, save)
     CORE_ADDR addr;
     char *save;		/* Throw away, let hms save instructions */
{
  if (num_brkpts > 0)
    {
      char buffer[100];

      num_brkpts--;
      sprintf (buffer, "b - %x", addr & 0xffff);
      hms_write_cr (buffer);
      expect_prompt ();

    }
  return (0);
}

/* Clear the hmss notion of what the break points are */
static int
hms_clear_breakpoints ()
{

  if (is_open)
    {
      hms_write_cr ("b -");
      expect_prompt ();
    }
  num_brkpts = 0;
}
static void
hms_mourn ()
{
  hms_clear_breakpoints ();
  unpush_target (&hms_ops);
  generic_mourn_inferior ();
}

/* Put a command string, in args, out to the hms.  The hms is assumed to
   be in raw mode, all writing/reading done through desc.
   Ouput from the hms is placed on the users terminal until the
   prompt from the hms is seen.
   FIXME: Can't handle commands that take input.  */

void
hms_com (args, fromtty)
     char *args;
     int fromtty;
{
  check_open ();

  if (!args)
    return;

  /* Clear all input so only command relative output is displayed */

  hms_write_cr (args);
/*  hms_write ("\030", 1); */
  expect_prompt ();
}

static void
hms_open (name, from_tty)
     char *name;
     int from_tty;
{
  unsigned int prl;
  char *p;

  if (name == 0)
    {
      name = "";
    }
  if (is_open)
    hms_close (0);
  dev_name = strdup (name);

  if (!(desc = SERIAL_OPEN (dev_name)))
    perror_with_name ((char *) dev_name);

  SERIAL_RAW (desc);
  is_open = 1;
  push_target (&hms_ops);
  dcache_ptr = dcache_init (hms_read_inferior_memory,
			    hms_write_inferior_memory);
  remote_dcache = 1;
  /* Hello?  Are you there?  */
  SERIAL_WRITE (desc, "\r\n", 2);
  expect_prompt ();

  /* Clear any break points */
  hms_clear_breakpoints ();

  printf_filtered ("Connected to remote board running HMS monitor.\n");
  add_commands ();
/*  hms_drain (); */
}

/* Define the target subroutine names */

struct target_ops hms_ops =
{
  "hms", "Remote HMS monitor",
  "Use the H8 evaluation board running the HMS monitor connected\n\
by a serial line.",

  hms_open, hms_close,
  0, hms_detach, hms_resume, hms_wait,	/* attach */
  hms_fetch_register, hms_store_register,
  hms_prepare_to_store,
  hms_xfer_inferior_memory,
  hms_files_info,
  hms_insert_breakpoint, hms_remove_breakpoint,		/* Breakpoints */
  0, 0, 0, 0, 0,		/* Terminal handling */
  hms_kill,			/* FIXME, kill */
  generic_load,
  0,				/* lookup_symbol */
  hms_create_inferior,		/* create_inferior */
  hms_mourn,			/* mourn_inferior FIXME */
  0,				/* can_run */
  0,				/* notice_signals */
  0,				/* to_thread_alive */
  0,				/* to_stop */
  process_stratum, 0,		/* next */
  1, 1, 1, 1, 1,		/* all mem, mem, stack, regs, exec */
  0, 0,				/* Section pointers */
  OPS_MAGIC,			/* Always the last thing */
};

hms_quiet ()			/* FIXME - this routine can be removed after Dec '94 */
{
  quiet = !quiet;
  if (quiet)
    printf_filtered ("Snoop disabled\n");
  else
    printf_filtered ("Snoop enabled\n");

  printf_filtered ("`snoop' is obsolete, please use `set remotedebug'.\n");
}

hms_device (s)
     char *s;
{
  if (s)
    {
      dev_name = get_word (&s);
    }
}

static
hms_speed (s)
     char *s;
{
  check_open ();

  if (s)
    {
      char buffer[100];
      int newrate = atoi (s);
      int which = 0;

      if (SERIAL_SETBAUDRATE (desc, newrate))
	error ("Can't use %d baud\n", newrate);

      printf_filtered ("Checking target is in sync\n");

      printf_filtered ("Sending commands to set target to %d\n",
		       baudrate);

      sprintf (buffer, "tm %d. N 8 1", baudrate);
      hms_write_cr (buffer);
    }
}

/***********************************************************************/

static void
hms_drain (args, fromtty)
     char *args;
     int fromtty;
{
  int c;
  while (1)
    {
      c = SERIAL_READCHAR (desc, 1);
      if (c == SERIAL_TIMEOUT)
	break;
      if (c == SERIAL_ERROR)
	break;
      if (c > ' ' && c < 127)
	printf ("%c", c & 0xff);
      else
	printf ("<%x>", c & 0xff);
    }
}

static void
add_commands ()
{

  add_com ("hms_drain", class_obscure, hms_drain,
	   "Drain pending hms text buffers.");
}

static void
remove_commands ()
{
  extern struct cmd_list_element *cmdlist;
  delete_cmd ("hms-drain", &cmdlist);
}


void
_initialize_remote_hms ()
{
  add_target (&hms_ops);

  add_com ("hms <command>", class_obscure, hms_com,
	   "Send a command to the HMS monitor.");

  /* FIXME - hms_quiet and `snoop' can be removed after Dec '94 */
  add_com ("snoop", class_obscure, hms_quiet,
	   "Show what commands are going to the monitor (OBSOLETE - see 'set remotedebug')");

  add_com ("device", class_obscure, hms_device,
	   "Set the terminal line for HMS communications");

  add_com ("speed", class_obscure, hms_speed,
	   "Set the terminal line speed for HMS communications");

  dev_name = NULL;
}
#endif

