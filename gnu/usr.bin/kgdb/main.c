/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)main.c	6.6 (Berkeley) 5/13/91";
#endif /* not lint */

/* Top level for GDB, the GNU debugger.
   Copyright (C) 1986, 1987, 1988, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include "defs.h"
#include "command.h"
#include "param.h"
#include "expression.h"

#ifdef USG
#include <sys/types.h>
#include <unistd.h>
#endif

#include <sys/file.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ctype.h>

#ifdef SET_STACK_LIMIT_HUGE
#include <sys/time.h>
#include <sys/resource.h>

int original_stack_limit;
#endif

/* If this definition isn't overridden by the header files, assume
   that isatty and fileno exist on this system.  */
#ifndef ISATTY
#define ISATTY(FP)	(isatty (fileno (FP)))
#endif

extern void free ();

/* Version number of GDB, as a string.  */

extern char *version;

/*
 * Declare all cmd_list_element's
 */

/* Chain containing all defined commands.  */

struct cmd_list_element *cmdlist;

/* Chain containing all defined info subcommands.  */

struct cmd_list_element *infolist;

/* Chain containing all defined enable subcommands. */

struct cmd_list_element *enablelist;

/* Chain containing all defined disable subcommands. */

struct cmd_list_element *disablelist;

/* Chain containing all defined delete subcommands. */

struct cmd_list_element *deletelist;

/* Chain containing all defined "enable breakpoint" subcommands. */

struct cmd_list_element *enablebreaklist;

/* Chain containing all defined set subcommands */

struct cmd_list_element *setlist;

/* Chain containing all defined \"set history\".  */

struct cmd_list_element *sethistlist;

/* Chain containing all defined \"unset history\".  */

struct cmd_list_element *unsethistlist;

/* stdio stream that command input is being read from.  */

FILE *instream;

/* Current working directory.  */

char *current_directory;

/* The directory name is actually stored here (usually).  */
static char dirbuf[MAXPATHLEN];

#ifdef KERNELDEBUG
/* Nonzero if we're debugging /dev/mem or a kernel crash dump */

int kernel_debugging = 1;
#endif

/* Nonzero to inhibit confirmation of quitting or restarting 
   a stopped inferior. */
int inhibit_confirm;
  
/* Nonzero if we can write in text or core file */

int writeable_text;

/* The number of lines on a page, and the number of spaces
   in a line.  */
int linesize, pagesize;

/* Nonzero if we should refrain from using an X window.  */

int inhibit_windows = 0;

/* Function to call before reading a command, if nonzero.
   The function receives two args: an input stream,
   and a prompt string.  */
   
void (*window_hook) ();

extern int frame_file_full_name;
int xgdb_verbose;

void execute_command();
void free_command_lines ();
char *gdb_readline ();
char *command_line_input ();
static void initialize_main ();
static void initialize_cmd_lists ();
void command_loop ();
static void source_command ();
static void print_gdb_version ();
static void float_handler ();
static void cd_command ();

char *getenv ();

/* gdb prints this when reading a command interactively */
static char *prompt;

/* Buffer used for reading command lines, and the size
   allocated for it so far.  */

char *line;
int linesize;


/* This is how `error' returns to command level.  */

jmp_buf to_top_level;

void
return_to_top_level ()
{
  quit_flag = 0;
  immediate_quit = 0;
  clear_breakpoint_commands ();
  clear_momentary_breakpoints ();
  disable_current_display ();
  do_cleanups (0);
  longjmp (to_top_level, 1);
}

/* Call FUNC with arg ARG, catching any errors.
   If there is no error, return the value returned by FUNC.
   If there is an error, return zero after printing ERRSTRING
    (which is in addition to the specific error message already printed).  */

int
catch_errors (func, arg, errstring)
     int (*func) ();
     int arg;
     char *errstring;
{
  jmp_buf saved;
  int val;
  struct cleanup *saved_cleanup_chain;

  saved_cleanup_chain = save_cleanups ();

  bcopy (to_top_level, saved, sizeof (jmp_buf));

  if (setjmp (to_top_level) == 0)
    val = (*func) (arg);
  else
    {
      fprintf (stderr, "%s\n", errstring);
      val = 0;
    }

  restore_cleanups (saved_cleanup_chain);

  bcopy (saved, to_top_level, sizeof (jmp_buf));
  return val;
}

/* Handler for SIGHUP.  */

static void
disconnect ()
{
  kill_inferior_fast ();
  signal (SIGHUP, SIG_DFL);
  kill (getpid (), SIGHUP);
}

/* Clean up on error during a "source" command (or execution of a
   user-defined command).
   Close the file opened by the command
   and restore the previous input stream.  */

static void
source_cleanup (stream)
     FILE *stream;
{
  /* Instream may be 0; set to it when executing user-defined command. */
  if (instream)
    fclose (instream);
  instream = stream;
}

/*
 * Source $HOME/.gdbinit and $cwd/.gdbinit.
 * If X is enabled, also $HOME/.xgdbinit and $cwd/.xgdbinit.source
 */
void
source_init_files()
{
	char *homedir, initfile[256];
	int samedir = 0;

	/* Read init file, if it exists in home directory  */
	homedir = getenv ("HOME");
	if (homedir) {
		struct stat homebuf, cwdbuf;

		sprintf(initfile, "%s/.gdbinit", homedir);
		if (access (initfile, R_OK) == 0)
			if (!setjmp (to_top_level))
				source_command (initfile);
		if (!inhibit_windows) {
			sprintf(initfile, "%s/.xgdbinit", homedir);
			if (access (initfile, R_OK) == 0)
				if (!setjmp (to_top_level))
					source_command (initfile);
		}
		/* Determine if current directory is the same as the home
		   directory, so we don't source the same file twice. */
	
		bzero (&homebuf, sizeof (struct stat));
		bzero (&cwdbuf, sizeof (struct stat));
	
		stat(homedir, &homebuf);
		stat(".", &cwdbuf);
		
		samedir = bcmp(&homebuf, &cwdbuf, sizeof(struct stat)) == 0;
	}
	/* Read the input file in the current directory, *if* it isn't
	   the same file (it should exist, also).  */
	if (!samedir) {
		if (access (".gdbinit", R_OK) == 0)
			if (!setjmp (to_top_level))
				source_command (".gdbinit");
		if (access (".xgdbinit", R_OK) == 0)
			if (!setjmp (to_top_level))
				source_command (".xgdbinit");
	}
}


int
main (argc, argv, envp)
     int argc;
     char **argv;
     char **envp;
{
  int count;
  int inhibit_gdbinit = 0;
  int quiet = 1;
  int batch = 0;
  register int i;
  char *cp;

  /* XXX Windows only for xgdb. */
  char *strrchr();
  if (cp = strrchr(argv[0], '/'))
	  ++cp;
  else
	  cp = argv[0];
  if (*cp != 'x')
	  inhibit_windows = 1;

#if defined (ALIGN_STACK_ON_STARTUP)
  i = (int) &count & 0x3;
  if (i != 0)
    alloca (4 - i);
#endif

  quit_flag = 0;
  linesize = 100;
  line = (char *) xmalloc (linesize);
  *line = 0;
  instream = stdin;

  getwd (dirbuf);
  current_directory = dirbuf;

#ifdef SET_STACK_LIMIT_HUGE
  {
    struct rlimit rlim;

    /* Set the stack limit huge so that alloca (particularly stringtab
     * in dbxread.c) does not fail. */
    getrlimit (RLIMIT_STACK, &rlim);
    original_stack_limit = rlim.rlim_cur;
    rlim.rlim_cur = rlim.rlim_max;
    setrlimit (RLIMIT_STACK, &rlim);
  }
#endif /* SET_STACK_LIMIT_HUGE */

  /* Look for flag arguments.  */

  for (i = 1; i < argc; i++)
    {
      if (!strcmp (argv[i], "-q") || !strcmp (argv[i], "-quiet"))
	quiet = 1;
      else if (!strcmp (argv[i], "-nx"))
	inhibit_gdbinit = 1;
      else if (!strcmp (argv[i], "-nw"))
	inhibit_windows = 1;
      else if (!strcmp (argv[i], "-batch"))
	batch = 1, quiet = 1;
      else if (!strcmp (argv[i], "-fullname"))
	frame_file_full_name = 1;
      else if (!strcmp (argv[i], "-xgdb_verbose"))
	xgdb_verbose = 1;
      /* -help: print a summary of command line switches.  */
      else if (!strcmp (argv[i], "-help"))
	{
	  fputs ("\
This is GDB, the GNU debugger.  Use the command\n\
    gdb [options] [executable [core-file]]\n\
to enter the debugger.\n\
\n\
Options available are:\n\
  -help             Print this message.\n\
  -quiet            Do not print version number on startup.\n\
  -fullname         Output information used by emacs-GDB interface.\n\
  -batch            Exit after processing options.\n\
  -nx               Do not read .gdbinit file.\n\
  -tty TTY          Use TTY for input/output by the program being debugged.\n\
  -cd DIR           Change current directory to DIR.\n\
  -directory DIR    Search for source files in DIR.\n\
  -command FILE     Execute GDB commands from FILE.\n\
  -symbols SYMFILE  Read symbols from SYMFILE.\n\
  -exec EXECFILE    Use EXECFILE as the executable.\n\
  -se FILE          Use FILE as symbol file and executable file.\n\
  -core COREFILE    Analyze the core dump COREFILE.\n\
  -w                Writeable text.\n\
  -v                Print GNU message and version number on startup.\n\
  -nc               Don't confirm quit or run commands.\n\
\n\
For more information, type \"help\" from within GDB, or consult the\n\
GDB manual (available as on-line info or a printed manual).\n", stderr);
	  /* Exiting after printing this message seems like
	     the most useful thing to do.  */
	  exit (0);
	}
      else if (!strcmp (argv[i], "-w"))
	writeable_text = 1;
      else if (!strcmp (argv[i], "-v"))
	quiet = 0;
      else if (!strcmp (argv[i], "-nc"))
	      inhibit_confirm = 1;
      else if (argv[i][0] == '-')
	/* Other options take arguments, so don't confuse an
	   argument with an option.  */
	i++;
    }

  /* Run the init function of each source file */

  initialize_cmd_lists ();	/* This needs to be done first */
  initialize_all_files ();
  initialize_main ();		/* But that omits this file!  Do it now */
  initialize_signals ();

  if (!quiet)
    print_gdb_version ();

  /* Process the command line arguments.  */

  count = 0;
  for (i = 1; i < argc; i++)
    {
      extern void exec_file_command (), symbol_file_command ();
      extern void core_file_command ();
      register char *arg = argv[i];
      /* Args starting with - say what to do with the following arg
	 as a filename.  */
      if (arg[0] == '-')
	{
	  extern void tty_command (), directory_command ();

	  if (!strcmp (arg, "-q") || !strcmp (arg, "-nx")
	      || !strcmp (arg, "-quiet") || !strcmp (arg, "-batch")
	      || !strcmp (arg, "-fullname") || !strcmp (arg, "-nw")
	      || !strcmp (arg, "-xgdb_verbose")
	      || !strcmp (arg, "-help")
	      || !strcmp (arg, "-k")
	      || !strcmp (arg, "-w")
	      || !strcmp (arg, "-v")
	      || !strcmp (arg, "-nc"))
	    /* Already processed above */
	    continue;

	  if (++i == argc)
	    fprintf (stderr, "No argument follows \"%s\".\n", arg);
	  if (!setjmp (to_top_level))
	    {
	      /* -s foo: get syms from foo.  -e foo: execute foo.
		 -se foo: do both with foo.  -c foo: use foo as core dump.  */
	      if (!strcmp (arg, "-se"))
		{
		  exec_file_command (argv[i], !batch);
		  symbol_file_command (argv[i], !batch);
		}
	      else if (!strcmp (arg, "-s") || !strcmp (arg, "-symbols"))
		symbol_file_command (argv[i], !batch);
	      else if (!strcmp (arg, "-e") || !strcmp (arg, "-exec"))
		exec_file_command (argv[i], !batch);
	      else if (!strcmp (arg, "-c") || !strcmp (arg, "-core"))
		core_file_command (argv[i], !batch);
	      /* -x foo: execute commands from foo.  */
	      else if (!strcmp (arg, "-x") || !strcmp (arg, "-command")
		       || !strcmp (arg, "-commands"))
		source_command (argv[i]);
	      /* -d foo: add directory `foo' to source-file directory
		         search-list */
	      else if (!strcmp (arg, "-d") || !strcmp (arg, "-dir")
		       || !strcmp (arg, "-directory"))
		directory_command (argv[i], 0);
	      /* -cd FOO: specify current directory as FOO.
		 GDB remembers the precise string FOO as the dirname.  */
	      else if (!strcmp (arg, "-cd"))
		{
		  cd_command (argv[i], 0);
		  init_source_path ();
		}
	      /* -t /def/ttyp1: use /dev/ttyp1 for inferior I/O.  */
	      else if (!strcmp (arg, "-t") || !strcmp (arg, "-tty"))
		tty_command (argv[i], 0);

	      else
		error ("Unknown command-line switch: \"%s\"\n", arg);
	    }
	}
      else
	{
	  /* Args not thus accounted for
	     are treated as, first, the symbol/executable file
	     and, second, the core dump file.  */
	  count++;
	  if (!setjmp (to_top_level))
	    switch (count)
	      {
	      case 1:
		exec_file_command (arg, !batch);
		symbol_file_command (arg, !batch);
		break;

	      case 2:
		core_file_command (arg, !batch);
		break;

	      case 3:
		fprintf (stderr, "Excess command line args ignored. (%s%s)\n",
			 arg, (i == argc - 1) ? "" : " ...");
	      }
	}
    }

  if (!inhibit_gdbinit)
	  source_init_files();

  if (batch)
    {
#if 0
      fatal ("Attempt to read commands from stdin in batch mode.");
#endif
      /* We have hit the end of the batch file.  */
      exit (0);
    }

  if (!quiet)
    printf ("Type \"help\" for a list of commands.\n");

  /* The command loop.  */

  while (1)
    {
      if (!setjmp (to_top_level))
	command_loop ();
      if (ISATTY(stdin))
        clearerr (stdin);	/* Don't get hung if C-d is typed.  */
      else if (feof(instream))	/* Avoid endless loops for redirected stdin */
	break;
    }
    exit (0);
}


static void
do_nothing ()
{
}

/* Read commands from `instream' and execute them
   until end of file.  */
void
command_loop ()
{
  struct cleanup *old_chain;
  register int toplevel = (instream == stdin);
  register int interactive = (toplevel && ISATTY(stdin));

  while (!feof (instream))
    {
      register char *cmd_line;

      quit_flag = 0;
      if (interactive)
	reinitialize_more_filter ();
      old_chain = make_cleanup (do_nothing, 0);
      cmd_line = command_line_input (prompt, toplevel);
      execute_command (cmd_line, toplevel);
      /* Do any commands attached to breakpoint we stopped at.  */
      do_breakpoint_commands ();
      do_cleanups (old_chain);
    }
}

/* Commands call this if they do not want to be repeated by null lines.  */

void
dont_repeat ()
{
  /* If we aren't reading from standard input, we are saving the last
     thing read from stdin in line and don't want to delete it.  Null lines
     won't repeat here in any case.  */
  if (instream == stdin)
    *line = 0;
}

/* Read a line from the stream "instream" without command line editing.

   It prints PROMPT once at the start.
   Action is compatible with "readline" (i.e., space for typing is
   malloced & should be freed by caller). */
char *
gdb_readline (prompt)
     char *prompt;
{
  int c;
  char *result;
  int input_index = 0;
  int result_size = 80;

  if (prompt)
    {
      printf (prompt);
      fflush (stdout);
    }
  
  result = (char *) xmalloc (result_size);

  while (1)
    {
      c = fgetc (instream ? instream : stdin);
      if (c == EOF)
	{
	  free(result);
	  return ((char *)0);
	}
      if (c == '\n')
	break;

      result[input_index++] = c;
      if (input_index >= result_size)
	{
	  result_size <= 1;
	  result = (char *)xrealloc(result, result_size);
	}
    }
    result[input_index++] = '\0';
    return result;
}

/* Declaration for fancy readline with command line editing.  */
char *readline ();

/* Variables which control command line editing and history
   substitution.  These variables are given default values at the end
   of this file.  */
static int command_editing_p;
static int history_expansion_p;
static int write_history_p;
static int history_size;
static char *history_filename;

/* Variables which are necessary for fancy command line editing.  */
char *gdb_completer_word_break_characters =
  " \t\n!@#$%^&*()-+=|~`}{[]\"';:?/>.<,";

/* Functions that are used as part of the fancy command line editing.  */

/* Generate symbol names one by one for the completer.  If STATE is
   zero, then we need to initialize, otherwise the initialization has
   already taken place.  TEXT is what we expect the symbol to start
   with.  RL_LINE_BUFFER is available to be looked at; it contains the
   entire text of the line.  RL_POINT is the offset in that line of
   the cursor.  You should pretend that the line ends at RL_POINT.  */
char *
symbol_completion_function (text, state)
     char *text;
     int state;
{
  char **make_symbol_completion_list ();
  static char **list = (char **)NULL;
  static int index;
  char *output;
  extern char *rl_line_buffer;
  extern int rl_point;
  char *tmp_command, *p;
  struct cmd_list_element *c, *result_list;

  if (!state)
    {
      /* Free the storage used by LIST, but not by the strings inside.  This is
	 because rl_complete_internal () frees the strings. */
      if (list)
	free (list);
      list = 0;
      index = 0;

      /* Decide whether to complete on a list of gdb commands or on
	 symbols.  */
      tmp_command = (char *) alloca (rl_point + 1);
      p = tmp_command;
      
      strncpy (tmp_command, rl_line_buffer, rl_point);
      tmp_command[rl_point] = '\0';

      if (rl_point == 0)
	{
	  /* An empty line we want to consider ambiguous; that is,
	     it could be any command.  */
	  c = (struct cmd_list_element *) -1;
	  result_list = 0;
	}
      else
	c = lookup_cmd_1 (&p, cmdlist, &result_list, 1);

      /* Move p up to the next interesting thing.  */
      while (*p == ' ' || *p == '\t')
	p++;

      if (!c)
	/* He's typed something unrecognizable.  Sigh.  */
	list = (char **) 0;
      else if (c == (struct cmd_list_element *) -1)
	{
	  if (p + strlen(text) != tmp_command + rl_point)
	    error ("Unrecognized command.");
	  
	  /* He's typed something ambiguous.  This is easier.  */
	  if (result_list)
	    list = complete_on_cmdlist (*result_list->prefixlist, text);
	  else
	    list = complete_on_cmdlist (cmdlist, text);
	}
      else
	{
	  /* If we've gotten this far, gdb has recognized a full
	     command.  There are several possibilities:

	     1) We need to complete on the command.
	     2) We need to complete on the possibilities coming after
	     the command.
	     2) We need to complete the text of what comes after the
	     command.   */

	  if (!*p && *text)
	    /* Always (might be longer versions of thie command).  */
	    list = complete_on_cmdlist (result_list, text);
	  else if (!*p && !*text)
	    {
	      if (c->prefixlist)
		list = complete_on_cmdlist (*c->prefixlist, "");
	      else
		list = make_symbol_completion_list ("");
	    }
	  else
	    {
	      if (c->prefixlist && !c->allow_unknown)
		{
		  *p = '\0';
		  error ("\"%s\" command requires a subcommand.",
			 tmp_command);
		}
	      else
		list = make_symbol_completion_list (text);
	    }
	}
    }

  /* If the debugged program wasn't compiled with symbols, or if we're
     clearly completing on a command and no command matches, return
     NULL.  */
  if (!list)
    return ((char *)NULL);

  output = list[index];
  if (output)
    index++;

  return (output);
}


void
print_prompt ()
{
  if (prompt)
    {
      printf ("%s", prompt);
      fflush (stdout);
    }
}


#ifdef HAVE_TERMIO
#include <termio.h>
static struct termio norm_tty;

static void
suspend_sig()
{
	int tty = fileno(stdin);
	struct termio cur_tty;

	ioctl(tty, TCGETA, &cur_tty);
	ioctl(tty, TCSETAW, &norm_tty);

	(void) sigsetmask(0);
	signal(SIGTSTP, SIG_DFL);
	kill(0, SIGTSTP);

	/*
	 * we've just been resumed -- current tty params become new
	 * 'normal' params (in case tset/stty was done while we were
	 * suspended).  Merge values that readline might have changed
	 * into new params, then restore term mode.
	 */
	ioctl(tty, TCGETA, &norm_tty);
	cur_tty.c_lflag = (cur_tty.c_lflag & (ICANON|ECHO|ISIG)) |
			  (norm_tty.c_lflag &~ (ICANON|ECHO|ISIG));
	cur_tty.c_iflag = (cur_tty.c_iflag & (IXON|ISTRIP|INPCK)) |
			  (norm_tty.c_iflag &~ (IXON|ISTRIP|INPCK));
	ioctl(tty, TCSETAW, &cur_tty);

	signal(SIGTSTP, suspend_sig);
	print_prompt();

	/*
	 * Forget about any previous command -- null line now will do
	 * nothing.
	 */
	dont_repeat();
}

#else

#include <sys/ioctl.h>
#include <fcntl.h>
#include <sgtty.h>

static struct sgttyb norm_tty;
static struct tchars norm_tchars;
static struct ltchars norm_ltchars;
static int norm_lflags;

#ifdef PASS8
#define RL_TFLAGS (RAW|CRMOD|ECHO|CBREAK|PASS8)
#else
#define RL_TFLAGS (RAW|CRMOD|ECHO|CBREAK)
#endif

static void
suspend_sig()
{
	int tty = fileno(stdin);
	struct sgttyb cur_tty;
	struct tchars cur_tchars;
	struct ltchars cur_ltchars;
	int cur_lflags;
	int cur_flags;

	ioctl(tty, TIOCGETP, &cur_tty);
	ioctl(tty, TIOCGETC, &cur_tchars);
	ioctl(tty, TIOCLGET, &cur_lflags);
	ioctl(tty, TIOCGLTC, &cur_ltchars);

	ioctl(tty, TIOCSETP, &norm_tty);
	ioctl(tty, TIOCSETC, &norm_tchars);
	ioctl(tty, TIOCLSET, &norm_lflags);
	ioctl(tty, TIOCSLTC, &norm_ltchars);

	(void) sigsetmask(0);
	signal(SIGTSTP, SIG_DFL);
	kill(0, SIGTSTP);

	/*
	 * we've just been resumed -- current tty params become new
	 * 'normal' params (in case tset/stty was done while we were
	 * suspended).  Merge values that readline might have changed
	 * into new params, then restore term mode.
	 */
	ioctl(tty, TIOCGETP, &norm_tty);
	cur_flags = cur_tty.sg_flags;
	cur_tty = norm_tty;
	cur_tty.sg_flags = (cur_tty.sg_flags &~ RL_TFLAGS)
			   | (cur_flags & RL_TFLAGS);

	ioctl(tty, TIOCLGET, &norm_lflags);
#ifdef LPASS8
	cur_lflags = (cur_lflags &~ LPASS8) | (cur_flags & LPASS8);
#endif
	ioctl(tty, TIOCGETC, &norm_tchars);
	ioctl(tty, TIOCGLTC, &norm_ltchars);

	ioctl(tty, TIOCSETP, &cur_tty);
	ioctl(tty, TIOCSETC, &cur_tchars);
	ioctl(tty, TIOCLSET, &cur_lflags);
	ioctl(tty, TIOCSLTC, &cur_ltchars);

	signal(SIGTSTP, suspend_sig);
	print_prompt();

	/*
	 * Forget about any previous command -- null line now will do
	 * nothing.
	 */
	dont_repeat();
}
#endif /* HAVE_TERMIO */

/* Initialize signal handlers. */
initialize_signals ()
{
  extern void request_quit ();
  int tty = fileno(stdin);

  signal (SIGINT, request_quit);

  /* If we initialize SIGQUIT to SIG_IGN, then the SIG_IGN will get
     passed to the inferior, which we don't want.  It would be
     possible to do a "signal (SIGQUIT, SIG_DFL)" after we fork, but
     on BSD4.3 systems using vfork, that will (apparently) affect the
     GDB process as well as the inferior (the signal handling tables
     being shared between the two, apparently).  Since we establish
     a handler for SIGQUIT, when we call exec it will set the signal
     to SIG_DFL for us.  */
  signal (SIGQUIT, do_nothing);
  if (signal (SIGHUP, do_nothing) != SIG_IGN)
    signal (SIGHUP, disconnect);
  signal (SIGFPE, float_handler);

  ioctl(tty, TIOCGETP, &norm_tty);
  ioctl(tty, TIOCLGET, &norm_lflags);
  ioctl(tty, TIOCGETC, &norm_tchars);
  ioctl(tty, TIOCGLTC, &norm_ltchars);
  signal(SIGTSTP, suspend_sig);
}

char *
finish_command_input(inputline, repeat, interactive)
	register char *inputline;
	int repeat;
	int interactive;
{
	static char *do_free;

	if (do_free) {
		free(do_free);
		do_free = NULL;
	}

	/* Do history expansion if that is wished.  */
	if (interactive && history_expansion_p) {
		int expanded;

		expanded = history_expand(inputline, &do_free);
		if (expanded) {
			/* Print the changes.  */
			puts(do_free);

			/* An error acts like no input. */
			if (expanded < 0) {
				*do_free = 0;
				return (do_free);
			}
		}
		inputline = do_free;
	}
	/* get rid of any leading whitespace */
	while (isspace(*inputline))
		++inputline;
	/*
	 * If we just got an empty line, and that is supposed to repeat the
	 * previous command, return the value in the global buffer.
	 */
	if (*inputline == 0) {
		if (repeat)
			return (line);
	} else if (interactive)
		add_history(inputline);

	/*
	 * If line is a comment, clear it out.
	 * Note:  comments are added to the command history. This is useful
	 * when you type a command, and then realize you don't want to
	 * execute it quite yet.  You can comment out the command and then
	 * later fetch it from the value history and remove the '#'.
	 */
	if (*inputline == '#')
		*inputline = 0;
	else if (repeat) {
		/* Save into global buffer. */
		register int i = strlen(inputline) + 1;

		if (i > linesize) {
			line = xrealloc(line, i);
			linesize = i;
		}
		strcpy(line, inputline);
	}
	return (inputline);
}

static char *
get_a_cmd_line(prompt, interactive)
	char *prompt;
	int interactive;
{
	register char *cp;

	/* Control-C quits instantly if typed while reading input. */
	immediate_quit++;
	if (interactive && command_editing_p) {
		extern void (*rl_event_hook)();

		rl_event_hook = window_hook;
		cp = readline(prompt);
	} else {
		if (interactive) {
			if (window_hook) {
				print_prompt();
				(*window_hook)();
			}
		} else
			prompt = NULL;
		cp = gdb_readline(prompt);
	}
	--immediate_quit;
	return (cp);
}

/* Read one line from the command input stream `instream'
   Returns the address of the start of the line.

   *If* the instream == stdin & stdin is a terminal, the line read
   is copied into the file line saver (global var char *line,
   length linesize) so that it can be duplicated.

   This routine either uses fancy command line editing or
   simple input as the user has requested.  */

char *
command_line_input(prompt, repeat)
	char *prompt;
	int repeat;
{
	static char *do_free;
	register int interactive = (instream == stdin && ISATTY(instream));
	register char *cp;
	register int i;

	if (do_free) {
		free(do_free);
		do_free = NULL;
	}
	cp = get_a_cmd_line(prompt, interactive);

	/*
	 * handle continued lines (this loop is not particularly
	 * efficient because it's rare).
	 */
	while (cp && cp[i = strlen(cp) - 1] == '\\') {
		register char *np = get_a_cmd_line(prompt, interactive);
		register int j;

		if (np == NULL) {
			cp[i] = 0;
			break;
		}
		j = strlen(np);
		cp = xrealloc(cp, i + j + 1);
		strcpy(cp + i, np);
		free(np);
	}
	if (cp == NULL)
		return ("");
	do_free = cp;
	return (finish_command_input(cp, repeat, interactive));
}


#define MAX_USER_ARGS 32

static struct user_args {
	struct {
		char *arg;
		int len;
	} a[10];
} uargs[MAX_USER_ARGS];

static struct user_args *user_arg = uargs;

static void
arg_cleanup(ap)
	struct user_args *ap;
{
	user_arg = ap;
}

/* Bind arguments $arg0, $arg1, ..., for a user defined command. */
struct cleanup *
setup_user_args(p)
	char *p;
{
	register int i;
	struct cleanup *old_chain = make_cleanup(arg_cleanup, user_arg);

	if (++user_arg >= &uargs[MAX_USER_ARGS])
		error("user defined functions nested too deeply\n");

	bzero(user_arg, sizeof(*user_arg));

	i = 0;
	while (*p) {
		while (isspace(*p))
			++p;
		user_arg->a[i].arg = p;
		while (*p && ! isspace(*p))
			++p;
		user_arg->a[i].len = p - user_arg->a[i].arg;
		++i;
	}
	return (old_chain);
}

static char *
findarg(str)
	register char *str;
{
	register char *cp = str;
	extern char *index();

	while (cp = index(cp, '$')) {
		if (strncmp(cp, "$arg", 4) == 0 && isdigit(cp[4]))
			return (cp);
		++cp;
	}
	return (char *)0;
}

/* expand arguments from "line" into "new" */
static void
expand_args(line, new)
	register char *line, *new;
{
	register char *cp = findarg(line);

	while (cp = findarg(line)) {
		int i, len;

		bcopy(line, new, cp - line);
		new += cp - line;
		i = cp[4] - '0';
		if (len = user_arg->a[i].len) {
			bcopy(user_arg->a[i].arg, new, len);
			new += len;
		}
		line = cp + 5;
	}
	strcpy(new, line);
}

/* expand any arguments in "line" then execute the result */
static void
expand_and_execute(line, from_tty)
	char *line;
	int from_tty;
{
	void execute_command();
	char new[1024];

	if (! findarg(line)) {
		execute_command(line, from_tty);
		return;
	}
	expand_args(line, new);
	execute_command(new, from_tty);
}

char *
read_one_command_line(prompt, from_tty)
	char *prompt;
{
	register char *p, *p1;

	dont_repeat();
	p = command_line_input(prompt, from_tty);

	/* Remove trailing blanks.  */
	p1 = p + strlen(p);
	while (--p1 > p && (*p1 == ' ' || *p1 == '\t'))
		;
	*++p1 = 0;
	return (p);
}

static char cmd_prompt[] = "                                                > ";

int
parse_control_structure(rootcmd, from_tty, level)
	struct command_line *rootcmd;
	int from_tty;
{
	struct command_line *cmd = (struct command_line *)xmalloc(sizeof(*cmd));
	char *prompt;

	++level;
	prompt = from_tty? &cmd_prompt[sizeof(cmd_prompt) - 1 - 2*level] :
			   (char *)0;
	bzero(cmd, sizeof(*cmd));
	rootcmd->body = cmd;
	while (1) {
		char *p = read_one_command_line(prompt, from_tty);

		p = savestring(p, strlen(p));
		cmd->line = p;
		if (!strncmp(p, "while ", 6)) {
			cmd->type = CL_WHILE;
			if (parse_control_structure(cmd, from_tty, level))
				return (1);
		} else if (!strncmp(p, "if ", 3)) {
			cmd->type = CL_IF;
			if (parse_control_structure(cmd, from_tty, level)) {
				struct command_line *tmp;
				int stat;

				cmd->elsebody = cmd->body;
				stat = parse_control_structure(cmd, from_tty,
							       level);
				tmp = cmd->elsebody;
				cmd->elsebody = cmd->body;
				cmd->body = tmp;
				if (stat)
					return (1);
			}
		} else if (!strcmp(p, "else")) {
			cmd->type = CL_END;
			return (1);
		} else if (!strcmp(p, "end")) {
			cmd->type = CL_END;
			return (0);
		} else if (!strcmp(p, "exitloop")) {
			cmd->type = CL_EXITLOOP;
		} else {
			cmd->type = CL_NORMAL;
		}
		cmd->next = (struct command_line *)xmalloc(sizeof(*cmd));
		cmd = cmd->next;
		bzero(cmd, sizeof(*cmd));
	}
	/* NOTREACHED */
}

int
execute_control_structure(cmd)
	register struct command_line *cmd;
{
	char expn[1024];
	struct expression *cond;
	int stat;

	while (cmd) {
		QUIT;
		switch (cmd->type) {
		case CL_END:
			return (0);
		case CL_NORMAL:
			expand_and_execute(cmd->line, 0);
			break;
		case CL_WHILE:
			expand_args(cmd->line + 6, expn);
			cond = parse_c_expression(expn);
			while (breakpoint_cond_eval(cond) == 0)
				if (execute_control_structure(cmd->body))
					break;
			free(cond);
			break;
		case CL_IF:
			expand_args(cmd->line + 3, expn);
			cond = parse_c_expression(expn);
			stat = breakpoint_cond_eval(cond);
			free(cond);
			if (stat == 0) {
				if (execute_control_structure(cmd->body))
					return (1);
			} else if (cmd->elsebody) {
				if (execute_control_structure(cmd->elsebody))
					return (1);
			}
			break;
		case CL_EXITLOOP:
			return (1);
		}
		cmd = cmd->next;
	}
	free_all_values();
}

execute_command_lines(cmd)
	struct command_line *cmd;
{
	struct cleanup *old_chain = make_cleanup(source_cleanup, instream);

	/*
	 * Set the instream to 0, indicating execution of a user-defined
	 * function.  
	 */
	++immediate_quit;
	instream = (FILE *) 0;
	(void)execute_control_structure(cmd);
	--immediate_quit;
	do_cleanups(old_chain);
}

/* do following command lines if expression true */
if_command(p, from_tty)
	char *p;
	int from_tty;
{
	struct cleanup *old_chain;
	struct command_line *cmd = (struct command_line *)xmalloc(sizeof(*cmd));
	char buf[128];

	sprintf(buf, "if %s", p);

	bzero(cmd, sizeof(*cmd));
	old_chain = make_cleanup(free_command_lines, cmd);
	cmd->type = CL_IF;
	cmd->line = savestring(buf, strlen(buf));
	/* XXX cmd->line? */
	if (parse_control_structure(cmd, from_tty, 0)) {
		struct command_line *tmp;

		cmd->elsebody = cmd->body;
		(void) parse_control_structure(cmd, from_tty, 0);
		tmp = cmd->elsebody;
		cmd->elsebody = cmd->body;
		cmd->body = tmp;
	}
	(void) execute_command_lines(cmd);
	do_cleanups(old_chain);
}

/* do following command lines while expression true */
while_command(p, from_tty)
	char *p;
	int from_tty;
{
	struct cleanup *old_chain;
	struct command_line *cmd = (struct command_line *)xmalloc(sizeof(*cmd));
	char buf[128];

	sprintf(buf, "while %s", p);

	bzero(cmd, sizeof(*cmd));
	old_chain = make_cleanup(free_command_lines, cmd);
	cmd->type = CL_WHILE;
	cmd->line = savestring(buf, strlen(buf));
	(void)parse_control_structure(cmd, from_tty, 0);
	(void)execute_command_lines(cmd);
	do_cleanups(old_chain);
}

/*
 * Execute the line P as a command.
 * Pass FROM_TTY as second argument to the defining function.
 */
void
execute_command (p, from_tty)
	char *p;
	int from_tty;
{
	register struct cmd_list_element *c;
	register struct command_line *cmdlines;

	free_all_values();
	if (*p) {
		c = lookup_cmd(&p, cmdlist, "", 0, 1);
		if (c->function == 0)
			error("That is not a command, just a help topic.");
		else if (c->class == (int) class_user) {
			struct cleanup *old_chain = setup_user_args(p);

			cmdlines = (struct command_line *) c->function;
			if (cmdlines)
				(void)execute_command_lines(cmdlines);

			do_cleanups(old_chain);
		} else
			/* Pass null arg rather than an empty one.  */
			(*c->function) (*p ? p : 0, from_tty);
	}
}

/*
 * Read lines from the input stream and accumulate them in a chain of struct
 * command_line's which is then returned.  
 */
struct command_line *
read_command_lines(from_tty)
	int from_tty;
{
	struct cleanup *old_chain;
	struct command_line *cmd = (struct command_line *)xmalloc(sizeof(*cmd));
	struct command_line *next;

	bzero(cmd, sizeof(*cmd));
	old_chain = make_cleanup(free_command_lines, cmd);
	cmd->type = CL_NOP;
	(void)parse_control_structure(cmd, from_tty, 0);
	dont_repeat();
	discard_cleanups(old_chain);
	next = cmd->body;
	free(cmd);
	return (next);
}

/* Free a chain of struct command_line's.  */

void
free_command_lines(cmds)
	struct command_line *cmds;
{
	struct command_line *next;

	while (cmds) {
		if (cmds->body)
			free(cmds->body);
		if (cmds->elsebody)
			free(cmds->elsebody);
		if (cmds->line)
			free(cmds->line);
		next = cmds->next;
		free(cmds);
		cmds = next;
	}
}

/* Add an element to the list of info subcommands.  */

void
add_info (name, fun, doc)
     char *name;
     void (*fun) ();
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

static void
info_command ()
{
  printf ("\"info\" must be followed by the name of an info command.\n");
  help_list (infolist, "info ", -1, stdout);
}

/* Add an element to the list of commands.  */

void
add_com (name, class, fun, doc)
     char *name;
     int class;
     void (*fun) ();
     char *doc;
{
  add_cmd (name, class, fun, doc, &cmdlist);
}

/* Add an alias or abbreviation command to the list of commands.  */

void
add_com_alias (name, oldname, class, abbrev_flag)
     char *name;
     char *oldname;
     int class;
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

static void
help_command (command, from_tty)
     char *command;
     int from_tty; /* Ignored */
{
  help_cmd (command, stdout);
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
      if (!(*p >= 'A' && *p <= 'Z')
	  && !(*p >= 'a' && *p <= 'z')
	  && !(*p >= '0' && *p <= '9')
	  && *p != '-')
	error ("Junk in argument list: \"%s\"", p);
      p++;
    }
}

static void
define_command (comname, from_tty)
     char *comname;
     int from_tty;
{
  register struct command_line *cmds;
  register struct cmd_list_element *c;
  char *tem = comname;

  validate_comname (comname);

  c = lookup_cmd (&tem, cmdlist, "", -1, 1);
  if (c)
    {
      if (c->class == (int) class_user || c->class == (int) class_alias)
	tem = "Redefine command \"%s\"? ";
      else
	tem = "Really redefine built-in command \"%s\"? ";
      if (!query (tem, comname))
	error ("Command \"%s\" not redefined.", comname);
    }

  if (from_tty)
    {
      printf ("Type commands for definition of \"%s\".\n\
End with a line saying just \"end\".\n", comname);
      fflush (stdout);
    }
  comname = savestring (comname, strlen (comname));

  cmds = read_command_lines (from_tty);

  if (c && c->class == (int) class_user)
    free_command_lines (c->function);

  add_com (comname, class_user, cmds,
	   (c && c->class == (int) class_user)
	   ? c->doc : savestring ("User-defined.", 13));
}

static void
document_command (comname, from_tty)
     char *comname;
     int from_tty;
{
  register struct cmd_list_element *c;
  register char *p;
  register char *cp;
  register char *doc = 0;
  register int len;
  char *tmp = comname;

  validate_comname (comname);
  c = lookup_cmd (&tmp, cmdlist, "", 0, 1);
  if (c->class != (int) class_user)
    error ("Command \"%s\" is built-in.", comname);

  if (from_tty)
    printf ("Type documentation for \"%s\".  \
End with a line saying just \"end\".\n", comname);

  while (p = read_one_command_line(from_tty? "> " : 0, from_tty))
    {
      if (strcmp(p, "end") == 0)
	break;
      len = strlen(p) + 1;
      if (! doc)
	{
	  doc = xmalloc(len);
	  cp = doc;
	}
      else
	{
	  int i = cp - doc;
	  doc = xrealloc(doc, i + len);
	  cp = doc + i;
	}
      strcpy(cp, p);
      cp += len;
      cp[-1] = '\n';
    }
  if (doc && cp > doc)
    cp[-1] = 0;
  if (c->doc)
    free (c->doc);
  c->doc = doc;
}

static void
print_gdb_version ()
{
  printf ("GDB %s, Copyright (C) 1989 Free Software Foundation, Inc.\n\
There is ABSOLUTELY NO WARRANTY for GDB; type \"info warranty\" for details.\n\
GDB is free software and you are welcome to distribute copies of it\n\
 under certain conditions; type \"info copying\" to see the conditions.\n",
	  version);
}

static void
version_info ()
{
  immediate_quit++;
  print_gdb_version ();
  immediate_quit--;
}


/* Command to specify a prompt string instead of "(gdb) ".  */

void
set_prompt_command (text)
     char *text;
{
  char *p, *q;
  register int c;
  char *new;

  if (text == 0)
    error_no_arg ("string to which to set prompt");

  new = (char *) xmalloc (strlen (text) + 2);
  p = text; q = new;
  while (c = *p++)
    {
      if (c == '\\')
	{
	  /* \ at end of argument is used after spaces
	     so they won't be lost.  */
	  if (*p == 0)
	    break;
	  c = parse_escape (&p);
	  if (c == 0)
	    break; /* C loses */
	  else if (c > 0)
	    *q++ = c;
	}
      else
	*q++ = c;
    }
  if (*(p - 1) != '\\')
    *q++ = ' ';
  *q++ = '\0';
  new = (char *) xrealloc (new, q - new);
  free (prompt);
  prompt = new;
}

static void
quit_command ()
{
  extern void exec_file_command ();
  if (have_inferior_p ())
    {
      if (inhibit_confirm || query ("The program is running.  Quit anyway? "))
	{
	  /* Prevent any warning message from reopen_exec_file, in case
	     we have a core file that's inconsistent with the exec file.  */
	  exec_file_command (0, 0);
	  kill_inferior ();
	}
      else
	error ("Not confirmed.");
    }
  /* Save the history information if it is appropriate to do so.  */
  if (write_history_p && history_filename)
    write_history (history_filename);
  exit (0);
}

int
input_from_terminal_p ()
{
  return instream == stdin;
}

static void
pwd_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  if (arg) error ("The \"pwd\" command does not take an argument: %s", arg);
  getwd (dirbuf);

  if (strcmp (dirbuf, current_directory))
    printf ("Working directory %s\n (canonically %s).\n",
	    current_directory, dirbuf);
  else
    printf ("Working directory %s.\n", current_directory);
}

static void
cd_command (dir, from_tty)
     char *dir;
     int from_tty;
{
  int len;
  int change;

  if (dir == 0)
    error_no_arg ("new working directory");

  dir = tilde_expand (dir);
  make_cleanup (free, dir);

  len = strlen (dir);
  dir = savestring (dir, len - (len > 1 && dir[len-1] == '/'));
  if (dir[0] == '/')
    current_directory = dir;
  else
    {
      current_directory = concat (current_directory, "/", dir);
      free (dir);
    }

  /* Now simplify any occurrences of `.' and `..' in the pathname.  */

  change = 1;
  while (change)
    {
      char *p;
      change = 0;

      for (p = current_directory; *p;)
	{
	  if (!strncmp (p, "/./", 2)
	      && (p[2] == 0 || p[2] == '/'))
	    strcpy (p, p + 2);
	  else if (!strncmp (p, "/..", 3)
		   && (p[3] == 0 || p[3] == '/')
		   && p != current_directory)
	    {
	      char *q = p;
	      while (q != current_directory && q[-1] != '/') q--;
	      if (q != current_directory)
		{
		  strcpy (q-1, p+3);
		  p = q-1;
		}
	    }
	  else p++;
	}
    }

  if (chdir (dir) < 0)
    perror_with_name (dir);

  if (from_tty)
    pwd_command ((char *) 0, 1);
}

static void
source_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  FILE *stream;
  struct cleanup *cleanups;
  char *file = arg;
  char *path;

  if (file == 0)
    /* Let source without arguments read .gdbinit.  */
    file = ".gdbinit";

  file = tilde_expand (file);
  make_cleanup (free, file);

#ifdef KERNELDEBUG
  if (path = getenv(kernel_debugging? "KGDBPATH" : "GDBPATH"))
#else
  if (path = getenv("GDBPATH"))
#endif
    {
      int fd = openp(path, 1, file, O_RDONLY, 0, 0);

      if (fd == -1)
	stream = 0;
      else
	stream = fdopen(fd, "r");
    }
  else
    stream = fopen (file, "r");

  if (stream == 0)
    perror_with_name (file);

  cleanups = make_cleanup (source_cleanup, instream);

  instream = stream;

  command_loop ();

  do_cleanups (cleanups);
}

static void
echo_command (text)
     char *text;
{
  char *p = text;
  register int c;

  if (text)
    while (c = *p++)
      {
	if (c == '\\')
	  {
	    /* \ at end of argument is used after spaces
	       so they won't be lost.  */
	    if (*p == 0)
	      return;

	    c = parse_escape (&p);
	    if (c >= 0)
	      fputc (c, stdout);
	  }
	else
	  fputc (c, stdout);
      }
      fflush(stdout);
}

static void
dump_me_command ()
{
  if (query ("Should GDB dump core? "))
    {
      signal (SIGQUIT, SIG_DFL);
      kill (getpid (), SIGQUIT);
    }
}

int
parse_binary_operation (caller, arg)
     char *caller, *arg;
{
  int length;

  if (!arg || !*arg)
    return 1;

  length = strlen (arg);

  while (arg[length - 1] == ' ' || arg[length - 1] == '\t')
    length--;

  if (!strncmp (arg, "on", length)
      || !strncmp (arg, "1", length)
      || !strncmp (arg, "yes", length))
    return 1;
  else
    if (!strncmp (arg, "off", length)
	|| !strncmp (arg, "0", length)
	|| !strncmp (arg, "no", length))
      return 0;
    else
      error ("\"%s\" not given a binary valued argument.", caller);
}

/* Functions to manipulate command line editing control variables.  */

static void
set_editing (arg, from_tty)
     char *arg;
     int from_tty;
{
  command_editing_p = parse_binary_operation ("set command-editing", arg);
}

/* Number of commands to print in each call to editing_info.  */
#define Hist_print 10
static void
editing_info (arg, from_tty)
     char *arg;
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

  struct _hist_entry {
    char *line;
    char *data;
  } *history_get();
  extern int history_base;

  printf_filtered ("Interactive command editing is %s.\n",
	  command_editing_p ? "on" : "off");

  printf_filtered ("History expansion of command input is %s.\n",
	  history_expansion_p ? "on" : "off");
  printf_filtered ("Writing of a history record upon exit is %s.\n",
	  write_history_p ? "enabled" : "disabled");
  printf_filtered ("The size of the history list (number of stored commands) is %d.\n",
	  history_size);
  printf_filtered ("The name of the history record is \"%s\".\n\n",
	  history_filename ? history_filename : "");

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

  if (arg)
    {
      if (arg[0] == '+' && arg[1] == '\0')
	/* "info editing +" should print from the stored position.  */
	;
      else
	/* "info editing <exp>" should print around command number <exp>.  */
	num = (parse_and_eval_address (arg) - history_base) - Hist_print / 2;
    }
  /* "info editing" means print the last Hist_print commands.  */
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

  if (num == hist_len - Hist_print)
    printf_filtered ("The list of the last %d commands is:\n\n", Hist_print);
  else
    printf_filtered ("Some of the stored commands are:\n\n");

  for (offset = num; offset < num + Hist_print && offset < hist_len; offset++)
    {
      printf_filtered ("%5d  %s\n", history_base + offset,
	      (history_get (history_base + offset))->line);
    }

  /* The next command we want to display is the next one that we haven't
     displayed yet.  */
  num += Hist_print;
  
  /* If the user repeats this command with return, it should do what
     "info editing +" does.  This is unnecessary if arg is null,
     because "info editing +" is not useful after "info editing".  */
  if (from_tty && arg)
    {
      arg[0] = '+';
      arg[1] = '\0';
    }
}

static void
set_history_expansion (arg, from_tty)
     char *arg;
     int from_tty;
{
  history_expansion_p = parse_binary_operation ("set history expansion", arg);
}

static void
set_history_write (arg, from_tty)
     char *arg;
     int from_tty;
{
  write_history_p = parse_binary_operation ("set history write", arg);
}

static void
set_history (arg, from_tty)
     char *arg;
     int from_tty;
{
  printf ("\"set history\" must be followed by the name of a history subcommand.\n");
  help_list (sethistlist, "set history ", -1, stdout);
}

static void
set_history_size (arg, from_tty)
     char *arg;
     int from_tty;
{
  if (!*arg)
    error_no_arg ("set history size");

  history_size = atoi (arg);
}

static void
set_history_filename (arg, from_tty)
     char *arg;
     int from_tty;
{
  int i;

  if (!arg)
    error_no_arg ("history file name");
  
  arg = tilde_expand (arg);
  make_cleanup (free, arg);

  i = strlen (arg) - 1;
  
  free (history_filename);
  
  while (i > 0 && (arg[i] == ' ' || arg[i] == '\t'))
    i--;
  ++i;

  if (!*arg)
    history_filename = (char *) 0;
  else
    history_filename = savestring (arg, i + 1);
  history_filename[i] = '\0';
}

int info_verbose;

static void
set_verbose_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  info_verbose = parse_binary_operation ("set verbose", arg);
}

static void
verbose_info (arg, from_tty)
     char *arg;
     int from_tty;
{
  if (arg)
    error ("\"info verbose\" does not take any arguments.\n");
  
  printf ("Verbose printing of information is %s.\n",
	  info_verbose ? "on" : "off");
}

static void
float_handler ()
{
  error ("Invalid floating value encountered or computed.");
}


static void
initialize_cmd_lists ()
{
  cmdlist = (struct cmd_list_element *) 0;
  infolist = (struct cmd_list_element *) 0;
  enablelist = (struct cmd_list_element *) 0;
  disablelist = (struct cmd_list_element *) 0;
  deletelist = (struct cmd_list_element *) 0;
  enablebreaklist = (struct cmd_list_element *) 0;
  setlist = (struct cmd_list_element *) 0;
  sethistlist = (struct cmd_list_element *) 0;
  unsethistlist = (struct cmd_list_element *) 0;
}

static void
initialize_main ()
{
  char *tmpenv;
  /* Command line editing externals.  */
  extern int (*rl_completion_entry_function)();
  extern char *rl_completer_word_break_characters;
  extern char *rl_readline_name;
  
  /* Set default verbose mode on.  */
  info_verbose = 1;

#ifdef KERNELDEBUG
  if (kernel_debugging)
	  prompt = savestring ("(kgdb) ", 7);
  else
#endif
  prompt = savestring ("(gdb) ", 6);

  /* Set the important stuff up for command editing.  */
  command_editing_p = 1;
  history_expansion_p = 0;
  write_history_p = 0;
  
  if (tmpenv = getenv ("HISTSIZE"))
    history_size = atoi (tmpenv);
  else
    history_size = 256;

  stifle_history (history_size);

  if (tmpenv = getenv ("GDBHISTFILE"))
    history_filename = savestring (tmpenv, strlen(tmpenv));
  else
    /* We include the current directory so that if the user changes
       directories the file written will be the same as the one
       that was read.  */
    history_filename = concat (current_directory, "/.gdb_history", "");

  read_history (history_filename);

  /* Setup important stuff for command line editing.  */
  rl_completion_entry_function = (int (*)()) symbol_completion_function;
  rl_completer_word_break_characters = gdb_completer_word_break_characters;
  rl_readline_name = "gdb";

  /* Define the classes of commands.
     They will appear in the help list in the reverse of this order.  */

  add_cmd ("obscure", class_obscure, 0, "Obscure features.", &cmdlist);
  add_cmd ("alias", class_alias, 0, "Aliases of other commands.", &cmdlist);
  add_cmd ("user", class_user, 0, "User-defined commands.\n\
The commands in this class are those defined by the user.\n\
Use the \"define\" command to define a command.", &cmdlist);
  add_cmd ("support", class_support, 0, "Support facilities.", &cmdlist);
  add_cmd ("status", class_info, 0, "Status inquiries.", &cmdlist);
  add_cmd ("files", class_files, 0, "Specifying and examining files.", &cmdlist);
  add_cmd ("breakpoints", class_breakpoint, 0, "Making program stop at certain points.", &cmdlist);
  add_cmd ("data", class_vars, 0, "Examining data.", &cmdlist);
  add_cmd ("stack", class_stack, 0, "Examining the stack.\n\
The stack is made up of stack frames.  Gdb assigns numbers to stack frames\n\
counting from zero for the innermost (currently executing) frame.\n\n\
At any time gdb identifies one frame as the \"selected\" frame.\n\
Variable lookups are done with respect to the selected frame.\n\
When the program being debugged stops, gdb selects the innermost frame.\n\
The commands below can be used to select other frames by number or address.",
	   &cmdlist);
  add_cmd ("running", class_run, 0, "Running the program.", &cmdlist);

  add_com ("pwd", class_files, pwd_command,
	   "Print working directory.  This is used for your program as well.");
  add_com ("cd", class_files, cd_command,
	   "Set working directory to DIR for debugger and program being debugged.\n\
The change does not take effect for the program being debugged\n\
until the next time it is started.");

  add_cmd ("prompt", class_support, set_prompt_command,
	   "Change gdb's prompt from the default of \"(gdb)\"",
	   &setlist);
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
Commands defined in this way do not take arguments.");

  add_com ("source", class_support, source_command,
	   "Read commands from a file named FILE.\n\
Note that the file \".gdbinit\" is read automatically in this way\n\
when gdb is started.");
  add_com ("quit", class_support, quit_command, "Exit gdb.");
  add_com ("help", class_support, help_command, "Print list of commands.");
  add_com_alias ("q", "quit", class_support, 1);
  add_com_alias ("h", "help", class_support, 1);
  add_com ("while", class_support, while_command,
	   "execute following commands while condition is true.\n\
Expression for condition follows \"while\" keyword.");
  add_com ("if", class_support, if_command,
	   "execute following commands if condition is true.\n\
Expression for condition follows \"if\" keyword.");
  add_cmd ("verbose", class_support, set_verbose_command,
	   "Change the number of informational messages gdb prints.",
	   &setlist);
  add_info ("verbose", verbose_info,
	    "Status of gdb's verbose printing option.\n");

  add_com ("dump-me", class_obscure, dump_me_command,
	   "Get fatal error; make debugger dump its core.");

  add_cmd ("editing", class_support, set_editing,
	   "Enable or disable command line editing.\n\
Use \"on\" to enable to enable the editing, and \"off\" to disable it.\n\
Without an argument, command line editing is enabled.", &setlist);

  add_prefix_cmd ("history", class_support, set_history,
		  "Generic command for setting command history parameters.",
		  &sethistlist, "set history ", 0, &setlist);

  add_cmd ("expansion", no_class, set_history_expansion,
	   "Enable or disable history expansion on command input.\n\
Without an argument, history expansion is enabled.", &sethistlist);

  add_cmd ("write", no_class, set_history_write,
	   "Enable or disable saving of the history record on exit.\n\
Use \"on\" to enable to enable the saving, and \"off\" to disable it.\n\
Without an argument, saving is enabled.", &sethistlist);

  add_cmd ("size", no_class, set_history_size,
	   "Set the size of the command history, \n\
ie. the number of previous commands to keep a record of.", &sethistlist);

  add_cmd ("filename", no_class, set_history_filename,
	   "Set the filename in which to record the command history\n\
 (the list of previous commands of which a record is kept).", &sethistlist);

  add_prefix_cmd ("info", class_info, info_command,
		  "Generic command for printing status.",
		  &infolist, "info ", 0, &cmdlist);
  add_com_alias ("i", "info", class_info, 1);

  add_info ("editing", editing_info, "Status of command editor.");

  add_info ("version", version_info, "Report what version of GDB this is.");
}
