/* Remote debugging interface for Array Tech RAID controller..
   Copyright 90, 91, 92, 93, 94, 1995  Free Software Foundation, Inc.
   Contributed by Cygnus Support. Written by Rob Savoye for Cygnus.

   This module talks to a debug monitor called 'MONITOR', which
   We communicate with MONITOR via either a direct serial line, or a TCP
   (or possibly TELNET) stream to a terminal multiplexor,
   which in turn talks to the target board.

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
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

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
#include <sys/types.h>
#include "gdb_string.h"
#include "command.h"
#include "serial.h"
#include "monitor.h"
#include "remote-utils.h"

extern int baud_rate;

#define ARRAY_PROMPT ">> "

#define SWAP_TARGET_AND_HOST(buffer,len) 				\
  do									\
    {									\
      if (TARGET_BYTE_ORDER != HOST_BYTE_ORDER)				\
	{								\
	  char tmp;							\
	  char *p = (char *)(buffer);					\
	  char *q = ((char *)(buffer)) + len - 1;		   	\
	  for (; p < q; p++, q--)				 	\
	    {								\
	      tmp = *q;							\
	      *q = *p;							\
	      *p = tmp;							\
	    }								\
	}								\
    }									\
  while (0)

static void debuglogs PARAMS((int, char *, ...));
static void array_open();
static void array_close();
static void array_detach();
static void array_attach();
static void array_resume();
static void array_fetch_register();
static void array_store_register();
static void array_fetch_registers();
static void array_store_registers();
static void array_prepare_to_store();
static void array_files_info();
static void array_kill();
static void array_create_inferior();
static void array_mourn_inferior();
static void make_gdb_packet();
static int array_xfer_memory();
static int array_wait();
static int array_insert_breakpoint();
static int array_remove_breakpoint();
static int tohex();
static int to_hex();
static int from_hex();
static int array_send_packet();
static int array_get_packet();
static unsigned long ascii2hexword();
static char *hexword2ascii();

extern char *version;

#define LOG_FILE "monitor.log"
#if defined (LOG_FILE)
FILE *log_file;
#endif

static int timeout = 30;
/* Having this larger than 400 causes us to be incompatible with m68k-stub.c
   and i386-stub.c.  Normally, no one would notice because it only matters
   for writing large chunks of memory (e.g. in downloads).  Also, this needs
   to be more than 400 if required to hold the registers (see below, where
   we round it up based on REGISTER_BYTES).  */
#define PBUFSIZ 400

/* 
 * Descriptor for I/O to remote machine.  Initialize it to NULL so that
 * array_open knows that we don't have a file open when the program starts.
 */
serial_t array_desc = NULL;

/*
 * this array of registers need to match the indexes used by GDB. The
 * whole reason this exists is cause the various ROM monitors use
 * different strings than GDB does, and doesn't support all the
 * registers either. So, typing "info reg sp" becomes a "r30".
 */
extern char *tmp_mips_processor_type;
extern int mips_set_processor_type();

static struct target_ops array_ops = {
  "array",			/* to_shortname */
				/* to_longname */
  "Debug using the standard GDB remote protocol for the Array Tech target.",
				/* to_doc */
  "Debug using the standard GDB remote protocol for the Array Tech target.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).",
  array_open,			/* to_open */
  array_close,			/* to_close */
  NULL,				/* to_attach */
  array_detach,			/* to_detach */
  array_resume,			/* to_resume */
  array_wait,			/* to_wait */
  array_fetch_registers,	/* to_fetch_registers */
  array_store_registers,	/* to_store_registers */
  array_prepare_to_store,	/* to_prepare_to_store */
  array_xfer_memory,		/* to_xfer_memory */
  array_files_info,		/* to_files_info */
  array_insert_breakpoint,	/* to_insert_breakpoint */
  array_remove_breakpoint,	/* to_remove_breakpoint */
  0,				/* to_terminal_init */
  0,				/* to_terminal_inferior */
  0,				/* to_terminal_ours_for_output */
  0,				/* to_terminal_ours */
  0,				/* to_terminal_info */
  array_kill,			/* to_kill */
  0,				/* to_load */
  0,				/* to_lookup_symbol */
  array_create_inferior,	/* to_create_inferior */
  array_mourn_inferior,		/* to_mourn_inferior */
  0,				/* to_can_run */
  0, 				/* to_notice_signals */
  0,				/* to_thread_alive */
  0,                            /* to_stop */
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

/*
 * printf_monitor -- send data to monitor.  Works just like printf.
 */
static void
#ifdef ANSI_PROTOTYPES
printf_monitor(char *pattern, ...)
#else
printf_monitor(va_alist)
     va_dcl
#endif
{
  va_list args;
  char buf[PBUFSIZ];
  int i;

#ifdef ANSI_PROTOTYPES
  va_start(args, pattern);
#else
  char *pattern;
  va_start(args);
  pattern = va_arg(args, char *);
#endif

  vsprintf(buf, pattern, args);

  debuglogs (1, "printf_monitor(), Sending: \"%s\".", buf);

  if (strlen(buf) > PBUFSIZ)
    error ("printf_monitor(): string too long");
  if (SERIAL_WRITE(array_desc, buf, strlen(buf)))
    fprintf(stderr, "SERIAL_WRITE failed: %s\n", safe_strerror(errno));
}
/*
 * write_monitor -- send raw data to monitor.
 */
static void
write_monitor(data, len)
     char data[];
     int len;
{
  if (SERIAL_WRITE(array_desc, data, len))
    fprintf(stderr, "SERIAL_WRITE failed: %s\n", safe_strerror(errno));
 
  *(data + len+1) = '\0';
  debuglogs (1, "write_monitor(), Sending: \"%s\".", data);

}

/*
 * debuglogs -- deal with debugging info to multiple sources. This takes
 *	two real args, the first one is the level to be compared against 
 *	the sr_get_debug() value, the second arg is a printf buffer and args
 *	to be formatted and printed. A CR is added after each string is printed.
 */
static void
#ifdef ANSI_PROTOTYPES
debuglogs(int level, char *pattern, ...)
#else
debuglogs(va_alist)
     va_dcl
#endif
{
  va_list args;
  char *p;
  unsigned char buf[PBUFSIZ];
  char newbuf[PBUFSIZ];
  int i;

#ifdef ANSI_PROTOTYPES
  va_start(args, pattern);
#else
  char *pattern;
  int level;
  va_start(args);
  level = va_arg(args, int);			/* get the debug level */
  pattern = va_arg(args, char *);		/* get the printf style pattern */
#endif

  if ((level <0) || (level > 100)) {
    error ("Bad argument passed to debuglogs(), needs debug level");
    return;
  }
      
  vsprintf(buf, pattern, args);			/* format the string */
  
  /* convert some characters so it'll look right in the log */
  p = newbuf;
  for (i = 0 ; buf[i] != '\0'; i++) {
    if (i > PBUFSIZ)
      error ("Debug message too long");
    switch (buf[i]) {
    case '\n':					/* newlines */
      *p++ = '\\';
      *p++ = 'n';
      continue;
    case '\r':					/* carriage returns */
      *p++ = '\\';
      *p++ = 'r';
      continue;
    case '\033':				/* escape */
      *p++ = '\\';
      *p++ = 'e';
      continue;
    case '\t':					/* tab */
      *p++ = '\\';
      *p++ = 't';
      continue;
    case '\b':					/* backspace */
      *p++ = '\\';
      *p++ = 'b';
      continue;
    default:					/* no change */
      *p++ = buf[i];
    }

    if (buf[i] < 26) {				/* modify control characters */
      *p++ = '^';
      *p++ = buf[i] + 'A';
      continue;
    }
     if (buf[i] >= 128) {			/* modify control characters */
      *p++ = '!';
      *p++ = buf[i] + 'A';
      continue;
    }
 }
  *p = '\0';					/* terminate the string */

  if (sr_get_debug() > level)
    printf_unfiltered ("%s\n", newbuf);

#ifdef LOG_FILE					/* write to the monitor log */
  if (log_file != 0x0) {
    fputs (newbuf, log_file);
    fputc ('\n', log_file);
    fflush (log_file);
  }
#endif
}

/* readchar -- read a character from the remote system, doing all the fancy
 *	timeout stuff.
 */
static int
readchar(timeout)
     int timeout;
{
  int c;

  c = SERIAL_READCHAR(array_desc, abs(timeout));

  if (sr_get_debug() > 5) {
    putchar(c & 0x7f);
    debuglogs (5, "readchar: timeout = %d\n", timeout);
  }

#ifdef LOG_FILE
  if (isascii (c))
    putc(c & 0x7f, log_file);
#endif

  if (c >= 0)
    return c & 0x7f;

  if (c == SERIAL_TIMEOUT) {
    if (timeout <= 0)
      return c;		/* Polls shouldn't generate timeout errors */
    error("Timeout reading from remote system.");
#ifdef LOG_FILE
      fputs ("ERROR: Timeout reading from remote system", log_file);
#endif
  }
  perror_with_name("readchar");
}

/* 
 * expect --  scan input from the remote system, until STRING is found.
 *	If DISCARD is non-zero, then discard non-matching input, else print
 *	it out. Let the user break out immediately.
 */
static void
expect (string, discard)
     char *string;
     int discard;
{
  char *p = string;
  int c;


  debuglogs (1, "Expecting \"%s\".", string);

  immediate_quit = 1;
  while (1) {
    c = readchar(timeout);
    if (!isascii (c))
      continue;
    if (c == *p++) {
      if (*p == '\0') {
	immediate_quit = 0;
	debuglogs (4, "Matched");
	return;
      }
    } else {
      if (!discard) {
	fputc_unfiltered (c, gdb_stdout);
      }
      p = string;
    }
  }
}

/* Keep discarding input until we see the MONITOR array_cmds->prompt.

   The convention for dealing with the expect_prompt is that you
   o give your command
   o *then* wait for the expect_prompt.

   Thus the last thing that a procedure does with the serial line
   will be an expect_prompt().  Exception:  array_resume does not
   wait for the expect_prompt, because the terminal is being handed over
   to the inferior.  However, the next thing which happens after that
   is a array_wait which does wait for the expect_prompt.
   Note that this includes abnormal exit, e.g. error().  This is
   necessary to prevent getting into states from which we can't
   recover.  */
static void
expect_prompt(discard)
     int discard;
{
  expect (ARRAY_PROMPT, discard);
}

/*
 * junk -- ignore junk characters. Returns a 1 if junk, 0 otherwise
 */
static int
junk(ch)
     char ch;
{
  switch (ch) {
  case '\0':
  case ' ':
  case '-':
  case '\t':
  case '\r':
  case '\n':
    if (sr_get_debug() > 5)
      debuglogs (5, "Ignoring \'%c\'.", ch);
    return 1;
  default:
    if (sr_get_debug() > 5)
      debuglogs (5, "Accepting \'%c\'.", ch);
    return 0;
  }
}

/* 
 *  get_hex_digit -- Get a hex digit from the remote system & return its value.
 *		If ignore is nonzero, ignore spaces, newline & tabs.
 */
static int
get_hex_digit(ignore)
     int ignore;
{
  static int ch;
  while (1) {
    ch = readchar(timeout);
    if (junk(ch))
      continue;
    if (sr_get_debug() > 4) {
      debuglogs (4, "get_hex_digit() got a 0x%x(%c)", ch, ch);
    } else {
#ifdef LOG_FILE					/* write to the monitor log */
      if (log_file != 0x0) {
	fputs ("get_hex_digit() got a 0x", log_file);
	fputc (ch, log_file);
	fputc ('\n', log_file);
	fflush (log_file);
      }
#endif
    }

    if (ch >= '0' && ch <= '9')
      return ch - '0';
    else if (ch >= 'A' && ch <= 'F')
      return ch - 'A' + 10;
    else if (ch >= 'a' && ch <= 'f')
      return ch - 'a' + 10;
    else if (ch == ' ' && ignore)
      ;
    else {
     expect_prompt(1);
      debuglogs (4, "Invalid hex digit from remote system. (0x%x)", ch);
      error("Invalid hex digit from remote system. (0x%x)", ch);
    }
  }
}

/* get_hex_byte -- Get a byte from monitor and put it in *BYT. 
 *	Accept any number leading spaces.
 */
static void
get_hex_byte (byt)
     char *byt;
{
  int val;

  val = get_hex_digit (1) << 4;
  debuglogs (4, "get_hex_byte() -- Read first nibble 0x%x", val);
 
  val |= get_hex_digit (0);
  debuglogs (4, "get_hex_byte() -- Read second nibble 0x%x", val);
  *byt = val;
  
  debuglogs (4, "get_hex_byte() -- Read a 0x%x", val);
}

/* 
 * get_hex_word --  Get N 32-bit words from remote, each preceded by a space,
 *	and put them in registers starting at REGNO.
 */
static int
get_hex_word ()
{
  long val, newval;
  int i;

  val = 0;

#if 0
  if (HOST_BYTE_ORDER == BIG_ENDIAN) {
#endif
    for (i = 0; i < 8; i++)
      val = (val << 4) + get_hex_digit (i == 0);
#if 0
  } else {
    for (i = 7; i >= 0; i--)
      val = (val << 4) + get_hex_digit (i == 0);
  }
#endif

  debuglogs (4, "get_hex_word() got a 0x%x for a %s host.", val, (HOST_BYTE_ORDER == BIG_ENDIAN) ? "big endian" : "little endian");

  return val;
}

/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */
static void
array_create_inferior (execfile, args, env)
     char *execfile;
     char *args;
     char **env;
{
  int entry_pt;

  if (args && *args)
    error("Can't pass arguments to remote MONITOR process");

  if (execfile == 0 || exec_bfd == 0)
    error("No exec file specified");

  entry_pt = (int) bfd_get_start_address (exec_bfd);

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

  /* Let 'er rip... */
  proceed ((CORE_ADDR)entry_pt, TARGET_SIGNAL_DEFAULT, 0);
}

/*
 * array_open -- open a connection to a remote debugger.
 *	NAME is the filename used for communication.
 */
static int baudrate = 9600;
static char dev_name[100];

static void
array_open(args, name, from_tty)
     char *args;
     char *name;
     int from_tty;
{
  char packet[PBUFSIZ];

  if (args == NULL)
    error ("Use `target %s DEVICE-NAME' to use a serial port, or \n\
`target %s HOST-NAME:PORT-NUMBER' to use a network connection.", name, name);

/*  if (is_open) */
    array_close(0);

  target_preopen (from_tty);
  unpush_target (&array_ops);

  tmp_mips_processor_type = "lsi33k";	/* change the default from r3051 */
  mips_set_processor_type_command ("lsi33k", 0);

  strcpy(dev_name, args);
  array_desc = SERIAL_OPEN(dev_name);

  if (array_desc == NULL)
    perror_with_name(dev_name);

  if (baud_rate != -1) {
    if (SERIAL_SETBAUDRATE (array_desc, baud_rate)) {
      SERIAL_CLOSE (array_desc);
      perror_with_name (name);
    }
  }
  
  SERIAL_RAW(array_desc);

#if defined (LOG_FILE)
  log_file = fopen (LOG_FILE, "w");
  if (log_file == NULL)
    perror_with_name (LOG_FILE);
  fprintf_filtered (log_file, "GDB %s (%s", version);
  fprintf_filtered (log_file, " --target %s)\n", array_ops.to_shortname);
  fprintf_filtered (log_file, "Remote target %s connected to %s\n\n", array_ops.to_shortname, dev_name);
#endif

  /* see if the target is alive. For a ROM monitor, we can just try to force the
     expect_prompt to print a few times. For the GDB remote protocol, the application
     being debugged is sitting at a breakpoint and waiting for GDB to initialize
     the connection. We force it to give us an empty packet to see if it's alive.
     */
    debuglogs (3, "Trying to ACK the target's debug stub");
    /* unless your are on the new hardware, the old board won't initialize
       because the '@' doesn't flush output like it does on the new ROMS.
     */
    printf_monitor ("@");	/* ask for the last signal */
    expect_prompt(1);		/* See if we get a expect_prompt */
#ifdef TEST_ARRAY		/* skip packet for testing */
    make_gdb_packet (packet, "?");	/* ask for a bogus packet */
    if (array_send_packet (packet) == 0)
      error ("Couldn't transmit packet\n");
    printf_monitor ("@\n");	/* force it to flush stdout */
   expect_prompt(1);		/* See if we get a expect_prompt */
#endif
  push_target (&array_ops);
  if (from_tty)
    printf("Remote target %s connected to %s\n", array_ops.to_shortname, dev_name);
}

/*
 * array_close -- Close out all files and local state before this
 *	target loses control.
 */

static void
array_close (quitting)
     int quitting;
{
  SERIAL_CLOSE(array_desc);
  array_desc = NULL;

  debuglogs (1, "array_close (quitting=%d)", quitting);

#if defined (LOG_FILE)
  if (log_file) {
    if (ferror(log_file))
      printf_filtered ("Error writing log file.\n");
    if (fclose(log_file) != 0)
      printf_filtered ("Error closing log file.\n");
  }
#endif
}

/* 
 * array_detach -- terminate the open connection to the remote
 *	debugger. Use this when you want to detach and do something
 *	else with your gdb.
 */
static void
array_detach (from_tty)
     int from_tty;
{

  debuglogs (1, "array_detach ()");

  pop_target();		/* calls array_close to do the real work */
  if (from_tty)
    printf ("Ending remote %s debugging\n", target_shortname);
}

/*
 * array_attach -- attach GDB to the target.
 */
static void
array_attach (args, from_tty)
     char *args;
     int from_tty;
{
  if (from_tty)
    printf ("Starting remote %s debugging\n", target_shortname);
 
  debuglogs (1, "array_attach (args=%s)", args);
  
  printf_monitor ("go %x\n");
  /* swallow the echo.  */
  expect ("go %x\n", 1);
}
  
/*
 * array_resume -- Tell the remote machine to resume.
 */
static void
array_resume (pid, step, sig)
     int pid, step;
     enum target_signal sig;
{
  debuglogs (1, "array_resume (step=%d, sig=%d)", step, sig);

  if (step) {
    printf_monitor ("s\n");
  } else {
    printf_monitor ("go\n");
  }
}

#define TMPBUFSIZ 5

/*
 * array_wait -- Wait until the remote machine stops, then return,
 *          storing status in status just as `wait' would.
 */
static int
array_wait (pid, status)
     int pid;
     struct target_waitstatus *status;
{
  int old_timeout = timeout;
  int result, i;
  char c;
  serial_t tty_desc;
  serial_ttystate ttystate;

  debuglogs(1, "array_wait (), printing extraneous text.");
  
  status->kind = TARGET_WAITKIND_EXITED;
  status->value.integer = 0;

  timeout = 0;		/* Don't time out -- user program is running. */
 
#if !defined(__GO32__) && !defined(__MSDOS__) && !defined(__WIN32__)
  tty_desc = SERIAL_FDOPEN (0);
  ttystate = SERIAL_GET_TTY_STATE (tty_desc);
  SERIAL_RAW (tty_desc);

  i = 0;
  /* poll on the serial port and the keyboard. */
  while (1) {
    c = readchar(timeout);
    if (c > 0) {
      if (c == *(ARRAY_PROMPT + i)) {
	if (++i >= strlen (ARRAY_PROMPT)) { /* matched the prompt */
	  debuglogs (4, "array_wait(), got the expect_prompt.");
	  break;
	}
      } else {		/* not the prompt */
	i = 0;
      }
      fputc_unfiltered (c, gdb_stdout);
      fflush (stdout);
    }
    c = SERIAL_READCHAR(tty_desc, timeout);
    if (c > 0) {
      SERIAL_WRITE(array_desc, &c, 1);
      /* do this so it looks like there's keyboard echo */
      if (c == 3)		/* exit on Control-C */
	break;
#if 0
      fputc_unfiltered (c, gdb_stdout);
      fflush (stdout);
#endif
    }
  }
  SERIAL_SET_TTY_STATE (tty_desc, ttystate);
#else
  expect_prompt(1);
  debuglogs (4, "array_wait(), got the expect_prompt.");
#endif

  status->kind = TARGET_WAITKIND_STOPPED;
  status->value.sig = TARGET_SIGNAL_TRAP;

  timeout = old_timeout;

  return 0;
}

/*
 * array_fetch_registers -- read the remote registers into the
 *	block regs.
 */
static void
array_fetch_registers (ignored)
     int ignored;
{
  int regno, i;
  char *p;
  unsigned char packet[PBUFSIZ];
  char regs[REGISTER_BYTES];

  debuglogs (1, "array_fetch_registers (ignored=%d)\n", ignored);

  memset (packet, 0, PBUFSIZ);
  /* Unimplemented registers read as all bits zero.  */
  memset (regs, 0, REGISTER_BYTES);
  make_gdb_packet (packet, "g");
  if (array_send_packet (packet) == 0)
    error ("Couldn't transmit packet\n");
  if (array_get_packet (packet) == 0)
    error ("Couldn't receive packet\n");  
  /* FIXME: read bytes from packet */
  debuglogs (4, "array_fetch_registers: Got a \"%s\" back\n", packet);
  for (regno = 0; regno <= PC_REGNUM+4; regno++) {
    /* supply register stores in target byte order, so swap here */
    /* FIXME: convert from ASCII hex to raw bytes */
    i = ascii2hexword (packet + (regno * 8));
    debuglogs (5, "Adding register %d = %x\n", regno, i);
    SWAP_TARGET_AND_HOST (&i, 4);
    supply_register (regno, (char *)&i);
  }
}

/* 
 * This is unused by targets like this one that use a
 * protocol based on GDB's remote protocol.
 */
static void
array_fetch_register (ignored)
     int ignored;
{
  array_fetch_registers ();
}

/*
 * Get all the registers from the targets. They come back in a large array.
 */
static void
array_store_registers (ignored)
     int ignored;
{
  int regno;
  unsigned long i;
  char packet[PBUFSIZ];
  char buf[PBUFSIZ];
  char num[9];
  
  debuglogs (1, "array_store_registers()");

  memset (packet, 0, PBUFSIZ);
  memset (buf, 0, PBUFSIZ);
  buf[0] = 'G';

  /* Unimplemented registers read as all bits zero.  */
  /* FIXME: read bytes from packet */
  for (regno = 0; regno < 41; regno++) { /* FIXME */
    /* supply register stores in target byte order, so swap here */
    /* FIXME: convert from ASCII hex to raw bytes */
    i = (unsigned long)read_register (regno);
    hexword2ascii (num, i);
    strcpy (buf+(regno * 8)+1, num);
  }
  *(buf + (regno * 8) + 2) = 0;
  make_gdb_packet (packet, buf);
  if (array_send_packet (packet) == 0)
    error ("Couldn't transmit packet\n");
  if (array_get_packet (packet) == 0)
    error ("Couldn't receive packet\n");  
  
  registers_changed ();
}

/* 
 * This is unused by targets like this one that use a
 * protocol based on GDB's remote protocol.
 */
static void
array_store_register (ignored)
     int ignored;
{
  array_store_registers ();
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
array_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

static void
array_files_info ()
{
  printf ("\tAttached to %s at %d baud.\n",
	  dev_name, baudrate);
}

/*
 * array_write_inferior_memory -- Copy LEN bytes of data from debugger
 *	memory at MYADDR to inferior's memory at MEMADDR.  Returns length moved.
 */
static int
array_write_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     unsigned char *myaddr;
     int len;
{
  unsigned long i;
  int j;
  char packet[PBUFSIZ];
  char buf[PBUFSIZ];
  char num[9];
  char *p;
  
  debuglogs (1, "array_write_inferior_memory (memaddr=0x%x, myaddr=0x%x, len=%d)", memaddr, myaddr, len);
  memset (buf, '\0', PBUFSIZ);		/* this also sets the string terminator */
  p = buf;

  *p++ = 'M';				/* The command to write memory */
  hexword2ascii (num, memaddr);	/* convert the address */
  strcpy (p, num);			/* copy the address */
  p += 8;
  *p++ = ',';				/* add comma delimeter */
  hexword2ascii (num, len);		/* Get the length as a 4 digit number */
  *p++ = num[4];
  *p++ = num[5];
  *p++ = num[6];
  *p++ = num[7];
  *p++ = ':';				/* add the colon delimeter */
  for (j = 0; j < len; j++) {		/* copy the data in after converting it */
    *p++ = tohex ((myaddr[j] >> 4) & 0xf);
    *p++ = tohex  (myaddr[j] & 0xf);
  }
  
  make_gdb_packet (packet, buf);
  if (array_send_packet (packet) == 0)
    error ("Couldn't transmit packet\n");
  if (array_get_packet (packet) == 0)
    error ("Couldn't receive packet\n");  

  return len;
}

/*
 * array_read_inferior_memory -- read LEN bytes from inferior memory
 *	at MEMADDR.  Put the result at debugger address MYADDR.  Returns
 *	length moved.
 */
static int
array_read_inferior_memory(memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int j;
  char buf[20];
  char packet[PBUFSIZ];
  int count;			/* Number of bytes read so far.  */
  unsigned long startaddr;	/* Starting address of this pass.  */
  int len_this_pass;		/* Number of bytes to read in this pass.  */

  debuglogs (1, "array_read_inferior_memory (memaddr=0x%x, myaddr=0x%x, len=%d)", memaddr, myaddr, len);

  /* Note that this code works correctly if startaddr is just less
     than UINT_MAX (well, really CORE_ADDR_MAX if there was such a
     thing).  That is, something like
     array_read_bytes (CORE_ADDR_MAX - 4, foo, 4)
     works--it never adds len To memaddr and gets 0.  */
  /* However, something like
     array_read_bytes (CORE_ADDR_MAX - 3, foo, 4)
     doesn't need to work.  Detect it and give up if there's an attempt
     to do that.  */
  if (((memaddr - 1) + len) < memaddr) {
    errno = EIO;
    return 0;
  }
  
  for (count = 0, startaddr = memaddr; count < len; startaddr += len_this_pass)
    {
      /* Try to align to 16 byte boundry (why?) */
      len_this_pass = 16;
      if ((startaddr % 16) != 0)
	{
	  len_this_pass -= startaddr % 16;
	}
      /* Only transfer bytes we need */
      if (len_this_pass > (len - count))
	{
	  len_this_pass = (len - count);
	}
      /* Fetch the bytes */
      debuglogs (3, "read %d bytes from inferior address %x", len_this_pass,
		 startaddr);
      sprintf (buf, "m%08x,%04x", startaddr, len_this_pass);
      make_gdb_packet (packet, buf);
      if (array_send_packet (packet) == 0)
	{
	  error ("Couldn't transmit packet\n");
	}
      if (array_get_packet (packet) == 0)
	{
	  error ("Couldn't receive packet\n");  
	}
      if (*packet == 0)
	{
	  error ("Got no data in the GDB packet\n");
	}
      /* Pick packet apart and xfer bytes to myaddr */
      debuglogs (4, "array_read_inferior_memory: Got a \"%s\" back\n", packet);
      for (j = 0; j < len_this_pass ; j++)
	{
	  /* extract the byte values */
	  myaddr[count++] = from_hex (*(packet+(j*2))) * 16 + from_hex (*(packet+(j*2)+1));
	  debuglogs (5, "myaddr[%d] set to %x\n", count-1, myaddr[count-1]);
	}
    }
  return (count);
}

/* FIXME-someday!  merge these two.  */
static int
array_xfer_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;		/* ignored */
{
  if (write)
    return array_write_inferior_memory (memaddr, myaddr, len);
  else
    return array_read_inferior_memory (memaddr, myaddr, len);
}

static void
array_kill (args, from_tty)
     char *args;
     int from_tty;
{
  return;		/* ignore attempts to kill target system */
}

/* Clean up when a program exits.
   The program actually lives on in the remote processor's RAM, and may be
   run again without a download.  Don't leave it full of breakpoint
   instructions.  */

static void
array_mourn_inferior ()
{
  remove_breakpoints ();
  generic_mourn_inferior ();	/* Do all the proper things now */
}

#define MAX_ARRAY_BREAKPOINTS 16

extern int memory_breakpoint_size;
static CORE_ADDR breakaddr[MAX_ARRAY_BREAKPOINTS] = {0};

/*
 * array_insert_breakpoint -- add a breakpoint
 */
static int
array_insert_breakpoint (addr, shadow)
     CORE_ADDR addr;
     char *shadow;
{
  int i;

  debuglogs (1, "array_insert_breakpoint() addr = 0x%x", addr);

  for (i = 0; i <= MAX_ARRAY_BREAKPOINTS; i++) {
    if (breakaddr[i] == 0) {
      breakaddr[i] = addr;
      if (sr_get_debug() > 4)
	printf ("Breakpoint at %x\n", addr);
      array_read_inferior_memory(addr, shadow, memory_breakpoint_size);
      printf_monitor("b 0x%x\n", addr);
      expect_prompt(1);
      return 0;
    }
  }

  fprintf(stderr, "Too many breakpoints (> 16) for monitor\n");
  return 1;
}

/*
 * _remove_breakpoint -- Tell the monitor to remove a breakpoint
 */
static int
array_remove_breakpoint (addr, shadow)
     CORE_ADDR addr;
     char *shadow;
{
  int i;

  debuglogs (1, "array_remove_breakpoint() addr = 0x%x", addr);

  for (i = 0; i < MAX_ARRAY_BREAKPOINTS; i++) {
    if (breakaddr[i] == addr) {
      breakaddr[i] = 0;
      /* some monitors remove breakpoints based on the address */
      printf_monitor("bd %x\n", i);
      expect_prompt(1);
      return 0;
    }
  }
  fprintf(stderr, "Can't find breakpoint associated with 0x%x\n", addr);
  return 1;
}

static void
array_stop ()
{
  debuglogs (1, "array_stop()");
  printf_monitor("\003");
 expect_prompt(1);
}

/* 
 * array_command -- put a command string, in args, out to MONITOR.
 *	Output from MONITOR is placed on the users terminal until the
 *	expect_prompt is seen. FIXME
 */
static void
monitor_command (args, fromtty)
     char	*args;
     int	fromtty;
{
  debuglogs (1, "monitor_command (args=%s)", args);

  if (array_desc == NULL)
    error("monitor target not open.");

  if (!args)
    error("Missing command.");
	
  printf_monitor ("%s\n", args);
  expect_prompt(0);
}

/*
 * make_gdb_packet -- make a GDB packet. The data is always ASCII.
 *	 A debug packet whose contents are <data>
 *	 is encapsulated for transmission in the form:
 *
 *		$ <data> # CSUM1 CSUM2
 *
 *       <data> must be ASCII alphanumeric and cannot include characters
 *       '$' or '#'.  If <data> starts with two characters followed by
 *       ':', then the existing stubs interpret this as a sequence number.
 *
 *       CSUM1 and CSUM2 are ascii hex representation of an 8-bit 
 *       checksum of <data>, the most significant nibble is sent first.
 *       the hex digits 0-9,a-f are used.
 *
 */
static void
make_gdb_packet (buf, data)
     char *buf, *data;
{
  int i;
  unsigned char csum = 0;
  int cnt;
  char *p;

  debuglogs (3, "make_gdb_packet(%s)\n", data);
  cnt  = strlen (data);
  if (cnt > PBUFSIZ)
    error ("make_gdb_packet(): to much data\n");

  /* start with the packet header */
  p = buf;
  *p++ = '$';

  /* calculate the checksum */
  for (i = 0; i < cnt; i++) {
    csum += data[i];
    *p++ = data[i];
  }

  /* terminate the data with a '#' */
  *p++ = '#';
  
  /* add the checksum as two ascii digits */
  *p++ = tohex ((csum >> 4) & 0xf);
  *p++ = tohex (csum & 0xf);
  *p  = 0x0;			/* Null terminator on string */
}

/*
 * array_send_packet -- send a GDB packet to the target with error handling. We
 *		get a '+' (ACK) back if the packet is received and the checksum
 *		matches. Otherwise a '-' (NAK) is returned. It returns a 1 for a
 *		successful transmition, or a 0 for a failure.
 */
static int
array_send_packet (packet)
     char *packet;
{
  int c, retries, i;
  char junk[PBUFSIZ];

  retries = 0;

#if 0
  /* scan the packet to make sure it only contains valid characters.
     this may sound silly, but sometimes a garbled packet will hang
     the target board. We scan the whole thing, then print the error
     message.
     */
  for (i = 0; i < strlen(packet); i++) {
    debuglogs (5, "array_send_packet(): Scanning \'%c\'\n", packet[i]);
    /* legit hex numbers or command */
    if ((isxdigit(packet[i])) || (isalpha(packet[i])))
      continue;
    switch (packet[i]) {
    case '+':			/* ACK */
    case '-':			/* NAK */
    case '#':			/* end of packet */
    case '$':			/* start of packet */
      continue;
    default:			/* bogus character */
      retries++;
      debuglogs (4, "array_send_packet(): Found a non-ascii digit \'%c\' in the packet.\n", packet[i]);
    }
  }
#endif  

  if (retries > 0)
    error ("Can't send packet, found %d non-ascii characters", retries);

  /* ok, try to send the packet */
  retries = 0;
  while (retries++ <= 10) {
    printf_monitor ("%s", packet);
    
    /* read until either a timeout occurs (-2) or '+' is read */
    while (retries <= 10) {
      c = readchar (-timeout);
      debuglogs (3, "Reading a GDB protocol packet... Got a '%c'\n", c);
      switch (c) {
      case '+':
	debuglogs (3, "Got Ack\n");
	return 1;
      case SERIAL_TIMEOUT:
	debuglogs (3, "Timed out reading serial port\n");
	printf_monitor("@");		/* resync with the monitor */
       expect_prompt(1);		/* See if we get a expect_prompt */   
	break;            /* Retransmit buffer */
      case '-':
	debuglogs (3, "Got NAK\n");
	printf_monitor("@");		/* resync with the monitor */
       expect_prompt(1);		/* See if we get a expect_prompt */   
	break;
      case '$':
	/* it's probably an old response, or the echo of our command.
	 * just gobble up the packet and ignore it.
	 */
	debuglogs (3, "Got a junk packet\n");
	i = 0;
	do {
	  c = readchar (timeout);
	  junk[i++] = c;
	} while (c != '#');
	c = readchar (timeout);
	junk[i++] = c;
	c = readchar (timeout);
	junk[i++] = c;
	junk[i++] = '\0';
	debuglogs (3, "Reading a junk packet, got a \"%s\"\n", junk);
	continue;               /* Now, go look for next packet */
      default:
	continue;
      }
      retries++;
      debuglogs (3, "Retransmitting packet \"%s\"\n", packet);
      break;                /* Here to retransmit */
    }
  } /* outer while */
  return 0;
}

/*
 * array_get_packet -- get a GDB packet from the target. Basically we read till we
 *		see a '#', then check the checksum. It returns a 1 if it's gotten a
 *		packet, or a 0 it the packet wasn't transmitted correctly.
 */
static int
array_get_packet (packet)
     char *packet;
{
  int c;
  int retries;
  unsigned char csum;
  unsigned char pktcsum;
  char *bp;

  csum = 0;
  bp = packet;

  memset (packet, 1, PBUFSIZ);
  retries = 0;
  while (retries <= 10) {
    do {
      c = readchar (timeout);
      if (c == SERIAL_TIMEOUT) {
	debuglogs (3, "array_get_packet: got time out from serial port.\n");
      }
      debuglogs (3, "Waiting for a '$', got a %c\n", c);
    } while (c != '$');
    
    retries = 0;
    while (retries <= 10) {
      c = readchar (timeout);
      debuglogs (3, "array_get_packet: got a '%c'\n", c);
      switch (c) {
      case SERIAL_TIMEOUT:
	debuglogs (3, "Timeout in mid-packet, retrying\n");
	return 0;
      case '$':
	debuglogs (3, "Saw new packet start in middle of old one\n");
	return 0;             /* Start a new packet, count retries */
      case '#':	
	*bp = '\0';
	pktcsum = from_hex (readchar (timeout)) << 4;
	pktcsum |= from_hex (readchar (timeout));
	if (csum == 0)
	  debuglogs (3, "\nGDB packet checksum zero, must be a bogus packet\n");
	if (csum == pktcsum) {
	  debuglogs (3, "\nGDB packet checksum correct, packet data is \"%s\",\n", packet);
	  printf_monitor ("@");
	 expect_prompt (1);
	  return 1;
	}
	debuglogs (3, "Bad checksum, sentsum=0x%x, csum=0x%x\n", pktcsum, csum);
	return 0;
      case '*':               /* Run length encoding */
	debuglogs (5, "Run length encoding in packet\n");
	csum += c;
	c = readchar (timeout);
	csum += c;
	c = c - ' ' + 3;      /* Compute repeat count */
	
	if (c > 0 && c < 255 && bp + c - 1 < packet + PBUFSIZ - 1) {
	  memset (bp, *(bp - 1), c);
	  bp += c;
	  continue;
	}
	*bp = '\0';
	printf_filtered ("Repeat count %d too large for buffer.\n", c);
	return 0;
	
      default:
	if ((!isxdigit(c)) && (!ispunct(c)))
	  debuglogs (4, "Got a non-ascii digit \'%c\'.\\n", c);
	if (bp < packet + PBUFSIZ - 1) {
	  *bp++ = c;
	  csum += c;
	  continue;
	}
	
	*bp = '\0';
	puts_filtered ("Remote packet too long.\n");
	return 0;
      }
    }
  }
}

/*
 * ascii2hexword -- convert an ascii number represented by 8 digits to a hex value.
 */
static unsigned long
ascii2hexword (mem)
     unsigned char *mem;
{
  unsigned long val;
  int i;
  char buf[9];

  val = 0;
  for (i = 0; i < 8; i++) {
    val <<= 4;
    if (mem[i] >= 'A' && mem[i] <= 'F')
      val = val + mem[i] - 'A' + 10;      
    if (mem[i] >= 'a' && mem[i] <= 'f')
      val = val + mem[i] - 'a' + 10;
    if (mem[i] >= '0' && mem[i] <= '9')
      val = val + mem[i] - '0';
    buf[i] = mem[i];
  }
  buf[8] = '\0';
  debuglogs (4, "ascii2hexword() got a 0x%x from %s(%x).\n", val, buf, mem);
  return val;
}

/*
 * ascii2hexword -- convert a hex value to an ascii number represented by 8
 *	digits.
 */
static char*
hexword2ascii (mem, num)
     unsigned char *mem;
     unsigned long num;
{
  int i;
  unsigned char ch;
  
  debuglogs (4, "hexword2ascii() converting %x ", num);
  for (i = 7; i >= 0; i--) {    
    mem[i] = tohex ((num >> 4) & 0xf);
    mem[i] = tohex (num & 0xf);
    num = num >> 4;
  }
  mem[8] = '\0';
  debuglogs (4, "\tto a %s", mem);
}

/* Convert hex digit A to a number.  */
static int
from_hex (a)
     int a;
{  
  if (a == 0)
    return 0;

  debuglogs (4, "from_hex got a 0x%x(%c)\n",a,a);
  if (a >= '0' && a <= '9')
    return a - '0';
  if (a >= 'a' && a <= 'f')
    return a - 'a' + 10;
  if (a >= 'A' && a <= 'F')
    return a - 'A' + 10;
  else {
    error ("Reply contains invalid hex digit 0x%x", a);
  }
}

/* Convert number NIB to a hex digit.  */
static int
tohex (nib)
     int nib;
{
  if (nib < 10)
    return '0'+nib;
  else
    return 'a'+nib-10;
}

/*
 * _initialize_remote_monitors -- setup a few addtitional commands that
 *		are usually only used by monitors.
 */
void
_initialize_remote_monitors ()
{
  /* generic monitor command */
  add_com ("monitor", class_obscure, monitor_command,
	   "Send a command to the debug monitor."); 

}

/*
 * _initialize_array -- do any special init stuff for the target.
 */
void
_initialize_array ()
{
  add_target (&array_ops);
}
