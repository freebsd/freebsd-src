/* Memory-access and commands for remote NINDY process, for GDB.

   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999,
   2000, 2001, 2002 Free Software Foundation, Inc.

   Contributed by Intel Corporation.  Modified from remote.c by Chris Benenati.

   GDB is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY.  No author or distributor accepts responsibility to anyone
   for the consequences of using it or for whether it serves any
   particular purpose or works at all, unless he says so in writing.
   Refer to the GDB General Public License for full details.

   Everyone is granted permission to copy, modify and redistribute GDB,
   but only under the conditions described in the GDB General Public
   License.  A copy of this license is supposed to have been given to you
   along with GDB so you can know your rights and responsibilities.  It
   should be in a file named COPYING.  Among other things, the copyright
   notice and this notice must be preserved on all copies.

   In other words, go ahead and share GDB, but don't try to stop
   anyone else from sharing it farther.  Help stamp out software hoarding!  */

/*
   Except for the data cache routines, this file bears little resemblence
   to remote.c.  A new (although similar) protocol has been specified, and
   portions of the code are entirely dependent on having an i80960 with a
   NINDY ROM monitor at the other end of the line.
 */

/*****************************************************************************
 *
 * REMOTE COMMUNICATION PROTOCOL BETWEEN GDB960 AND THE NINDY ROM MONITOR.
 *
 *
 * MODES OF OPERATION
 * ----- -- ---------
 *	
 * As far as NINDY is concerned, GDB is always in one of two modes: command
 * mode or passthrough mode.
 *
 * In command mode (the default) pre-defined packets containing requests
 * are sent by GDB to NINDY.  NINDY never talks except in reponse to a request.
 *
 * Once the the user program is started, GDB enters passthrough mode, to give
 * the user program access to the terminal.  GDB remains in this mode until
 * NINDY indicates that the program has stopped.
 *
 *
 * PASSTHROUGH MODE
 * ----------- ----
 *
 * GDB writes all input received from the keyboard directly to NINDY, and writes
 * all characters received from NINDY directly to the monitor.
 *
 * Keyboard input is neither buffered nor echoed to the monitor.
 *
 * GDB remains in passthrough mode until NINDY sends a single ^P character,
 * to indicate that the user process has stopped.
 *
 * Note:
 *	GDB assumes NINDY performs a 'flushreg' when the user program stops.
 *
 *
 * COMMAND MODE
 * ------- ----
 *
 * All info (except for message ack and nak) is transferred between gdb
 * and the remote processor in messages of the following format:
 *
 *		<info>#<checksum>
 *
 * where 
 *	#	is a literal character
 *
 *	<info>	ASCII information;  all numeric information is in the
 *		form of hex digits ('0'-'9' and lowercase 'a'-'f').
 *
 *	<checksum>
 *		is a pair of ASCII hex digits representing an 8-bit
 *		checksum formed by adding together each of the
 *		characters in <info>.
 *
 * The receiver of a message always sends a single character to the sender
 * to indicate that the checksum was good ('+') or bad ('-');  the sender
 * re-transmits the entire message over until a '+' is received.
 *
 * In response to a command NINDY always sends back either data or
 * a result code of the form "Xnn", where "nn" are hex digits and "X00"
 * means no errors.  (Exceptions: the "s" and "c" commands don't respond.)
 *
 * SEE THE HEADER OF THE FILE "gdb.c" IN THE NINDY MONITOR SOURCE CODE FOR A
 * FULL DESCRIPTION OF LEGAL COMMANDS.
 *
 * SEE THE FILE "stop.h" IN THE NINDY MONITOR SOURCE CODE FOR A LIST
 * OF STOP CODES.
 *
 ***************************************************************************/

#include "defs.h"
#include <signal.h>
#include <sys/types.h>
#include <setjmp.h>

#include "frame.h"
#include "inferior.h"
#include "bfd.h"
#include "symfile.h"
#include "target.h"
#include "gdbcore.h"
#include "command.h"
#include "floatformat.h"
#include "regcache.h"

#include <sys/file.h>
#include <ctype.h>
#include "serial.h"
#include "nindy-share/env.h"
#include "nindy-share/stop.h"
#include "remote-utils.h"

extern int unlink ();
extern char *getenv ();
extern char *mktemp ();

extern void generic_mourn_inferior ();

extern struct target_ops nindy_ops;
extern FILE *instream;

extern char ninStopWhy ();
extern int ninMemGet ();
extern int ninMemPut ();

int nindy_initial_brk;		/* nonzero if want to send an initial BREAK to nindy */
int nindy_old_protocol;		/* nonzero if want to use old protocol */
char *nindy_ttyname;		/* name of tty to talk to nindy on, or null */

#define DLE	'\020'		/* Character NINDY sends to indicate user program has
				   * halted.  */
#define TRUE	1
#define FALSE	0

/* From nindy-share/nindy.c.  */
extern struct serial *nindy_serial;

static int have_regs = 0;	/* 1 iff regs read since i960 last halted */
static int regs_changed = 0;	/* 1 iff regs were modified since last read */

extern char *exists ();

static void nindy_fetch_registers (int);

static void nindy_store_registers (int);

static char *savename;

static void
nindy_close (int quitting)
{
  if (nindy_serial != NULL)
    serial_close (nindy_serial);
  nindy_serial = NULL;

  if (savename)
    xfree (savename);
  savename = 0;
}

/* Open a connection to a remote debugger.   
   FIXME, there should be "set" commands for the options that are
   now specified with gdb command-line options (old_protocol,
   and initial_brk).  */
void
nindy_open (char *name,		/* "/dev/ttyXX", "ttyXX", or "XX": tty to be opened */
	    int from_tty)
{
  char baudrate[1024];

  if (!name)
    error_no_arg ("serial port device name");

  target_preopen (from_tty);

  nindy_close (0);

  have_regs = regs_changed = 0;

  /* Allow user to interrupt the following -- we could hang if there's
     no NINDY at the other end of the remote tty.  */
  immediate_quit++;
  /* If baud_rate is -1, then ninConnect will not recognize the baud rate
     and will deal with the situation in a (more or less) reasonable
     fashion.  */
  sprintf (baudrate, "%d", baud_rate);
  ninConnect (name, baudrate,
	      nindy_initial_brk, !from_tty, nindy_old_protocol);
  immediate_quit--;

  if (nindy_serial == NULL)
    {
      perror_with_name (name);
    }

  savename = savestring (name, strlen (name));
  push_target (&nindy_ops);

  target_fetch_registers (-1);

  init_thread_list ();
  init_wait_for_inferior ();
  clear_proceed_status ();
  normal_stop ();
}

/* User-initiated quit of nindy operations.  */

static void
nindy_detach (char *name, int from_tty)
{
  if (name)
    error ("Too many arguments");
  pop_target ();
}

static void
nindy_files_info (void)
{
  /* FIXME: this lies about the baud rate if we autobauded.  */
  printf_unfiltered ("\tAttached to %s at %d bits per second%s%s.\n", savename,
		     baud_rate,
		     nindy_old_protocol ? " in old protocol" : "",
		     nindy_initial_brk ? " with initial break" : "");
}

/* Return the number of characters in the buffer BUF before
   the first DLE character.  N is maximum number of characters to
   consider.  */

static
int
non_dle (char *buf, int n)
{
  int i;

  for (i = 0; i < n; i++)
    {
      if (buf[i] == DLE)
	{
	  break;
	}
    }
  return i;
}

/* Tell the remote machine to resume.  */

void
nindy_resume (ptid_t ptid, int step, enum target_signal siggnal)
{
  if (siggnal != TARGET_SIGNAL_0 && siggnal != stop_signal)
    warning ("Can't send signals to remote NINDY targets.");

  if (regs_changed)
    {
      nindy_store_registers (-1);
      regs_changed = 0;
    }
  have_regs = 0;
  ninGo (step);
}

/* FIXME, we can probably use the normal terminal_inferior stuff here.
   We have to do terminal_inferior and then set up the passthrough
   settings initially.  Thereafter, terminal_ours and terminal_inferior
   will automatically swap the settings around for us.  */

struct clean_up_tty_args
{
  serial_ttystate state;
  struct serial *serial;
};
static struct clean_up_tty_args tty_args;

static void
clean_up_tty (PTR ptrarg)
{
  struct clean_up_tty_args *args = (struct clean_up_tty_args *) ptrarg;
  serial_set_tty_state (args->serial, args->state);
  xfree (args->state);
  warning ("\n\nYou may need to reset the 80960 and/or reload your program.\n");
}

/* Recover from ^Z or ^C while remote process is running */
static void (*old_ctrlc) ();
#ifdef SIGTSTP
static void (*old_ctrlz) ();
#endif

static void
clean_up_int (void)
{
  serial_set_tty_state (tty_args.serial, tty_args.state);
  xfree (tty_args.state);

  signal (SIGINT, old_ctrlc);
#ifdef SIGTSTP
  signal (SIGTSTP, old_ctrlz);
#endif
  error ("\n\nYou may need to reset the 80960 and/or reload your program.\n");
}

/* Wait until the remote machine stops. While waiting, operate in passthrough
 * mode; i.e., pass everything NINDY sends to gdb_stdout, and everything from
 * stdin to NINDY.
 *
 * Return to caller, storing status in 'status' just as `wait' would.
 */

static ptid_t
nindy_wait (ptid_t ptid, struct target_waitstatus *status)
{
  fd_set fds;
  int c;
  char buf[2];
  int i, n;
  unsigned char stop_exit;
  unsigned char stop_code;
  struct cleanup *old_cleanups;
  long ip_value, fp_value, sp_value;	/* Reg values from stop */

  status->kind = TARGET_WAITKIND_EXITED;
  status->value.integer = 0;

  /* OPERATE IN PASSTHROUGH MODE UNTIL NINDY SENDS A DLE CHARACTER */

  /* Save current tty attributes, and restore them when done.  */
  tty_args.serial = serial_fdopen (0);
  tty_args.state = serial_get_tty_state (tty_args.serial);
  old_ctrlc = signal (SIGINT, clean_up_int);
#ifdef SIGTSTP
  old_ctrlz = signal (SIGTSTP, clean_up_int);
#endif

  old_cleanups = make_cleanup (clean_up_tty, &tty_args);

  /* Pass input from keyboard to NINDY as it arrives.  NINDY will interpret
     <CR> and perform echo.  */
  /* This used to set CBREAK and clear ECHO and CRMOD.  I hope this is close
     enough.  */
  serial_raw (tty_args.serial);

  while (1)
    {
      /* Input on remote */
      c = serial_readchar (nindy_serial, -1);
      if (c == SERIAL_ERROR)
	{
	  error ("Cannot read from serial line");
	}
      else if (c == 0x1b)	/* ESC */
	{
	  c = serial_readchar (nindy_serial, -1);
	  c &= ~0x40;
	}
      else if (c != 0x10)	/* DLE */
	/* Write out any characters preceding DLE */
	{
	  buf[0] = (char) c;
	  write (1, buf, 1);
	}
      else
	{
	  stop_exit = ninStopWhy (&stop_code,
				  &ip_value, &fp_value, &sp_value);
	  if (!stop_exit && (stop_code == STOP_SRQ))
	    {
	      immediate_quit++;
	      ninSrq ();
	      immediate_quit--;
	    }
	  else
	    {
	      /* Get out of loop */
	      supply_register (IP_REGNUM,
			       (char *) &ip_value);
	      supply_register (FP_REGNUM,
			       (char *) &fp_value);
	      supply_register (SP_REGNUM,
			       (char *) &sp_value);
	      break;
	    }
	}
    }

  serial_set_tty_state (tty_args.serial, tty_args.state);
  xfree (tty_args.state);
  discard_cleanups (old_cleanups);

  if (stop_exit)
    {
      status->kind = TARGET_WAITKIND_EXITED;
      status->value.integer = stop_code;
    }
  else
    {
      /* nindy has some special stop code need to be handled */
      if (stop_code == STOP_GDB_BPT)
	stop_code = TRACE_STEP;
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = i960_fault_to_signal (stop_code);
    }
  return inferior_ptid;
}

/* Read the remote registers into the block REGS.  */

/* This is the block that ninRegsGet and ninRegsPut handles.  */
struct nindy_regs
{
  char local_regs[16 * 4];
  char global_regs[16 * 4];
  char pcw_acw[2 * 4];
  char ip[4];
  char tcw[4];
  char fp_as_double[4 * 8];
};

static void
nindy_fetch_registers (int regno)
{
  struct nindy_regs nindy_regs;
  int regnum;

  immediate_quit++;
  ninRegsGet ((char *) &nindy_regs);
  immediate_quit--;

  memcpy (&registers[REGISTER_BYTE (R0_REGNUM)], nindy_regs.local_regs, 16 * 4);
  memcpy (&registers[REGISTER_BYTE (G0_REGNUM)], nindy_regs.global_regs, 16 * 4);
  memcpy (&registers[REGISTER_BYTE (PCW_REGNUM)], nindy_regs.pcw_acw, 2 * 4);
  memcpy (&registers[REGISTER_BYTE (IP_REGNUM)], nindy_regs.ip, 1 * 4);
  memcpy (&registers[REGISTER_BYTE (TCW_REGNUM)], nindy_regs.tcw, 1 * 4);
  memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)], nindy_regs.fp_as_double, 4 * 8);

  registers_fetched ();
}

static void
nindy_prepare_to_store (void)
{
  /* Fetch all regs if they aren't already here.  */
  read_register_bytes (0, NULL, REGISTER_BYTES);
}

static void
nindy_store_registers (int regno)
{
  struct nindy_regs nindy_regs;
  int regnum;

  memcpy (nindy_regs.local_regs, &registers[REGISTER_BYTE (R0_REGNUM)], 16 * 4);
  memcpy (nindy_regs.global_regs, &registers[REGISTER_BYTE (G0_REGNUM)], 16 * 4);
  memcpy (nindy_regs.pcw_acw, &registers[REGISTER_BYTE (PCW_REGNUM)], 2 * 4);
  memcpy (nindy_regs.ip, &registers[REGISTER_BYTE (IP_REGNUM)], 1 * 4);
  memcpy (nindy_regs.tcw, &registers[REGISTER_BYTE (TCW_REGNUM)], 1 * 4);
  memcpy (nindy_regs.fp_as_double, &registers[REGISTER_BYTE (FP0_REGNUM)], 8 * 4);

  immediate_quit++;
  ninRegsPut ((char *) &nindy_regs);
  immediate_quit--;
}

/* Copy LEN bytes to or from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.   Copy to inferior if
   SHOULD_WRITE is nonzero.  Returns the length copied.  TARGET is
   unused.  */

int
nindy_xfer_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len,
			    int should_write, struct mem_attrib *attrib,
			    struct target_ops *target)
{
  int res;

  if (len <= 0)
    return 0;

  if (should_write)
    res = ninMemPut (memaddr, myaddr, len);
  else
    res = ninMemGet (memaddr, myaddr, len);

  return res;
}

static void
nindy_create_inferior (char *execfile, char *args, char **env)
{
  int entry_pt;
  int pid;

  if (args && *args)
    error ("Can't pass arguments to remote NINDY process");

  if (execfile == 0 || exec_bfd == 0)
    error ("No executable file specified");

  entry_pt = (int) bfd_get_start_address (exec_bfd);

  pid = 42;

  /* The "process" (board) is already stopped awaiting our commands, and
     the program is already downloaded.  We just set its PC and go.  */

  inferior_ptid = pid_to_ptid (pid);	/* Needed for wait_for_inferior below */

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
  proceed ((CORE_ADDR) entry_pt, TARGET_SIGNAL_DEFAULT, 0);
}

static void
reset_command (char *args, int from_tty)
{
  if (nindy_serial == NULL)
    {
      error ("No target system to reset -- use 'target nindy' command.");
    }
  if (query ("Really reset the target system?", 0, 0))
    {
      serial_send_break (nindy_serial);
      tty_flush (nindy_serial);
    }
}

void
nindy_kill (char *args, int from_tty)
{
  return;			/* Ignore attempts to kill target system */
}

/* Clean up when a program exits.

   The program actually lives on in the remote processor's RAM, and may be
   run again without a download.  Don't leave it full of breakpoint
   instructions.  */

void
nindy_mourn_inferior (void)
{
  remove_breakpoints ();
  unpush_target (&nindy_ops);
  generic_mourn_inferior ();	/* Do all the proper things now */
}

/* Pass the args the way catch_errors wants them.  */
static int
nindy_open_stub (char *arg)
{
  nindy_open (arg, 1);
  return 1;
}

static void
nindy_load (char *filename, int from_tty)
{
  asection *s;
  /* Can't do unix style forking on a VMS system, so we'll use bfd to do
     all the work for us
   */

  bfd *file = bfd_openr (filename, 0);
  if (!file)
    {
      perror_with_name (filename);
      return;
    }

  if (!bfd_check_format (file, bfd_object))
    {
      error ("can't prove it's an object file\n");
      return;
    }

  for (s = file->sections; s; s = s->next)
    {
      if (s->flags & SEC_LOAD)
	{
	  char *buffer = xmalloc (s->_raw_size);
	  bfd_get_section_contents (file, s, buffer, 0, s->_raw_size);
	  printf ("Loading section %s, size %x vma %x\n",
		  s->name,
		  s->_raw_size,
		  s->vma);
	  ninMemPut (s->vma, buffer, s->_raw_size);
	  xfree (buffer);
	}
    }
  bfd_close (file);
}

static int
load_stub (char *arg)
{
  target_load (arg, 1);
  return 1;
}

/* This routine is run as a hook, just before the main command loop is
   entered.  If gdb is configured for the i960, but has not had its
   nindy target specified yet, this will loop prompting the user to do so.

   Unlike the loop provided by Intel, we actually let the user get out
   of this with a RETURN.  This is useful when e.g. simply examining
   an i960 object file on the host system.  */

void
nindy_before_main_loop (void)
{
  char ttyname[100];
  char *p, *p2;

  while (target_stack->target_ops != &nindy_ops)	/* What is this crap??? */
    {				/* remote tty not specified yet */
      if (instream == stdin)
	{
	  printf_unfiltered ("\nAttach /dev/ttyNN -- specify NN, or \"quit\" to quit:  ");
	  gdb_flush (gdb_stdout);
	}
      fgets (ttyname, sizeof (ttyname) - 1, stdin);

      /* Strip leading and trailing whitespace */
      for (p = ttyname; isspace (*p); p++)
	{
	  ;
	}
      if (*p == '\0')
	{
	  return;		/* User just hit spaces or return, wants out */
	}
      for (p2 = p; !isspace (*p2) && (*p2 != '\0'); p2++)
	{
	  ;
	}
      *p2 = '\0';
      if (STREQ ("quit", p))
	{
	  exit (1);
	}

      if (catch_errors (nindy_open_stub, p, "", RETURN_MASK_ALL))
	{
	  /* Now that we have a tty open for talking to the remote machine,
	     download the executable file if one was specified.  */
	  if (exec_bfd)
	    {
	      catch_errors (load_stub, bfd_get_filename (exec_bfd), "",
			    RETURN_MASK_ALL);
	    }
	}
    }
}

/* Define the target subroutine names */

struct target_ops nindy_ops;

static void
init_nindy_ops (void)
{
  nindy_ops.to_shortname = "nindy";
  "Remote serial target in i960 NINDY-specific protocol",
    nindy_ops.to_longname = "Use a remote i960 system running NINDY connected by a serial line.\n\
Specify the name of the device the serial line is connected to.\n\
The speed (baud rate), whether to use the old NINDY protocol,\n\
and whether to send a break on startup, are controlled by options\n\
specified when you started GDB.";
  nindy_ops.to_doc = "";
  nindy_ops.to_open = nindy_open;
  nindy_ops.to_close = nindy_close;
  nindy_ops.to_attach = 0;
  nindy_ops.to_post_attach = NULL;
  nindy_ops.to_require_attach = NULL;
  nindy_ops.to_detach = nindy_detach;
  nindy_ops.to_require_detach = NULL;
  nindy_ops.to_resume = nindy_resume;
  nindy_ops.to_wait = nindy_wait;
  nindy_ops.to_post_wait = NULL;
  nindy_ops.to_fetch_registers = nindy_fetch_registers;
  nindy_ops.to_store_registers = nindy_store_registers;
  nindy_ops.to_prepare_to_store = nindy_prepare_to_store;
  nindy_ops.to_xfer_memory = nindy_xfer_inferior_memory;
  nindy_ops.to_files_info = nindy_files_info;
  nindy_ops.to_insert_breakpoint = memory_insert_breakpoint;
  nindy_ops.to_remove_breakpoint = memory_remove_breakpoint;
  nindy_ops.to_terminal_init = 0;
  nindy_ops.to_terminal_inferior = 0;
  nindy_ops.to_terminal_ours_for_output = 0;
  nindy_ops.to_terminal_ours = 0;
  nindy_ops.to_terminal_info = 0;	/* Terminal crud */
  nindy_ops.to_kill = nindy_kill;
  nindy_ops.to_load = nindy_load;
  nindy_ops.to_lookup_symbol = 0;	/* lookup_symbol */
  nindy_ops.to_create_inferior = nindy_create_inferior;
  nindy_ops.to_post_startup_inferior = NULL;
  nindy_ops.to_acknowledge_created_inferior = NULL;
  nindy_ops.to_clone_and_follow_inferior = NULL;
  nindy_ops.to_post_follow_inferior_by_clone = NULL;
  nindy_ops.to_insert_fork_catchpoint = NULL;
  nindy_ops.to_remove_fork_catchpoint = NULL;
  nindy_ops.to_insert_vfork_catchpoint = NULL;
  nindy_ops.to_remove_vfork_catchpoint = NULL;
  nindy_ops.to_has_forked = NULL;
  nindy_ops.to_has_vforked = NULL;
  nindy_ops.to_can_follow_vfork_prior_to_exec = NULL;
  nindy_ops.to_post_follow_vfork = NULL;
  nindy_ops.to_insert_exec_catchpoint = NULL;
  nindy_ops.to_remove_exec_catchpoint = NULL;
  nindy_ops.to_has_execd = NULL;
  nindy_ops.to_reported_exec_events_per_exec_call = NULL;
  nindy_ops.to_has_exited = NULL;
  nindy_ops.to_mourn_inferior = nindy_mourn_inferior;
  nindy_ops.to_can_run = 0;	/* can_run */
  nindy_ops.to_notice_signals = 0;	/* notice_signals */
  nindy_ops.to_thread_alive = 0;	/* to_thread_alive */
  nindy_ops.to_stop = 0;	/* to_stop */
  nindy_ops.to_pid_to_exec_file = NULL;
  nindy_ops.to_stratum = process_stratum;
  nindy_ops.DONT_USE = 0;	/* next */
  nindy_ops.to_has_all_memory = 1;
  nindy_ops.to_has_memory = 1;
  nindy_ops.to_has_stack = 1;
  nindy_ops.to_has_registers = 1;
  nindy_ops.to_has_execution = 1;	/* all mem, mem, stack, regs, exec */
  nindy_ops.to_sections = 0;
  nindy_ops.to_sections_end = 0;	/* Section pointers */
  nindy_ops.to_magic = OPS_MAGIC;	/* Always the last thing */
}

void
_initialize_nindy (void)
{
  init_nindy_ops ();
  add_target (&nindy_ops);
  add_com ("reset", class_obscure, reset_command,
	   "Send a 'break' to the remote target system.\n\
Only useful if the target has been equipped with a circuit\n\
to perform a hard reset when a break is detected.");
}
