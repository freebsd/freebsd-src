/* Remote debugging interface for Hitachi E7000 ICE, for GDB
   Copyright 1993, 1994, 1996 Free Software Foundation, Inc.
   Contributed by Cygnus Support. 

   Written by Steve Chamberlain for Cygnus Support.

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

/* The E7000 is an in-circuit emulator for the Hitachi H8/300-H and
   Hitachi-SH processor.  It has serial port and a lan port.  

   The monitor command set makes it difficult to load large ammounts of
   data over the lan without using ftp - so try not to issue load
   commands when communicating over ethernet; use the ftpload command.

   The monitor pauses for a second when dumping srecords to the serial
   line too, so we use a slower per byte mechanism but without the
   startup overhead.  Even so, it's pretty slow... */

#include "defs.h"
#include "gdbcore.h"
#include "inferior.h"
#include "target.h"
#include "wait.h"
#include "value.h"
#include "command.h"
#include <signal.h>
#include "gdb_string.h"
#include <sys/types.h>
#include "serial.h"
#include "remote-utils.h"
#include "symfile.h"
#include <sys/time.h>

#if 0
#define HARD_BREAKPOINTS
#define BC_BREAKPOINTS 0
#endif

#define CTRLC 0x03
#define ENQ  0x05
#define ACK  0x06
#define CTRLZ 0x1a

extern void notice_quit PARAMS ((void));

/* Local function declarations.  */

static void e7000_close PARAMS ((int));

static void e7000_fetch_register PARAMS ((int));

static void e7000_store_register PARAMS ((int));

static void e7000_command PARAMS ((char *, int));

static void e7000_login_command PARAMS ((char *, int));

static void e7000_ftp_command PARAMS ((char *, int));

static void e7000_drain_command PARAMS ((char *, int));

static void expect PARAMS ((char *));

static void expect_full_prompt PARAMS ((void));

static void expect_prompt PARAMS ((void));

/* Variables. */

static serial_t e7000_desc;

/* Nonzero if using the tcp serial driver.  */

static int using_tcp;

/* Nonzero if using the pc isa card.  */

static int using_pc;

extern struct target_ops e7000_ops;	/* Forward declaration */

char *ENQSTRING = "\005";

/* Nonzero if some routine (as opposed to the user) wants echoing.
   FIXME: Do this reentrantly with an extra parameter.  */

static int echo;

static int ctrl_c;

static int timeout = 5;

/* Send data to e7000debug.  */

static void
puts_e7000debug (buf)
     char *buf;
{
  if (!e7000_desc)
    error ("Use \"target e7000 ...\" first.");

  if (remote_debug)
    printf("Sending %s\n", buf);

  if (SERIAL_WRITE (e7000_desc, buf, strlen (buf)))
    fprintf (stderr, "SERIAL_WRITE failed: %s\n", safe_strerror (errno));

  /* And expect to see it echoed, unless using the pc interface */
#if 0
  if (!using_pc)
#endif
    expect (buf);
}

static void
putchar_e7000 (x)
     int x;
{
  char b[1];

  b[0] = x;
  SERIAL_WRITE (e7000_desc, b, 1);
}

static void
write_e7000 (s)
     char *s;
{
  SERIAL_WRITE (e7000_desc, s, strlen (s));
}

static int
normal (x)
     int x;
{
  if (x == '\n')
    return '\r';
  return x;
}

/* Read a character from the remote system, doing all the fancy timeout
   stuff.  */

static int
readchar (timeout)
     int timeout;
{
  int c;

  do
    {
      c = SERIAL_READCHAR (e7000_desc, timeout);
    }
  while (c > 127);

  if (c == SERIAL_TIMEOUT)
    {
      if (timeout == 0)
	return -1;
      echo = 0;
      error ("Timeout reading from remote system.");
    }
  if (remote_debug) 
    {
      putchar (c);
      fflush (stdout);
    }

  return normal (c);
}

#if 0
char *
tl (x)
{
  static char b[8][10];
  static int p;

  p++;
  p &= 7;
  if (x >= ' ') 
    { 
      b[p][0] = x;
      b[p][1] = 0;
    }
  else
    {
      sprintf(b[p], "<%d>", x);
    }

  return b[p];
}
#endif

/* Scan input from the remote system, until STRING is found.  If
   DISCARD is non-zero, then discard non-matching input, else print it
   out.  Let the user break out immediately.  */

static void
expect (string)
     char *string;
{
  char *p = string;
  int c;
  int nl = 0;

  while (1)
    {
      c = readchar (timeout);
      notice_quit ();
      if (quit_flag == 1) 
	{
	  if (ctrl_c)
	    {
	      putchar_e7000(CTRLC);
	      --ctrl_c;
	    }
	  else 
	    {
	      quit ();
	    }
	}
      
      if (c == SERIAL_ERROR)
	{
	  error ("Serial communication error");
	}
      if (echo || remote_debug)
	{
	  if (c == '\r' || c == '\n')
	    {
	      if (!nl)
		putchar ('\n');
	      nl = 1;
	    }
	  else
	    {
	      nl = 0;
	      putchar (c);
	    }
	  fflush (stdout);
	}
      if (normal (c) == normal (*p++))
	{
	  if (*p == '\0')
	    return;
	}
      else
	{
	  p = string;

	  if (normal (c) == normal (string[0]))
	    p++;
	}
    }
}

/* Keep discarding input until we see the e7000 prompt.

   The convention for dealing with the prompt is that you
   o give your command
   o *then* wait for the prompt.

   Thus the last thing that a procedure does with the serial line will
   be an expect_prompt().  Exception: e7000_resume does not wait for
   the prompt, because the terminal is being handed over to the
   inferior.  However, the next thing which happens after that is a
   e7000_wait which does wait for the prompt.  Note that this includes
   abnormal exit, e.g. error().  This is necessary to prevent getting
   into states from which we can't recover.  */

static void
expect_prompt ()
{
  expect (":");
}

static void
expect_full_prompt ()
{
  expect ("\r:");
}

static int
convert_hex_digit (ch)
     int ch;
{
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  else if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  else if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  return -1;
}

static int
get_hex (start)
     int *start;
{
  int value = convert_hex_digit (*start);
  int try;

  *start = readchar (timeout);
  while ((try = convert_hex_digit (*start)) >= 0)
    {
      value <<= 4;
      value += try;
      *start = readchar (timeout);
    }
  return value;
}

#if 0
/* Get N 32-bit words from remote, each preceded by a space, and put
   them in registers starting at REGNO.  */

static void
get_hex_regs (n, regno)
     int n;
     int regno;
{
  long val;
  int i;

  for (i = 0; i < n; i++)
    {
      int j;

      val = 0;
      for (j = 0; j < 8; j++)
	val = (val << 4) + get_hex_digit (j == 0);
      supply_register (regno++, (char *) &val);
    }
}
#endif

/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */

static void
e7000_create_inferior (execfile, args, env)
     char *execfile;
     char *args;
     char **env;
{
  int entry_pt;

  if (args && *args)
    error ("Can't pass arguments to remote E7000DEBUG process");

  if (execfile == 0 || exec_bfd == 0)
    error ("No exec file specified");

  entry_pt = (int) bfd_get_start_address (exec_bfd);

#ifdef CREATE_INFERIOR_HOOK
  CREATE_INFERIOR_HOOK (0);	/* No process-ID */
#endif

  /* The "process" (board) is already stopped awaiting our commands, and
     the program is already downloaded.  We just set its PC and go.  */

  clear_proceed_status ();

  /* Tell wait_for_inferior that we've started a new process.  */
  init_wait_for_inferior ();

  /* Set up the "saved terminal modes" of the inferior
     based on what modes we are starting it with.  */
  target_terminal_init ();

  /* Install inferior's terminal modes.  */
  target_terminal_inferior ();

  /* insert_step_breakpoint ();  FIXME, do we need this?  */
  proceed ((CORE_ADDR) entry_pt, -1, 0);	/* Let 'er rip... */
}

/* Open a connection to a remote debugger.  NAME is the filename used
   for communication.  */

static int baudrate = 9600;
static char dev_name[100];

static char *machine = "";
static char *user = "";
static char *passwd = "";
static char *dir = "";

/* Grab the next token and buy some space for it */

static char *
next (ptr)
     char **ptr;
{
  char *p = *ptr;
  char *s;
  char *r;
  int l = 0;

  while (*p && *p == ' ')
    p++;
  s = p;
  while (*p && (*p != ' ' && *p != '\t'))
    {
      l++;
      p++;
    }
  r = xmalloc (l + 1);
  memcpy (r, s, l);
  r[l] = 0;
  *ptr = p;
  return r;
}

static void
e7000_login_command (args, from_tty)
     char *args;
     int from_tty;
{
  if (args)
    {
      machine = next (&args);
      user = next (&args);
      passwd = next (&args);
      dir = next (&args);
      if (from_tty)
	{
	  printf ("Set info to %s %s %s %s\n", machine, user, passwd, dir);
	}
    }
  else
    {
      error ("Syntax is ftplogin <machine> <user> <passwd> <directory>");
    }
}

/* Start an ftp transfer from the E7000 to a host */

static void
e7000_ftp_command (args, from_tty)
     char *args;
     int from_tty;
{
  /* FIXME: arbitrary limit on machine names and such.  */
  char buf[200];

  int oldtimeout = timeout;
  timeout = 10;

  sprintf (buf, "ftp %s\r", machine);
  puts_e7000debug (buf);
  expect (" Username : ");
  sprintf (buf, "%s\r", user);
  puts_e7000debug (buf);
  expect (" Password : ");
  write_e7000 (passwd);
  write_e7000 ("\r");
  expect ("success\r");
  expect ("FTP>");
  sprintf (buf, "cd %s\r", dir);
  puts_e7000debug (buf);
  expect ("FTP>");
  sprintf (buf, "ll 0;s:%s\r", args);
  puts_e7000debug (buf);
  expect ("FTP>");
  puts_e7000debug ("bye\r");
  expect (":");
  timeout = oldtimeout;
}

static void
e7000_open (args, from_tty)
     char *args;
     int from_tty;
{
  int n;
  int loop;
  char junk[100];
  int sync;
  target_preopen (from_tty);

  n = 0;
  if (args && strcasecmp (args, "pc") == 0)
    {
      strcpy (dev_name, args);
    }
  else 
    {
      if (args) 
	{
	  n = sscanf (args, " %s %d %s", dev_name, &baudrate, junk);
	}

      if (n != 1 && n != 2)
	{
	  error ("Bad arguments.  Usage:\ttarget e7000 <device> <speed>\n\
or \t\ttarget e7000 <host>[:<port>]\n\
or \t\ttarget e7000 pc\n");
	}

#ifndef __GO32__
      if (n == 1 && strchr (dev_name, ':') == 0)
	{
	  /* Default to normal telnet port */
	  strcat (dev_name, ":23");
	}
#endif
    }

  push_target (&e7000_ops);

  e7000_desc = SERIAL_OPEN (dev_name);

  if (!e7000_desc)
    perror_with_name (dev_name);

  using_tcp = strcmp (e7000_desc->ops->name, "tcp") == 0;
  using_pc = strcmp (e7000_desc->ops->name, "pc") == 0;

  SERIAL_SETBAUDRATE (e7000_desc, baudrate);
  SERIAL_RAW (e7000_desc);

  /* Hello?  Are you there?  */
  sync = 0;
  loop =  0;
  putchar_e7000 (CTRLC);
  while (!sync)
    {
      int c;

      if (from_tty)
	printf_unfiltered ("[waiting for e7000...]\n");

      write_e7000 ("\r");
      c = SERIAL_READCHAR (e7000_desc, 1);
      while (c != SERIAL_TIMEOUT)
	{
	  /* Dont echo cr's */
	  if (from_tty && c != '\r')
	    {
	      putchar (c);
	      fflush (stdout);
	    }
	  if (c == ':')
	    sync = 1;

	  if (loop++ == 20) 
	    {
	      putchar_e7000 (CTRLC);
	      loop = 0;
	    }

	  QUIT ;


	  if (quit_flag)
	    {
	      putchar_e7000 (CTRLC);
	      quit_flag = 0;
	    }
	  c = SERIAL_READCHAR (e7000_desc, 1);
	}
    }
  puts_e7000debug ("\r");

  expect_prompt ();

  puts_e7000debug ("b -\r");

  expect_prompt ();

  if (from_tty)
    printf_filtered ("Remote target %s connected to %s\n", target_shortname,
		     dev_name);

#ifdef GDB_TARGET_IS_H8300
  h8300hmode = 1;
#endif
}

/* Close out all files and local state before this target loses control. */

static void
e7000_close (quitting)
     int quitting;
{
  if (e7000_desc)
    {
      SERIAL_CLOSE (e7000_desc);
      e7000_desc = 0;
    }
}

/* Terminate the open connection to the remote debugger.  Use this
   when you want to detach and do something else with your gdb.  */

static void
e7000_detach (from_tty)
     int from_tty;
{
  pop_target ();		/* calls e7000_close to do the real work */
  if (from_tty)
    printf ("Ending remote %s debugging\n", target_shortname);
}

/* Tell the remote machine to resume.  */

static void
e7000_resume (pid, step, sig)
     int pid, step, sig;
{
  if (step)
    puts_e7000debug ("S\r");
  else
    puts_e7000debug ("G\r");
}

/* Read the remote registers into the block REGS.  

   For the H8/300 a register dump looks like:

   PC=00021A  CCR=80:I*******
   ER0 - ER3  0000000A 0000002E 0000002E 00000000
   ER4 - ER7  00000000 00000000 00000000 00FFEFF6
   000218           MOV.B     R1L,R2L
   STEP NORMAL END or
   BREAK POINT
   */

#ifdef GDB_TARGET_IS_H8300

char *want = "PC=%p CCR=%c\n\
 ER0 - ER3  %0 %1 %2 %3\n\
 ER4 - ER7  %4 %5 %6 %7\n";

char *want_nopc = "%p CCR=%c\n\
 ER0 - ER3  %0 %1 %2 %3\n\
 ER4 - ER7  %4 %5 %6 %7";

#endif

#ifdef GDB_TARGET_IS_SH

char *want = "PC=%16 SR=%22\n\
PR=%17 GBR=%18 VBR=%19\n\
MACH=%20 MACL=%21\n\
R0-7  %0 %1 %2 %3 %4 %5 %6 %7\n\
R8-15 %8 %9 %10 %11 %12 %13 %14 %15\n";

char *want_nopc = "%16 SR=%22\n\
 PR=%17 GBR=%18 VBR=%19\n\
 MACH=%20 MACL=%21\n\
 R0-7  %0 %1 %2 %3 %4 %5 %6 %7\n\
 R8-15 %8 %9 %10 %11 %12 %13 %14 %15";

#endif

static int
gch ()
{
  int c = readchar (timeout);

  if (remote_debug)
    {
      if (c >= ' ')
	printf ("%c", c);
      else if (c == '\n')
	printf ("\n");
    }
  return c;
}

static unsigned int
gbyte ()
{
  int high = convert_hex_digit (gch ());
  int low = convert_hex_digit (gch ());

  return (high << 4) + low;
}

void
fetch_regs_from_dump (nextchar, want)
     int (*nextchar)();
     char *want;
{
  int regno;
  char buf[MAX_REGISTER_RAW_SIZE];

  int thischar = nextchar ();

  while (*want)
    {
      switch (*want)
	{
	case '\n':
	  /* Skip to end of line and then eat all new line type stuff */
	  while (thischar != '\n' && thischar != '\r') 
	    thischar = nextchar ();
	  while (thischar == '\n' || thischar == '\r') 
	    thischar = nextchar ();
	  want++;
	  break;

	case ' ':
	  while (thischar == ' '
		 || thischar == '\t'
		 || thischar == '\r'
		 || thischar == '\n')
	    thischar = nextchar ();
	  want++;
	  break;
	  
	default:
	  if (*want == thischar)
	    {
	      want++;
	      if (*want)
		thischar = nextchar ();
	      
	    }
	  else if (thischar == ' ' || thischar == '\n' || thischar == '\r')
	    {
	      thischar = nextchar ();
	    }
	  else {
	    error ("out of sync in fetch registers wanted <%s>, got <%c 0x%x>",
		   want, thischar, thischar);
	  }
    
	  break;
	case '%':
	  /* Got a register command */
	  want++;
	  switch (*want)
	    {
#ifdef PC_REGNUM
	    case 'p':
	      regno = PC_REGNUM;
	      want++;
	      break;
#endif
#ifdef CCR_REGNUM
	    case 'c':
	      regno = CCR_REGNUM;
	      want++;
	      break;
#endif
#ifdef SP_REGNUM
	    case 's':
	      regno = SP_REGNUM;
	      want++;
	      break;
#endif
#ifdef FP_REGNUM
	    case 'f':
	      regno = FP_REGNUM;
	      want++;
	      break;
#endif

	    default:
	      if (isdigit (want[0])) 
		{
		  if (isdigit (want[1]))
		    {
		      regno = (want[0] - '0') * 10 + want[1] - '0';
		      want += 2;
		    }
		  else 
		    {
		      regno = want[0] - '0';
		      want++;
		    }
		}
	      
	      else
		abort ();
	    }
	  store_signed_integer (buf,
				REGISTER_RAW_SIZE(regno),
				(LONGEST) get_hex (&thischar, nextchar));
	  supply_register (regno, buf);
	  break;
	}
    }
}

static void
e7000_fetch_registers ()
{
  int regno;

  puts_e7000debug ("R\r");
  fetch_regs_from_dump (gch, want);

  /* And supply the extra ones the simulator uses */
  for (regno = NUM_REALREGS; regno < NUM_REGS; regno++)
    {
      int buf = 0;

      supply_register (regno, (char *) (&buf));
    }
}

/* Fetch register REGNO, or all registers if REGNO is -1.  Returns
   errno value.  */

static void
e7000_fetch_register (regno)
     int regno;
{
  e7000_fetch_registers ();
}

/* Store the remote registers from the contents of the block REGS.  */

static void
e7000_store_registers ()
{
  int regno;

  for (regno = 0; regno < NUM_REALREGS; regno++)
    e7000_store_register (regno);

  registers_changed ();
}

/* Store register REGNO, or all if REGNO == 0.  Return errno value.  */

static void
e7000_store_register (regno)
     int regno;
{
  char buf[200];

  if (regno == -1)
    {
      e7000_store_registers ();
      return;
    }

#ifdef GDB_TARGET_IS_H8300
  if (regno <= 7)
    {
      sprintf (buf, ".ER%d %x\r", regno, read_register (regno));
      puts_e7000debug (buf);
    }
  else if (regno == PC_REGNUM)
    {
      sprintf (buf, ".PC %x\r", read_register (regno));
      puts_e7000debug (buf);
    }
  else if (regno == CCR_REGNUM)
    {
      sprintf (buf, ".CCR %x\r", read_register (regno));
      puts_e7000debug (buf);
    }
#endif /* GDB_TARGET_IS_H8300 */

#ifdef  GDB_TARGET_IS_SH
  switch (regno)
    {
    default:
      sprintf (buf, ".R%d %x\r", regno, read_register (regno));
      puts_e7000debug (buf);
      break;

    case PC_REGNUM:
      sprintf (buf, ".PC %x\r", read_register (regno));
      puts_e7000debug (buf);
      break;

    case SR_REGNUM:
      sprintf (buf, ".SR %x\r", read_register (regno));
      puts_e7000debug (buf);
      break;

    case PR_REGNUM:
      sprintf (buf, ".PR %x\r", read_register (regno));
      puts_e7000debug (buf);
      break;

    case GBR_REGNUM:
      sprintf (buf, ".GBR %x\r", read_register (regno));
      puts_e7000debug (buf);
      break;

    case VBR_REGNUM:
      sprintf (buf, ".VBR %x\r", read_register (regno));
      puts_e7000debug (buf);
      break;

    case MACH_REGNUM:
      sprintf (buf, ".MACH %x\r", read_register (regno));
      puts_e7000debug (buf);
      break;

    case MACL_REGNUM:
      sprintf (buf, ".MACL %x\r", read_register (regno));
      puts_e7000debug (buf);
      break;
    }

#endif /* GDB_TARGET_IS_SH */

  expect_prompt ();
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
e7000_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

static void
e7000_files_info ()
{
  printf ("\tAttached to %s at %d baud.\n", dev_name, baudrate);
}

static int
stickbyte (where, what)
     char *where;
     unsigned int what;
{
  static CONST char digs[] = "0123456789ABCDEF";

  where[0] = digs[(what >> 4) & 0xf];
  where[1] = digs[(what & 0xf) & 0xf];

  return what;
}

/* Write a small ammount of memory. */

static int
write_small (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     unsigned char *myaddr;
     int len;
{
  int i;
  char buf[200];

  for (i = 0; i < len; i++)
    {
      if (((memaddr + i) & 3) == 0 && (i + 3 < len))
	{
	  /* Can be done with a long word */
	  sprintf (buf, "m %x %x%02x%02x%02x;l\r",
		   memaddr + i,
		   myaddr[i], myaddr[i + 1], myaddr[i + 2], myaddr[i + 3]);
	  puts_e7000debug (buf);
	  i += 3;
	}
      else
	{
	  sprintf (buf, "m %x %x\r", memaddr + i, myaddr[i]);
	  puts_e7000debug (buf);
	}
    }

  expect_prompt ();

  return len;
}

/* Write a large ammount of memory, this only works with the serial
   mode enabled.  Command is sent as

	il ;s:s\r	 ->
			<- il ;s:s\r
			<-  	ENQ
	ACK		->
			<- LO s\r
	Srecords...
	^Z		->
			<-	ENQ
	ACK		->  
			<-	:   	
  */

static int
write_large (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     unsigned char *myaddr;
     int len;
{
  int i;
#define maxstride  128
  int stride;

  puts_e7000debug ("IL ;S:FK\r");
  expect (ENQSTRING);
  putchar_e7000 (ACK);
  expect ("LO FK\r");

  for (i = 0; i < len; i += stride)
    {
      char compose[maxstride * 2 + 50];
      int address = i + memaddr;
      int j;
      int check_sum;
      int where = 0;
      int alen;

      stride = len - i;
      if (stride > maxstride)
	stride = maxstride;

      compose[where++] = 'S';
      check_sum = 0;
      if (address >= 0xffffff)
	alen = 4;
      else if (address >= 0xffff)
	alen = 3;
      else
	alen = 2;
      /* Insert type. */
      compose[where++] = alen - 1 + '0';
      /* Insert length. */
      check_sum += stickbyte (compose + where, alen + stride + 1);
      where += 2;
      while (alen > 0)
	{
	  alen--;
	  check_sum += stickbyte (compose + where, address >> (8 * (alen)));
	  where += 2;
	}

      for (j = 0; j < stride; j++)
	{
	  check_sum += stickbyte (compose + where, myaddr[i + j]);
	  where += 2;
	}
      stickbyte (compose + where, ~check_sum);
      where += 2;
      compose[where++] = '\r';
      compose[where++] = '\n';
      compose[where++] = 0;

      SERIAL_WRITE (e7000_desc, compose, where);
      j = SERIAL_READCHAR (e7000_desc, 0);
      if (j == SERIAL_TIMEOUT)
	{
	  /* This is ok - nothing there */
	}
      else if (j == ENQ)
	{
	  /* Hmm, it's trying to tell us something */
	  expect (":");
	  error ("Error writing memory");
	}
      else
	{
	  printf ("@%d}@", j);
	  while ((j = SERIAL_READCHAR(e7000_desc,0)) > 0) 
	    {
	      printf ("@{%d}@",j);
	    }
	}
    }

  /* Send the trailer record */
  write_e7000 ("S70500000000FA\r");
  putchar_e7000 (CTRLZ);
  expect (ENQSTRING);
  putchar_e7000 (ACK);
  expect (":");

  return len;
}

/* Copy LEN bytes of data from debugger memory at MYADDR to inferior's
   memory at MEMADDR.  Returns length moved.

   Can't use the Srecord load over ethernet, so don't use fast method
   then.  */

static int
e7000_write_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     unsigned char *myaddr;
     int len;
{
  if (len < 16 || using_tcp || using_pc)
    return write_small (memaddr, myaddr, len);
  else
    return write_large (memaddr, myaddr, len);
}

/* Read LEN bytes from inferior memory at MEMADDR.  Put the result
   at debugger address MYADDR.  Returns length moved. 

  Small transactions we send
  m <addr>;l
  and receive
    00000000 12345678 ?
 */

static int
e7000_read_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     unsigned char *myaddr;
     int len;
{
  int count;
  int c;
  int i;
  char buf[200];
  /* Starting address of this pass.  */

/*  printf("READ INF %x %x %d\n", memaddr, myaddr, len);*/
  if (((memaddr - 1) + len) < memaddr)
    {
      errno = EIO;
      return 0;
    }

  sprintf (buf, "m %x;l\r", memaddr);
  puts_e7000debug (buf);

  for (count = 0; count < len; count += 4) 
    {
      /* Suck away the address */
      c = gch ();	
      while (c != ' ')
	c = gch ();	
      c = gch ();
      if (c == '*') 
	{			/* Some kind of error */
	  expect_prompt();
	  return -1;
	}
      while (c != ' ')
	c = gch ();	

      /* Now read in the data */
      for (i = 0; i < 4; i++) 
	{
	  int b = gbyte();
	  if (count + i < len) {
	    myaddr[count + i] = b;
	  }
	}

      /* Skip the trailing ? and send a . to end and a cr for more */
      gch ();	
      gch ();
      if (count + 4 >= len)
	puts_e7000debug(".\r");
      else
	puts_e7000debug("\r");

    }
  expect_prompt();
  return len;
}


#if 0
/*
  For large transfers we used to send


  d <addr> <endaddr>\r

  and receive
   <ADDR>              <    D   A   T   A    >               <   ASCII CODE   >
   000000  5F FD FD FF DF 7F DF FF  01 00 01 00 02 00 08 04  "_..............."
   000010  FF D7 FF 7F D7 F1 7F FF  00 05 00 00 08 00 40 00  "..............@."
   000020  7F FD FF F7 7F FF FF F7  00 00 00 00 00 00 00 00  "................"

  A cost in chars for each transaction of 80 + 5*n-bytes. 

  Large transactions could be done with the srecord load code, but
  there is a pause for a second before dumping starts, which slows the
  average rate down!
*/

static int
e7000_read_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     unsigned char *myaddr;
     int len;
{
  int count;
  int c;
  char buf[200];

  /* Starting address of this pass.  */

  if (((memaddr - 1) + len) < memaddr)
    {
      errno = EIO;
      return 0;
    }

  sprintf (buf, "d %x %x\r", memaddr, memaddr + len - 1);
  puts_e7000debug (buf);

  count = 0;
  c = gch ();

  /* First skip the command */
  while (c == '\n')
    c = gch ();

  while (c == ' ')
    c = gch ();
  if (c == '*')
    {
      expect ("\r");
      return -1;
    }

  /* Skip the title line */
  while (c != '\n')
    c = gch ();
  c = gch ();
  while (count < len)
    {
      /* Skip the address */
      while (c <= ' ')
	c = gch ();

      get_hex (&c);

      /* read in the bytes on the line */
      while (c != '"' && count < len)
	{
	  if (c == ' ')
	    c = gch ();
	  else
	    {
	      myaddr[count++] = get_hex (&c);
	    }
	}

      while (c != '\n')
	c = gch ();
    }

  while (c != ':')
    c = gch ();

  return len;
}

static int
fast_but_for_the_pause_e7000_read_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int loop;
  int c;
  char buf[200];

  if (((memaddr - 1) + len) < memaddr)
    {
      errno = EIO;
      return 0;
    }

  sprintf (buf, "is %x@%x:s\r", memaddr, len);
  puts_e7000debug (buf);
  gch ();
  c = gch ();
  if (c != ENQ)
    {
      /* Got an error */
      error ("Memory read error");
    }
  putchar_e7000 (ACK);
  expect ("SV s");
  loop = 1;
  while (loop)
    {
      int type;
      int length;
      int addr;
      int i;

      c = gch ();
      switch (c)
	{
	case ENQ:		/* ENQ, at the end */
	  loop = 0;
	  break;
	case 'S':
	  /* Start of an Srecord */
	  type = gch ();
	  length = gbyte ();
	  switch (type)
	    {
	    case '7':		/* Termination record, ignore */
	    case '0':
	    case '8':
	    case '9':
	      /* Header record - ignore it */
	      while (length--)
		{
		  gbyte ();
		}
	      break;
	    case '1':
	    case '2':
	    case '3':
	      {
		int alen;

		alen = type - '0' + 1;
		addr = 0;
		while (alen--)
		  {
		    addr = (addr << 8) + gbyte ();
		    length--;
		  }

		for (i = 0; i < length - 1; i++)
		  myaddr[i + addr - memaddr] = gbyte ();

		gbyte ();	/* Ignore checksum */
	      }
	    }
	}
    }

  putchar_e7000 (ACK);
  expect ("TOP ADDRESS =");
  expect ("END ADDRESS =");
  expect (":");

  return len;
}

#endif

static int
e7000_xfer_inferior_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     unsigned char *myaddr;
     int len;
     int write;
     struct target_ops *target;	/* ignored */
{
  if (write)
    return e7000_write_inferior_memory( memaddr, myaddr, len);
  else
    return e7000_read_inferior_memory( memaddr, myaddr, len);
}

static void
e7000_kill (args, from_tty)
     char *args;
     int from_tty;
{
}

static void
e7000_load (args, from_tty)
     char *args;
     int from_tty;
{
  struct cleanup *old_chain;
  asection *section;
  bfd *pbfd;
  bfd_vma entry;
  int i;
#define WRITESIZE 0x1000
  char buf[2 + 4 + 4 + WRITESIZE]; /* `DT' + <addr> + <len> + <data> */
  char *filename;
  int quiet;
  int nostart;
  time_t start_time, end_time;	/* Start and end times of download */
  unsigned long data_count;	/* Number of bytes transferred to memory */

  if (!strchr (dev_name, ':'))
    {
      generic_load (args, from_tty);
      return;
    }

  buf[0] = 'D';
  buf[1] = 'T';
  quiet = 0;
  nostart = 0;
  filename = NULL;

  while (*args != '\000')
    {
      char *arg;

      while (isspace (*args)) args++;

      arg = args;

      while ((*args != '\000') && !isspace (*args)) args++;

      if (*args != '\000')
	*args++ = '\000';

      if (*arg != '-')
	filename = arg;
      else if (strncmp (arg, "-quiet", strlen (arg)) == 0)
	quiet = 1;
      else if (strncmp (arg, "-nostart", strlen (arg)) == 0)
	nostart = 1;
      else
	error ("unknown option `%s'", arg);
    }

  if (!filename)
    filename = get_exec_file (1);

  pbfd = bfd_openr (filename, gnutarget);
  if (pbfd == NULL)
    {
      perror_with_name (filename);
      return;
    }
  old_chain = make_cleanup (bfd_close, pbfd);

  if (!bfd_check_format (pbfd, bfd_object)) 
    error ("\"%s\" is not an object file: %s", filename,
	   bfd_errmsg (bfd_get_error ()));

  start_time = time (NULL);
  data_count = 0;

  puts_e7000debug ("mw\r");

  expect ("\nOK");

  for (section = pbfd->sections; section; section = section->next) 
    {
      if (bfd_get_section_flags (pbfd, section) & SEC_LOAD)
	{
	  bfd_vma section_address;
	  bfd_size_type section_size;
	  file_ptr fptr;

	  section_address = bfd_get_section_vma (pbfd, section);
	  section_size = bfd_get_section_size_before_reloc (section);

	  if (!quiet)
	    printf_filtered ("[Loading section %s at 0x%x (%d bytes)]\n",
			     bfd_get_section_name (pbfd, section),
			     section_address,
			     section_size);

	  fptr = 0;
	  
	  data_count += section_size;

	  while (section_size > 0)
	    {
	      int count;
	      static char inds[] = "|/-\\";
	      static int k = 0;

	      QUIT;

	      count = min (section_size, WRITESIZE);

	      buf[2] = section_address >> 24;
	      buf[3] = section_address >> 16;
	      buf[4] = section_address >> 8;
	      buf[5] = section_address;

	      buf[6] = count >> 24;
	      buf[7] = count >> 16;
	      buf[8] = count >> 8;
	      buf[9] = count;

	      bfd_get_section_contents (pbfd, section, buf + 10, fptr, count);

	      if (SERIAL_WRITE (e7000_desc, buf, count + 10))
		fprintf_unfiltered (gdb_stderr,
				    "e7000_load: SERIAL_WRITE failed: %s\n",
				    safe_strerror(errno));

	      expect ("OK");

	      if (!quiet)
		{
		  printf_unfiltered ("\r%c", inds[k++ % 4]);
		  gdb_flush (gdb_stdout);
		}

	      section_address += count;
	      fptr += count;
	      section_size -= count;
	    }
	}
    }

  write_e7000 ("ED");

  expect_prompt ();

  end_time = time (NULL);

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

  if (!nostart)
    {
      entry = bfd_get_start_address (pbfd);

      if (!quiet)
	printf_unfiltered ("[Starting %s at 0x%x]\n", filename, entry);

/*      start_routine (entry);*/
    }

  printf_filtered ("Transfer rate: %d bits/sec.\n",
		   (data_count * 8)/(end_time - start_time));

  do_cleanups (old_chain);
}

/* Clean up when a program exits.

   The program actually lives on in the remote processor's RAM, and may be
   run again without a download.  Don't leave it full of breakpoint
   instructions.  */

static void
e7000_mourn_inferior ()
{
  remove_breakpoints ();
  unpush_target (&e7000_ops);
  generic_mourn_inferior ();	/* Do all the proper things now */
}

#ifdef  HARD_BREAKPOINTS
#define MAX_E7000DEBUG_BREAKPOINTS (BC_BREAKPOINTS ? 5 :  200)
#else
#define MAX_E7000DEBUG_BREAKPOINTS 200
#endif

extern int memory_breakpoint_size;

static CORE_ADDR breakaddr[MAX_E7000DEBUG_BREAKPOINTS] = {0};

static int
e7000_insert_breakpoint (addr, shadow)
     CORE_ADDR addr;
     unsigned char *shadow;
{
  int i;
  char buf[200];
  static char nop[2] = NOP;

  for (i = 0; i <= MAX_E7000DEBUG_BREAKPOINTS; i++)
    if (breakaddr[i] == 0)
      {
	breakaddr[i] = addr;
	/* Save old contents, and insert a nop in the space */
#ifdef HARD_BREAKPOINTS
	if (BC_BREAKPOINTS) 
	  {
	    sprintf (buf, "BC%d A=%x\r", i+1, addr);
	    puts_e7000debug (buf);
	  }
	else 
	  {
	    sprintf (buf, "B %x\r", addr);
	    puts_e7000debug (buf);
	  }
#else
#if 0
	e7000_read_inferior_memory (addr, shadow, 2);
	e7000_write_inferior_memory (addr, nop, 2);
#endif

	sprintf (buf, "B %x\r", addr);
	puts_e7000debug (buf);
#endif
	expect_prompt ();
	return 0;
      }

  error ("Too many breakpoints ( > %d) for the E7000\n",
	 MAX_E7000DEBUG_BREAKPOINTS);
  return 1;
}

static int
e7000_remove_breakpoint (addr, shadow)
     CORE_ADDR addr;
     unsigned char *shadow;
{
  int i;
  char buf[200];

  for (i = 0; i < MAX_E7000DEBUG_BREAKPOINTS; i++)
    if (breakaddr[i] == addr)
      {
	breakaddr[i] = 0;
#ifdef HARD_BREAKPOINTS
	if (BC_BREAKPOINTS) 
	  {
	    sprintf (buf, "BC%d - \r",  i+1);
	    puts_e7000debug (buf);
	  }
	else 
	  {
	    sprintf (buf, "B - %x\r",  addr);
	    puts_e7000debug (buf);
	  }
	expect_prompt ();
#else
	sprintf (buf, "B - %x\r", addr);
	puts_e7000debug (buf);
	expect_prompt ();

#if 0
	/* Replace the insn under the break */
	e7000_write_inferior_memory (addr, shadow, 2);
#endif
#endif

	return 0;
      }

  warning ("Can't find breakpoint associated with 0x%x\n", addr);
  return 1;
}

/* Put a command string, in args, out to STDBUG.  Output from STDBUG
   is placed on the users terminal until the prompt is seen. */

static void
e7000_command (args, fromtty)
     char *args;
     int fromtty;
{
  /* FIXME: arbitrary limit on length of args.  */
  char buf[200];

  echo = 0;

  if (!e7000_desc)
    error ("e7000 target not open.");
  if (!args)
    {
      puts_e7000debug ("\r");
    }
  else
    {
      sprintf (buf, "%s\r", args);
      puts_e7000debug (buf);
    }

  echo++;
  ctrl_c = 2;
  expect_full_prompt ();
  echo--;
  ctrl_c = 0;
  printf_unfiltered ("\n");

  /* Who knows what the command did... */
  registers_changed ();
}


static void
e7000_drain_command (args, fromtty)
     char *args;
     int fromtty;

{
  int c;

  puts_e7000debug("end\r");
  putchar_e7000 (CTRLC);

  while ((c = SERIAL_READCHAR (e7000_desc, 1) != SERIAL_TIMEOUT))
    {
      if (quit_flag)
	{
	  putchar_e7000(CTRLC);
	  quit_flag = 0;
	}
      if (c > ' ' && c < 127)
	printf ("%c", c & 0xff);
      else
	printf ("<%x>", c & 0xff);
    }
}

#define NITEMS 7

static int
why_stop ()
{
  static  char *strings[NITEMS] = {
    "STEP NORMAL",
    "BREAK POINT",
    "BREAK KEY",
    "BREAK CONDI",
    "CYCLE ACCESS",
    "ILLEGAL INSTRUCTION",
    "WRITE PROTECT",
  };
  char *p[NITEMS];
  int c;
  int i;

  for (i = 0; i < NITEMS; ++i)
    p[i] = strings[i];
  
  c = gch ();
  while (1)
    {
      for (i = 0; i < NITEMS; i++)
	{
	  if (c == *(p[i])) 
	    {
	      p[i]++;
	      if (*(p[i]) == 0) 
		{ 
		  /* found one of the choices */
		  return i;
		}
	    }
	  else
	    p[i] = strings[i];
	}

      c = gch ();
    }
}

/* Suck characters, if a string match, then return the strings index
   otherwise echo them.  */

int
expect_n (strings)
char **strings;
{
  char *(ptr[10]);
  int n; 
  int c;
  char saveaway[100];
  char *buffer = saveaway;
  /* Count number of expect strings  */

  for (n = 0; strings[n]; n++) 
    {
      ptr[n] = strings[n];
    }

  while (1)
    {
      int i;
      int gotone = 0;

      c = SERIAL_READCHAR (e7000_desc, 1);
      if (c == SERIAL_TIMEOUT)
	{
	  printf_unfiltered ("[waiting for e7000...]\n");
	}
#ifdef __GO32__
      if (kbhit ())
	{
	  int k = getkey();

	  if (k == 1)
	    quit_flag = 1;
	}
#endif
      if (quit_flag)
	{
	  putchar_e7000 (CTRLC);	/* interrupt the running program */
	  quit_flag = 0;
	}

      for (i = 0; i < n; i++)
	{
	  if (c == ptr[i][0]) 
	    {
	      ptr[i]++;
	      if (ptr[i][0] == 0)
		{
		  /* Gone all the way */
		  return i;
		}
	      gotone = 1;
	    }
	  else
	    {
	      ptr[i] = strings[i];
	    }
	}
    
      if (gotone)
	{
	  /* Save it up incase we find that there was no match */
	  *buffer ++ = c;
	}
      else
	{
	  if (buffer != saveaway) 
	    {
	      *buffer++ = 0;
	      printf ("%s", buffer);
	      buffer = saveaway;
	    }
	  if (c != SERIAL_TIMEOUT)
	    {
	      putchar (c);
	      fflush (stdout);
	    }
	}
    }
}

/* We subtract two from the pc here rather than use
   DECR_PC_AFTER_BREAK since the e7000 doesn't always add two to the
   pc, and the simulators never do. */

static void
sub2_from_pc ()
{
  char buf[4];
  char buf2[200];

  store_signed_integer (buf,
			REGISTER_RAW_SIZE(PC_REGNUM), 
			read_register (PC_REGNUM) -2);
  supply_register (PC_REGNUM, buf);
  sprintf (buf2, ".PC %x\r", read_register (PC_REGNUM));
  puts_e7000debug (buf2);
}

#define WAS_SLEEP 0
#define WAS_INT 1
#define WAS_RUNNING 2
#define WAS_OTHER 3

static char *estrings[] = {
  "** SLEEP",
  "BREAK !",
  "** PC",
  "PC",
  NULL
};

/* Wait until the remote machine stops, then return, storing status in
   STATUS just as `wait' would.  */

static int
e7000_wait (pid, status)
     int pid;
     struct target_waitstatus *status;
{
  int stop_reason;
  int regno;
  int running_count = 0;
  int had_sleep = 0;
  int loop = 1;

  /* Then echo chars until PC= string seen */
  gch ();			/* Drop cr */
  gch ();			/* and space */

  while (loop)
    {
      switch (expect_n (estrings))
	{	 
	case WAS_OTHER:
	  /* how did this happen ? */
	  loop = 0;
	  break;
	case WAS_SLEEP:
	  had_sleep = 1;
	  putchar_e7000 (CTRLC);
	  loop = 0;
	  break;
	case WAS_INT:
	  loop = 0;
	  break;
	case WAS_RUNNING:
	  running_count++;
	  if (running_count == 20)
	    {
	      printf_unfiltered ("[running...]\n");
	      running_count = 0;
	    }
	  break;
	default:
	  /* error? */
	  break;
	}
    }

  /* Skip till the PC= */
  expect ("=");
  fetch_regs_from_dump (gch, want_nopc);

  /* And supply the extra ones the simulator uses */
  for (regno = NUM_REALREGS; regno < NUM_REGS; regno++)
    {
      int buf = 0;
      supply_register (regno, (char *) &buf);
    }

  stop_reason = why_stop ();
  expect_full_prompt ();

  status->kind = TARGET_WAITKIND_STOPPED;
  status->value.sig = TARGET_SIGNAL_TRAP;

  switch (stop_reason)
    {
    case 1:			/* Breakpoint */
      write_pc (read_pc ()); /* PC is always off by 2 for breakpoints */
      status->value.sig = TARGET_SIGNAL_TRAP;      
      break;
    case 0:			/* Single step */
      status->value.sig = TARGET_SIGNAL_TRAP;      
      break;
    case 2:			/* Interrupt */
      if (had_sleep)
	{
	  status->value.sig = TARGET_SIGNAL_TRAP;      
	  sub2_from_pc ();
	}
      else
	{
	  status->value.sig = TARGET_SIGNAL_INT;      
	}
      break;
    case 3:
      break;
    case 4:
      printf_unfiltered ("a cycle address error?\n");
      status->value.sig = TARGET_SIGNAL_UNKNOWN;      
      break;
    case 5:
      status->value.sig = TARGET_SIGNAL_ILL;      
      break;
    case 6:
      status->value.sig = TARGET_SIGNAL_SEGV;      
      break;
    case 7:			/* Anything else (NITEMS + 1) */
      printf_unfiltered ("a write protect error?\n");
      status->value.sig = TARGET_SIGNAL_UNKNOWN;      
      break;
    default:
      /* Get the user's attention - this should never happen. */
      abort ();
    }

  return 0;
}

/* Define the target subroutine names. */

struct target_ops e7000_ops =
{
  "e7000",
  "Remote Hitachi e7000 target",
  "Use a remote Hitachi e7000 ICE connected by a serial line,\n\
or a network connection.\n\
Arguments are the name of the device for the serial line,\n\
the speed to connect at in bits per second.\n\
eg\n\
target e7000 /dev/ttya 9600\n\
target e7000 foobar",
  e7000_open,			/* to_open */
  e7000_close,			/* to_close */
  0,				/* to_attach */
  e7000_detach,			/* to_detach */
  e7000_resume,			/* to_resume */
  e7000_wait,			/* to_wait */
  e7000_fetch_register,		/* to_fetch_registers */
  e7000_store_register,		/* to_store_registers */
  e7000_prepare_to_store,	/* to_prepare_to_store */
  e7000_xfer_inferior_memory,	/* to_xfer_memory */
  e7000_files_info,		/* to_files_info */
  e7000_insert_breakpoint,	/* to_insert_breakpoint */
  e7000_remove_breakpoint,	/* to_remove_breakpoint */
  0,				/* to_terminal_init */
  0,				/* to_terminal_inferior */
  0,				/* to_terminal_ours_for_output */
  0,				/* to_terminal_ours */
  0,				/* to_terminal_info */
  e7000_kill,			/* to_kill */
  e7000_load,			/* to_load */
  0,				/* to_lookup_symbol */
  e7000_create_inferior,	/* to_create_inferior */
  e7000_mourn_inferior,		/* to_mourn_inferior */
  0,				/* to_can_run */
  0,				/* to_notice_signals */
  0,				/* to_thread_alive */
  0,				/* to_stop */
  process_stratum,		/* to_stratum */
  0,				/* next (unused) */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  1,				/* to_has_execution */
  0,				/* to_sections */
  0,				/* to_sections_end */
  OPS_MAGIC,			/* Always the last thing */
};

void
_initialize_remote_e7000 ()
{
  add_target (&e7000_ops);

  add_com ("e7000 <command>", class_obscure, e7000_command,
	   "Send a command to the e7000 monitor.");

  add_com ("ftplogin <machine> <name> <passwd> <dir>", class_obscure, e7000_login_command,
	   "Login to machine and change to directory.");

  add_com ("ftpload <file>", class_obscure, e7000_ftp_command,
	   "Fetch and load a file from previously described place.");

  add_com ("drain", class_obscure, e7000_drain_command,
	   "Drain pending e7000 text buffers.");
}
