/* Top level stuff for GDB, the GNU debugger.
   Copyright 1986, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 1998
   Free Software Foundation, Inc.

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
#include "gdbcmd.h"
#include "call-cmds.h"
#include "symtab.h"
#include "inferior.h"
#include "signals.h"
#include "target.h"
#include "breakpoint.h"
#include "gdbtypes.h"
#include "expression.h"
#include "value.h"
#include "language.h"
#include "terminal.h" /* For job_control.  */
#include "annotate.h"
#include <setjmp.h>
#include "top.h"

/* readline include files */
#include <readline/readline.h>
#include <readline/history.h>

/* readline defines this.  */
#undef savestring

#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "gdb_string.h"
#include "gdb_stat.h"
#include <ctype.h>

extern void initialize_utils PARAMS ((void));

/* Prototypes for local functions */

static void dont_repeat_command PARAMS ((char *, int));

static void source_cleanup_lines PARAMS ((PTR));

static void user_defined_command PARAMS ((char *, int));

static void init_signals PARAMS ((void));

#ifdef STOP_SIGNAL
static void stop_sig PARAMS ((int));
#endif

static char * line_completion_function PARAMS ((char *, int, char *, int));

static char * readline_line_completion_function PARAMS ((char *, int));

static void command_loop_marker PARAMS ((int));

static void while_command PARAMS ((char *, int));

static void if_command PARAMS ((char *, int));

static struct command_line *
build_command_line PARAMS ((enum command_control_type, char *));

static struct command_line *
get_command_line PARAMS ((enum command_control_type, char *));

static void realloc_body_list PARAMS ((struct command_line *, int));

static enum misc_command_type read_next_line PARAMS ((struct command_line **));

static enum command_control_type
recurse_read_control_structure PARAMS ((struct command_line *));

static struct cleanup * setup_user_args PARAMS ((char *));

static char * locate_arg PARAMS ((char *));

static char * insert_args PARAMS ((char *));

static void arg_cleanup PARAMS ((void));

static void init_main PARAMS ((void));

static void init_cmd_lists PARAMS ((void));

static void float_handler PARAMS ((int));

static void init_signals PARAMS ((void));

static void set_verbose PARAMS ((char *, int, struct cmd_list_element *));

static void show_history PARAMS ((char *, int));

static void set_history PARAMS ((char *, int));

static void set_history_size_command PARAMS ((char *, int,
					      struct cmd_list_element *));

static void show_commands PARAMS ((char *, int));

static void echo_command PARAMS ((char *, int));

static void pwd_command PARAMS ((char *, int));

static void show_version PARAMS ((char *, int));

static void document_command PARAMS ((char *, int));

static void define_command PARAMS ((char *, int));

static void validate_comname PARAMS ((char *));

static void help_command PARAMS ((char *, int));

static void show_command PARAMS ((char *, int));

static void info_command PARAMS ((char *, int));

static void complete_command PARAMS ((char *, int));

static void do_nothing PARAMS ((int));

#ifdef SIGHUP
static int quit_cover PARAMS ((PTR));

static void disconnect PARAMS ((int));
#endif

static void source_cleanup PARAMS ((FILE *));

/* If this definition isn't overridden by the header files, assume
   that isatty and fileno exist on this system.  */
#ifndef ISATTY
#define ISATTY(FP)	(isatty (fileno (FP)))
#endif

/* Initialization file name for gdb.  This is overridden in some configs.  */

#ifndef	GDBINIT_FILENAME
#define	GDBINIT_FILENAME	".gdbinit"
#endif
char gdbinit[] = GDBINIT_FILENAME;

int inhibit_gdbinit = 0;

/* If nonzero, and GDB has been configured to be able to use windows,
   attempt to open them upon startup.  */

int use_windows = 1;

/* Version number of GDB, as a string.  */

extern char *version;

/* Canonical host name as a string. */

extern char *host_name;

/* Canonical target name as a string. */

extern char *target_name;

extern char lang_frame_mismatch_warn[];		/* language.c */

/* Flag for whether we want all the "from_tty" gubbish printed.  */

int caution = 1;			/* Default is yes, sigh. */

/* Define all cmd_list_elements.  */

/* Chain containing all defined commands.  */

struct cmd_list_element *cmdlist;

/* Chain containing all defined info subcommands.  */

struct cmd_list_element *infolist;

/* Chain containing all defined enable subcommands. */

struct cmd_list_element *enablelist;

/* Chain containing all defined disable subcommands. */

struct cmd_list_element *disablelist;

/* Chain containing all defined toggle subcommands. */

struct cmd_list_element *togglelist;

/* Chain containing all defined stop subcommands. */

struct cmd_list_element *stoplist;

/* Chain containing all defined delete subcommands. */

struct cmd_list_element *deletelist;

/* Chain containing all defined "enable breakpoint" subcommands. */

struct cmd_list_element *enablebreaklist;

/* Chain containing all defined set subcommands */

struct cmd_list_element *setlist;

/* Chain containing all defined unset subcommands */

struct cmd_list_element *unsetlist;

/* Chain containing all defined show subcommands.  */

struct cmd_list_element *showlist;

/* Chain containing all defined \"set history\".  */

struct cmd_list_element *sethistlist;

/* Chain containing all defined \"show history\".  */

struct cmd_list_element *showhistlist;

/* Chain containing all defined \"unset history\".  */

struct cmd_list_element *unsethistlist;

/* Chain containing all defined maintenance subcommands. */

#if MAINTENANCE_CMDS
struct cmd_list_element *maintenancelist;
#endif

/* Chain containing all defined "maintenance info" subcommands. */

#if MAINTENANCE_CMDS
struct cmd_list_element *maintenanceinfolist;
#endif

/* Chain containing all defined "maintenance print" subcommands. */

#if MAINTENANCE_CMDS
struct cmd_list_element *maintenanceprintlist;
#endif

struct cmd_list_element *setprintlist;

struct cmd_list_element *showprintlist;

struct cmd_list_element *setchecklist;

struct cmd_list_element *showchecklist;

/* stdio stream that command input is being read from.  Set to stdin normally.
   Set by source_command to the file we are sourcing.  Set to NULL if we are
   executing a user-defined command or interacting via a GUI.  */

FILE *instream;

/* Current working directory.  */

char *current_directory;

/* The directory name is actually stored here (usually).  */
char gdb_dirbuf[1024];

/* Function to call before reading a command, if nonzero.
   The function receives two args: an input stream,
   and a prompt string.  */

void (*window_hook) PARAMS ((FILE *, char *));

int epoch_interface;
int xgdb_verbose;

/* gdb prints this when reading a command interactively */
static char *prompt;

/* Buffer used for reading command lines, and the size
   allocated for it so far.  */

char *line;
int linesize = 100;

/* Nonzero if the current command is modified by "server ".  This
   affects things like recording into the command history, comamnds
   repeating on RETURN, etc.  This is so a user interface (emacs, GUI,
   whatever) can issue its own commands and also send along commands
   from the user, and have the user not notice that the user interface
   is issuing commands too.  */
int server_command;

/* Baud rate specified for talking to serial target systems.  Default
   is left as -1, so targets can choose their own defaults.  */
/* FIXME: This means that "show remotebaud" and gr_files_info can print -1
   or (unsigned int)-1.  This is a Bad User Interface.  */

int baud_rate = -1;

/* Timeout limit for response from target. */

int remote_timeout = 1;		/* Set default to 1 second */

/* Non-zero tells remote* modules to output debugging info.  */

int remote_debug = 0;

/* Level of control structure.  */
static int control_level;

/* Structure for arguments to user defined functions.  */
#define MAXUSERARGS 10
struct user_args
{
  struct user_args *next;
  struct
    {
      char *arg;
      int len;
    } a[MAXUSERARGS];
  int count;
} *user_args;

/* Signal to catch ^Z typed while reading a command: SIGTSTP or SIGCONT.  */

#ifndef STOP_SIGNAL
#ifdef SIGTSTP
#define STOP_SIGNAL SIGTSTP
static void stop_sig PARAMS ((int));
#endif
#endif

/* Some System V have job control but not sigsetmask(). */
#if !defined (HAVE_SIGSETMASK)
#if !defined (USG)
#define HAVE_SIGSETMASK 1
#else
#define HAVE_SIGSETMASK 0
#endif
#endif

#if 0 == (HAVE_SIGSETMASK)
#define sigsetmask(n)
#endif

/* Hooks for alternate command interfaces.  */

/* Called after most modules have been initialized, but before taking users
   command file.  */

void (*init_ui_hook) PARAMS ((char *argv0));
#ifdef __CYGWIN32__
void (*ui_loop_hook) PARAMS ((int));
#endif

/* Called instead of command_loop at top level.  Can be invoked via
   return_to_top_level.  */

void (*command_loop_hook) PARAMS ((void));


/* Called instead of fputs for all output.  */

void (*fputs_unfiltered_hook) PARAMS ((const char *linebuffer, GDB_FILE *stream));

/* Called when the target says something to the host, which may
   want to appear in a different window. */

void (*target_output_hook) PARAMS ((char *));

/* Called from print_frame_info to list the line we stopped in.  */

void (*print_frame_info_listing_hook) PARAMS ((struct symtab *s, int line,
					       int stopline, int noerror));
/* Replaces most of query.  */

int (*query_hook) PARAMS ((const char *, va_list));

/* Replaces most of warning.  */

void (*warning_hook) PARAMS ((const char *, va_list));

/* Called from gdb_flush to flush output.  */

void (*flush_hook) PARAMS ((GDB_FILE *stream));

/* These three functions support getting lines of text from the user.  They
   are used in sequence.  First readline_begin_hook is called with a text
   string that might be (for example) a message for the user to type in a
   sequence of commands to be executed at a breakpoint.  If this function
   calls back to a GUI, it might take this opportunity to pop up a text
   interaction window with this message.  Next, readline_hook is called
   with a prompt that is emitted prior to collecting the user input.
   It can be called multiple times.  Finally, readline_end_hook is called
   to notify the GUI that we are done with the interaction window and it
   can close it. */

void (*readline_begin_hook) PARAMS ((char *, ...));
char * (*readline_hook) PARAMS ((char *));
void (*readline_end_hook) PARAMS ((void));

/* Called as appropriate to notify the interface of the specified breakpoint
   conditions.  */

void (*create_breakpoint_hook) PARAMS ((struct breakpoint *bpt));
void (*delete_breakpoint_hook) PARAMS ((struct breakpoint *bpt));
void (*modify_breakpoint_hook) PARAMS ((struct breakpoint *bpt));

/* Called during long calculations to allow GUI to repair window damage, and to
   check for stop buttons, etc... */

void (*interactive_hook) PARAMS ((void));

/* Called when the registers have changed, as a hint to a GUI
   to minimize window update. */

void (*registers_changed_hook) PARAMS ((void));

/* Tell the GUI someone changed the register REGNO. -1 means
   that the caller does not know which register changed or
   that several registers have changed (see value_assign).*/
void (*register_changed_hook) PARAMS ((int regno));

/* Tell the GUI someone changed LEN bytes of memory at ADDR */
void (*memory_changed_hook) PARAMS ((CORE_ADDR addr, int len));

/* Called when going to wait for the target.  Usually allows the GUI to run
   while waiting for target events.  */

int (*target_wait_hook) PARAMS ((int pid, struct target_waitstatus *status));

/* Used by UI as a wrapper around command execution.  May do various things
   like enabling/disabling buttons, etc...  */

void (*call_command_hook) PARAMS ((struct cmd_list_element *c, char *cmd,
				   int from_tty));

/* Called when the current thread changes.  Argument is thread id.  */

void (*context_hook) PARAMS ((int id));

/* Takes control from error ().  Typically used to prevent longjmps out of the
   middle of the GUI.  Usually used in conjunction with a catch routine.  */

NORETURN void (*error_hook) PARAMS ((void)) ATTR_NORETURN;


/* Where to go for return_to_top_level (RETURN_ERROR).  */
SIGJMP_BUF error_return;
/* Where to go for return_to_top_level (RETURN_QUIT).  */
SIGJMP_BUF quit_return;

/* Return for reason REASON.  This generally gets back to the command
   loop, but can be caught via catch_errors.  */

NORETURN void
return_to_top_level (reason)
     enum return_reason reason;
{
  quit_flag = 0;
  immediate_quit = 0;

  /* Perhaps it would be cleaner to do this via the cleanup chain (not sure
     I can think of a reason why that is vital, though).  */
  bpstat_clear_actions(stop_bpstat);	/* Clear queued breakpoint commands */

  disable_current_display ();
  do_cleanups (ALL_CLEANUPS);

  if (annotation_level > 1)
    switch (reason)
      {
      case RETURN_QUIT:
	annotate_quit ();
	break;
      case RETURN_ERROR:
	annotate_error ();
	break;
      }

  (NORETURN void) SIGLONGJMP
    (reason == RETURN_ERROR ? error_return : quit_return, 1);
}

/* Call FUNC with arg ARGS, catching any errors.  If there is no
   error, return the value returned by FUNC.  If there is an error,
   print ERRSTRING, print the specific error message, then return
   zero.

   Must not be called with immediate_quit in effect (bad things might
   happen, say we got a signal in the middle of a memcpy to quit_return).
   This is an OK restriction; with very few exceptions immediate_quit can
   be replaced by judicious use of QUIT.

   MASK specifies what to catch; it is normally set to
   RETURN_MASK_ALL, if for no other reason than that the code which
   calls catch_errors might not be set up to deal with a quit which
   isn't caught.  But if the code can deal with it, it generally
   should be RETURN_MASK_ERROR, unless for some reason it is more
   useful to abort only the portion of the operation inside the
   catch_errors.  Note that quit should return to the command line
   fairly quickly, even if some further processing is being done.  */

int
catch_errors (func, args, errstring, mask)
     catch_errors_ftype *func;
     PTR args;
     char *errstring;
     return_mask mask;
{
  SIGJMP_BUF saved_error;
  SIGJMP_BUF saved_quit;
  SIGJMP_BUF tmp_jmp;
  int val;
  struct cleanup *saved_cleanup_chain;
  char *saved_error_pre_print;
  char *saved_quit_pre_print;

  saved_cleanup_chain = save_cleanups ();
  saved_error_pre_print = error_pre_print;
  saved_quit_pre_print = quit_pre_print;

  if (mask & RETURN_MASK_ERROR)
    {
      memcpy ((char *)saved_error, (char *)error_return, sizeof (SIGJMP_BUF));
      error_pre_print = errstring;
    }
  if (mask & RETURN_MASK_QUIT)
    {
      memcpy (saved_quit, quit_return, sizeof (SIGJMP_BUF));
      quit_pre_print = errstring;
    }

  if (SIGSETJMP (tmp_jmp) == 0)
    {
      if (mask & RETURN_MASK_ERROR)
	memcpy (error_return, tmp_jmp, sizeof (SIGJMP_BUF));
      if (mask & RETURN_MASK_QUIT)
	memcpy (quit_return, tmp_jmp, sizeof (SIGJMP_BUF));
      val = (*func) (args);
    }
  else
    val = 0;

  restore_cleanups (saved_cleanup_chain);

  if (mask & RETURN_MASK_ERROR)
    {
      memcpy (error_return, saved_error, sizeof (SIGJMP_BUF));
      error_pre_print = saved_error_pre_print;
    }
  if (mask & RETURN_MASK_QUIT)
    {
      memcpy (quit_return, saved_quit, sizeof (SIGJMP_BUF));
      quit_pre_print = saved_quit_pre_print;
    }
  return val;
}

/* Handler for SIGHUP.  */

#ifdef SIGHUP
static void
disconnect (signo)
int signo;
{
  catch_errors (quit_cover, NULL,
		"Could not kill the program being debugged", RETURN_MASK_ALL);
  signal (SIGHUP, SIG_DFL);
  kill (getpid (), SIGHUP);
}

/* Just a little helper function for disconnect().  */

static int
quit_cover (s)
     PTR s;
{
  caution = 0;		/* Throw caution to the wind -- we're exiting.
			   This prevents asking the user dumb questions.  */
  quit_command((char *)0, 0);
  return 0;
}
#endif /* defined SIGHUP */

/* Line number we are currently in in a file which is being sourced.  */
static int source_line_number;

/* Name of the file we are sourcing.  */
static char *source_file_name;

/* Buffer containing the error_pre_print used by the source stuff.
   Malloc'd.  */
static char *source_error;
static int source_error_allocated;

/* Something to glom on to the start of error_pre_print if source_file_name
   is set.  */
static char *source_pre_error;

/* Clean up on error during a "source" command (or execution of a
   user-defined command).  */

static void
source_cleanup (stream)
     FILE *stream;
{
  /* Restore the previous input stream.  */
  instream = stream;
}

/* Read commands from STREAM.  */
void
read_command_file (stream)
     FILE *stream;
{
  struct cleanup *cleanups;

  cleanups = make_cleanup ((make_cleanup_func) source_cleanup, instream);
  instream = stream;
  command_loop ();
  do_cleanups (cleanups);
}

extern void init_proc PARAMS ((void));

void (*pre_init_ui_hook) PARAMS ((void));

void
gdb_init (argv0)
     char *argv0;
{
  if (pre_init_ui_hook)
    pre_init_ui_hook ();

  /* Run the init function of each source file */

  getcwd (gdb_dirbuf, sizeof (gdb_dirbuf));
  current_directory = gdb_dirbuf;

  init_cmd_lists ();	/* This needs to be done first */
  initialize_targets (); /* Setup target_terminal macros for utils.c */
  initialize_utils ();	/* Make errors and warnings possible */
  initialize_all_files ();
  init_main ();		/* But that omits this file!  Do it now */
  init_signals ();

  init_proc ();

  /* We need a default language for parsing expressions, so simple things like
     "set width 0" won't fail if no language is explicitly set in a config file
     or implicitly set by reading an executable during startup. */
  set_language (language_c);
  expected_language = current_language;	/* don't warn about the change.  */

  if (init_ui_hook)
    init_ui_hook (argv0);
}

/* Allocate, initialize a new command line structure for one of the
   control commands (if/while).  */

static struct command_line *
build_command_line (type, args)
     enum command_control_type type;
     char *args;
{
  struct command_line *cmd;

  if (args == NULL)
    error ("if/while commands require arguments.\n");

  cmd = (struct command_line *)xmalloc (sizeof (struct command_line));
  cmd->next = NULL;
  cmd->control_type = type;

  cmd->body_count = 1;
  cmd->body_list
    = (struct command_line **)xmalloc (sizeof (struct command_line *)
				       * cmd->body_count);
  memset (cmd->body_list, 0, sizeof (struct command_line *) * cmd->body_count);
  cmd->line = savestring (args, strlen (args));
  return cmd;
}

/* Build and return a new command structure for the control commands
   such as "if" and "while".  */

static struct command_line *
get_command_line (type, arg)
     enum command_control_type type;
     char *arg;
{
  struct command_line *cmd;
  struct cleanup *old_chain = NULL;

  /* Allocate and build a new command line structure.  */
  cmd = build_command_line (type, arg);

  old_chain = make_cleanup ((make_cleanup_func) free_command_lines, &cmd);

  /* Read in the body of this command.  */
  if (recurse_read_control_structure (cmd) == invalid_control)
    {
      warning ("error reading in control structure\n");
      do_cleanups (old_chain);
      return NULL;
    }

  discard_cleanups (old_chain);
  return cmd;
}

/* Recursively print a command (including full control structures).  */
void
print_command_line (cmd, depth)
     struct command_line *cmd;
     unsigned int depth;
{
  unsigned int i;

  if (depth)
    {
      for (i = 0; i < depth; i++)
	fputs_filtered ("  ", gdb_stdout);
    }

  /* A simple command, print it and return.  */
  if (cmd->control_type == simple_control)
    {
      fputs_filtered (cmd->line, gdb_stdout);
      fputs_filtered ("\n", gdb_stdout);
      return;
    }

  /* loop_continue to jump to the start of a while loop, print it
     and return. */
  if (cmd->control_type == continue_control)
    {
      fputs_filtered ("loop_continue\n", gdb_stdout);
      return;
    }

  /* loop_break to break out of a while loop, print it and return.  */
  if (cmd->control_type == break_control)
    {
      fputs_filtered ("loop_break\n", gdb_stdout);
      return;
    }

  /* A while command.  Recursively print its subcommands before returning.  */
  if (cmd->control_type == while_control)
    {
      struct command_line *list;
      fputs_filtered ("while ", gdb_stdout);
      fputs_filtered (cmd->line, gdb_stdout);
      fputs_filtered ("\n", gdb_stdout);
      list = *cmd->body_list;
      while (list)
	{
	  print_command_line (list, depth + 1);
	  list = list->next;
	}
    }

  /* An if command.  Recursively print both arms before returning.  */
  if (cmd->control_type == if_control)
    {
      fputs_filtered ("if ", gdb_stdout);
      fputs_filtered (cmd->line, gdb_stdout);
      fputs_filtered ("\n", gdb_stdout);
      /* The true arm. */
      print_command_line (cmd->body_list[0], depth + 1);

      /* Show the false arm if it exists.  */
      if (cmd->body_count == 2)
	  {
	    if (depth)
	      {
		for (i = 0; i < depth; i++)
		  fputs_filtered ("  ", gdb_stdout);
	      }
	    fputs_filtered ("else\n", gdb_stdout);
	    print_command_line (cmd->body_list[1], depth + 1);
	  }
      if (depth)
	{
	  for (i = 0; i < depth; i++)
	    fputs_filtered ("  ", gdb_stdout);
	}
      fputs_filtered ("end\n", gdb_stdout);
    }
}

/* Execute the command in CMD.  */

enum command_control_type
execute_control_command (cmd)
     struct command_line *cmd;
{
  struct expression *expr;
  struct command_line *current;
  struct cleanup *old_chain = 0;
  value_ptr val;
  value_ptr val_mark;
  int loop;
  enum command_control_type ret;
  char *new_line;

  switch (cmd->control_type)
    {
    case simple_control:
      /* A simple command, execute it and return.  */
      new_line = insert_args (cmd->line);
      if (!new_line)
	return invalid_control;
      old_chain = make_cleanup ((make_cleanup_func) free_current_contents, 
                                &new_line);
      execute_command (new_line, 0);
      ret = cmd->control_type;
      break;

    case continue_control:
    case break_control:
      /* Return for "continue", and "break" so we can either
	 continue the loop at the top, or break out.  */
      ret = cmd->control_type;
      break;

    case while_control:
      {
	/* Parse the loop control expression for the while statement.  */
	new_line = insert_args (cmd->line);
	if (!new_line)
	  return invalid_control;
	old_chain = make_cleanup ((make_cleanup_func) free_current_contents, 
                                  &new_line);
	expr = parse_expression (new_line);
	make_cleanup ((make_cleanup_func) free_current_contents, &expr);
	
	ret = simple_control;
	loop = 1;

	/* Keep iterating so long as the expression is true.  */
	while (loop == 1)
	  {
	    int cond_result;

	    QUIT;

	    /* Evaluate the expression.  */
	    val_mark = value_mark ();
	    val = evaluate_expression (expr);
	    cond_result = value_true (val);
	    value_free_to_mark (val_mark);

	    /* If the value is false, then break out of the loop.  */
	    if (!cond_result)
	      break;

	    /* Execute the body of the while statement.  */
	    current = *cmd->body_list;
	    while (current)
	      {
		ret = execute_control_command (current);

		/* If we got an error, or a "break" command, then stop
		   looping.  */
		if (ret == invalid_control || ret == break_control)
		  {
		    loop = 0;
		    break;
		  }

		/* If we got a "continue" command, then restart the loop
		   at this point.  */
		if (ret == continue_control)
		  break;
		
		/* Get the next statement.  */
		current = current->next; 
	      }
	  }

	/* Reset RET so that we don't recurse the break all the way down.  */
	if (ret == break_control)
	  ret = simple_control;

	break;
      }

    case if_control:
      {
	new_line = insert_args (cmd->line);
	if (!new_line)
	  return invalid_control;
	old_chain = make_cleanup ((make_cleanup_func) free_current_contents, 
                                  &new_line);
	/* Parse the conditional for the if statement.  */
	expr = parse_expression (new_line);
	make_cleanup ((make_cleanup_func) free_current_contents, &expr);

	current = NULL;
	ret = simple_control;

	/* Evaluate the conditional.  */
	val_mark = value_mark ();
	val = evaluate_expression (expr);

	/* Choose which arm to take commands from based on the value of the
	   conditional expression.  */
	if (value_true (val))
	  current = *cmd->body_list;
	else if (cmd->body_count == 2)
	  current = *(cmd->body_list + 1);
	value_free_to_mark (val_mark);

	/* Execute commands in the given arm.  */
	while (current)
	  {
	    ret = execute_control_command (current);

	    /* If we got an error, get out.  */
	    if (ret != simple_control)
	      break;

	    /* Get the next statement in the body.  */
	    current = current->next;
	  }

	break;
      }

    default:
      warning ("Invalid control type in command structure.");
      return invalid_control;
    }

  if (old_chain)
    do_cleanups (old_chain);

  return ret;
}

/* "while" command support.  Executes a body of statements while the
   loop condition is nonzero.  */

static void
while_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  struct command_line *command = NULL;

  control_level = 1;
  command = get_command_line (while_control, arg);

  if (command == NULL)
    return;

  execute_control_command (command);
  free_command_lines (&command);
}

/* "if" command support.  Execute either the true or false arm depending
   on the value of the if conditional.  */

static void
if_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  struct command_line *command = NULL;

  control_level = 1;
  command = get_command_line (if_control, arg);

  if (command == NULL)
    return;

  execute_control_command (command);
  free_command_lines (&command);
}

/* Cleanup */
static void
arg_cleanup ()
{
  struct user_args *oargs = user_args;
  if (!user_args)
    fatal ("Internal error, arg_cleanup called with no user args.\n");

  user_args = user_args->next;
  free (oargs);
}

/* Bind the incomming arguments for a user defined command to
   $arg0, $arg1 ... $argMAXUSERARGS.  */

static struct cleanup *
setup_user_args (p)
     char *p;
{
  struct user_args *args;
  struct cleanup *old_chain;
  unsigned int arg_count = 0;

  args = (struct user_args *)xmalloc (sizeof (struct user_args));
  memset (args, 0, sizeof (struct user_args));

  args->next = user_args;
  user_args = args;

  old_chain = make_cleanup ((make_cleanup_func) arg_cleanup, 0);

  if (p == NULL)
    return old_chain;

  while (*p)
    {
      char *start_arg;
      int squote = 0;
      int dquote = 0;
      int bsquote = 0;

      if (arg_count >= MAXUSERARGS)
	{
	  error ("user defined function may only have %d arguments.\n",
		 MAXUSERARGS);
	  return old_chain;
	}

      /* Strip whitespace.  */
      while (*p == ' ' || *p == '\t')
	p++;

      /* P now points to an argument.  */
      start_arg = p;
      user_args->a[arg_count].arg = p;

      /* Get to the end of this argument.  */
      while (*p)
	{
	  if (((*p == ' ' || *p == '\t')) && !squote && !dquote && !bsquote)
	    break;
	  else
	    {
	      if (bsquote)
		bsquote = 0;
	      else if (*p == '\\')
		bsquote = 1;
	      else if (squote)
		{
		  if (*p == '\'')
		    squote = 0;
		}
	      else if (dquote)
		{
		  if (*p == '"')
		    dquote = 0;
		}
	      else
		{
		  if (*p == '\'')
		    squote = 1;
		  else if (*p == '"')
		    dquote = 1;
		}
	      p++;
	    }
	}

      user_args->a[arg_count].len = p - start_arg;
      arg_count++;
      user_args->count++;
    }
  return old_chain;
}

/* Given character string P, return a point to the first argument ($arg),
   or NULL if P contains no arguments.  */

static char *
locate_arg (p)
     char *p;
{
  while ((p = strchr (p, '$')))
    {
      if (strncmp (p, "$arg", 4) == 0 && isdigit (p[4]))
	return p;
      p++;
    }
  return NULL;
}

/* Insert the user defined arguments stored in user_arg into the $arg
   arguments found in line, with the updated copy being placed into nline.  */

static char *
insert_args (line)
     char *line;
{
  char *p, *save_line, *new_line;
  unsigned len, i;

  /* First we need to know how much memory to allocate for the new line.  */
  save_line = line;
  len = 0;
  while ((p = locate_arg (line)))
    {
      len += p - line;
      i = p[4] - '0';
      
      if (i >= user_args->count)
	{
	  error ("Missing argument %d in user function.\n", i);
	  return NULL;
	}
      len += user_args->a[i].len;
      line = p + 5;
    }

  /* Don't forget the tail.  */
  len += strlen (line);

  /* Allocate space for the new line and fill it in.  */
  new_line = (char *)xmalloc (len + 1);
  if (new_line == NULL)
    return NULL;

  /* Restore pointer to beginning of old line.  */
  line = save_line;

  /* Save pointer to beginning of new line.  */
  save_line = new_line;

  while ((p = locate_arg (line)))
    {
      int i, len;

      memcpy (new_line, line, p - line);
      new_line += p - line;
      i = p[4] - '0';

      len = user_args->a[i].len;
      if (len)
	{
	  memcpy (new_line, user_args->a[i].arg, len);
	  new_line += len;
	}
      line = p + 5;
    }
  /* Don't forget the tail.  */
  strcpy (new_line, line);

  /* Return a pointer to the beginning of the new line.  */
  return save_line;
}

void
execute_user_command (c, args)
     struct cmd_list_element *c;
     char *args;
{
  register struct command_line *cmdlines;
  struct cleanup *old_chain;
  enum command_control_type ret;

  old_chain = setup_user_args (args);

  cmdlines = c->user_commands;
  if (cmdlines == 0)
    /* Null command */
    return;

  /* Set the instream to 0, indicating execution of a
     user-defined function.  */
  old_chain = make_cleanup ((make_cleanup_func) source_cleanup, instream);
  instream = (FILE *) 0;
  while (cmdlines)
    {
      ret = execute_control_command (cmdlines);
      if (ret != simple_control && ret != break_control)
	{
	  warning ("Error in control structure.\n");
	  break;
	}
      cmdlines = cmdlines->next;
    }
  do_cleanups (old_chain);
}

/* Execute the line P as a command.
   Pass FROM_TTY as second argument to the defining function.  */

void
execute_command (p, from_tty)
     char *p;
     int from_tty;
{
  register struct cmd_list_element *c;
  register enum language flang;
  static int warned = 0;
  /* FIXME: These should really be in an appropriate header file */
  extern void serial_log_command PARAMS ((const char *));

  free_all_values ();

  /* Force cleanup of any alloca areas if using C alloca instead of
     a builtin alloca.  */
  alloca (0);

  /* This can happen when command_line_input hits end of file.  */
  if (p == NULL)
      return;

  serial_log_command (p);

  while (*p == ' ' || *p == '\t') p++;
  if (*p)
    {
      char *arg;

      c = lookup_cmd (&p, cmdlist, "", 0, 1);
      /* Pass null arg rather than an empty one.  */
      arg = *p ? p : 0;

      /* Clear off trailing whitespace, except for set and complete command.  */
      if (arg && c->type != set_cmd && c->function.cfunc != complete_command)
	{
	  p = arg + strlen (arg) - 1;
	  while (p >= arg && (*p == ' ' || *p == '\t'))
	    p--;
	  *(p + 1) = '\0';
	}

      /* If this command has been hooked, run the hook first. */
      if (c->hook)
	execute_user_command (c->hook, (char *)0);

      if (c->class == class_user)
	execute_user_command (c, arg);
      else if (c->type == set_cmd || c->type == show_cmd)
	do_setshow_command (arg, from_tty & caution, c);
      else if (c->function.cfunc == NO_FUNCTION)
	error ("That is not a command, just a help topic.");
      else if (call_command_hook)
	call_command_hook (c, arg, from_tty & caution);
      else
	(*c->function.cfunc) (arg, from_tty & caution);
   }

  /* Tell the user if the language has changed (except first time).  */
  if (current_language != expected_language)
  {
    if (language_mode == language_mode_auto) {
      language_info (1);	/* Print what changed.  */
    }
    warned = 0;
  }

  /* Warn the user if the working language does not match the
     language of the current frame.  Only warn the user if we are
     actually running the program, i.e. there is a stack. */
  /* FIXME:  This should be cacheing the frame and only running when
     the frame changes.  */

  if (target_has_stack)
    {
      flang = get_frame_language ();
      if (!warned
	  && flang != language_unknown
	  && flang != current_language->la_language)
	{
	  printf_filtered ("%s\n", lang_frame_mismatch_warn);
	  warned = 1;
	}
    }
}

/* ARGSUSED */
static void
command_loop_marker (foo)
     int foo;
{
}

/* Read commands from `instream' and execute them
   until end of file or error reading instream.  */

void
command_loop ()
{
  struct cleanup *old_chain;
  char *command;
  int stdin_is_tty = ISATTY (stdin);
  long time_at_cmd_start;
#ifdef HAVE_SBRK
  long space_at_cmd_start = 0;
#endif
  extern int display_time;
  extern int display_space;

  while (instream && !feof (instream))
    {
#if defined(TUI)
      extern int insert_mode;
#endif
      if (window_hook && instream == stdin)
	(*window_hook) (instream, prompt);

      quit_flag = 0;
      if (instream == stdin && stdin_is_tty)
	reinitialize_more_filter ();
      old_chain = make_cleanup ((make_cleanup_func) command_loop_marker, 0);

#if defined(TUI)
      /* A bit of paranoia: I want to make sure the "insert_mode" global
       * is clear except when it is being used for command-line editing
       * (see tuiIO.c, utils.c); otherwise normal output will
       * get messed up in the TUI. So clear it before/after
       * the command-line-input call. - RT
       */
      insert_mode = 0;
#endif
      /* Get a command-line. This calls the readline package. */
      command = command_line_input (instream == stdin ? prompt : (char *) NULL,
				    instream == stdin, "prompt");
#if defined(TUI)
      insert_mode = 0;
#endif
      if (command == 0)
	return;

      time_at_cmd_start = get_run_time ();

      if (display_space)
	{
#ifdef HAVE_SBRK
	  extern char **environ;
	  char *lim = (char *) sbrk (0);

	  space_at_cmd_start = (long) (lim - (char *) &environ);
#endif
	}

      execute_command (command, instream == stdin);
      /* Do any commands attached to breakpoint we stopped at.  */
      bpstat_do_actions (&stop_bpstat);
      do_cleanups (old_chain);

      if (display_time)
	{
	  long cmd_time = get_run_time () - time_at_cmd_start;

	  printf_unfiltered ("Command execution time: %ld.%06ld\n",
			     cmd_time / 1000000, cmd_time % 1000000);
	}

      if (display_space)
	{
#ifdef HAVE_SBRK
	  extern char **environ;
	  char *lim = (char *) sbrk (0);
	  long space_now = lim - (char *) &environ;
	  long space_diff = space_now - space_at_cmd_start;

	  printf_unfiltered ("Space used: %ld (%c%ld for this command)\n",
			     space_now,
			     (space_diff >= 0 ? '+' : '-'),
			     space_diff);
#endif
	}
    }
}

/* Commands call this if they do not want to be repeated by null lines.  */

void
dont_repeat ()
{
  if (server_command)
    return;

  /* If we aren't reading from standard input, we are saving the last
     thing read from stdin in line and don't want to delete it.  Null lines
     won't repeat here in any case.  */
  if (instream == stdin)
    *line = 0;
}

/* Read a line from the stream "instream" without command line editing.

   It prints PRROMPT once at the start.
   Action is compatible with "readline", e.g. space for the result is
   malloc'd and should be freed by the caller.

   A NULL return means end of file.  */
char *
gdb_readline (prrompt)
     char *prrompt;
{
  int c;
  char *result;
  int input_index = 0;
  int result_size = 80;

  if (prrompt)
    {
      /* Don't use a _filtered function here.  It causes the assumed
	 character position to be off, since the newline we read from
	 the user is not accounted for.  */
      fputs_unfiltered (prrompt, gdb_stdout);
#ifdef MPW
      /* Move to a new line so the entered line doesn't have a prompt
	 on the front of it. */
      fputs_unfiltered ("\n", gdb_stdout);
#endif /* MPW */
      gdb_flush (gdb_stdout);
    }

  result = (char *) xmalloc (result_size);

  while (1)
    {
      /* Read from stdin if we are executing a user defined command.
	 This is the right thing for prompt_for_continue, at least.  */
      c = fgetc (instream ? instream : stdin);

      if (c == EOF)
	{
	  if (input_index > 0)
	    /* The last line does not end with a newline.  Return it, and
	       if we are called again fgetc will still return EOF and
	       we'll return NULL then.  */
	    break;
	  free (result);
	  return NULL;
	}

      if (c == '\n')
#ifndef CRLF_SOURCE_FILES
	break;
#else
	{
	  if (input_index > 0 && result[input_index - 1] == '\r')
	    input_index--;
	  break;
	}
#endif

      result[input_index++] = c;
      while (input_index >= result_size)
	{
	  result_size *= 2;
	  result = (char *) xrealloc (result, result_size);
	}
    }

  result[input_index++] = '\0';
  return result;
}

/* Variables which control command line editing and history
   substitution.  These variables are given default values at the end
   of this file.  */
static int command_editing_p;
static int history_expansion_p;
static int write_history_p;
static int history_size;
static char *history_filename;

/* readline uses the word breaks for two things:
   (1) In figuring out where to point the TEXT parameter to the
   rl_completion_entry_function.  Since we don't use TEXT for much,
   it doesn't matter a lot what the word breaks are for this purpose, but
   it does affect how much stuff M-? lists.
   (2) If one of the matches contains a word break character, readline
   will quote it.  That's why we switch between
   gdb_completer_word_break_characters and
   gdb_completer_command_word_break_characters.  I'm not sure when
   we need this behavior (perhaps for funky characters in C++ symbols?).  */

/* Variables which are necessary for fancy command line editing.  */
char *gdb_completer_word_break_characters =
  " \t\n!@#$%^&*()+=|~`}{[]\"';:?/>.<,-";

/* When completing on command names, we remove '-' from the list of
   word break characters, since we use it in command names.  If the
   readline library sees one in any of the current completion strings,
   it thinks that the string needs to be quoted and automatically supplies
   a leading quote. */
char *gdb_completer_command_word_break_characters =
  " \t\n!@#$%^&*()+=|~`}{[]\"';:?/>.<,";

/* Characters that can be used to quote completion strings.  Note that we
   can't include '"' because the gdb C parser treats such quoted sequences
   as strings. */
char *gdb_completer_quote_characters =
  "'";

/* Functions that are used as part of the fancy command line editing.  */

/* This can be used for functions which don't want to complete on symbols
   but don't want to complete on anything else either.  */
/* ARGSUSED */
char **
noop_completer (text, prefix)
     char *text;
     char *prefix;
{
  return NULL;
}

/* Complete on filenames.  */
char **
filename_completer (text, word)
     char *text;
     char *word;
{
  /* From readline.  */
  extern char *filename_completion_function PARAMS ((char *, int));
  int subsequent_name;
  char **return_val;
  int return_val_used;
  int return_val_alloced;

  return_val_used = 0;
  /* Small for testing.  */
  return_val_alloced = 1;
  return_val = (char **) xmalloc (return_val_alloced * sizeof (char *));

  subsequent_name = 0;
  while (1)
    {
      char *p;
      p = filename_completion_function (text, subsequent_name);
      if (return_val_used >= return_val_alloced)
	{
	  return_val_alloced *= 2;
	  return_val =
	    (char **) xrealloc (return_val,
				return_val_alloced * sizeof (char *));
	}
      if (p == NULL)
	{
	  return_val[return_val_used++] = p;
	  break;
	}
      /* Like emacs, don't complete on old versions.  Especially useful
	 in the "source" command.  */
      if (p[strlen (p) - 1] == '~')
	continue;

      {
	char *q;
	if (word == text)
	  /* Return exactly p.  */
	  return_val[return_val_used++] = p;
	else if (word > text)
	  {
	    /* Return some portion of p.  */
	    q = xmalloc (strlen (p) + 5);
	    strcpy (q, p + (word - text));
	    return_val[return_val_used++] = q;
	    free (p);
	  }
	else
	  {
	    /* Return some of TEXT plus p.  */
	    q = xmalloc (strlen (p) + (text - word) + 5);
	    strncpy (q, word, text - word);
	    q[text - word] = '\0';
	    strcat (q, p);
	    return_val[return_val_used++] = q;
	    free (p);
	  }
      }
      subsequent_name = 1;
    }
#if 0
  /* There is no way to do this just long enough to affect quote inserting
     without also affecting the next completion.  This should be fixed in
     readline.  FIXME.  */
  /* Insure that readline does the right thing
     with respect to inserting quotes.  */
  rl_completer_word_break_characters = "";
#endif
  return return_val;
}

/* Here are some useful test cases for completion.  FIXME: These should
   be put in the test suite.  They should be tested with both M-? and TAB.

   "show output-" "radix"
   "show output" "-radix"
   "p" ambiguous (commands starting with p--path, print, printf, etc.)
   "p "  ambiguous (all symbols)
   "info t foo" no completions
   "info t " no completions
   "info t" ambiguous ("info target", "info terminal", etc.)
   "info ajksdlfk" no completions
   "info ajksdlfk " no completions
   "info" " "
   "info " ambiguous (all info commands)
   "p \"a" no completions (string constant)
   "p 'a" ambiguous (all symbols starting with a)
   "p b-a" ambiguous (all symbols starting with a)
   "p b-" ambiguous (all symbols)
   "file Make" "file" (word break hard to screw up here)
   "file ../gdb.stabs/we" "ird" (needs to not break word at slash)
   */

/* Generate completions one by one for the completer.  Each time we are
   called return another potential completion to the caller.
   line_completion just completes on commands or passes the buck to the
   command's completer function, the stuff specific to symbol completion
   is in make_symbol_completion_list.

   TEXT is the caller's idea of the "word" we are looking at.

   MATCHES is the number of matches that have currently been collected from
   calling this completion function.  When zero, then we need to initialize,
   otherwise the initialization has already taken place and we can just
   return the next potential completion string.

   LINE_BUFFER is available to be looked at; it contains the entire text
   of the line.  POINT is the offset in that line of the cursor.  You
   should pretend that the line ends at POINT.

   Returns NULL if there are no more completions, else a pointer to a string
   which is a possible completion, it is the caller's responsibility to
   free the string.  */

static char *
line_completion_function (text, matches, line_buffer, point)
     char *text;
     int matches;
     char *line_buffer;
     int point;
{
  static char **list = (char **)NULL;		/* Cache of completions */
  static int index;				/* Next cached completion */
  char *output = NULL;
  char *tmp_command, *p;
  /* Pointer within tmp_command which corresponds to text.  */
  char *word;
  struct cmd_list_element *c, *result_list;

  if (matches == 0)
    {
      /* The caller is beginning to accumulate a new set of completions, so
	 we need to find all of them now, and cache them for returning one at
	 a time on future calls. */

      if (list)
	{
	  /* Free the storage used by LIST, but not by the strings inside.
	     This is because rl_complete_internal () frees the strings. */
	  free ((PTR)list);
	}
      list = 0;
      index = 0;

      /* Choose the default set of word break characters to break completions.
	 If we later find out that we are doing completions on command strings
	 (as opposed to strings supplied by the individual command completer
	 functions, which can be any string) then we will switch to the
	 special word break set for command strings, which leaves out the
	 '-' character used in some commands.  */

      rl_completer_word_break_characters =
	  gdb_completer_word_break_characters;

      /* Decide whether to complete on a list of gdb commands or on symbols. */
      tmp_command = (char *) alloca (point + 1);
      p = tmp_command;

      strncpy (tmp_command, line_buffer, point);
      tmp_command[point] = '\0';
      /* Since text always contains some number of characters leading up
	 to point, we can find the equivalent position in tmp_command
	 by subtracting that many characters from the end of tmp_command.  */
      word = tmp_command + point - strlen (text);

      if (point == 0)
	{
	  /* An empty line we want to consider ambiguous; that is, it
	     could be any command.  */
	  c = (struct cmd_list_element *) -1;
	  result_list = 0;
	}
      else
	{
	  c = lookup_cmd_1 (&p, cmdlist, &result_list, 1);
	}

      /* Move p up to the next interesting thing.  */
      while (*p == ' ' || *p == '\t')
	{
	  p++;
	}

      if (!c)
	{
	  /* It is an unrecognized command.  So there are no
	     possible completions.  */
	  list = NULL;
	}
      else if (c == (struct cmd_list_element *) -1)
	{
	  char *q;

	  /* lookup_cmd_1 advances p up to the first ambiguous thing, but
	     doesn't advance over that thing itself.  Do so now.  */
	  q = p;
	  while (*q && (isalnum (*q) || *q == '-' || *q == '_'))
	    ++q;
	  if (q != tmp_command + point)
	    {
	      /* There is something beyond the ambiguous
		 command, so there are no possible completions.  For
		 example, "info t " or "info t foo" does not complete
		 to anything, because "info t" can be "info target" or
		 "info terminal".  */
	      list = NULL;
	    }
	  else
	    {
	      /* We're trying to complete on the command which was ambiguous.
		 This we can deal with.  */
	      if (result_list)
		{
		  list = complete_on_cmdlist (*result_list->prefixlist, p,
					      word);
		}
	      else
		{
		  list = complete_on_cmdlist (cmdlist, p, word);
		}
	      /* Insure that readline does the right thing with respect to
		 inserting quotes.  */
	      rl_completer_word_break_characters =
		gdb_completer_command_word_break_characters;
	    }
	}
      else
	{
	  /* We've recognized a full command.  */

	  if (p == tmp_command + point)
	    {
	      /* There is no non-whitespace in the line beyond the command.  */

	      if (p[-1] == ' ' || p[-1] == '\t')
		{
		  /* The command is followed by whitespace; we need to complete
		     on whatever comes after command.  */
		  if (c->prefixlist)
		    {
		      /* It is a prefix command; what comes after it is
			 a subcommand (e.g. "info ").  */
		      list = complete_on_cmdlist (*c->prefixlist, p, word);

		      /* Insure that readline does the right thing
			 with respect to inserting quotes.  */
		      rl_completer_word_break_characters =
			gdb_completer_command_word_break_characters;
		    }
		  else if (c->enums)
		    {
		      list = complete_on_enum (c->enums, p, word);
		      rl_completer_word_break_characters =
			gdb_completer_command_word_break_characters;
		    }
		  else
		    {
		      /* It is a normal command; what comes after it is
			 completed by the command's completer function.  */
		      list = (*c->completer) (p, word);
		    }
		}
	      else
		{
		  /* The command is not followed by whitespace; we need to
		     complete on the command itself.  e.g. "p" which is a
		     command itself but also can complete to "print", "ptype"
		     etc.  */
		  char *q;

		  /* Find the command we are completing on.  */
		  q = p;
		  while (q > tmp_command)
		    {
		      if (isalnum (q[-1]) || q[-1] == '-' || q[-1] == '_')
			--q;
		      else
			break;
		    }

		  list = complete_on_cmdlist (result_list, q, word);

		  /* Insure that readline does the right thing
		     with respect to inserting quotes.  */
		  rl_completer_word_break_characters =
		    gdb_completer_command_word_break_characters;
		}
	    }
	  else
	    {
	      /* There is non-whitespace beyond the command.  */

	      if (c->prefixlist && !c->allow_unknown)
		{
		  /* It is an unrecognized subcommand of a prefix command,
		     e.g. "info adsfkdj".  */
		  list = NULL;
		}
	      else if (c->enums)
		{
		  list = complete_on_enum (c->enums, p, word);
		}
	      else
		{
		  /* It is a normal command.  */
		  list = (*c->completer) (p, word);
		}
	    }
	}
    }

  /* If we found a list of potential completions during initialization then
     dole them out one at a time.  The vector of completions is NULL
     terminated, so after returning the last one, return NULL (and continue
     to do so) each time we are called after that, until a new list is
     available. */

  if (list)
    {
      output = list[index];
      if (output)
	{
	  index++;
	}
    }

#if 0
  /* Can't do this because readline hasn't yet checked the word breaks
     for figuring out whether to insert a quote.  */
  if (output == NULL)
    /* Make sure the word break characters are set back to normal for the
       next time that readline tries to complete something.  */
    rl_completer_word_break_characters =
      gdb_completer_word_break_characters;
#endif

  return (output);
}

/* Line completion interface function for readline.  */

static char *
readline_line_completion_function (text, matches)
     char *text;
     int matches;
{
  return line_completion_function (text, matches, rl_line_buffer, rl_point);
}

/* Skip over a possibly quoted word (as defined by the quote characters
   and word break characters the completer uses).  Returns pointer to the
   location after the "word". */

char *
skip_quoted (str)
     char *str;
{
  char quote_char = '\0';
  char *scan;

  for (scan = str; *scan != '\0'; scan++)
    {
      if (quote_char != '\0')
	{
	  /* Ignore everything until the matching close quote char */
	  if (*scan == quote_char)
	    {
	      /* Found matching close quote. */
	      scan++;
	      break;
	    }
	}
      else if (strchr (gdb_completer_quote_characters, *scan))
	{
	  /* Found start of a quoted string. */
	  quote_char = *scan;
	}
      else if (strchr (gdb_completer_word_break_characters, *scan))
	{
	  break;
	}
    }
  return (scan);
}


#ifdef STOP_SIGNAL
static void
stop_sig (signo)
int signo;
{
#if STOP_SIGNAL == SIGTSTP
  signal (SIGTSTP, SIG_DFL);
  sigsetmask (0);
  kill (getpid (), SIGTSTP);
  signal (SIGTSTP, stop_sig);
#else
  signal (STOP_SIGNAL, stop_sig);
#endif
  printf_unfiltered ("%s", prompt);
  gdb_flush (gdb_stdout);

  /* Forget about any previous command -- null line now will do nothing.  */
  dont_repeat ();
}
#endif /* STOP_SIGNAL */

/* Initialize signal handlers. */
static void
do_nothing (signo)
int signo;
{
  /* Under System V the default disposition of a signal is reinstated after
     the signal is caught and delivered to an application process.  On such
     systems one must restore the replacement signal handler if one wishes
     to continue handling the signal in one's program.  On BSD systems this
     is not needed but it is harmless, and it simplifies the code to just do
     it unconditionally. */
  signal (signo, do_nothing);
}

static void
init_signals ()
{
  signal (SIGINT, request_quit);

  /* If SIGTRAP was set to SIG_IGN, then the SIG_IGN will get passed
     to the inferior and breakpoints will be ignored.  */
#ifdef SIGTRAP
  signal (SIGTRAP, SIG_DFL);
#endif

  /* If we initialize SIGQUIT to SIG_IGN, then the SIG_IGN will get
     passed to the inferior, which we don't want.  It would be
     possible to do a "signal (SIGQUIT, SIG_DFL)" after we fork, but
     on BSD4.3 systems using vfork, that can affect the
     GDB process as well as the inferior (the signal handling tables
     might be in memory, shared between the two).  Since we establish
     a handler for SIGQUIT, when we call exec it will set the signal
     to SIG_DFL for us.  */
  signal (SIGQUIT, do_nothing);
#ifdef SIGHUP
  if (signal (SIGHUP, do_nothing) != SIG_IGN)
    signal (SIGHUP, disconnect);
#endif
  signal (SIGFPE, float_handler);

#if defined(SIGWINCH) && defined(SIGWINCH_HANDLER)
  signal (SIGWINCH, SIGWINCH_HANDLER);
#endif
}

/* Read one line from the command input stream `instream'
   into the local static buffer `linebuffer' (whose current length
   is `linelength').
   The buffer is made bigger as necessary.
   Returns the address of the start of the line.

   NULL is returned for end of file.

   *If* the instream == stdin & stdin is a terminal, the line read
   is copied into the file line saver (global var char *line,
   length linesize) so that it can be duplicated.

   This routine either uses fancy command line editing or
   simple input as the user has requested.  */

char *
command_line_input (prrompt, repeat, annotation_suffix)
     char *prrompt;
     int repeat;
     char *annotation_suffix;
{
  static char *linebuffer = 0;
  static unsigned linelength = 0;
  register char *p;
  char *p1;
  char *rl;
  char *local_prompt = prrompt;
  char *nline;
  char got_eof = 0;

  /* The annotation suffix must be non-NULL.  */
  if (annotation_suffix == NULL)
    annotation_suffix = "";

  if (annotation_level > 1 && instream == stdin)
    {
      local_prompt = alloca ((prrompt == NULL ? 0 : strlen (prrompt))
			     + strlen (annotation_suffix) + 40);
      if (prrompt == NULL)
	local_prompt[0] = '\0';
      else
	strcpy (local_prompt, prrompt);
      strcat (local_prompt, "\n\032\032");
      strcat (local_prompt, annotation_suffix);
      strcat (local_prompt, "\n");
    }

  if (linebuffer == 0)
    {
      linelength = 80;
      linebuffer = (char *) xmalloc (linelength);
    }

  p = linebuffer;

  /* Control-C quits instantly if typed while in this loop
     since it should not wait until the user types a newline.  */
  immediate_quit++;
#ifdef STOP_SIGNAL
  if (job_control)
    signal (STOP_SIGNAL, stop_sig);
#endif

  while (1)
    {
      /* Make sure that all output has been output.  Some machines may let
	 you get away with leaving out some of the gdb_flush, but not all.  */
      wrap_here ("");
      gdb_flush (gdb_stdout);
      gdb_flush (gdb_stderr);

      if (source_file_name != NULL)
	{
	  ++source_line_number;
	  sprintf (source_error,
		   "%s%s:%d: Error in sourced command file:\n",
		   source_pre_error,
		   source_file_name,
		   source_line_number);
	  error_pre_print = source_error;
	}

      if (annotation_level > 1 && instream == stdin)
	{
	  printf_unfiltered ("\n\032\032pre-");
	  printf_unfiltered (annotation_suffix);
	  printf_unfiltered ("\n");
	}

      /* Don't use fancy stuff if not talking to stdin.  */
      if (readline_hook && instream == NULL)
	{
	  rl = (*readline_hook) (local_prompt);
	}
      else if (command_editing_p && instream == stdin && ISATTY (instream))
	{
	  rl = readline (local_prompt);
	}
      else
	{
	  rl = gdb_readline (local_prompt);
	}

      if (annotation_level > 1 && instream == stdin)
	{
	  printf_unfiltered ("\n\032\032post-");
	  printf_unfiltered (annotation_suffix);
	  printf_unfiltered ("\n");
	}

      if (!rl || rl == (char *) EOF)
	{
	  got_eof = 1;
	  break;
	}
      if (strlen(rl) + 1 + (p - linebuffer) > linelength)
	{
	  linelength = strlen(rl) + 1 + (p - linebuffer);
	  nline = (char *) xrealloc (linebuffer, linelength);
	  p += nline - linebuffer;
	  linebuffer = nline;
	}
      p1 = rl;
      /* Copy line.  Don't copy null at end.  (Leaves line alone
         if this was just a newline)  */
      while (*p1)
	*p++ = *p1++;

      free (rl);			/* Allocated in readline.  */

      if (p == linebuffer || *(p - 1) != '\\')
	break;

      p--;			/* Put on top of '\'.  */
      local_prompt = (char *) 0;
  }

#ifdef STOP_SIGNAL
  if (job_control)
    signal (STOP_SIGNAL, SIG_DFL);
#endif
  immediate_quit--;

  if (got_eof)
    return NULL;

#define SERVER_COMMAND_LENGTH 7
  server_command =
    (p - linebuffer > SERVER_COMMAND_LENGTH)
      && STREQN (linebuffer, "server ", SERVER_COMMAND_LENGTH);
  if (server_command)
    {
      /* Note that we don't set `line'.  Between this and the check in
	 dont_repeat, this insures that repeating will still do the
	 right thing.  */
      *p = '\0';
      return linebuffer + SERVER_COMMAND_LENGTH;
    }

  /* Do history expansion if that is wished.  */
  if (history_expansion_p && instream == stdin
      && ISATTY (instream))
    {
      char *history_value;
      int expanded;

      *p = '\0';		/* Insert null now.  */
      expanded = history_expand (linebuffer, &history_value);
      if (expanded)
	{
	  /* Print the changes.  */
	  printf_unfiltered ("%s\n", history_value);

	  /* If there was an error, call this function again.  */
	  if (expanded < 0)
	    {
	      free (history_value);
	      return command_line_input (prrompt, repeat, annotation_suffix);
	    }
	  if (strlen (history_value) > linelength)
	    {
	      linelength = strlen (history_value) + 1;
	      linebuffer = (char *) xrealloc (linebuffer, linelength);
	    }
	  strcpy (linebuffer, history_value);
	  p = linebuffer + strlen(linebuffer);
	  free (history_value);
	}
    }

  /* If we just got an empty line, and that is supposed
     to repeat the previous command, return the value in the
     global buffer.  */
  if (repeat && p == linebuffer)
    return line;
  for (p1 = linebuffer; *p1 == ' ' || *p1 == '\t'; p1++) ;
  if (repeat && !*p1)
    return line;

  *p = 0;

  /* Add line to history if appropriate.  */
  if (instream == stdin
      && ISATTY (stdin) && *linebuffer)
    add_history (linebuffer);

  /* Note: lines consisting solely of comments are added to the command
     history.  This is useful when you type a command, and then
     realize you don't want to execute it quite yet.  You can comment
     out the command and then later fetch it from the value history
     and remove the '#'.  The kill ring is probably better, but some
     people are in the habit of commenting things out.  */
  if (*p1 == '#')
    *p1 = '\0';  /* Found a comment. */

  /* Save into global buffer if appropriate.  */
  if (repeat)
    {
      if (linelength > linesize)
	{
	  line = xrealloc (line, linelength);
	  linesize = linelength;
	}
      strcpy (line, linebuffer);
      return line;
    }

  return linebuffer;
}


/* Expand the body_list of COMMAND so that it can hold NEW_LENGTH
   code bodies.  This is typically used when we encounter an "else"
   clause for an "if" command.  */

static void
realloc_body_list (command, new_length)
     struct command_line *command;
     int new_length;
{
  int n;
  struct command_line **body_list;

  n = command->body_count;

  /* Nothing to do?  */
  if (new_length <= n)
    return;

  body_list = (struct command_line **)
    xmalloc (sizeof (struct command_line *) * new_length);

  memcpy (body_list, command->body_list, sizeof (struct command_line *) * n);

  free (command->body_list);
  command->body_list = body_list;
  command->body_count = new_length;
}

/* Read one line from the input stream.  If the command is an "else" or
   "end", return such an indication to the caller.  */

static enum misc_command_type
read_next_line (command)
     struct command_line **command;
{
  char *p, *p1, *prompt_ptr, control_prompt[256];
  int i = 0;

  if (control_level >= 254)
    error ("Control nesting too deep!\n");

  /* Set a prompt based on the nesting of the control commands.  */
  if (instream == stdin || (instream == 0 && readline_hook != NULL))
    {
      for (i = 0; i < control_level; i++)
	control_prompt[i] = ' ';
      control_prompt[i] = '>';
      control_prompt[i+1] = '\0';
      prompt_ptr = (char *)&control_prompt[0];
    }
  else
    prompt_ptr = NULL;

  p = command_line_input (prompt_ptr, instream == stdin, "commands");

  /* Not sure what to do here.  */
  if (p == NULL)
    return end_command;

  /* Strip leading and trailing whitespace.  */
  while (*p == ' ' || *p == '\t')
    p++;

  p1 = p + strlen (p);
  while (p1 != p && (p1[-1] == ' ' || p1[-1] == '\t'))
    p1--;

  /* Blanks and comments don't really do anything, but we need to
     distinguish them from else, end and other commands which can be
     executed.  */
  if (p1 == p || p[0] == '#')
    return nop_command;
      
  /* Is this the end of a simple, while, or if control structure?  */
  if (p1 - p == 3 && !strncmp (p, "end", 3))
    return end_command;

  /* Is the else clause of an if control structure?  */
  if (p1 - p == 4 && !strncmp (p, "else", 4))
    return else_command;

  /* Check for while, if, break, continue, etc and build a new command
     line structure for them.  */
  if (p1 - p > 5 && !strncmp (p, "while", 5))
    *command = build_command_line (while_control, p + 6);
  else if (p1 - p > 2 && !strncmp (p, "if", 2))
    *command = build_command_line (if_control, p + 3);
  else if (p1 - p == 10 && !strncmp (p, "loop_break", 10))
    {
      *command = (struct command_line *)
	xmalloc (sizeof (struct command_line));
      (*command)->next = NULL;
      (*command)->line = NULL;
      (*command)->control_type = break_control;
      (*command)->body_count = 0;
      (*command)->body_list = NULL;
    }
  else if (p1 - p == 13 && !strncmp (p, "loop_continue", 13))
    {
      *command = (struct command_line *)
	xmalloc (sizeof (struct command_line));
      (*command)->next = NULL;
      (*command)->line = NULL;
      (*command)->control_type = continue_control;
      (*command)->body_count = 0;
      (*command)->body_list = NULL;
    }
  else
    {
      /* A normal command.  */
      *command = (struct command_line *)
	xmalloc (sizeof (struct command_line));
      (*command)->next = NULL;
      (*command)->line = savestring (p, p1 - p);
      (*command)->control_type = simple_control;
      (*command)->body_count = 0;
      (*command)->body_list = NULL;
  }

  /* Nothing special.  */
  return ok_command;
}

/* Recursively read in the control structures and create a command_line 
   structure from them.

   The parent_control parameter is the control structure in which the
   following commands are nested.  */

static enum command_control_type
recurse_read_control_structure (current_cmd)
     struct command_line *current_cmd;
{
  int current_body, i;
  enum misc_command_type val;
  enum command_control_type ret;
  struct command_line **body_ptr, *child_tail, *next;

  child_tail = NULL;
  current_body = 1;

  /* Sanity checks.  */
  if (current_cmd->control_type == simple_control)
    {
      error ("Recursed on a simple control type\n");
      return invalid_control;
    }

  if (current_body > current_cmd->body_count)
    {
      error ("Allocated body is smaller than this command type needs\n");
      return invalid_control;
    }

  /* Read lines from the input stream and build control structures.  */
  while (1)
    {
      dont_repeat ();

      next = NULL;
      val = read_next_line (&next);

      /* Just skip blanks and comments.  */
      if (val == nop_command)
	continue;

      if (val == end_command)
	{
	  if (current_cmd->control_type == while_control
	      || current_cmd->control_type == if_control)
	    {
	      /* Success reading an entire control structure.  */
	      ret = simple_control;
	      break;
	    }
	  else
	    {
	      ret = invalid_control;
	      break;
	    }
	}
      
      /* Not the end of a control structure.  */
      if (val == else_command)
	{
	  if (current_cmd->control_type == if_control
	      && current_body == 1)
	    {
	      realloc_body_list (current_cmd, 2);
	      current_body = 2;
	      child_tail = NULL;
	      continue;
	    }
	  else
	    {
	      ret = invalid_control;
	      break;
	    }
	}

      if (child_tail)
	{
	  child_tail->next = next;
	}
      else
	{
	  body_ptr = current_cmd->body_list;
	  for (i = 1; i < current_body; i++)
	    body_ptr++;

	  *body_ptr = next;

	}

      child_tail = next;

      /* If the latest line is another control structure, then recurse
	 on it.  */
      if (next->control_type == while_control
	  || next->control_type == if_control)
	{
	  control_level++;
	  ret = recurse_read_control_structure (next);
	  control_level--;

	  if (ret != simple_control)
	    break;
	}
    }

  dont_repeat ();

  return ret;
}

/* Read lines from the input stream and accumulate them in a chain of
   struct command_line's, which is then returned.  For input from a
   terminal, the special command "end" is used to mark the end of the
   input, and is not included in the returned chain of commands. */

#define END_MESSAGE "End with a line saying just \"end\"."

struct command_line *
read_command_lines (prompt, from_tty)
char *prompt;
int from_tty;
{
  struct command_line *head, *tail, *next;
  struct cleanup *old_chain;
  enum command_control_type ret;
  enum misc_command_type val;

  if (readline_begin_hook)
    {
      /* Note - intentional to merge messages with no newline */
      (*readline_begin_hook) ("%s  %s\n", prompt, END_MESSAGE);
    }
  else if (from_tty && input_from_terminal_p ())
    {
      printf_unfiltered ("%s\n%s\n", prompt, END_MESSAGE);
      gdb_flush (gdb_stdout);
    }

  head = tail = NULL;
  old_chain = NULL;

  while (1)
    {
      val = read_next_line (&next);

      /* Ignore blank lines or comments.  */
      if (val == nop_command)
	continue;

      if (val == end_command)
	{
	  ret = simple_control;
	  break;
	}

      if (val != ok_command)
	{
	  ret = invalid_control;
	  break;
	}

      if (next->control_type == while_control
	  || next->control_type == if_control)
	{
	  control_level++;
	  ret = recurse_read_control_structure (next);
	  control_level--;

	  if (ret == invalid_control)
	    break;
	}
      
      if (tail)
	{
	  tail->next = next;
	}
      else
	{
	  head = next;
	  old_chain = make_cleanup ((make_cleanup_func) free_command_lines, 
                                    &head);
	}
      tail = next;
    }

  dont_repeat ();

  if (head)
    {
      if (ret != invalid_control)
	{
	  discard_cleanups (old_chain);
	}
      else
	do_cleanups (old_chain);
    }

  if (readline_end_hook)
    {
      (*readline_end_hook) ();
    }
  return (head);
}

/* Free a chain of struct command_line's.  */

void
free_command_lines (lptr)
      struct command_line **lptr;
{
  register struct command_line *l = *lptr;
  register struct command_line *next;
  struct command_line **blist;
  int i;

  while (l)
    {
      if (l->body_count > 0)
	{
	  blist = l->body_list;
	  for (i = 0; i < l->body_count; i++, blist++)
	    free_command_lines (blist);
	}
      next = l->next;
      free (l->line);
      free ((PTR)l);
      l = next;
    }
}

/* Add an element to the list of info subcommands.  */

void
add_info (name, fun, doc)
     char *name;
     void (*fun) PARAMS ((char *, int));
     char *doc;
{
  add_cmd (name, no_class, fun, doc, &infolist);
}

/* Add an alias to the list of info subcommands.  */

void
add_info_alias (name, oldname, abbrev_flag)
     char *name;
     char *oldname;
     int abbrev_flag;
{
  add_alias_cmd (name, oldname, 0, abbrev_flag, &infolist);
}

/* The "info" command is defined as a prefix, with allow_unknown = 0.
   Therefore, its own definition is called only for "info" with no args.  */

/* ARGSUSED */
static void
info_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  printf_unfiltered ("\"info\" must be followed by the name of an info command.\n");
  help_list (infolist, "info ", -1, gdb_stdout);
}

/* The "complete" command is used by Emacs to implement completion.  */

/* ARGSUSED */
static void
complete_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  int i;
  int argpoint;
  char *completion;

  dont_repeat ();

  if (arg == NULL)
    arg = "";
  argpoint = strlen (arg);

  for (completion = line_completion_function (arg, i = 0, arg, argpoint);
       completion;
       completion = line_completion_function (arg, ++i, arg, argpoint))
    {
      printf_unfiltered ("%s\n", completion);
      free (completion);
    }
}

/* The "show" command with no arguments shows all the settings.  */

/* ARGSUSED */
static void
show_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  cmd_show_list (showlist, from_tty, "");
}

/* Add an element to the list of commands.  */

void
add_com (name, class, fun, doc)
     char *name;
     enum command_class class;
     void (*fun) PARAMS ((char *, int));
     char *doc;
{
  add_cmd (name, class, fun, doc, &cmdlist);
}

/* Add an alias or abbreviation command to the list of commands.  */

void
add_com_alias (name, oldname, class, abbrev_flag)
     char *name;
     char *oldname;
     enum command_class class;
     int abbrev_flag;
{
  add_alias_cmd (name, oldname, class, abbrev_flag, &cmdlist);
}

void
error_no_arg (why)
     char *why;
{
  error ("Argument required (%s).", why);
}

/* ARGSUSED */
static void
help_command (command, from_tty)
     char *command;
     int from_tty; /* Ignored */
{
  help_cmd (command, gdb_stdout);
}

static void
validate_comname (comname)
     char *comname;
{
  register char *p;

  if (comname == 0)
    error_no_arg ("name of command to define");

  p = comname;
  while (*p)
    {
      if (!isalnum(*p) && *p != '-' && *p != '_')
	error ("Junk in argument list: \"%s\"", p);
      p++;
    }
}

/* This is just a placeholder in the command data structures.  */
static void
user_defined_command (ignore, from_tty)
     char *ignore;
     int from_tty;
{
}

static void
define_command (comname, from_tty)
     char *comname;
     int from_tty;
{
  register struct command_line *cmds;
  register struct cmd_list_element *c, *newc, *hookc = 0;
  char *tem = comname;
  char tmpbuf[128];
#define	HOOK_STRING	"hook-"
#define	HOOK_LEN 5

  validate_comname (comname);

  /* Look it up, and verify that we got an exact match.  */
  c = lookup_cmd (&tem, cmdlist, "", -1, 1);
  if (c && !STREQ (comname, c->name))
    c = 0;

  if (c)
    {
      if (c->class == class_user || c->class == class_alias)
	tem = "Redefine command \"%s\"? ";
      else
	tem = "Really redefine built-in command \"%s\"? ";
      if (!query (tem, c->name))
	error ("Command \"%s\" not redefined.", c->name);
    }

  /* If this new command is a hook, then mark the command which it
     is hooking.  Note that we allow hooking `help' commands, so that
     we can hook the `stop' pseudo-command.  */

  if (!strncmp (comname, HOOK_STRING, HOOK_LEN))
    {
      /* Look up cmd it hooks, and verify that we got an exact match.  */
      tem = comname+HOOK_LEN;
      hookc = lookup_cmd (&tem, cmdlist, "", -1, 0);
      if (hookc && !STREQ (comname+HOOK_LEN, hookc->name))
	hookc = 0;
      if (!hookc)
	{
	  warning ("Your new `%s' command does not hook any existing command.",
		   comname);
	  if (!query ("Proceed? "))
	    error ("Not confirmed.");
	}
    }

  comname = savestring (comname, strlen (comname));

  /* If the rest of the commands will be case insensitive, this one
     should behave in the same manner. */
  for (tem = comname; *tem; tem++)
    if (isupper(*tem)) *tem = tolower(*tem);

  control_level = 0;
  sprintf (tmpbuf, "Type commands for definition of \"%s\".", comname);
  cmds = read_command_lines (tmpbuf, from_tty);

  if (c && c->class == class_user)
    free_command_lines (&c->user_commands);

  newc = add_cmd (comname, class_user, user_defined_command,
	   (c && c->class == class_user)
	   ? c->doc : savestring ("User-defined.", 13), &cmdlist);
  newc->user_commands = cmds;

  /* If this new command is a hook, then mark both commands as being
     tied.  */
  if (hookc)
    {
      hookc->hook = newc;	/* Target gets hooked.  */
      newc->hookee = hookc;	/* We are marked as hooking target cmd.  */
    }
}

static void
document_command (comname, from_tty)
     char *comname;
     int from_tty;
{
  struct command_line *doclines;
  register struct cmd_list_element *c;
  char *tem = comname;
  char tmpbuf[128];

  validate_comname (comname);

  c = lookup_cmd (&tem, cmdlist, "", 0, 1);

  if (c->class != class_user)
    error ("Command \"%s\" is built-in.", comname);

  sprintf (tmpbuf, "Type documentation for \"%s\".", comname);
  doclines = read_command_lines (tmpbuf, from_tty);

  if (c->doc) free (c->doc);

  {
    register struct command_line *cl1;
    register int len = 0;

    for (cl1 = doclines; cl1; cl1 = cl1->next)
      len += strlen (cl1->line) + 1;

    c->doc = (char *) xmalloc (len + 1);
    *c->doc = 0;

    for (cl1 = doclines; cl1; cl1 = cl1->next)
      {
	strcat (c->doc, cl1->line);
	if (cl1->next)
	  strcat (c->doc, "\n");
      }
  }

  free_command_lines (&doclines);
}

/* Print the GDB banner. */
void
print_gdb_version (stream)
  GDB_FILE *stream;
{
  /* From GNU coding standards, first line is meant to be easy for a
     program to parse, and is just canonical program name and version
     number, which starts after last space. */

  fprintf_filtered (stream, "GNU gdb %s\n", version);

  /* Second line is a copyright notice. */

  fprintf_filtered (stream, "Copyright 1998 Free Software Foundation, Inc.\n");

  /* Following the copyright is a brief statement that the program is
     free software, that users are free to copy and change it on
     certain conditions, that it is covered by the GNU GPL, and that
     there is no warranty. */

  fprintf_filtered (stream, "\
GDB is free software, covered by the GNU General Public License, and you are\n\
welcome to change it and/or distribute copies of it under certain conditions.\n\
Type \"show copying\" to see the conditions.\n\
There is absolutely no warranty for GDB.  Type \"show warranty\" for details.\n");

  /* After the required info we print the configuration information. */

  fprintf_filtered (stream, "This GDB was configured as \"");
  if (!STREQ (host_name, target_name))
    {
      fprintf_filtered (stream, "--host=%s --target=%s", host_name, target_name);
    }
  else
    {
      fprintf_filtered (stream, "%s", host_name);
    }
  fprintf_filtered (stream, "\".");
}

/* ARGSUSED */
static void
show_version (args, from_tty)
     char *args;
     int from_tty;
{
  immediate_quit++;
  print_gdb_version (gdb_stdout);
  printf_filtered ("\n");
  immediate_quit--;
}

/* xgdb calls this to reprint the usual GDB prompt.  Obsolete now that xgdb
   is obsolete.  */

void
print_prompt ()
{
  printf_unfiltered ("%s", prompt);
  gdb_flush (gdb_stdout);
}

/* This replaces the above for the frontends: it returns a pointer
   to the prompt. */
char *
get_prompt ()
{
  return prompt;
}

void
set_prompt (s)
     char *s;
{
/* ??rehrauer: I don't know why this fails, since it looks as though
   assignments to prompt are wrapped in calls to savestring...
  if (prompt != NULL)
    free (prompt);
*/
  prompt = savestring (s, strlen (s));
}


/* If necessary, make the user confirm that we should quit.  Return
   non-zero if we should quit, zero if we shouldn't.  */

int
quit_confirm ()
{
  if (inferior_pid != 0 && target_has_execution)
    {
      char *s;

      /* This is something of a hack.  But there's no reliable way to
	 see if a GUI is running.  The `use_windows' variable doesn't
	 cut it.  */
      if (init_ui_hook)
	s = "A debugging session is active.\nDo you still want to close the debugger?";
      else if (attach_flag)
	s = "The program is running.  Quit anyway (and detach it)? ";
      else
	s = "The program is running.  Exit anyway? ";

      if (! query (s))
	return 0;
    }

  return 1;
}

/* Quit without asking for confirmation.  */

void
quit_force (args, from_tty)
     char *args;
     int from_tty;
{
  int exit_code = 0;

  /* An optional expression may be used to cause gdb to terminate with the 
     value of that expression. */
  if (args)
    {
      value_ptr val = parse_and_eval (args);

      exit_code = (int) value_as_long (val);
    }

  if (inferior_pid != 0 && target_has_execution)
    {
      if (attach_flag)
	target_detach (args, from_tty);
      else
	target_kill ();
    }

  /* UDI wants this, to kill the TIP.  */
  target_close (1);

  /* Save the history information if it is appropriate to do so.  */
  if (write_history_p && history_filename)
    write_history (history_filename);

  do_final_cleanups(ALL_CLEANUPS);	/* Do any final cleanups before exiting */

#if defined(TUI)
  /* tuiDo((TuiOpaqueFuncPtr)tuiCleanUp); */
  /* The above does not need to be inside a tuiDo(), since
   * it is not manipulating the curses screen, but rather,
   * it is tearing it down.
   */
  if (tui_version)
    tuiCleanUp();
#endif

  exit (exit_code);
}

/* Handle the quit command.  */

void
quit_command (args, from_tty)
     char *args;
     int from_tty;
{
  if (! quit_confirm ())
    error ("Not confirmed.");
  quit_force (args, from_tty);
}

/* Returns whether GDB is running on a terminal and whether the user
   desires that questions be asked of them on that terminal.  */

int
input_from_terminal_p ()
{
  return gdb_has_a_terminal () && (instream == stdin) & caution;
}

/* ARGSUSED */
static void
pwd_command (args, from_tty)
     char *args;
     int from_tty;
{
  if (args) error ("The \"pwd\" command does not take an argument: %s", args);
  getcwd (gdb_dirbuf, sizeof (gdb_dirbuf));

  if (!STREQ (gdb_dirbuf, current_directory))
    printf_unfiltered ("Working directory %s\n (canonically %s).\n",
	    current_directory, gdb_dirbuf);
  else
    printf_unfiltered ("Working directory %s.\n", current_directory);
}

void
cd_command (dir, from_tty)
     char *dir;
     int from_tty;
{
  int len;
  /* Found something other than leading repetitions of "/..".  */
  int found_real_path;
  char *p;

  /* If the new directory is absolute, repeat is a no-op; if relative,
     repeat might be useful but is more likely to be a mistake.  */
  dont_repeat ();

  if (dir == 0)
    error_no_arg ("new working directory");

  dir = tilde_expand (dir);
  make_cleanup (free, dir);

  if (chdir (dir) < 0)
    perror_with_name (dir);

  len = strlen (dir);
  dir = savestring (dir, len - (len > 1 && SLASH_P(dir[len-1])));
  if (ROOTED_P(dir))
    current_directory = dir;
  else
    {
      if (SLASH_P (current_directory[0]) && current_directory[1] == '\0')
	current_directory = concat (current_directory, dir, NULL);
      else
	current_directory = concat (current_directory, SLASH_STRING, dir, NULL);
      free (dir);
    }

  /* Now simplify any occurrences of `.' and `..' in the pathname.  */

  found_real_path = 0;
  for (p = current_directory; *p;)
    {
      if (SLASH_P (p[0]) && p[1] == '.' && (p[2] == 0 || SLASH_P (p[2])))
	strcpy (p, p + 2);
      else if (SLASH_P (p[0]) && p[1] == '.' && p[2] == '.'
	       && (p[3] == 0 || SLASH_P (p[3])))
	{
	  if (found_real_path)
	    {
	      /* Search backwards for the directory just before the "/.."
		 and obliterate it and the "/..".  */
	      char *q = p;
	      while (q != current_directory && ! SLASH_P (q[-1]))
		--q;

	      if (q == current_directory)
		/* current_directory is
		   a relative pathname ("can't happen"--leave it alone).  */
		++p;
	      else
		{
		  strcpy (q - 1, p + 3);
		  p = q - 1;
		}
	    }
	  else
	    /* We are dealing with leading repetitions of "/..", for example
	       "/../..", which is the Mach super-root.  */
	    p += 3;
	}
      else
	{
	  found_real_path = 1;
	  ++p;
	}
    }

  forget_cached_source_info ();

  if (from_tty)
    pwd_command ((char *) 0, 1);
}

struct source_cleanup_lines_args {
  int old_line;
  char *old_file;
  char *old_pre_error;
  char *old_error_pre_print;
};

static void
source_cleanup_lines (args)
     PTR args;
{
  struct source_cleanup_lines_args *p =
    (struct source_cleanup_lines_args *)args;
  source_line_number = p->old_line;
  source_file_name = p->old_file;
  source_pre_error = p->old_pre_error;
  error_pre_print = p->old_error_pre_print;
}

/* ARGSUSED */
void
source_command (args, from_tty)
     char *args;
     int from_tty;
{
  FILE *stream;
  struct cleanup *old_cleanups;
  char *file = args;
  struct source_cleanup_lines_args old_lines;
  int needed_length;

  if (file == NULL)
    {
      error ("source command requires pathname of file to source.");
    }

  file = tilde_expand (file);
  old_cleanups = make_cleanup (free, file);

  stream = fopen (file, FOPEN_RT);
  if (!stream)
    if (from_tty)
      perror_with_name (file);
    else
      return;

  make_cleanup ((make_cleanup_func) fclose, stream);

  old_lines.old_line = source_line_number;
  old_lines.old_file = source_file_name;
  old_lines.old_pre_error = source_pre_error;
  old_lines.old_error_pre_print = error_pre_print;
  make_cleanup (source_cleanup_lines, &old_lines);
  source_line_number = 0;
  source_file_name = file;
  source_pre_error = error_pre_print == NULL ? "" : error_pre_print;
  source_pre_error = savestring (source_pre_error, strlen (source_pre_error));
  make_cleanup (free, source_pre_error);
  /* This will get set every time we read a line.  So it won't stay "" for
     long.  */
  error_pre_print = "";

  needed_length = strlen (source_file_name) + strlen (source_pre_error) + 80;
  if (source_error_allocated < needed_length)
    {
      source_error_allocated *= 2;
      if (source_error_allocated < needed_length)
	source_error_allocated = needed_length;
      if (source_error == NULL)
	source_error = xmalloc (source_error_allocated);
      else
	source_error = xrealloc (source_error, source_error_allocated);
    }

  read_command_file (stream);

  do_cleanups (old_cleanups);
}

/* ARGSUSED */
static void
echo_command (text, from_tty)
     char *text;
     int from_tty;
{
  char *p = text;
  register int c;

  if (text)
    while ((c = *p++) != '\0')
      {
	if (c == '\\')
	  {
	    /* \ at end of argument is used after spaces
	       so they won't be lost.  */
	    if (*p == 0)
	      return;

	    c = parse_escape (&p);
	    if (c >= 0)
	      printf_filtered ("%c", c);
	  }
	else
	  printf_filtered ("%c", c);
      }

  /* Force this output to appear now.  */
  wrap_here ("");
  gdb_flush (gdb_stdout);
}

/* ARGSUSED */
static void
dont_repeat_command (ignored, from_tty)
     char *ignored;
     int from_tty;
{
  *line = 0;		/* Can't call dont_repeat here because we're not
			   necessarily reading from stdin.  */
}

/* Functions to manipulate command line editing control variables.  */

/* Number of commands to print in each call to show_commands.  */
#define Hist_print 10
static void
show_commands (args, from_tty)
     char *args;
     int from_tty;
{
  /* Index for history commands.  Relative to history_base.  */
  int offset;

  /* Number of the history entry which we are planning to display next.
     Relative to history_base.  */
  static int num = 0;

  /* The first command in the history which doesn't exist (i.e. one more
     than the number of the last command).  Relative to history_base.  */
  int hist_len;

  extern HIST_ENTRY *history_get PARAMS ((int));

  /* Print out some of the commands from the command history.  */
  /* First determine the length of the history list.  */
  hist_len = history_size;
  for (offset = 0; offset < history_size; offset++)
    {
      if (!history_get (history_base + offset))
	{
	  hist_len = offset;
	  break;
	}
    }

  if (args)
    {
      if (args[0] == '+' && args[1] == '\0')
	/* "info editing +" should print from the stored position.  */
	;
      else
	/* "info editing <exp>" should print around command number <exp>.  */
	num = (parse_and_eval_address (args) - history_base) - Hist_print / 2;
    }
  /* "show commands" means print the last Hist_print commands.  */
  else
    {
      num = hist_len - Hist_print;
    }

  if (num < 0)
    num = 0;

  /* If there are at least Hist_print commands, we want to display the last
     Hist_print rather than, say, the last 6.  */
  if (hist_len - num < Hist_print)
    {
      num = hist_len - Hist_print;
      if (num < 0)
	num = 0;
    }

  for (offset = num; offset < num + Hist_print && offset < hist_len; offset++)
    {
      printf_filtered ("%5d  %s\n", history_base + offset,
	      (history_get (history_base + offset))->line);
    }

  /* The next command we want to display is the next one that we haven't
     displayed yet.  */
  num += Hist_print;

  /* If the user repeats this command with return, it should do what
     "show commands +" does.  This is unnecessary if arg is null,
     because "show commands +" is not useful after "show commands".  */
  if (from_tty && args)
    {
      args[0] = '+';
      args[1] = '\0';
    }
}

/* Called by do_setshow_command.  */
/* ARGSUSED */
static void
set_history_size_command (args, from_tty, c)
     char *args;
     int from_tty;
     struct cmd_list_element *c;
{
  if (history_size == INT_MAX)
    unstifle_history ();
  else if (history_size >= 0)
    stifle_history (history_size);
  else
    {
      history_size = INT_MAX;
      error ("History size must be non-negative");
    }
}

/* ARGSUSED */
static void
set_history (args, from_tty)
     char *args;
     int from_tty;
{
  printf_unfiltered ("\"set history\" must be followed by the name of a history subcommand.\n");
  help_list (sethistlist, "set history ", -1, gdb_stdout);
}

/* ARGSUSED */
static void
show_history (args, from_tty)
     char *args;
     int from_tty;
{
  cmd_show_list (showhistlist, from_tty, "");
}

int info_verbose = 0;		/* Default verbose msgs off */

/* Called by do_setshow_command.  An elaborate joke.  */
/* ARGSUSED */
static void
set_verbose (args, from_tty, c)
     char *args;
     int from_tty;
     struct cmd_list_element *c;
{
  char *cmdname = "verbose";
  struct cmd_list_element *showcmd;

  showcmd = lookup_cmd_1 (&cmdname, showlist, NULL, 1);

  if (info_verbose)
    {
      c->doc = "Set verbose printing of informational messages.";
      showcmd->doc = "Show verbose printing of informational messages.";
    }
  else
    {
      c->doc = "Set verbosity.";
      showcmd->doc = "Show verbosity.";
    }
}

static void
float_handler (signo)
int signo;
{
  /* This message is based on ANSI C, section 4.7.  Note that integer
     divide by zero causes this, so "float" is a misnomer.  */
  signal (SIGFPE, float_handler);
  error ("Erroneous arithmetic operation.");
}


static void
init_cmd_lists ()
{
  cmdlist = NULL;
  infolist = NULL;
  enablelist = NULL;
  disablelist = NULL;
  togglelist = NULL;
  stoplist = NULL;
  deletelist = NULL;
  enablebreaklist = NULL;
  setlist = NULL;
  unsetlist = NULL;
  showlist = NULL;
  sethistlist = NULL;
  showhistlist = NULL;
  unsethistlist = NULL;
#if MAINTENANCE_CMDS
  maintenancelist = NULL;
  maintenanceinfolist = NULL;
  maintenanceprintlist = NULL;
#endif
  setprintlist = NULL;
  showprintlist = NULL;
  setchecklist = NULL;
  showchecklist = NULL;
}

/* Init the history buffer.  Note that we are called after the init file(s)
 * have been read so that the user can change the history file via his
 * .gdbinit file (for instance).  The GDBHISTFILE environment variable
 * overrides all of this.
 */

void
init_history()
{
  char *tmpenv;

  tmpenv = getenv ("HISTSIZE");
  if (tmpenv)
    history_size = atoi (tmpenv);
  else if (!history_size)
    history_size = 256;

  stifle_history (history_size);

  tmpenv = getenv ("GDBHISTFILE");
  if (tmpenv)
    history_filename = savestring (tmpenv, strlen(tmpenv));
  else if (!history_filename) {
    /* We include the current directory so that if the user changes
       directories the file written will be the same as the one
       that was read.  */
    history_filename = concat (current_directory, "/.gdb_history", NULL);
  }
  read_history (history_filename);
}

static void
init_main ()
{
  struct cmd_list_element *c;

#ifdef DEFAULT_PROMPT
  prompt = savestring (DEFAULT_PROMPT, strlen(DEFAULT_PROMPT));
#else
  prompt = savestring ("(gdb) ", 6);
#endif

  /* Set the important stuff up for command editing.  */
  command_editing_p = 1;
  history_expansion_p = 0;
  write_history_p = 0;

  /* Setup important stuff for command line editing.  */
  rl_completion_entry_function = (int (*)()) readline_line_completion_function;
  rl_completer_word_break_characters = gdb_completer_word_break_characters;
  rl_completer_quote_characters = gdb_completer_quote_characters;
  rl_readline_name = "gdb";

  /* Define the classes of commands.
     They will appear in the help list in the reverse of this order.  */

  add_cmd ("internals", class_maintenance, NO_FUNCTION,
	   "Maintenance commands.\n\
Some gdb commands are provided just for use by gdb maintainers.\n\
These commands are subject to frequent change, and may not be as\n\
well documented as user commands.",
	   &cmdlist);
  add_cmd ("obscure", class_obscure, NO_FUNCTION, "Obscure features.", &cmdlist);
  add_cmd ("aliases", class_alias, NO_FUNCTION, "Aliases of other commands.", &cmdlist);
  add_cmd ("user-defined", class_user, NO_FUNCTION, "User-defined commands.\n\
The commands in this class are those defined by the user.\n\
Use the \"define\" command to define a command.", &cmdlist);
  add_cmd ("support", class_support, NO_FUNCTION, "Support facilities.", &cmdlist);
  if (!dbx_commands)
    add_cmd ("status", class_info, NO_FUNCTION, "Status inquiries.", &cmdlist);
  add_cmd ("files", class_files, NO_FUNCTION, "Specifying and examining files.", &cmdlist);
  add_cmd ("breakpoints", class_breakpoint, NO_FUNCTION, "Making program stop at certain points.", &cmdlist);
  add_cmd ("data", class_vars, NO_FUNCTION, "Examining data.", &cmdlist);
  add_cmd ("stack", class_stack, NO_FUNCTION, "Examining the stack.\n\
The stack is made up of stack frames.  Gdb assigns numbers to stack frames\n\
counting from zero for the innermost (currently executing) frame.\n\n\
At any time gdb identifies one frame as the \"selected\" frame.\n\
Variable lookups are done with respect to the selected frame.\n\
When the program being debugged stops, gdb selects the innermost frame.\n\
The commands below can be used to select other frames by number or address.",
	   &cmdlist);
  add_cmd ("running", class_run, NO_FUNCTION, "Running the program.", &cmdlist);

  add_com ("pwd", class_files, pwd_command,
	   "Print working directory.  This is used for your program as well.");
  c = add_cmd ("cd", class_files, cd_command,
	   "Set working directory to DIR for debugger and program being debugged.\n\
The change does not take effect for the program being debugged\n\
until the next time it is started.", &cmdlist);
  c->completer = filename_completer;

  add_show_from_set
    (add_set_cmd ("prompt", class_support, var_string, (char *)&prompt,
	   "Set gdb's prompt",
	   &setlist),
     &showlist);

  add_com ("echo", class_support, echo_command,
	   "Print a constant string.  Give string as argument.\n\
C escape sequences may be used in the argument.\n\
No newline is added at the end of the argument;\n\
use \"\\n\" if you want a newline to be printed.\n\
Since leading and trailing whitespace are ignored in command arguments,\n\
if you want to print some you must use \"\\\" before leading whitespace\n\
to be printed or after trailing whitespace.");
  add_com ("document", class_support, document_command,
	   "Document a user-defined command.\n\
Give command name as argument.  Give documentation on following lines.\n\
End with a line of just \"end\".");
  add_com ("define", class_support, define_command,
	   "Define a new command name.  Command name is argument.\n\
Definition appears on following lines, one command per line.\n\
End with a line of just \"end\".\n\
Use the \"document\" command to give documentation for the new command.\n\
Commands defined in this way may have up to ten arguments.");

#ifdef __STDC__
  c = add_cmd ("source", class_support, source_command,
	   "Read commands from a file named FILE.\n\
Note that the file \"" GDBINIT_FILENAME "\" is read automatically in this way\n\
when gdb is started.", &cmdlist);
#else
  /* Punt file name, we can't help it easily.  */
  c = add_cmd ("source", class_support, source_command,
	   "Read commands from a file named FILE.\n\
Note that the file \".gdbinit\" is read automatically in this way\n\
when gdb is started.", &cmdlist);
#endif
  c->completer = filename_completer;

  add_com ("quit", class_support, quit_command, "Exit gdb.");
  add_com ("help", class_support, help_command, "Print list of commands.");
  add_com_alias ("q", "quit", class_support, 1);
  add_com_alias ("h", "help", class_support, 1);

  add_com ("dont-repeat", class_support, dont_repeat_command, "Don't repeat this command.\n\
Primarily used inside of user-defined commands that should not be repeated when\n\
hitting return.");

  c = add_set_cmd ("verbose", class_support, var_boolean, (char *)&info_verbose,
		   "Set ",
		   &setlist),
  add_show_from_set (c, &showlist);
  c->function.sfunc = set_verbose;
  set_verbose (NULL, 0, c);

  add_show_from_set
    (add_set_cmd ("editing", class_support, var_boolean, (char *)&command_editing_p,
	   "Set editing of command lines as they are typed.\n\
Use \"on\" to enable the editing, and \"off\" to disable it.\n\
Without an argument, command line editing is enabled.  To edit, use\n\
EMACS-like or VI-like commands like control-P or ESC.", &setlist),
     &showlist);

  add_prefix_cmd ("history", class_support, set_history,
		  "Generic command for setting command history parameters.",
		  &sethistlist, "set history ", 0, &setlist);
  add_prefix_cmd ("history", class_support, show_history,
		  "Generic command for showing command history parameters.",
		  &showhistlist, "show history ", 0, &showlist);

  add_show_from_set
    (add_set_cmd ("expansion", no_class, var_boolean, (char *)&history_expansion_p,
	   "Set history expansion on command input.\n\
Without an argument, history expansion is enabled.", &sethistlist),
     &showhistlist);

  add_show_from_set
    (add_set_cmd ("save", no_class, var_boolean, (char *)&write_history_p,
	   "Set saving of the history record on exit.\n\
Use \"on\" to enable the saving, and \"off\" to disable it.\n\
Without an argument, saving is enabled.", &sethistlist),
     &showhistlist);

  c = add_set_cmd ("size", no_class, var_integer, (char *)&history_size,
		   "Set the size of the command history, \n\
ie. the number of previous commands to keep a record of.", &sethistlist);
  add_show_from_set (c, &showhistlist);
  c->function.sfunc = set_history_size_command;

  add_show_from_set
    (add_set_cmd ("filename", no_class, var_filename, (char *)&history_filename,
	   "Set the filename in which to record the command history\n\
 (the list of previous commands of which a record is kept).", &sethistlist),
     &showhistlist);

  add_show_from_set
    (add_set_cmd ("confirm", class_support, var_boolean,
		  (char *)&caution,
		  "Set whether to confirm potentially dangerous operations.",
		  &setlist),
     &showlist);

  add_prefix_cmd ("info", class_info, info_command,
        "Generic command for showing things about the program being debugged.",
		  &infolist, "info ", 0, &cmdlist);
  add_com_alias ("i", "info", class_info, 1);

  add_com ("complete", class_obscure, complete_command,
	   "List the completions for the rest of the line as a command.");

  add_prefix_cmd ("show", class_info, show_command,
		  "Generic command for showing things about the debugger.",
		  &showlist, "show ", 0, &cmdlist);
  /* Another way to get at the same thing.  */
  add_info ("set", show_command, "Show all GDB settings.");

  add_cmd ("commands", no_class, show_commands,
	   "Show the history of commands you typed.\n\
You can supply a command number to start with, or a `+' to start after\n\
the previous command number shown.",
	   &showlist);

  add_cmd ("version", no_class, show_version,
	   "Show what version of GDB this is.", &showlist);

  add_com ("while", class_support, while_command,
"Execute nested commands WHILE the conditional expression is non zero.\n\
The conditional expression must follow the word `while' and must in turn be\n\
followed by a new line.  The nested commands must be entered one per line,\n\
and should be terminated by the word `end'.");

  add_com ("if", class_support, if_command,
"Execute nested commands once IF the conditional expression is non zero.\n\
The conditional expression must follow the word `if' and must in turn be\n\
followed by a new line.  The nested commands must be entered one per line,\n\
and should be terminated by the word 'else' or `end'.  If an else clause\n\
is used, the same rules apply to its nested commands as to the first ones.");

  /* If target is open when baud changes, it doesn't take effect until the
     next open (I think, not sure).  */
  add_show_from_set (add_set_cmd ("remotebaud", no_class,
				  var_zinteger, (char *)&baud_rate,
				  "Set baud rate for remote serial I/O.\n\
This value is used to set the speed of the serial port when debugging\n\
using remote targets.", &setlist),
		     &showlist);

  add_show_from_set (
    add_set_cmd ("remotedebug", no_class, var_zinteger, (char *)&remote_debug,
		   "Set debugging of remote protocol.\n\
When enabled, each packet sent or received with the remote target\n\
is displayed.", &setlist),
		     &showlist);

  add_show_from_set (
    add_set_cmd ("remotetimeout", no_class, var_integer, (char *)&remote_timeout,
		   "Set timeout limit to wait for target to respond.\n\
This value is used to set the time limit for gdb to wait for a response\n\
from the target.", &setlist),
		     &showlist);

  c = add_set_cmd ("annotate", class_obscure, var_zinteger, 
		   (char *)&annotation_level, "Set annotation_level.\n\
0 == normal;     1 == fullname (for use when running under emacs)\n\
2 == output annotated suitably for use by programs that control GDB.",
		 &setlist);
  c = add_show_from_set (c, &showlist);
}
