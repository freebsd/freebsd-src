/* Remote debugging interface for AMD 29000 EBMON on IBM PC, for GDB.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
   Contributed by Cygnus Support.  Written by Jim Kingdon for Cygnus.

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

/* This is like remote.c but is for an esoteric situation--
   having a a29k board in a PC hooked up to a unix machine with
   a serial line, and running ctty com1 on the PC, through which
   the unix machine can run ebmon.  Not to mention that the PC
   has PC/NFS, so it can access the same executables that gdb can,
   over the net in real time.  */

#include "defs.h"
#include "gdb_string.h"

#include "inferior.h"
#include "bfd.h"
#include "symfile.h"
#include "wait.h"
#include "value.h"
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "terminal.h"
#include "target.h"
#include "gdbcore.h"

extern struct target_ops eb_ops;		/* Forward declaration */

static void eb_close();

#define LOG_FILE "eb.log"
#if defined (LOG_FILE)
FILE *log_file;
#endif

static int timeout = 24;

/* Descriptor for I/O to remote machine.  Initialize it to -1 so that
   eb_open knows that we don't have a file open when the program
   starts.  */
int eb_desc = -1;

/* stream which is fdopen'd from eb_desc.  Only valid when
   eb_desc != -1.  */
FILE *eb_stream;

/* Read a character from the remote system, doing all the fancy
   timeout stuff.  */
static int
readchar ()
{
  char buf;

  buf = '\0';
#ifdef HAVE_TERMIO
  /* termio does the timeout for us.  */
  read (eb_desc, &buf, 1);
#else
  alarm (timeout);
  if (read (eb_desc, &buf, 1) < 0)
    {
      if (errno == EINTR)
	error ("Timeout reading from remote system.");
      else
	perror_with_name ("remote");
    }
  alarm (0);
#endif

  if (buf == '\0')
    error ("Timeout reading from remote system.");
#if defined (LOG_FILE)
  putc (buf & 0x7f, log_file);
#endif
  return buf & 0x7f;
}

/* Keep discarding input from the remote system, until STRING is found. 
   Let the user break out immediately.  */
static void
expect (string)
     char *string;
{
  char *p = string;

  immediate_quit = 1;
  while (1)
    {
      if (readchar() == *p)
	{
	  p++;
	  if (*p == '\0')
	    {
	      immediate_quit = 0;
	      return;
	    }
	}
      else
	p = string;
    }
}

/* Keep discarding input until we see the ebmon prompt.

   The convention for dealing with the prompt is that you
   o give your command
   o *then* wait for the prompt.

   Thus the last thing that a procedure does with the serial line
   will be an expect_prompt().  Exception:  eb_resume does not
   wait for the prompt, because the terminal is being handed over
   to the inferior.  However, the next thing which happens after that
   is a eb_wait which does wait for the prompt.
   Note that this includes abnormal exit, e.g. error().  This is
   necessary to prevent getting into states from which we can't
   recover.  */
static void
expect_prompt ()
{
#if defined (LOG_FILE)
  /* This is a convenient place to do this.  The idea is to do it often
     enough that we never lose much data if we terminate abnormally.  */
  fflush (log_file);
#endif
  expect ("\n# ");
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

/* Get a byte from eb_desc and put it in *BYT.  Accept any number
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

/* Get N 32-bit words from remote, each preceded by a space,
   and put them in registers starting at REGNO.  */
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

/* Called when SIGALRM signal sent due to alarm() timeout.  */
#ifndef HAVE_TERMIO

#ifndef __STDC__
#define volatile /**/
#endif
volatile int n_alarms;

void
eb_timer ()
{
#if 0
  if (kiodebug)
    printf ("eb_timer called\n");
#endif
  n_alarms++;
}
#endif

/* malloc'd name of the program on the remote system.  */
static char *prog_name = NULL;

/* Nonzero if we have loaded the file ("yc") and not yet issued a "gi"
   command.  "gi" is supposed to happen exactly once for each "yc".  */
static int need_gi = 0;

/* Number of SIGTRAPs we need to simulate.  That is, the next
   NEED_ARTIFICIAL_TRAP calls to eb_wait should just return
   SIGTRAP without actually waiting for anything.  */

static int need_artificial_trap = 0;

/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */
static void
eb_create_inferior (execfile, args, env)
     char *execfile;
     char *args;
     char **env;
{
  int entry_pt;

  if (args && *args)
    error ("Can't pass arguments to remote EBMON process");

  if (execfile == 0 || exec_bfd == 0)
    error ("No exec file specified");

  entry_pt = (int) bfd_get_start_address (exec_bfd);

  {
    /* OK, now read in the file.  Y=read, C=COFF, D=no symbols
       0=start address, %s=filename.  */

    fprintf (eb_stream, "YC D,0:%s", prog_name);

    if (args != NULL)
	fprintf(eb_stream, " %s", args);

    fprintf (eb_stream, "\n");
    fflush (eb_stream);

    expect_prompt ();

    need_gi = 1;
  }

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
  proceed ((CORE_ADDR)entry_pt, TARGET_SIGNAL_DEFAULT, 0);		/* Let 'er rip... */
}

/* Translate baud rates from integers to damn B_codes.  Unix should
   have outgrown this crap years ago, but even POSIX wouldn't buck it.  */

#ifndef B19200
#define B19200 EXTA
#endif
#ifndef B38400
#define B38400 EXTB
#endif

struct {int rate, damn_b;} baudtab[] = {
	{0, B0},
	{50, B50},
	{75, B75},
	{110, B110},
	{134, B134},
	{150, B150},
	{200, B200},
	{300, B300},
	{600, B600},
	{1200, B1200},
	{1800, B1800},
	{2400, B2400},
	{4800, B4800},
	{9600, B9600},
	{19200, B19200},
	{38400, B38400},
	{-1, -1},
};

int damn_b (rate)
     int rate;
{
  int i;

  for (i = 0; baudtab[i].rate != -1; i++)
    if (rate == baudtab[i].rate) return baudtab[i].damn_b;
  return B38400;	/* Random */
}


/* Open a connection to a remote debugger.
   NAME is the filename used for communication, then a space,
   then the name of the program as we should name it to EBMON.  */

static int baudrate = 9600;
static char *dev_name;
void
eb_open (name, from_tty)
     char *name;
     int from_tty;
{
  TERMINAL sg;

  char *p;

  target_preopen (from_tty);
  
  /* Find the first whitespace character, it separates dev_name from
     prog_name.  */
  if (name == 0)
    goto erroid;

  for (p = name;
       *p != '\0' && !isspace (*p); p++)
    ;
  if (*p == '\0')
erroid:
    error ("\
Please include the name of the device for the serial port,\n\
the baud rate, and the name of the program to run on the remote system.");
  dev_name = alloca (p - name + 1);
  strncpy (dev_name, name, p - name);
  dev_name[p - name] = '\0';

  /* Skip over the whitespace after dev_name */
  for (; isspace (*p); p++)
    /*EMPTY*/;
  
  if (1 != sscanf (p, "%d ", &baudrate))
    goto erroid;

  /* Skip the number and then the spaces */
  for (; isdigit (*p); p++)
    /*EMPTY*/;
  for (; isspace (*p); p++)
    /*EMPTY*/;
  
  if (prog_name != NULL)
    free (prog_name);
  prog_name = savestring (p, strlen (p));

  eb_close (0);

  eb_desc = open (dev_name, O_RDWR);
  if (eb_desc < 0)
    perror_with_name (dev_name);
  ioctl (eb_desc, TIOCGETP, &sg);
#ifdef HAVE_TERMIO
  sg.c_cc[VMIN] = 0;		/* read with timeout.  */
  sg.c_cc[VTIME] = timeout * 10;
  sg.c_lflag &= ~(ICANON | ECHO);
  sg.c_cflag = (sg.c_cflag & ~CBAUD) | damn_b (baudrate);
#else
  sg.sg_ispeed = damn_b (baudrate);
  sg.sg_ospeed = damn_b (baudrate);
  sg.sg_flags |= RAW | ANYP;
  sg.sg_flags &= ~ECHO;
#endif

  ioctl (eb_desc, TIOCSETP, &sg);
  eb_stream = fdopen (eb_desc, "r+");

  push_target (&eb_ops);
  if (from_tty)
    printf ("Remote %s debugging %s using %s\n", target_shortname,
	    prog_name, dev_name);

#ifndef HAVE_TERMIO
#ifndef NO_SIGINTERRUPT
  /* Cause SIGALRM's to make reads fail with EINTR instead of resuming
     the read.  */
  if (siginterrupt (SIGALRM, 1) != 0)
    perror ("eb_open: error in siginterrupt");
#endif

  /* Set up read timeout timer.  */
  if ((void (*)) signal (SIGALRM, eb_timer) == (void (*)) -1)
    perror ("eb_open: error in signal");
#endif

#if defined (LOG_FILE)
  log_file = fopen (LOG_FILE, "w");
  if (log_file == NULL)
    perror_with_name (LOG_FILE);
#endif

  /* Hello?  Are you there?  */
  write (eb_desc, "\n", 1);
  
  expect_prompt ();
}

/* Close out all files and local state before this target loses control. */

static void
eb_close (quitting)
     int quitting;
{

  /* Due to a bug in Unix, fclose closes not only the stdio stream,
     but also the file descriptor.  So we don't actually close
     eb_desc.  */
  if (eb_stream)
    fclose (eb_stream);	/* This also closes eb_desc */
  if (eb_desc >= 0)
    /* close (eb_desc); */

  /* Do not try to close eb_desc again, later in the program.  */
  eb_stream = NULL;
  eb_desc = -1;

#if defined (LOG_FILE)
  if (log_file) {
    if (ferror (log_file))
      printf ("Error writing log file.\n");
    if (fclose (log_file) != 0)
      printf ("Error closing log file.\n");
  }
#endif
}

/* Terminate the open connection to the remote debugger.
   Use this when you want to detach and do something else
   with your gdb.  */
void
eb_detach (from_tty)
     int from_tty;
{
  pop_target();		/* calls eb_close to do the real work */
  if (from_tty)
    printf ("Ending remote %s debugging\n", target_shortname);
}
 
/* Tell the remote machine to resume.  */

void
eb_resume (pid, step, sig)
     int pid, step;
     enum target_signal sig;
{
  if (step)
    {
      write (eb_desc, "t 1,s\n", 6);
      /* Wait for the echo.  */
      expect ("t 1,s\r");
      /* Then comes a line containing the instruction we stepped to.  */
      expect ("\n@");
      /* Then we get the prompt.  */
      expect_prompt ();

      /* Force the next eb_wait to return a trap.  Not doing anything
         about I/O from the target means that the user has to type
         "continue" to see any.  This should be fixed.  */
      need_artificial_trap = 1;
    }
  else
    {
      if (need_gi)
	{
	  need_gi = 0;
	  write (eb_desc, "gi\n", 3);
	  
	  /* Swallow the echo of "gi".  */
	  expect ("gi\r");
	}
      else
	{
	  write (eb_desc, "GR\n", 3);
	  /* Swallow the echo.  */
	  expect ("GR\r");
	}
    }
}

/* Wait until the remote machine stops, then return,
   storing status in STATUS just as `wait' would.  */

int
eb_wait (status)
     struct target_waitstatus *status;
{
  /* Strings to look for.  '?' means match any single character.  
     Note that with the algorithm we use, the initial character
     of the string cannot recur in the string, or we will not
     find some cases of the string in the input.  */
  
  static char bpt[] = "Invalid interrupt taken - #0x50 - ";
  /* It would be tempting to look for "\n[__exit + 0x8]\n"
     but that requires loading symbols with "yc i" and even if
     we did do that we don't know that the file has symbols.  */
  static char exitmsg[] = "\n@????????I    JMPTI     GR121,LR0";
  char *bp = bpt;
  char *ep = exitmsg;

  /* Large enough for either sizeof (bpt) or sizeof (exitmsg) chars.  */
  char swallowed[50];
  /* Current position in swallowed.  */
  char *swallowed_p = swallowed;

  int ch;
  int ch_handled;

  int old_timeout = timeout;

  status->kind = TARGET_WAITKIND_EXITED;
  status->value.integer = 0;

  if (need_artificial_trap != 0)
    {
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_TRAP;
      need_artificial_trap--;
      return 0;
    }

  timeout = 0;		/* Don't time out -- user program is running. */
  while (1)
    {
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
	bp = bpt;

      if (ch == *ep || *ep == '?')
	{
	  ep++;
	  if (*ep == '\0')
	    break;

	  if (!ch_handled)
	    *swallowed_p++ = ch;
	  ch_handled = 1;
	}
      else
	ep = exitmsg;

      if (!ch_handled)
	{
	  char *p;

	  /* Print out any characters which have been swallowed.  */
	  for (p = swallowed; p < swallowed_p; ++p)
	    putc (*p, stdout);
	  swallowed_p = swallowed;
	  
	  putc (ch, stdout);
	}
    }
  expect_prompt ();
  if (*bp== '\0')
    {
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_TRAP;
    }
  else
    {
      status->kind = TARGET_WAITKIND_EXITED;
      status->value.integer = 0;
    }
  timeout = old_timeout;

  return 0;
}

/* Return the name of register number REGNO
   in the form input and output by EBMON.

   Returns a pointer to a static buffer containing the answer.  */
static char *
get_reg_name (regno)
     int regno;
{
  static char buf[80];
  if (regno >= GR96_REGNUM && regno < GR96_REGNUM + 32)
    sprintf (buf, "GR%03d", regno - GR96_REGNUM + 96);
  else if (regno >= LR0_REGNUM && regno < LR0_REGNUM + 128)
    sprintf (buf, "LR%03d", regno - LR0_REGNUM);
  else if (regno == Q_REGNUM)
    strcpy (buf, "SR131");
  else if (regno >= BP_REGNUM && regno <= CR_REGNUM)
    sprintf (buf, "SR%03d", regno - BP_REGNUM + 133);
  else if (regno == ALU_REGNUM)
    strcpy (buf, "SR132");
  else if (regno >= IPC_REGNUM && regno <= IPB_REGNUM)
    sprintf (buf, "SR%03d", regno - IPC_REGNUM + 128);
  else if (regno >= VAB_REGNUM && regno <= LRU_REGNUM)
    sprintf (buf, "SR%03d", regno - VAB_REGNUM);
  else if (regno == GR1_REGNUM)
    strcpy (buf, "GR001");
  return buf;
}

/* Read the remote registers into the block REGS.  */

static void
eb_fetch_registers ()
{
  int reg_index;
  int regnum_index;
  char tempbuf[10];
  int i;

#if 0
  /* This should not be necessary, because one is supposed to read the
     registers only when the inferior is stopped (at least with
     ptrace() and why not make it the same for remote?).  */
  /* ^A is the "normal character" used to make sure we are talking to EBMON
     and not to the program being debugged.  */
  write (eb_desc, "\001\n");
  expect_prompt ();
#endif

  write (eb_desc, "dw gr96,gr127\n", 14);
  for (reg_index = 96, regnum_index = GR96_REGNUM;
       reg_index < 128;
       reg_index += 4, regnum_index += 4)
    {
      sprintf (tempbuf, "GR%03d ", reg_index);
      expect (tempbuf);
      get_hex_regs (4, regnum_index);
      expect ("\n");
    }

  for (i = 0; i < 128; i += 32)
    {
      /* The PC has a tendency to hang if we get these
	 all in one fell swoop ("dw lr0,lr127").  */
      sprintf (tempbuf, "dw lr%d\n", i);
      write (eb_desc, tempbuf, strlen (tempbuf));
      for (reg_index = i, regnum_index = LR0_REGNUM + i;
	   reg_index < i + 32;
	   reg_index += 4, regnum_index += 4)
	{
	  sprintf (tempbuf, "LR%03d ", reg_index);
	  expect (tempbuf);
	  get_hex_regs (4, regnum_index);
	  expect ("\n");
	}
    }

  write (eb_desc, "dw sr133,sr133\n", 15);
  expect ("SR133          ");
  get_hex_regs (1, BP_REGNUM);
  expect ("\n");

  write (eb_desc, "dw sr134,sr134\n", 15);
  expect ("SR134                   ");
  get_hex_regs (1, FC_REGNUM);
  expect ("\n");

  write (eb_desc, "dw sr135,sr135\n", 15);
  expect ("SR135                            ");
  get_hex_regs (1, CR_REGNUM);
  expect ("\n");

  write (eb_desc, "dw sr131,sr131\n", 15);
  expect ("SR131                            ");
  get_hex_regs (1, Q_REGNUM);
  expect ("\n");

  write (eb_desc, "dw sr0,sr14\n", 12);
  for (reg_index = 0, regnum_index = VAB_REGNUM;
       regnum_index <= LRU_REGNUM;
       regnum_index += 4, reg_index += 4)
    {
      sprintf (tempbuf, "SR%03d ", reg_index);
      expect (tempbuf);
      get_hex_regs (reg_index == 12 ? 3 : 4, regnum_index);
      expect ("\n");
    }

  /* There doesn't seem to be any way to get these.  */
  {
    int val = -1;
    supply_register (FPE_REGNUM, (char *) &val);
    supply_register (INTE_REGNUM, (char *) &val);
    supply_register (FPS_REGNUM, (char *) &val);
    supply_register (EXO_REGNUM, (char *) &val);
  }

  write (eb_desc, "dw gr1,gr1\n", 11);
  expect ("GR001 ");
  get_hex_regs (1, GR1_REGNUM);
  expect_prompt ();
}

/* Fetch register REGNO, or all registers if REGNO is -1.
   Returns errno value.  */
void
eb_fetch_register (regno)
     int regno;
{
  if (regno == -1)
    eb_fetch_registers ();
  else
    {
      char *name = get_reg_name (regno);
      fprintf (eb_stream, "dw %s,%s\n", name, name);
      expect (name);
      expect (" ");
      get_hex_regs (1, regno);
      expect_prompt ();
    }
  return;
}

/* Store the remote registers from the contents of the block REGS.  */

static void
eb_store_registers ()
{
  int i, j;
  fprintf (eb_stream, "s gr1,%x\n", read_register (GR1_REGNUM));
  expect_prompt ();

  for (j = 0; j < 32; j += 16)
    {
      fprintf (eb_stream, "s gr%d,", j + 96);
      for (i = 0; i < 15; ++i)
	fprintf (eb_stream, "%x,", read_register (GR96_REGNUM + j + i));
      fprintf (eb_stream, "%x\n", read_register (GR96_REGNUM + j + 15));
      expect_prompt ();
    }

  for (j = 0; j < 128; j += 16)
    {
      fprintf (eb_stream, "s lr%d,", j);
      for (i = 0; i < 15; ++i)
	fprintf (eb_stream, "%x,", read_register (LR0_REGNUM + j + i));
      fprintf (eb_stream, "%x\n", read_register (LR0_REGNUM + j + 15));
      expect_prompt ();
    }

  fprintf (eb_stream, "s sr133,%x,%x,%x\n", read_register (BP_REGNUM),
	   read_register (FC_REGNUM), read_register (CR_REGNUM));
  expect_prompt ();
  fprintf (eb_stream, "s sr131,%x\n", read_register (Q_REGNUM));
  expect_prompt ();
  fprintf (eb_stream, "s sr0,");
  for (i = 0; i < 11; ++i)
    fprintf (eb_stream, "%x,", read_register (VAB_REGNUM + i));
  fprintf (eb_stream, "%x\n", read_register (VAB_REGNUM + 11));
  expect_prompt ();
}

/* Store register REGNO, or all if REGNO == 0.
   Return errno value.  */
void
eb_store_register (regno)
     int regno;
{
  if (regno == -1)
    eb_store_registers ();
  else
    {
      char *name = get_reg_name (regno);
      fprintf (eb_stream, "s %s,%x\n", name, read_register (regno));
      /* Setting GR1 changes the numbers of all the locals, so
	 invalidate the register cache.  Do this *after* calling
	 read_register, because we want read_register to return the
	 value that write_register has just stuffed into the registers
	 array, not the value of the register fetched from the
	 inferior.  */
      if (regno == GR1_REGNUM)
	registers_changed ();
      expect_prompt ();
    }
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

void
eb_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}


/* FIXME-someday!  Merge these two.  */
int
eb_xfer_inferior_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;		/* ignored */
{
  if (write)
    return eb_write_inferior_memory (memaddr, myaddr, len);
  else
    return eb_read_inferior_memory (memaddr, myaddr, len);
}

void
eb_files_info ()
{
  printf ("\tAttached to %s at %d baud and running program %s.\n",
	  dev_name, baudrate, prog_name);
}

/* Copy LEN bytes of data from debugger memory at MYADDR
   to inferior's memory at MEMADDR.  Returns length moved.  */
int
eb_write_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int i;

  for (i = 0; i < len; i++)
    {
      if ((i % 16) == 0)
	fprintf (eb_stream, "sb %x,", memaddr + i);
      if ((i % 16) == 15 || i == len - 1)
	{
	  fprintf (eb_stream, "%x\n", ((unsigned char *)myaddr)[i]);
	  expect_prompt ();
	}
      else
	fprintf (eb_stream, "%x,", ((unsigned char *)myaddr)[i]);
    }
  return len;
}

/* Read LEN bytes from inferior memory at MEMADDR.  Put the result
   at debugger address MYADDR.  Returns length moved.  */
int
eb_read_inferior_memory(memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int i;

  /* Number of bytes read so far.  */
  int count;

  /* Starting address of this pass.  */
  unsigned long startaddr;

  /* Number of bytes to read in this pass.  */
  int len_this_pass;

  /* Note that this code works correctly if startaddr is just less
     than UINT_MAX (well, really CORE_ADDR_MAX if there was such a
     thing).  That is, something like
     eb_read_bytes (CORE_ADDR_MAX - 4, foo, 4)
     works--it never adds len to memaddr and gets 0.  */
  /* However, something like
     eb_read_bytes (CORE_ADDR_MAX - 3, foo, 4)
     doesn't need to work.  Detect it and give up if there's an attempt
     to do that.  */
  if (((memaddr - 1) + len) < memaddr) {
    errno = EIO;
    return 0;
  }
  
  startaddr = memaddr;
  count = 0;
  while (count < len)
    {
      len_this_pass = 16;
      if ((startaddr % 16) != 0)
	len_this_pass -= startaddr % 16;
      if (len_this_pass > (len - count))
	len_this_pass = (len - count);

      fprintf (eb_stream, "db %x,%x\n", startaddr,
	       (startaddr - 1) + len_this_pass);
      expect ("\n");

      /* Look for 8 hex digits.  */
      i = 0;
      while (1)
	{
	  if (isxdigit (readchar ()))
	    ++i;
	  else
	    {
	      expect_prompt ();
	      error ("Hex digit expected from remote system.");
	    }
	  if (i >= 8)
	    break;
	}

      expect ("  ");

      for (i = 0; i < len_this_pass; i++)
	get_hex_byte (&myaddr[count++]);

      expect_prompt ();

      startaddr += len_this_pass;
    }
  return len;
}

static void
eb_kill (args, from_tty)
     char *args;
     int from_tty;
{
  return;		/* Ignore attempts to kill target system */
}

/* Clean up when a program exits.

   The program actually lives on in the remote processor's RAM, and may be
   run again without a download.  Don't leave it full of breakpoint
   instructions.  */

void
eb_mourn_inferior ()
{
  remove_breakpoints ();
  unpush_target (&eb_ops);
  generic_mourn_inferior ();	/* Do all the proper things now */
}
/* Define the target subroutine names */

struct target_ops eb_ops = {
	"amd-eb", "Remote serial AMD EBMON target",
	"Use a remote computer running EBMON connected by a serial line.\n\
Arguments are the name of the device for the serial line,\n\
the speed to connect at in bits per second, and the filename of the\n\
executable as it exists on the remote computer.  For example,\n\
        target amd-eb /dev/ttya 9600 demo",
	eb_open, eb_close, 
	0, eb_detach, eb_resume, eb_wait,
	eb_fetch_register, eb_store_register,
	eb_prepare_to_store,
	eb_xfer_inferior_memory, eb_files_info,
	0, 0,	/* Breakpoints */
	0, 0, 0, 0, 0,	/* Terminal handling */
	eb_kill,
	generic_load,	/* load */
	0, /* lookup_symbol */
	eb_create_inferior,
	eb_mourn_inferior,
  	0,	/* can_run */
  	0, /* notice_signals */
	0,			/* to_stop */
	process_stratum, 0, /* next */
	1, 1, 1, 1, 1,	/* all mem, mem, stack, regs, exec */
	0, 0,			/* Section pointers */
	OPS_MAGIC,		/* Always the last thing */
};

void
_initialize_remote_eb ()
{
  add_target (&eb_ops);
}
