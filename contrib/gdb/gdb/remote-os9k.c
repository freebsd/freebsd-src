/* Remote debugging interface for boot monitors, for GDB.
   Copyright 1990, 1991, 1992, 1993 Free Software Foundation, Inc.

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

/* This file was derived from remote-eb.c, which did a similar job, but for
   an AMD-29K running EBMON.  That file was in turn derived from remote.c
   as mentioned in the following comment (left in for comic relief):

  "This is like remote.c but is for a different situation--
   having a PC running os9000 hook up with a unix machine with
   a serial line, and running ctty com2 on the PC. os9000 has a debug
   monitor called ROMBUG running.  Not to mention that the PC
   has PC/NFS, so it can access the same executables that gdb can,
   over the net in real time."

   In reality, this module talks to a debug monitor called 'ROMBUG', which
   We communicate with ROMBUG via a direct serial line, the network version
   of ROMBUG is not available yet.
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
#include "gdb_string.h"
#include <sys/types.h>
#include "command.h"
#include "serial.h"
#include "monitor.h"
#include "remote-utils.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb-stabs.h"

struct monitor_ops *current_monitor;
struct cmd_list_element *showlist;
extern struct target_ops rombug_ops;		/* Forward declaration */
extern struct monitor_ops rombug_cmds;		/* Forward declaration */
extern struct cmd_list_element *setlist;
extern struct cmd_list_element *unsetlist;
extern int attach_flag;

static void rombug_close();
static void rombug_fetch_register();
static void rombug_fetch_registers();
static void rombug_store_register();
#if 0
static int sr_get_debug();			/* flag set by "set remotedebug" */
#endif
static int hashmark;				/* flag set by "set hash" */
static int rombug_is_open = 0;

/* FIXME: Replace with sr_get_debug ().  */
#define LOG_FILE "monitor.log"
FILE *log_file;
static int monitor_log = 0;
static int tty_xon = 0;
static int tty_xoff = 0;

static int timeout = 10;
static int is_trace_mode = 0;
/* Descriptor for I/O to remote machine.  Initialize it to NULL*/
static serial_t monitor_desc = NULL;

static CORE_ADDR bufaddr = 0;
static int buflen = 0;
static char readbuf[16];

/* Send data to monitor.  Works just like printf. */
static void
#ifdef ANSI_PROTOTYPES
printf_monitor(char *pattern, ...)
#else
printf_monitor(va_alist)
     va_dcl
#endif
{
  va_list args;
  char buf[200];
  int i;

#ifdef ANSI_PROTOTYPES
  va_start (args, pattern);
#else
  char *pattern;
  va_start(args);
  pattern = va_arg(args, char *);
#endif

  vsprintf(buf, pattern, args);
  va_end(args);

  if (SERIAL_WRITE(monitor_desc, buf, strlen(buf)))
    fprintf(stderr, "SERIAL_WRITE failed: %s\n", safe_strerror(errno));
}

/* Read a character from the remote system, doing all the fancy timeout stuff*/
static int
readchar(timeout)
     int timeout;
{
  int c;

  c = SERIAL_READCHAR(monitor_desc, timeout);

  if (sr_get_debug())
    putchar(c & 0x7f);

  if (monitor_log && isascii(c))
    putc(c & 0x7f, log_file);

  if (c >= 0)
    return c & 0x7f;

  if (c == SERIAL_TIMEOUT)
    {
      if (timeout == 0)
	return c;		/* Polls shouldn't generate timeout errors */

      error("Timeout reading from remote system.");
    }

  perror_with_name("remote-monitor");
}

/* Scan input from the remote system, until STRING is found.  If DISCARD is
   non-zero, then discard non-matching input, else print it out.
   Let the user break out immediately.  */
static void
expect(string, discard)
     char *string;
     int discard;
{
  char *p = string;
  int c;

  if (sr_get_debug())
    printf ("Expecting \"%s\"\n", string);

  immediate_quit = 1;
  while (1)
    {
      c = readchar(timeout);
      if (!isascii (c))
	continue;
      if (c == *p++)
	{
	  if (*p == '\0')
	    {
	      immediate_quit = 0;
	      if (sr_get_debug())
		printf ("\nMatched\n");
	      return;
	    }
	}
      else
	{
	  if (!discard)
	    {
	      fwrite(string, 1, (p - 1) - string, stdout);
	      putchar((char)c);
	      fflush(stdout);
	    }
	  p = string;
	}
    }
}

/* Keep discarding input until we see the ROMBUG prompt.

   The convention for dealing with the prompt is that you
   o give your command
   o *then* wait for the prompt.

   Thus the last thing that a procedure does with the serial line
   will be an expect_prompt().  Exception:  rombug_resume does not
   wait for the prompt, because the terminal is being handed over
   to the inferior.  However, the next thing which happens after that
   is a rombug_wait which does wait for the prompt.
   Note that this includes abnormal exit, e.g. error().  This is
   necessary to prevent getting into states from which we can't
   recover.  */
static void
expect_prompt(discard)
     int discard;
{
  if (monitor_log)
  /* This is a convenient place to do this.  The idea is to do it often
     enough that we never lose much data if we terminate abnormally.  */
    fflush(log_file);

  if (is_trace_mode) {
    expect("trace", discard);
  } else {
    expect (PROMPT, discard);
  }
}

/* Get a hex digit from the remote system & return its value.
   If ignore_space is nonzero, ignore spaces (not newline, tab, etc).  */
static int
get_hex_digit(ignore_space)
     int ignore_space;
{
  int ch;
  while (1)
    {
      ch = readchar(timeout);
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
	  expect_prompt(1);
	  error("Invalid hex digit from remote system.");
	}
    }
}

/* Get a byte from monitor and put it in *BYT.  Accept any number
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
  unsigned char b;

  for (i = 0; i < n; i++)
    {
      int j;
      
      val = 0;
      for (j = 0; j < 4; j++)
	{
	  get_hex_byte (&b);
	  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
	    val = (val << 8) + b;
	  else
	    val = val + (b << (j*8));
	}
      supply_register (regno++, (char *) &val);
    }
}

/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */
static void
rombug_create_inferior (execfile, args, env)
     char *execfile;
     char *args;
     char **env;
{
  int entry_pt;

  if (args && *args)
    error("Can't pass arguments to remote ROMBUG process");

  if (execfile == 0 || exec_bfd == 0)
    error("No exec file specified");

  entry_pt = (int) bfd_get_start_address (exec_bfd);

  if (monitor_log)
    fputs ("\nIn Create_inferior()", log_file);


/* The "process" (board) is already stopped awaiting our commands, and
   the program is already downloaded.  We just set its PC and go.  */

  init_wait_for_inferior ();
  proceed ((CORE_ADDR)entry_pt, TARGET_SIGNAL_DEFAULT, 0);
}

/* Open a connection to a remote debugger.
   NAME is the filename used for communication.  */

static char dev_name[100];

static void
rombug_open(args, from_tty)
     char *args;
     int from_tty;
{
  if (args == NULL)
    error ("Use `target RomBug DEVICE-NAME' to use a serial port, or \n\
`target RomBug HOST-NAME:PORT-NUMBER' to use a network connection.");

  target_preopen(from_tty);

  if (rombug_is_open)
    unpush_target(&rombug_ops);

  strcpy(dev_name, args);
  monitor_desc = SERIAL_OPEN(dev_name);
  if (monitor_desc == NULL)
    perror_with_name(dev_name);

  /* if baud rate is set by 'set remotebaud' */
  if (SERIAL_SETBAUDRATE (monitor_desc, sr_get_baud_rate()))
    {
      SERIAL_CLOSE (monitor_desc);
      perror_with_name ("RomBug");
    }
  SERIAL_RAW(monitor_desc);
  if (tty_xon || tty_xoff)
    {
    struct hardware_ttystate { struct termios t;} *tty_s;

      tty_s =(struct hardware_ttystate  *)SERIAL_GET_TTY_STATE(monitor_desc);
      if (tty_xon) tty_s->t.c_iflag |= IXON; 
      if (tty_xoff) tty_s->t.c_iflag |= IXOFF;
      SERIAL_SET_TTY_STATE(monitor_desc, (serial_ttystate) tty_s);
    }

  rombug_is_open = 1;

  log_file = fopen (LOG_FILE, "w");
  if (log_file == NULL)
    perror_with_name (LOG_FILE);

  push_monitor (&rombug_cmds);
  printf_monitor("\r");	/* CR wakes up monitor */
  expect_prompt(1);
  push_target (&rombug_ops);
  attach_flag = 1;

  if (from_tty)
    printf("Remote %s connected to %s\n", target_shortname,
	   dev_name);

  rombug_fetch_registers();

  printf_monitor ("ov e \r");
  expect_prompt(1);
  bufaddr = 0;
  buflen = 0;
}

/*
 * Close out all files and local state before this target loses control.
 */

static void
rombug_close (quitting)
     int quitting;
{
  if (rombug_is_open) {
    SERIAL_CLOSE(monitor_desc);
    monitor_desc = NULL;
    rombug_is_open = 0;
  }

  if (log_file) {
    if (ferror(log_file))
      fprintf(stderr, "Error writing log file.\n");
    if (fclose(log_file) != 0)
      fprintf(stderr, "Error closing log file.\n");
    log_file = 0;
  }
}

int
rombug_link(mod_name, text_reloc)
     char *mod_name;
     CORE_ADDR *text_reloc;
{
  int i, j;
  unsigned long val;
  unsigned char b;

  printf_monitor("l %s \r", mod_name); 
  expect_prompt(1);
  printf_monitor(".r \r");
  expect(REG_DELIM, 1);
  for (i=0; i <= 7; i++)
    {
      val = 0;
      for (j = 0; j < 4; j++)
        {
          get_hex_byte(&b);
          val = (val << 8) + b;
	}
    }
  expect_prompt(1);
  *text_reloc = val;
  return 1;
}

/* Terminate the open connection to the remote debugger.
   Use this when you want to detach and do something else
   with your gdb.  */
static void
rombug_detach (from_tty)
     int from_tty;
{
  if (attach_flag) {
    printf_monitor (GO_CMD);
    attach_flag = 0;
  }
  pop_target();		/* calls rombug_close to do the real work */
  if (from_tty)
    printf ("Ending remote %s debugging\n", target_shortname);
}
 
/*
 * Tell the remote machine to resume.
 */
static void
rombug_resume (pid, step, sig)
     int pid, step;
     enum target_signal sig;
{
  if (monitor_log)
    fprintf (log_file, "\nIn Resume (step=%d, sig=%d)\n", step, sig);

  if (step)
    {
      is_trace_mode = 1;
      printf_monitor (STEP_CMD);
      /* wait for the echo.  **
      expect (STEP_CMD, 1);
      */
    }
  else
    {
      printf_monitor (GO_CMD);
      /* swallow the echo.  **
      expect (GO_CMD, 1);
      */
    }
  bufaddr = 0;
  buflen= 0;
}

/*
 * Wait until the remote machine stops, then return,
 * storing status in status just as `wait' would.
 */

static int
rombug_wait (pid, status)
     int pid;
     struct target_waitstatus *status;
{
  int old_timeout = timeout;
  struct section_offsets *offs;
  CORE_ADDR addr, pc;
  struct obj_section *obj_sec;

  if (monitor_log)
    fputs ("\nIn wait ()", log_file);

  status->kind = TARGET_WAITKIND_EXITED;
  status->value.integer = 0;

  timeout = -1;		/* Don't time out -- user program is running. */
  expect ("eax:", 0);   /* output any message before register display */
  expect_prompt(1);     /* Wait for prompt, outputting extraneous text */

  status->kind = TARGET_WAITKIND_STOPPED;
  status->value.sig = TARGET_SIGNAL_TRAP;
  timeout = old_timeout;
  rombug_fetch_registers();
  bufaddr = 0;
  buflen = 0;
  pc = read_register(PC_REGNUM);
  addr = read_register(DATABASE_REG);
  obj_sec = find_pc_section (pc);
  if (obj_sec != NULL)
    {
      if (obj_sec->objfile != symfile_objfile)
        new_symfile_objfile(obj_sec->objfile, 1, 0);
      offs = ((struct section_offsets *)
	 alloca (sizeof (struct section_offsets)
	 + (symfile_objfile->num_sections * sizeof (offs->offsets))));
      memcpy (offs, symfile_objfile->section_offsets,
         (sizeof (struct section_offsets) + 
	 (symfile_objfile->num_sections * sizeof (offs->offsets))));
      ANOFFSET (offs, SECT_OFF_DATA) = addr;
      ANOFFSET (offs, SECT_OFF_BSS) = addr;

      objfile_relocate(symfile_objfile, offs);
    }

  return 0;
}

/* Return the name of register number regno in the form input and output by
   monitor.  Currently, register_names just happens to contain exactly what
   monitor wants.  Lets take advantage of that just as long as possible! */

static char *
get_reg_name (regno)
     int regno;
{
  static char buf[50];
  char *p;
  char *b;

  b = buf;

  if (regno < 0)
    return ("");
/*
  for (p = reg_names[regno]; *p; p++)
    *b++ = toupper(*p);
  *b = '\000';
*/
  p = (char *)reg_names[regno];
  return p;
/*
  return buf;
*/
}

/* read the remote registers into the block regs.  */

static void
rombug_fetch_registers ()
{
  int regno, j, i;
  long val;
  unsigned char b;

  printf_monitor (GET_REG);
  expect("eax:", 1);
  expect("\n", 1);
  get_hex_regs(1, 0);
  get_hex_regs(1, 3);
  get_hex_regs(1, 1);
  get_hex_regs(1, 2);
  get_hex_regs(1, 6);
  get_hex_regs(1, 7);
  get_hex_regs(1, 5);
  get_hex_regs(1, 4);
  for (regno = 8; regno <= 15; regno++)
    {
      expect(REG_DELIM, 1);
      if (regno >= 8 && regno <= 13)
	{
	  val = 0;
	  for (j = 0; j < 2; j++)
            {
              get_hex_byte (&b);
	      if (TARGET_BYTE_ORDER == BIG_ENDIAN)
		val = (val << 8) + b;
	      else
		val = val + (b << (j*8));
            }

	  if (regno == 8) i = 10;
	  if (regno >=  9 && regno <= 12) i = regno + 3;
	  if (regno == 13) i = 11;
	  supply_register (i, (char *) &val);
	}
      else if (regno == 14)
	{
	  get_hex_regs(1, PC_REGNUM);
	}
      else if (regno == 15)
	{
	  get_hex_regs(1, 9);
	}
      else
	{
	  val = 0;
	  supply_register(regno, (char *) &val);
	}
    }
  is_trace_mode = 0;
  expect_prompt (1);
}

/* Fetch register REGNO, or all registers if REGNO is -1.
   Returns errno value.  */
static void
rombug_fetch_register (regno)
     int regno;
{
  int val, j;
  unsigned char b;

  if (monitor_log) {
    fprintf (log_file, "\nIn Fetch Register (reg=%s)\n", get_reg_name (regno));
    fflush (log_file);
  }

  if (regno < 0)
    {
      rombug_fetch_registers ();
    }
  else
    {
      char *name = get_reg_name (regno);
      printf_monitor (GET_REG);
      if (regno >= 10 && regno <= 15)
	{
	  expect ("\n", 1);
	  expect ("\n", 1);
          expect (name, 1);
          expect (REG_DELIM, 1);
	  val = 0;
	  for (j = 0; j < 2; j++)
            {
              get_hex_byte (&b);
	      if (TARGET_BYTE_ORDER == BIG_ENDIAN)
		val = (val << 8) + b;
	      else
		val = val + (b << (j*8));
            }
	  supply_register (regno, (char *) &val);
	}
      else if (regno == 8 || regno == 9)
	{
	  expect ("\n", 1);
	  expect ("\n", 1);
	  expect ("\n", 1);
          expect (name, 1);
          expect (REG_DELIM, 1);
	  get_hex_regs (1, regno);
	}
      else
	{
          expect (name, 1);
          expect (REG_DELIM, 1);
	  expect("\n", 1);
	  get_hex_regs(1, 0);
	  get_hex_regs(1, 3);
	  get_hex_regs(1, 1);
	  get_hex_regs(1, 2);
	  get_hex_regs(1, 6);
	  get_hex_regs(1, 7);
	  get_hex_regs(1, 5);
	  get_hex_regs(1, 4);
	}
      expect_prompt (1);
    }
  return;
}

/* Store the remote registers from the contents of the block REGS.  */

static void
rombug_store_registers ()
{
  int regno;

  for (regno = 0; regno <= PC_REGNUM; regno++)
    rombug_store_register(regno);

  registers_changed ();
}

/* Store register REGNO, or all if REGNO == 0.
   return errno value.  */
static void
rombug_store_register (regno)
     int regno;
{
char *name;

  if (monitor_log)
    fprintf (log_file, "\nIn Store_register (regno=%d)\n", regno);

  if (regno == -1)
    rombug_store_registers ();
  else
    {
      if (sr_get_debug())
	printf ("Setting register %s to 0x%x\n", get_reg_name (regno), read_register (regno));

      name = get_reg_name(regno);
      if (name == 0) return;
      printf_monitor (SET_REG, name, read_register (regno));

      is_trace_mode = 0;
      expect_prompt (1);
    }
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
rombug_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

static void
rombug_files_info ()
{
  printf ("\tAttached to %s at %d baud.\n",
	  dev_name, sr_get_baud_rate());
}

/* Copy LEN bytes of data from debugger memory at MYADDR
   to inferior's memory at MEMADDR.  Returns length moved.  */
static int
rombug_write_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     unsigned char *myaddr;
     int len;
{
  int i;
  char buf[10];

  if (monitor_log)
    fprintf (log_file, "\nIn Write_inferior_memory (memaddr=%x, len=%d)\n", memaddr, len);

  printf_monitor (MEM_SET_CMD, memaddr);
  for (i = 0; i < len; i++)
    {
      expect (CMD_DELIM, 1);
      printf_monitor ("%x \r", myaddr[i]);
      if (sr_get_debug())
	printf ("\nSet 0x%x to 0x%x\n", memaddr + i, myaddr[i]);
    }
  expect (CMD_DELIM, 1);
  if (CMD_END)
    printf_monitor (CMD_END);
  is_trace_mode = 0;
  expect_prompt (1);

  bufaddr = 0;
  buflen = 0;
  return len;
}

/* Read LEN bytes from inferior memory at MEMADDR.  Put the result
   at debugger address MYADDR.  Returns length moved.  */
static int
rombug_read_inferior_memory(memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int i, j;

  /* Number of bytes read so far.  */
  int count;

  /* Starting address of this pass.  */
  unsigned long startaddr;

  /* Number of bytes to read in this pass.  */
  int len_this_pass;

  if (monitor_log)
    fprintf (log_file, "\nIn Read_inferior_memory (memaddr=%x, len=%d)\n", memaddr, len);

  /* Note that this code works correctly if startaddr is just less
     than UINT_MAX (well, really CORE_ADDR_MAX if there was such a
     thing).  That is, something like
     rombug_read_bytes (CORE_ADDR_MAX - 4, foo, 4)
     works--it never adds len To memaddr and gets 0.  */
  /* However, something like
     rombug_read_bytes (CORE_ADDR_MAX - 3, foo, 4)
     doesn't need to work.  Detect it and give up if there's an attempt
     to do that.  */
  if (((memaddr - 1) + len) < memaddr) {
    errno = EIO;
    return 0;
  }
  if (bufaddr <= memaddr && (memaddr+len) <= (bufaddr+buflen))
    {
      memcpy(myaddr, &readbuf[memaddr-bufaddr], len);
      return len;
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
      if (sr_get_debug())
	printf ("\nDisplay %d bytes at %x\n", len_this_pass, startaddr);

      printf_monitor (MEM_DIS_CMD, startaddr, 8);
      expect ("- ", 1);
      for (i = 0; i < 16; i++)
	{
	  get_hex_byte (&readbuf[i]);
	}
      bufaddr = startaddr;
      buflen = 16;
      memcpy(&myaddr[count], readbuf, len_this_pass); 
      count += len_this_pass;
      startaddr += len_this_pass;
      expect(CMD_DELIM, 1);
    }
  if (CMD_END) 
      printf_monitor (CMD_END);
  is_trace_mode = 0;
  expect_prompt (1);

  return len;
}

/* FIXME-someday!  merge these two.  */
static int
rombug_xfer_inferior_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;		/* ignored */
{
  if (write)
    return rombug_write_inferior_memory (memaddr, myaddr, len);
  else
    return rombug_read_inferior_memory (memaddr, myaddr, len);
}

static void
rombug_kill (args, from_tty)
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
rombug_mourn_inferior ()
{
  remove_breakpoints ();
  generic_mourn_inferior ();	/* Do all the proper things now */
}

#define MAX_MONITOR_BREAKPOINTS 16

extern int memory_breakpoint_size;
static CORE_ADDR breakaddr[MAX_MONITOR_BREAKPOINTS] = {0};

static int
rombug_insert_breakpoint (addr, shadow)
     CORE_ADDR addr;
     char *shadow;
{
  int i;

  if (monitor_log)
    fprintf (log_file, "\nIn Insert_breakpoint (addr=%x)\n", addr);

  for (i = 0; i <= MAX_MONITOR_BREAKPOINTS; i++)
    if (breakaddr[i] == 0)
      {
	breakaddr[i] = addr;
	if (sr_get_debug())
	  printf ("Breakpoint at %x\n", addr);
	rombug_read_inferior_memory(addr, shadow, memory_breakpoint_size);
	printf_monitor(SET_BREAK_CMD, addr);
	is_trace_mode = 0;
	expect_prompt(1);
	return 0;
      }

  fprintf(stderr, "Too many breakpoints (> 16) for monitor\n");
  return 1;
}

/*
 * _remove_breakpoint -- Tell the monitor to remove a breakpoint
 */
static int
rombug_remove_breakpoint (addr, shadow)
     CORE_ADDR addr;
     char *shadow;
{
  int i;

  if (monitor_log)
    fprintf (log_file, "\nIn Remove_breakpoint (addr=%x)\n", addr);

  for (i = 0; i < MAX_MONITOR_BREAKPOINTS; i++)
    if (breakaddr[i] == addr)
      {
	breakaddr[i] = 0;
	printf_monitor(CLR_BREAK_CMD, addr);
	is_trace_mode = 0;
	expect_prompt(1);
	return 0;
      }

  fprintf(stderr, "Can't find breakpoint associated with 0x%x\n", addr);
  return 1;
}

/* Load a file. This is usually an srecord, which is ascii. No 
   protocol, just sent line by line. */

#define DOWNLOAD_LINE_SIZE 100
static void
rombug_load (arg)
    char	*arg;
{
/* this part comment out for os9* */
#if 0
  FILE *download;
  char buf[DOWNLOAD_LINE_SIZE];
  int i, bytes_read;

  if (sr_get_debug())
    printf ("Loading %s to monitor\n", arg);

  download = fopen (arg, "r");
  if (download == NULL)
    {
    error (sprintf (buf, "%s Does not exist", arg));
    return;
  }

  printf_monitor (LOAD_CMD);
/*  expect ("Waiting for S-records from host... ", 1); */

  while (!feof (download))
    {
      bytes_read = fread (buf, sizeof (char), DOWNLOAD_LINE_SIZE, download);
      if (hashmark)
	{
	  putchar ('.');
	  fflush (stdout);
	}

      if (SERIAL_WRITE(monitor_desc, buf, bytes_read)) {
	fprintf(stderr, "SERIAL_WRITE failed: (while downloading) %s\n", safe_strerror(errno));
	break;
      }
      i = 0;
      while (i++ <=200000) {} ;     			/* Ugly HACK, probably needs flow control */
      if (bytes_read < DOWNLOAD_LINE_SIZE)
	{
	  if (!feof (download))
	    error ("Only read %d bytes\n", bytes_read);
	  break;
	}
    }

  if (hashmark)
    {
      putchar ('\n');
    }
  if (!feof (download))
    error ("Never got EOF while downloading");
  fclose (download);
#endif 0
}

/* Put a command string, in args, out to MONITOR.  
   Output from MONITOR is placed on the users terminal until the prompt 
   is seen. */

static void
rombug_command (args, fromtty)
     char	*args;
     int	fromtty;
{
  if (monitor_desc == NULL)
    error("monitor target not open.");
  
  if (monitor_log)
    fprintf (log_file, "\nIn command (args=%s)\n", args);

  if (!args)
    error("Missing command.");
	
  printf_monitor("%s\r", args);
  expect_prompt(0);
}

#if 0
/* Connect the user directly to MONITOR.  This command acts just like the
   'cu' or 'tip' command.  Use <CR>~. or <CR>~^D to break out.  */

static struct ttystate ttystate;

static void
cleanup_tty()
{  printf("\r\n[Exiting connect mode]\r\n");
  /*SERIAL_RESTORE(0, &ttystate);*/
}

static void
connect_command (args, fromtty)
     char	*args;
     int	fromtty;
{
  fd_set readfds;
  int numfds;
  int c;
  char cur_esc = 0;

  dont_repeat();

  if (monitor_desc == NULL)
    error("monitor target not open.");
  
  if (args)
    fprintf("This command takes no args.  They have been ignored.\n");
	
  printf("[Entering connect mode.  Use ~. or ~^D to escape]\n");

  serial_raw(0, &ttystate);

  make_cleanup(cleanup_tty, 0);

  FD_ZERO(&readfds);

  while (1)
    {
      do
	{
	  FD_SET(0, &readfds);
	  FD_SET(monitor_desc, &readfds);
	  numfds = select(sizeof(readfds)*8, &readfds, 0, 0, 0);
	}
      while (numfds == 0);

      if (numfds < 0)
	perror_with_name("select");

      if (FD_ISSET(0, &readfds))
	{			/* tty input, send to monitor */
	  c = getchar();
	  if (c < 0)
	    perror_with_name("connect");

	  printf_monitor("%c", c);
	  switch (cur_esc)
	    {
	    case 0:
	      if (c == '\r')
		cur_esc = c;
	      break;
	    case '\r':
	      if (c == '~')
		cur_esc = c;
	      else
		cur_esc = 0;
	      break;
	    case '~':
	      if (c == '.' || c == '\004')
		return;
	      else
		cur_esc = 0;
	    }
	}

      if (FD_ISSET(monitor_desc, &readfds))
	{
	  while (1)
	    {
	      c = readchar(0);
	      if (c < 0)
		break;
	      putchar(c);
	    }
	  fflush(stdout);
	}
    }
}
#endif

/*
 * Define the monitor command strings. Since these are passed directly
 * through to a printf style function, we need can include formatting
 * strings. We also need a CR or LF on the end.
 */
struct monitor_ops rombug_cmds = {
  "g \r",				/* execute or usually GO command */
  "g \r",				/* continue command */
  "t \r",				/* single step */
  "b %x\r",				/* set a breakpoint */
  "k %x\r",				/* clear a breakpoint */
  "c %x\r",				/* set memory to a value */
  "d %x %d\r",				/* display memory */
  "$%08X",				/* prompt memory commands use */
  ".%s %x\r",				/* set a register */
  ":",					/* delimiter between registers */
  ". \r",				/* read a register */
  "mf \r",				/* download command */
  "RomBug: ",				/* monitor command prompt */
  ": ",					/* end-of-command delimitor */
  ".\r"					/* optional command terminator */
};

struct target_ops rombug_ops = {
  "rombug",
  "Microware's ROMBUG debug monitor",
  "Use a remote computer running the ROMBUG debug monitor.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).",
  rombug_open,
  rombug_close, 
  0,
  rombug_detach,
  rombug_resume,
  rombug_wait,
  rombug_fetch_register,
  rombug_store_register,
  rombug_prepare_to_store,
  rombug_xfer_inferior_memory,
  rombug_files_info,
  rombug_insert_breakpoint,
  rombug_remove_breakpoint,	/* Breakpoints */
  0,
  0,
  0,
  0,
  0,				/* Terminal handling */
  rombug_kill,
  rombug_load,			/* load */
  rombug_link,				/* lookup_symbol */
  rombug_create_inferior,
  rombug_mourn_inferior,
  0,				/* can_run */
  0, 				/* notice_signals */
  0,				/* to_stop */
  process_stratum,
  0,				/* next */
  1,
  1,
  1,
  1,
  1,				/* has execution */
  0,
  0,				/* Section pointers */
  OPS_MAGIC,			/* Always the last thing */
};

void
_initialize_remote_os9k ()
{
  add_target (&rombug_ops);

  add_show_from_set (
        add_set_cmd ("hash", no_class, var_boolean, (char *)&hashmark,
		"Set display of activity while downloading a file.\nWhen enabled, a period \'.\' is displayed.",
                &setlist),
	&showlist);

  add_show_from_set (
        add_set_cmd ("timeout", no_class, var_zinteger,
                 (char *) &timeout,
                 "Set timeout in seconds for remote MIPS serial I/O.",
                 &setlist),
        &showlist);

  add_show_from_set (
        add_set_cmd ("remotelog", no_class, var_zinteger,
                 (char *) &monitor_log,
                 "Set monitor activity log on(=1) or off(=0).",
                 &setlist),
        &showlist);

  add_show_from_set (
        add_set_cmd ("remotexon", no_class, var_zinteger,
                 (char *) &tty_xon,
                 "Set remote tty line XON control",
                 &setlist),
        &showlist);

  add_show_from_set (
        add_set_cmd ("remotexoff", no_class, var_zinteger,
                 (char *) &tty_xoff,
                 "Set remote tty line XOFF control",
                 &setlist),
        &showlist);

  add_com ("rombug <command>", class_obscure, rombug_command,
	   "Send a command to the debug monitor."); 
#if 0
  add_com ("connect", class_obscure, connect_command,
   	   "Connect the terminal directly up to a serial based command monitor.\nUse <CR>~. or <CR>~^D to break out.");
#endif
}
