/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)readline.c	6.4 (Berkeley) 5/8/91";
#endif /* not lint */

/* readline.c -- a general facility for reading lines of input
   with emacs style editing and completion. */

/* Copyright (C) 1987,1989 Free Software Foundation, Inc.

   This file contains the Readline Library (the Library), a set of
   routines for providing Emacs style line input to programs that ask
   for it.

   The Library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   The Library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */

/* Remove these declarations when we have a complete libgnu.a. */
#define STATIC_MALLOC
#ifndef STATIC_MALLOC
extern char *xmalloc (), *xrealloc ();
#else
static char *xmalloc (), *xrealloc ();
#endif

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/file.h>
#include <signal.h>

#ifdef __GNUC__
#define alloca __builtin_alloca
#else
#if defined (sparc) && defined (sun)
#include <alloca.h>
#endif
#endif

#define NEW_TTY_DRIVER
#if defined (SYSV) || defined (hpux)
#undef NEW_TTY_DRIVER
#include <termio.h>
#else
#include <sgtty.h>
#endif

#include <errno.h>
extern int errno;

#include <setjmp.h>

/* These next are for filename completion.  Perhaps this belongs
   in a different place. */
#include <sys/stat.h>

#include <pwd.h>
#ifdef SYSV
struct passwd *getpwuid (), *getpwent ();
#endif

#define HACK_TERMCAP_MOTION

#ifndef SYSV
#include <sys/dir.h>
#else  /* SYSV */
#ifdef hpux
#include <ndir.h>
#else
#include <dirent.h>
#define direct dirent
#define d_namlen d_reclen
#endif  /* hpux */
#endif  /* SYSV */

/* Some standard library routines. */
#include "readline.h"
#include "history.h"

#ifndef digit
#define digit(c)  ((c) >= '0' && (c) <= '9')
#endif

#ifndef isletter
#define isletter(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#endif

#ifndef digit_value
#define digit_value(c) ((c) - '0')
#endif

#ifndef member
char *index ();
#define member(c, s) ((c) ? index ((s), (c)) : 0)
#endif

#ifndef isident
#define isident(c) ((isletter(c) || digit(c) || c == '_'))
#endif

#ifndef exchange
#define exchange(x, y) {int temp = x; x = y; y = temp;}
#endif

static update_line ();
static void output_character_function ();
static delete_chars ();
static start_insert ();
static end_insert ();

/* This typedef is equivalant to the one for Function; it allows us
   to say SigHandler *foo = signal (SIGKILL, SIG_IGN); */
typedef void SigHandler ();

#ifdef SIGWINCH
static void rl_handle_sigwinch ();
static SigHandler *old_sigwinch = (SigHandler *)NULL;
#endif

/* If on, then readline handles signals in a way that doesn't screw. */
/* #define HANDLE_SIGNALS */

#if defined (SYSV)
#ifdef HANDLE_SIGNALS
#undef HANDLE_SIGNALS
#endif
#endif

/* Stupid comparison routine for qsort () ing strings. */
static int
compare_strings (s1, s2)
  char **s1, **s2;
{
  return (strcmp (*s1, *s2));
}


/* **************************************************************** */
/*								    */
/*			Line editing input utility		    */
/*								    */
/* **************************************************************** */

/* A pointer to the keymap that is currently in use.
   By default, it is the standard emacs keymap. */
Keymap keymap = emacs_standard_keymap;

#define vi_mode 0
#define emacs_mode 1

/* The current style of editing. */
int rl_editing_mode = emacs_mode;

/* Non-zero if the previous command was a kill command. */
static int last_command_was_kill = 0;

/* The current value of the numeric argument specified by the user. */
int rl_numeric_arg = 1;

/* Non-zero if an argument was typed. */
int rl_explicit_arg = 0;

/* Temporary value used while generating the argument. */
static int arg_sign = 1;

/* Non-zero means we have been called at least once before. */
static int rl_initialized = 0;

/* If non-zero, this program is running in an EMACS buffer. */
static char *running_in_emacs = (char *)NULL;

/* The current offset in the current input line. */
int rl_point;

/* Mark in the current input line. */
int rl_mark;

/* Length of the current input line. */
int rl_end;

/* Make this non-zero to return the current input_line. */
int rl_done;

/* The last function executed by readline. */
Function *rl_last_func = (Function *)NULL;

/* Top level environment for readline_internal (). */
static jmp_buf readline_top_level;

/* The streams we interact with. */
static FILE *in_stream, *out_stream;

/* The names of the streams that we do input and output to. */
FILE *rl_instream = stdin, *rl_outstream = stdout;

/* Non-zero means echo characters as they are read. */
int readline_echoing_p = 1;

/* Current prompt. */
char *rl_prompt;

/* The number of characters read in order to type this complete command. */
int rl_key_sequence_length = 0;

/* If non-zero, then this is the address of a function to call just
   before readline_internal () prints the first prompt. */
Function *rl_startup_hook = (Function *)NULL;

/* What we use internally.  You should always refer to RL_LINE_BUFFER. */
static char *the_line;

/* The character that can generate an EOF.  Really read from
   the terminal driver... just defaulted here. */
static int eof_char = CTRL ('D');

/* Non-zero makes this the next keystroke to read. */
int rl_pending_input = 0;

/* Pointer to a useful terminal name. */
char *rl_terminal_name = (char *)NULL;

/* Line buffer and maintenence. */
char *rl_line_buffer = (char *)NULL;
static int rl_line_buffer_len = 0;
#define DEFAULT_BUFFER_SIZE 256


/* **************************************************************** */
/*								    */
/*			Top Level Functions			    */
/*								    */
/* **************************************************************** */

/* Read a line of input.  Prompt with PROMPT.  A NULL PROMPT means
   none.  A return value of NULL means that EOF was encountered. */
char *
readline (prompt)
     char *prompt;
{
  static rl_prep_terminal (), rl_deprep_terminal ();
  char *readline_internal ();
  char *value;

  rl_prompt = prompt;

  /* If we are at EOF return a NULL string. */
  if (rl_pending_input == EOF)
    {
      rl_pending_input = 0;
      return ((char *)NULL);
    }

  rl_initialize ();
  rl_prep_terminal ();

#ifdef SIGWINCH
  old_sigwinch = (SigHandler *)signal (SIGWINCH, rl_handle_sigwinch);
#endif

#ifdef HANDLE_SIGNALS
  rl_set_signals ();
#endif

  value = readline_internal ();
  rl_deprep_terminal ();

#ifdef SIGWINCH
  signal (SIGWINCH, old_sigwinch);
#endif

#ifdef HANDLE_SIGNALS
  rl_clear_signals ();
#endif

  return (value);
}

/* Read a line of input from the global rl_instream, doing output on
   the global rl_outstream.
   If rl_prompt is non-null, then that is our prompt. */
char *
readline_internal ()
{
  int lastc, c, eof_found;

  in_stream = rl_instream; out_stream = rl_outstream;
  lastc = eof_found = 0;

  if (rl_startup_hook)
    (*rl_startup_hook) ();

  if (!readline_echoing_p)
    {
      if (rl_prompt) {
	fprintf (out_stream, "%s", rl_prompt);
	fflush(out_stream);
      }
    }
  else
    {
      rl_on_new_line ();
      rl_redisplay ();
#ifdef VI_MODE
      if (rl_editing_mode == vi_mode)
	rl_vi_insertion_mode ();
#endif /* VI_MODE */
    }

  while (!rl_done)
    {
      int lk = last_command_was_kill;
      int code = setjmp (readline_top_level);

      if (code)
	rl_redisplay ();

      if (!rl_pending_input)
	{
	  /* Then initialize the argument and number of keys read. */
	  rl_init_argument ();
	  rl_key_sequence_length = 0;
	}

      c = rl_read_key ();

      /* EOF typed to a non-blank line is a <NL>. */
      if (c == EOF && rl_end)
	c = NEWLINE;

      /* The character eof_char typed to blank line, and not as the
	 previous character is interpreted as EOF. */
      if (((c == eof_char && lastc != c) || c == EOF) && !rl_end)
	{
	  eof_found = 1;
	  break;
	}

      lastc = c;
      rl_dispatch (c, keymap);

      /* If there was no change in last_command_was_kill, then no kill
	 has taken place.  Note that if input is pending we are reading
	 a prefix command, so nothing has changed yet. */
      if (!rl_pending_input)
	{
	  if (lk == last_command_was_kill)
	    last_command_was_kill = 0;
	}

#ifdef VI_MODE
      /* In vi mode, when you exit insert mode, the cursor moves back
	 over the previous character.  We explicitly check for that here. */
      if (rl_editing_mode == vi_mode && keymap == vi_movement_keymap)
	rl_vi_check ();
#endif

      if (!rl_done)
	rl_redisplay ();
    }

  /* Restore the original of this history line, iff the line that we
     are editing was originally in the history, AND the line has changed. */
  {
    HIST_ENTRY *entry = current_history ();

    if (entry && rl_undo_list)
      {
	char *temp = savestring (the_line);
	rl_revert_line ();
	entry = replace_history_entry (where_history (), the_line,
				       (HIST_ENTRY *)NULL);
	free_history_entry (entry);

	strcpy (the_line, temp);
	free (temp);
      }
  }

  /* At any rate, it is highly likely that this line has an undo list.  Get
     rid of it now. */
  if (rl_undo_list)
    free_undo_list ();

  if (eof_found)
    return (char *)NULL;
  else
    return (savestring (the_line));
}


/* Variables for keyboard macros.  */

/* The currently executing macro string.  If this is non-zero,
   then it is a malloc ()'ed string where input is coming from. */
static char *executing_macro = (char *)NULL;

/* The offset in the above string to the next character to be read. */
static int executing_macro_index = 0;

/* Non-zero means to save keys that we dispatch on in a kbd macro. */
static int defining_kbd_macro = 0;

/* The current macro string being built.  Characters get stuffed
   in here by add_macro_char (). */
static char *current_macro = (char *)NULL;

/* The size of the buffer allocated to current_macro. */
static int current_macro_size = 0;

/* The index at which characters are being added to current_macro. */
static int current_macro_index = 0;

/* A structure used to save nested macro strings.
   It is a linked list of string/index for each saved macro. */
struct saved_macro {
  struct saved_macro *next;
  char *string;
  int index;
};

/* The list of saved macros. */
struct saved_macro *macro_list = (struct saved_macro *)NULL;


/* **************************************************************** */
/*					        		    */
/*			   Signal Handling                          */
/*								    */
/* **************************************************************** */

#ifdef SIGWINCH
static void
rl_handle_sigwinch (sig, code, scp)
     int sig, code;
     struct sigcontext *scp;
{
  char *term = rl_terminal_name, *getenv ();

  if (readline_echoing_p)
    {
      if (!term)
	term = getenv ("TERM");
      if (!term)
	term = "dumb";
      rl_reset_terminal (term);
#ifdef NEVER
      crlf ();
      rl_forced_update_display ();
#endif
    }

  if (old_sigwinch &&
      old_sigwinch != (SigHandler *)SIG_IGN &&
      old_sigwinch != (SigHandler *)SIG_DFL)
    (*old_sigwinch)(sig, code, scp);
}
#endif  /* SIGWINCH */

#ifdef HANDLE_SIGNALS
/* Interrupt handling. */
static SigHandler *old_int  = (SigHandler *)NULL,
		  *old_tstp = (SigHandler *)NULL,
		  *old_ttou = (SigHandler *)NULL,
		  *old_ttin = (SigHandler *)NULL,
		  *old_cont = (SigHandler *)NULL;

/* Handle an interrupt character. */
static void
rl_signal_handler (sig, code, scp)
     int sig, code;
     struct sigcontext *scp;
{
  static rl_prep_terminal (), rl_deprep_terminal ();

  switch (sig)
    {
    case SIGINT:
      free_undo_list ();
      rl_clear_message ();
      rl_init_argument ();
#ifdef SIGWINCH
      signal (SIGWINCH, old_sigwinch);
#endif

#ifdef SIGTSTP
    case SIGTSTP:
    case SIGTTOU:
    case SIGTTIN:
#endif

      rl_clean_up_for_exit ();
      rl_deprep_terminal ();
      rl_clear_signals ();
      rl_pending_input = 0;

      kill (getpid (), sig);
      sigsetmask (0);

      rl_prep_terminal ();
      rl_set_signals ();
    }
}

rl_set_signals ()
{
  old_int = (SigHandler *)signal (SIGINT, rl_signal_handler);

  if (old_int == (SigHandler *)SIG_IGN)
    signal (SIGINT, SIG_IGN);

#ifdef SIGTSTP
  old_tstp = (SigHandler *)signal (SIGTSTP, rl_signal_handler);
  if (old_tstp == (SigHandler *)SIG_IGN)
    signal (SIGTSTP, SIG_IGN);
#endif
#ifdef SIGTTOU
  old_ttou = (SigHandler *)signal (SIGTTOU, rl_signal_handler);
  old_ttin = (SigHandler *)signal (SIGTTIN, rl_signal_handler);
#endif
}

rl_clear_signals ()
{
  signal (SIGINT, old_int);

#ifdef SIGTSTP
  signal (SIGTSTP, old_tstp);
#endif
#ifdef SIGTTOU
  signal (SIGTTOU, old_ttou);
  signal (SIGTTIN, old_ttin);
#endif
}
#endif  /* HANDLE_SIGNALS */



/* **************************************************************** */
/*								    */
/*			Character Input Buffering       	    */
/*								    */
/* **************************************************************** */

/* If the terminal was in xoff state when we got to it, then xon_char
   contains the character that is supposed to start it again. */
static int xon_char, xoff_state;
static int pop_index = 0, push_index = 0, ibuffer_len = 511;
static unsigned char ibuffer[512];

/* Non-null means it is a pointer to a function to run while waiting for
   character input. */
Function *rl_event_hook = (Function *)NULL;

#define any_typein (push_index != pop_index)

/* Add KEY to the buffer of characters to be read. */
rl_stuff_char (key)
     int key;
{
  if (key == EOF)
    {
      key = NEWLINE;
      rl_pending_input = EOF;
    }
  ibuffer[push_index++] = key;
  if (push_index >= ibuffer_len)
    push_index = 0;
}

/* Return the amount of space available in the
   buffer for stuffing characters. */
int
ibuffer_space ()
{
  if (pop_index > push_index)
    return (pop_index - push_index);
  else
    return (ibuffer_len - (push_index - pop_index));
}

/* Get a key from the buffer of characters to be read.
   Result is KEY if there was a key, or -2 if there wasn't. */
int
rl_get_char ()
{
  int key;

  if (push_index == pop_index)
    return (-2);

  key = ibuffer[pop_index++];

  if (pop_index >= ibuffer_len)
    pop_index = 0;

  return (key);
}

/* Stuff KEY into the *front* of the input buffer.
   Returns non-zero if successful, zero if there is
   no space left in the buffer. */
int
rl_unget_char (key)
     int key;
{
  if (ibuffer_space ())
    {
      pop_index--;
      if (pop_index < 0)
	pop_index = ibuffer_len - 1;
      ibuffer[pop_index] = key;
      return (1);
    }
  return (0);
}



static void
rl_getc (stream)
     FILE *stream;
{
  int result;
  int nchar;
  int tty = fileno(stream);
  char buf[512];	/* XXX - must be at least as large as ibuffer */

  while (1)
    {
      if (ioctl(tty, FIONREAD, &nchar) == -1)
	nchar = sizeof(buf);
      else if (nchar <= 0)
	nchar = 1;
      result = ibuffer_space();
      if (nchar > result)
	nchar = result;
      result = read(tty, buf, nchar);
      if (result > 0)
	{
	  register char *cp = buf;

	  while (--result >= 0)
	    rl_stuff_char(*cp++);
	  return;
	}
      if (errno != EINTR)
	{
	  rl_stuff_char(EOF);
	  return;
	}
    }
}

/* Read a key, including pending input. */
int
rl_read_key ()
{
  int c;

  rl_key_sequence_length++;

  if (rl_pending_input)
    {
      c = rl_pending_input;
      rl_pending_input = 0;
    }
  else
    {
      static int next_macro_key ();

      /* If input is coming from a macro, then use that. */
      if (c = next_macro_key ())
	return (c);

      while ((c = rl_get_char()) == -2)
	{
	  if (rl_event_hook)
	    {
	      (*rl_event_hook) ();
	      if ((c = rl_get_char()) != -2)
		return (c);
	    }
	  rl_getc(in_stream);
	}
    }
#ifdef TIOCSTART
  /* Ugh.  But I can't think of a better way. */
  if (xoff_state && c == xon_char)
    {
      ioctl (fileno (in_stream), TIOCSTART, 0);
      xoff_state = 0;
      return rl_read_key ();
    }
#endif /* TIOCSTART */
  return (c);
}

/* Do the command associated with KEY in MAP.
   If the associated command is really a keymap, then read
   another key, and dispatch into that map. */
rl_dispatch (key, map)
     register int key;
     Keymap map;
{
  if (defining_kbd_macro)
    {
      static add_macro_char ();

      add_macro_char (key);
    }

  if (key > 127 && key < 256)
    {
      if (map[ESC].type == ISKMAP)
	{
	  map = (Keymap)map[ESC].function;
	  key -= 128;
	  rl_dispatch (key, map);
	}
      else
	ding ();
      return;
    }

  switch (map[key].type)
    {
    case ISFUNC:
      {
	Function *func = map[key].function;

	if (func != (Function *)NULL)
	  {
	    /* Special case rl_do_lowercase_version (). */
	    if (func == rl_do_lowercase_version)
	      {
		rl_dispatch (to_lower (key), map);
		return;
	      }

	    (*map[key].function)(rl_numeric_arg * arg_sign, key);
	  }
	else
	  {
	    ding ();
	    return;
	  }
      }
      break;

    case ISKMAP:
      if (map[key].function != (Function *)NULL)
	{
	  int newkey;

	  rl_key_sequence_length++;
	  newkey = rl_read_key ();
	  rl_dispatch (newkey, (Keymap)map[key].function);
	}
      else
	{
	  ding ();
	  return;
	}
      break;

    case ISMACR:
      if (map[key].function != (Function *)NULL)
	{
	  static with_macro_input ();
	  char *macro = savestring ((char *)map[key].function);

	  with_macro_input (macro);
	  return;
	}
      break;
    }

  /* If we have input pending, then the last command was a prefix
     command.  Don't change the state of rl_last_func. */
  if (!rl_pending_input)
    rl_last_func = map[key].function;
}


/* **************************************************************** */
/*								    */
/*			Hacking Keyboard Macros 		    */
/*								    */
/* **************************************************************** */

/* Set up to read subsequent input from STRING.
   STRING is free ()'ed when we are done with it. */
static
with_macro_input (string)
     char *string;
{
  static push_executing_macro ();

  push_executing_macro ();
  executing_macro = string;
  executing_macro_index = 0;
}

/* Return the next character available from a macro, or 0 if
   there are no macro characters. */
static int
next_macro_key ()
{
  if (!executing_macro)
    return (0);

  if (!executing_macro[executing_macro_index])
    {
      static pop_executing_macro ();

      pop_executing_macro ();
      return (next_macro_key ());
    }

  return (executing_macro[executing_macro_index++]);
}

/* Save the currently executing macro on a stack of saved macros. */
static
push_executing_macro ()
{
  struct saved_macro *saver;

  saver = (struct saved_macro *)xmalloc (sizeof (struct saved_macro));
  saver->next = macro_list;
  saver->index = executing_macro_index;
  saver->string = executing_macro;

  macro_list = saver;
}

/* Discard the current macro, replacing it with the one
   on the top of the stack of saved macros. */
static
pop_executing_macro ()
{
  if (executing_macro)
    free (executing_macro);

  executing_macro = (char *)NULL;
  executing_macro_index = 0;

  if (macro_list)
    {
      struct saved_macro *disposer = macro_list;
      executing_macro = macro_list->string;
      executing_macro_index = macro_list->index;
      macro_list = macro_list->next;
      free (disposer);
    }
}

/* Add a character to the macro being built. */
static
add_macro_char (c)
     int c;
{
  if (current_macro_index + 1 >= current_macro_size)
    {
      if (!current_macro)
	current_macro = (char *)xmalloc (current_macro_size = 25);
      else
	current_macro =
	  (char *)xrealloc (current_macro, current_macro_size += 25);
    }

  current_macro[current_macro_index++] = c;
  current_macro[current_macro_index] = '\0';
}

/* Begin defining a keyboard macro.
   Keystrokes are recorded as they are executed.
   End the definition with rl_end_kbd_macro ().
   If a numeric argument was explicitly typed, then append this
   definition to the end of the existing macro, and start by
   re-executing the existing macro. */
rl_start_kbd_macro (ignore1, ignore2)
     int ignore1, ignore2;
{
  if (defining_kbd_macro)
    rl_abort ();

  if (rl_explicit_arg)
    {
      if (current_macro)
	with_macro_input (savestring (current_macro));
    }
  else
    current_macro_index = 0;

  defining_kbd_macro = 1;
}

/* Stop defining a keyboard macro.
   A numeric argument says to execute the macro right now,
   that many times, counting the definition as the first time. */
rl_end_kbd_macro (count, ignore)
     int count, ignore;
{
  if (!defining_kbd_macro)
    rl_abort ();

  current_macro_index -= (rl_key_sequence_length - 1);
  current_macro[current_macro_index] = '\0';

  defining_kbd_macro = 0;

  rl_call_last_kbd_macro (--count, 0);
}

/* Execute the most recently defined keyboard macro.
   COUNT says how many times to execute it. */
rl_call_last_kbd_macro (count, ignore)
     int count, ignore;
{
  if (!current_macro)
    rl_abort ();

  while (count--)
    with_macro_input (savestring (current_macro));
}


/* Non-zero means do not parse any lines other than comments and
   parser directives. */
static unsigned char parsing_conditionalized_out = 0;

/* **************************************************************** */
/*								    */
/*			Initializations 			    */
/*								    */
/* **************************************************************** */

/* Initliaze readline (and terminal if not already). */
rl_initialize ()
{
  extern char *rl_display_prompt;

  /* If we have never been called before, initialize the
     terminal and data structures. */
  if (!rl_initialized)
    {
      readline_initialize_everything ();
      rl_initialized++;
    }

  /* Initalize the current line information. */
  rl_point = rl_end = 0;
  the_line = rl_line_buffer;
  the_line[0] = 0;

  /* We aren't done yet.  We haven't even gotten started yet! */
  rl_done = 0;

  /* Tell the history routines what is going on. */
  start_using_history ();

  /* Make the display buffer match the state of the line. */
  {
    extern char *rl_display_prompt;
    extern int forced_display;

    rl_on_new_line ();

    rl_display_prompt = rl_prompt ? rl_prompt : "";
    forced_display = 1;
  }

  /* No such function typed yet. */
  rl_last_func = (Function *)NULL;

  /* Parsing of key-bindings begins in an enabled state. */
  {
    parsing_conditionalized_out = 0;
  }
}

/* Initialize the entire state of the world. */
readline_initialize_everything ()
{
  /* Find out if we are running in Emacs. */
  running_in_emacs = (char *)getenv ("EMACS");

  /* Allocate data structures. */
  if (!rl_line_buffer)
    rl_line_buffer =
      (char *)xmalloc (rl_line_buffer_len = DEFAULT_BUFFER_SIZE);

  /* Initialize the terminal interface. */
  init_terminal_io ((char *)NULL);

  /* Bind tty characters to readline functions. */
  readline_default_bindings ();

  /* Initialize the function names. */
  rl_initialize_funmap ();

  /* Read in the init file. */
  rl_read_init_file ((char *)NULL);

  /* If the completion parser's default word break characters haven't
     been set yet, then do so now. */
  {
    extern char *rl_completer_word_break_characters;
    extern char *rl_basic_word_break_characters;

    if (rl_completer_word_break_characters == (char *)NULL)
      rl_completer_word_break_characters = rl_basic_word_break_characters;
  }
}

/* If this system allows us to look at the values of the regular
   input editing characters, then bind them to their readline
   equivalents. */
readline_default_bindings ()
{
#ifdef TIOCGETP
  struct sgttyb ttybuff;
  int tty = fileno (rl_instream);

  if (ioctl (tty, TIOCGETP, &ttybuff) != -1)
    {
      int erase = ttybuff.sg_erase, kill = ttybuff.sg_kill;

      if (erase != -1 && keymap[erase].type == ISFUNC)
	keymap[erase].function = rl_rubout;

      if (kill != -1 && keymap[kill].type == ISFUNC)
	keymap[kill].function = rl_unix_line_discard;
    }

#ifdef TIOCGLTC
  {
    struct ltchars lt;

    if (ioctl (tty, TIOCGLTC, &lt) != -1)
      {
	int erase = lt.t_werasc, nextc = lt.t_lnextc;

	if (erase != -1 && keymap[erase].type == ISFUNC)
	  keymap[erase].function = rl_unix_word_rubout;

	if (nextc != -1 && keymap[nextc].type == ISFUNC)
	  keymap[nextc].function = rl_quoted_insert;
      }
  }
#endif /* TIOCGLTC */
#endif /*  TIOCGETP */
}


/* **************************************************************** */
/*								    */
/*			Numeric Arguments			    */
/*								    */
/* **************************************************************** */

/* Handle C-u style numeric args, as well as M--, and M-digits. */

/* Add the current digit to the argument in progress. */
rl_digit_argument (ignore, key)
     int ignore, key;
{
  rl_pending_input = key;
  rl_digit_loop ();
}

/* What to do when you abort reading an argument. */
rl_discard_argument ()
{
  ding ();
  rl_clear_message ();
  rl_init_argument ();
}

/* Create a default argument. */
rl_init_argument ()
{
  rl_numeric_arg = arg_sign = 1;
  rl_explicit_arg = 0;
}

/* C-u, universal argument.  Multiply the current argument by 4.
   Read a key.  If the key has nothing to do with arguments, then
   dispatch on it.  If the key is the abort character then abort. */
rl_universal_argument ()
{
  rl_numeric_arg *= 4;
  rl_digit_loop ();
}

rl_digit_loop ()
{
  int key, c;
  while (1)
    {
      rl_message ("(arg: %d) ", arg_sign * rl_numeric_arg);
      key = c = rl_read_key ();

      if (keymap[c].type == ISFUNC &&
	  keymap[c].function == rl_universal_argument)
	{
	  rl_numeric_arg *= 4;
	  continue;
	}
      c = UNMETA (c);
      if (numeric (c))
	{
	  if (rl_explicit_arg)
	    rl_numeric_arg = (rl_numeric_arg * 10) + (c - '0');
	  else
	    rl_numeric_arg = (c - '0');
	  rl_explicit_arg = 1;
	}
      else
	{
	  if (c == '-' && !rl_explicit_arg)
	    {
	      rl_numeric_arg = 1;
	      arg_sign = -1;
	    }
	  else
	    {
	      rl_clear_message ();
	      rl_dispatch (key, keymap);
	      return;
	    }
	}
    }
}


/* **************************************************************** */
/*								    */
/*			Display stuff				    */
/*								    */
/* **************************************************************** */

/* This is the stuff that is hard for me.  I never seem to write good
   display routines in C.  Let's see how I do this time. */

/* (PWP) Well... Good for a simple line updater, but totally ignores
   the problems of input lines longer than the screen width.

   update_line and the code that calls it makes a multiple line,
   automatically wrapping line update.  Carefull attention needs
   to be paid to the vertical position variables.

   handling of terminals with autowrap on (incl. DEC braindamage)
   could be improved a bit.  Right now I just cheat and decrement
   screenwidth by one. */

/* Keep two buffers; one which reflects the current contents of the
   screen, and the other to draw what we think the new contents should
   be.  Then compare the buffers, and make whatever changes to the
   screen itself that we should.  Finally, make the buffer that we
   just drew into be the one which reflects the current contents of the
   screen, and place the cursor where it belongs.

   Commands that want to can fix the display themselves, and then let
   this function know that the display has been fixed by setting the
   RL_DISPLAY_FIXED variable.  This is good for efficiency. */

/* Termcap variables: */
extern char *term_up, *term_dc, *term_cr;
extern int screenheight, screenwidth, terminal_can_insert;

/* What YOU turn on when you have handled all redisplay yourself. */
int rl_display_fixed = 0;

/* The visible cursor position.  If you print some text, adjust this. */
int last_c_pos = 0;
int last_v_pos = 0;

/* The last left edge of text that was displayed.  This is used when
   doing horizontal scrolling.  It shifts in thirds of a screenwidth. */
static int last_lmargin = 0;

/* The line display buffers.  One is the line currently displayed on
   the screen.  The other is the line about to be displayed. */
static char *visible_line = (char *)NULL;
static char *invisible_line = (char *)NULL;

/* Number of lines currently on screen minus 1. */
int vis_botlin = 0;

/* A buffer for `modeline' messages. */
char msg_buf[128];

/* Non-zero forces the redisplay even if we thought it was unnecessary. */
int forced_display = 0;

/* The stuff that gets printed out before the actual text of the line.
   This is usually pointing to rl_prompt. */
char *rl_display_prompt = (char *)NULL;

/* Default and initial buffer size.  Can grow. */
static int line_size = 1024;

/* Non-zero means to always use horizontal scrolling in line display. */
int horizontal_scroll_mode = 0;

/* I really disagree with this, but my boss (among others) insists that we
   support compilers that don't work.  I don't think we are gaining by doing
   so; what is the advantage in producing better code if we can't use it? */
/* The following two declarations belong inside the
   function block, not here. */
static void move_cursor_relative ();
static void output_some_chars ();

/* Basic redisplay algorithm. */
rl_redisplay ()
{
  register int in, out, c, linenum;
  register char *line = invisible_line;
  int c_pos = 0;
  int inv_botlin = 0;		/* Number of lines in newly drawn buffer. */

  extern int readline_echoing_p;

  if (!readline_echoing_p)
    return;

  if (!rl_display_prompt)
    rl_display_prompt = "";

  if (!invisible_line)
    {
      visible_line = (char *)xmalloc (line_size);
      invisible_line = (char *)xmalloc (line_size);
      line = invisible_line;
      for (in = 0; in < line_size; in++)
	{
	  visible_line[in] = 0;
	  invisible_line[in] = 1;
	}
      rl_on_new_line ();
    }

  /* Draw the line into the buffer. */
  c_pos = -1;

  /* Mark the line as modified or not.  We only do this for history
     lines. */
  out = 0;
  if (current_history () && rl_undo_list)
    {
      line[out++] = '*';
      line[out] = '\0';
    }

  /* If someone thought that the redisplay was handled, but the currently
     visible line has a different modification state than the one about
     to become visible, then correct the callers misconception. */
  if (visible_line[0] != invisible_line[0])
    rl_display_fixed = 0;

  strncpy (line + out,  rl_display_prompt, strlen (rl_display_prompt));
  out += strlen (rl_display_prompt);
  line[out] = '\0';

  for (in = 0; in < rl_end; in++)
    {
      c = the_line[in];

      if (out + 1 >= line_size)
	{
	  line_size *= 2;
	  visible_line = (char *)xrealloc (visible_line, line_size);
	  invisible_line = (char *)xrealloc (invisible_line, line_size);
	  line = invisible_line;
	}

      if (in == rl_point)
	c_pos = out;

      if (c > 127)
	{
	  line[out++] = 'M';
	  line[out++] = '-';
	  line[out++] = c - 128;
	}
#define DISPLAY_TABS
#ifdef DISPLAY_TABS
      else if (c == '\t')
	{
	  register int newout = (out | (int)7) + 1;
	  while (out < newout)
	    line[out++] = ' ';
	}
#endif
      else if (c < 32)
	{
	  line[out++] = 'C';
	  line[out++] = '-';
	  line[out++] = c + 64;
	}
      else
	line[out++] = c;
    }
  line[out] = '\0';
  if (c_pos < 0)
    c_pos = out;

  /* PWP: now is when things get a bit hairy.  The visible and invisible
     line buffers are really multiple lines, which would wrap every
     (screenwidth - 1) characters.  Go through each in turn, finding
     the changed region and updating it.  The line order is top to bottom. */

  /* If we can move the cursor up and down, then use multiple lines,
     otherwise, let long lines display in a single terminal line, and
     horizontally scroll it. */

  if (!horizontal_scroll_mode && term_up && *term_up)
    {
      int total_screen_chars = (screenwidth * screenheight);

      if (!rl_display_fixed || forced_display)
	{
	  forced_display = 0;

	  /* If we have more than a screenful of material to display, then
	     only display a screenful.  We should display the last screen,
	     not the first.  I'll fix this in a minute. */
	  if (out >= total_screen_chars)
	    out = total_screen_chars - 1;

	  /* Number of screen lines to display. */
	  inv_botlin = out / screenwidth;

	  /* For each line in the buffer, do the updating display. */
	  for (linenum = 0; linenum <= inv_botlin; linenum++)
	    update_line (linenum > vis_botlin ? ""
			 : &visible_line[linenum * screenwidth],
			 &invisible_line[linenum * screenwidth],
			 linenum);

	  /* We may have deleted some lines.  If so, clear the left over
	     blank ones at the bottom out. */
	  if (vis_botlin > inv_botlin)
	    {
	      char *tt;
	      for (; linenum <= vis_botlin; linenum++)
		{
		  tt = &visible_line[linenum * screenwidth];
		  move_vert (linenum);
		  move_cursor_relative (0, tt);
		  clear_to_eol ((linenum == vis_botlin)?
				strlen (tt) : screenwidth);
		}
	    }
	  vis_botlin = inv_botlin;

	  /* Move the cursor where it should be. */
	  move_vert (c_pos / screenwidth);
	  move_cursor_relative (c_pos % screenwidth,
				&invisible_line[(c_pos / screenwidth) * screenwidth]);
	}
    }
  else				/* Do horizontal scrolling. */
    {
      int lmargin;

      /* Always at top line. */
      last_v_pos = 0;

      /* If the display position of the cursor would be off the edge
	 of the screen, start the display of this line at an offset that
	 leaves the cursor on the screen. */
      if (c_pos - last_lmargin > screenwidth - 2)
	lmargin = (c_pos / (screenwidth / 3) - 2) * (screenwidth / 3);
      else if (c_pos - last_lmargin < 1)
	lmargin = ((c_pos - 1) / (screenwidth / 3)) * (screenwidth / 3);
      else
	lmargin = last_lmargin;

      /* If the first character on the screen isn't the first character
	 in the display line, indicate this with a special character. */
      if (lmargin > 0)
	line[lmargin] = '<';

      if (lmargin + screenwidth < out)
	line[lmargin + screenwidth - 1] = '>';

      if (!rl_display_fixed || forced_display || lmargin != last_lmargin)
	{
	  forced_display = 0;
	  update_line (&visible_line[last_lmargin],
		       &invisible_line[lmargin], 0);

	  move_cursor_relative (c_pos - lmargin, &invisible_line[lmargin]);
	  last_lmargin = lmargin;
	}
    }
  fflush (out_stream);

  /* Swap visible and non-visible lines. */
  {
    char *temp = visible_line;
    visible_line = invisible_line;
    invisible_line = temp;
    rl_display_fixed = 0;
  }
}

/* PWP: update_line() is based on finding the middle difference of each
   line on the screen; vis:

			     /old first difference
	/beginning of line   |              /old last same       /old EOL
	v		     v              v                    v
old:	eddie> Oh, my little gruntle-buggy is to me, as lurgid as
new:	eddie> Oh, my little buggy says to me, as lurgid as
	^		     ^        ^			   ^
	\beginning of line   |        \new last same	   \new end of line
			     \new first difference

   All are character pointers for the sake of speed.  Special cases for
   no differences, as well as for end of line additions must be handeled.

   Could be made even smarter, but this works well enough */
static
update_line (old, new, current_line)
     register char *old, *new;
     int current_line;
{
  register char *ofd, *ols, *oe, *nfd, *nls, *ne;
  int lendiff, wsatend;

  /* Find first difference. */
  for (ofd = old, nfd = new;
       (ofd - old < screenwidth) && *ofd && (*ofd == *nfd);
       ofd++, nfd++)
    ;

  /* Move to the end of the screen line. */
  for (oe = ofd; ((oe - old) < screenwidth) && *oe; oe++);
  for (ne = nfd; ((ne - new) < screenwidth) && *ne; ne++);

  /* If no difference, continue to next line. */
  if (ofd == oe && nfd == ne)
    return;

  wsatend = 1;			/* flag for trailing whitespace */
  ols = oe - 1;			/* find last same */
  nls = ne - 1;
  while ((*ols == *nls) && (ols > ofd) && (nls > nfd))
    {
      if (*ols != ' ')
	wsatend = 0;
      ols--;
      nls--;
    }

  if (wsatend)
    {
      ols = oe;
      nls = ne;
    }
  else if (*ols != *nls)
    {
      if (*ols)			/* don't step past the NUL */
	ols++;
      if (*nls)
	nls++;
    }

  move_vert (current_line);
  move_cursor_relative (ofd - old, old);

  /* if (len (new) > len (old)) */
  lendiff = (nls - nfd) - (ols - ofd);

  /* Insert (diff(len(old),len(new)) ch */
  if (lendiff > 0)
    {
      if (terminal_can_insert)
	{
	  extern char *term_IC;

	  /* Sometimes it is cheaper to print the characters rather than
	     use the terminal's capabilities. */
	  if ((2 * (ne - nfd)) < lendiff && (!term_IC || !*term_IC))
	    {
	      output_some_chars (nfd, (ne - nfd));
	      last_c_pos += (ne - nfd);
	    }
	  else
	    {
	      if (*ols)
		{
		  start_insert (lendiff);
		  output_some_chars (nfd, lendiff);
		  last_c_pos += lendiff;
		  end_insert ();
		}
	      else
		{
		  /* At the end of a line the characters do not have to
		     be "inserted".  They can just be placed on the screen. */
		  output_some_chars (nfd, lendiff);
		  last_c_pos += lendiff;
		}
	      /* Copy (new) chars to screen from first diff to last match. */
	      if (((nls - nfd) - lendiff) > 0)
		{
		  output_some_chars (&nfd[lendiff], ((nls - nfd) - lendiff));
		  last_c_pos += ((nls - nfd) - lendiff);
		}
	    }
	}
      else
	{		/* cannot insert chars, write to EOL */
	  output_some_chars (nfd, (ne - nfd));
	  last_c_pos += (ne - nfd);
	}
    }
  else				/* Delete characters from line. */
    {
      /* If possible and inexpensive to use terminal deletion, then do so. */
      if (term_dc && (2 * (ne - nfd)) >= (-lendiff))
	{
	  if (lendiff)
	    delete_chars (-lendiff); /* delete (diff) characters */

	  /* Copy (new) chars to screen from first diff to last match */
	  if ((nls - nfd) > 0)
	    {
	      output_some_chars (nfd, (nls - nfd));
	      last_c_pos += (nls - nfd);
	    }
	}
      /* Otherwise, print over the existing material. */
      else
	{
	  output_some_chars (nfd, (ne - nfd));
	  last_c_pos += (ne - nfd);
	  clear_to_eol ((oe - old) - (ne - new));
	}
    }
}

/* (PWP) tell the update routines that we have moved onto a
   new (empty) line. */
rl_on_new_line ()
{
  if (visible_line)
    visible_line[0] = '\0';

  last_c_pos = last_v_pos = 0;
  vis_botlin = last_lmargin = 0;
}

/* Actually update the display, period. */
rl_forced_update_display ()
{
  if (visible_line)
    {
      register char *temp = visible_line;

      while (*temp) *temp++ = '\0';
    }
  rl_on_new_line ();
  forced_display++;
  rl_redisplay ();
}

/* Move the cursor from last_c_pos to NEW, which are buffer indices.
   DATA is the contents of the screen line of interest; i.e., where
   the movement is being done. */
static void
move_cursor_relative (new, data)
     int new;
     char *data;
{
  register int i;
  static void output_character_function ();

  /* It may be faster to output a CR, and then move forwards instead
     of moving backwards. */
  if (new + 1 < last_c_pos - new)
    {
      tputs (term_cr, 1, output_character_function);
      last_c_pos = 0;
    }

  if (last_c_pos == new) return;

  if (last_c_pos < new)
    {
      /* Move the cursor forward.  We do it by printing the command
	 to move the cursor forward if there is one, else print that
	 portion of the output buffer again.  Which is cheaper? */

      /* The above comment is left here for posterity.  It is faster
	 to print one character (non-control) than to print a control
	 sequence telling the terminal to move forward one character.
	 That kind of control is for people who don't know what the
	 data is underneath the cursor. */
#ifdef HACK_TERMCAP_MOTION
      extern char *term_forward_char;

      if (term_forward_char)
	for (i = last_c_pos; i < new; i++)
	  tputs (term_forward_char, 1, output_character_function);
      else
	for (i = last_c_pos; i < new; i++)
	  putc (data[i], out_stream);
#else
      for (i = last_c_pos; i < new; i++)
	putc (data[i], out_stream);
#endif				/* HACK_TERMCAP_MOTION */
    }
  else
    backspace (last_c_pos - new);
  last_c_pos = new;
}

/* PWP: move the cursor up or down. */
move_vert (to)
     int to;
{
  void output_character_function ();
  register int delta, i;

  if (last_v_pos == to) return;

  if (to > screenheight)
    return;

  if ((delta = to - last_v_pos) > 0)
    {
      for (i = 0; i < delta; i++)
	putc ('\n', out_stream);
      tputs (term_cr, 1, output_character_function);
      last_c_pos = 0;		/* because crlf() will do \r\n */
    }
  else
    {			/* delta < 0 */
      if (term_up && *term_up)
	for (i = 0; i < -delta; i++)
	  tputs (term_up, 1, output_character_function);
    }
  last_v_pos = to;		/* now to is here */
}

/* Physically print C on out_stream.  This is for functions which know
   how to optimize the display. */
rl_show_char (c)
     int c;
{
  if (c > 127)
    {
      fprintf (out_stream, "M-");
      c -= 128;
    }

#ifdef DISPLAY_TABS
  if (c < 32 && c != '\t')
#else
  if (c < 32)
#endif
    {

      c += 64;
    }

  putc (c, out_stream);
  fflush (out_stream);
}

#ifdef DISPLAY_TABS
int
rl_character_len (c, pos)
     register int c, pos;
{
  if (c < ' ' || c > 126)
    {
      if (c == '\t')
	return (((pos | (int)7) + 1) - pos);
      else
	return (3);
    }
  else
    return (1);
}
#else
int
rl_character_len (c)
     int c;
{
  if (c < ' ' || c > 126)
    return (3);
  else
    return (1);
}
#endif  /* DISPLAY_TAB */

/* How to print things in the "echo-area".  The prompt is treated as a
   mini-modeline. */
rl_message (string, arg1, arg2)
     char *string;
{
  sprintf (msg_buf, string, arg1, arg2);
  rl_display_prompt = msg_buf;
  rl_redisplay ();
}

/* How to clear things from the "echo-area". */
rl_clear_message ()
{
  rl_display_prompt = rl_prompt;
  rl_redisplay ();
}

/* **************************************************************** */
/*								    */
/*			Terminal and Termcap			    */
/*								    */
/* **************************************************************** */

static char *term_buffer = (char *)NULL;
static char *term_string_buffer = (char *)NULL;

/* Non-zero means this terminal can't really do anything. */
int dumb_term = 0;

char PC;
char *BC, *UP;

/* Some strings to control terminal actions.  These are output by tputs (). */
char *term_goto, *term_clreol, *term_cr, *term_clrpag, *term_backspace;

int screenwidth, screenheight;

/* Non-zero if we determine that the terminal can do character insertion. */
int terminal_can_insert = 0;

/* How to insert characters. */
char *term_im, *term_ei, *term_ic, *term_ip, *term_IC;

/* How to delete characters. */
char *term_dc, *term_DC;

#ifdef HACK_TERMCAP_MOTION
char *term_forward_char;
#endif  /* HACK_TERMCAP_MOTION */

/* How to go up a line. */
char *term_up;

/* Re-initialize the terminal considering that the TERM/TERMCAP variable
   has changed. */
rl_reset_terminal (terminal_name)
     char *terminal_name;
{
  init_terminal_io (terminal_name);
}

init_terminal_io (terminal_name)
     char *terminal_name;
{
  char *term = (terminal_name? terminal_name : (char *)getenv ("TERM"));
  char *tgetstr (), *buffer;


  if (!term_string_buffer)
    term_string_buffer = (char *)xmalloc (2048);

  if (!term_buffer)
    term_buffer = (char *)xmalloc (2048);

  buffer = term_string_buffer;

  term_clrpag = term_cr = term_clreol = (char *)NULL;

  if (!term)
    term = "dumb";

  if (tgetent (term_buffer, term) < 0)
    {
      dumb_term = 1;
      return;
    }

  BC = tgetstr ("pc", &buffer);
  PC = buffer ? *buffer : 0;

  term_backspace = tgetstr ("le", &buffer);

  term_cr = tgetstr ("cr", &buffer);
  term_clreol = tgetstr ("ce", &buffer);
  term_clrpag = tgetstr ("cl", &buffer);

  if (!term_cr)
    term_cr =  "\r";

#ifdef HACK_TERMCAP_MOTION
  term_forward_char = tgetstr ("nd", &buffer);
#endif  /* HACK_TERMCAP_MOTION */

  screenwidth = tgetnum ("co");
  if (screenwidth <= 0)
    screenwidth = 80;
  screenwidth--;		/* PWP: avoid autowrap bugs */

  screenheight = tgetnum ("li");
  if (screenheight <= 0)
    screenheight = 24;

  term_im = tgetstr ("im", &buffer);
  term_ei = tgetstr ("ei", &buffer);
  term_IC = tgetstr ("IC", &buffer);
  term_ic = tgetstr ("ic", &buffer);
  term_ip = tgetstr ("ip", &buffer);
  term_IC = tgetstr ("IC", &buffer);

  /* "An application program can assume that the terminal can do
      character insertion if *any one of* the capabilities `IC',
      `im', `ic' or `ip' is provided." */
#ifdef notdef
  /* XXX Circumvent broken code. */
  terminal_can_insert = (term_IC || term_im || term_ic || term_ip);
#endif

  term_up = tgetstr ("up", &buffer);
  term_dc = tgetstr ("dc", &buffer);
  term_DC = tgetstr ("DC", &buffer);
}

/* A function for the use of tputs () */
static void
output_character_function (c)
     int c;
{
  putc (c, out_stream);
}

/* Write COUNT characters from STRING to the output stream. */
static void
output_some_chars (string, count)
     char *string;
     int count;
{
  fwrite (string, 1, count, out_stream);
}


/* Delete COUNT characters from the display line. */
static
delete_chars (count)
     int count;
{
  if (count > screenwidth)
    return;

  if (term_DC && *term_DC)
    {
      char *tgoto (), *buffer;
      buffer = tgoto (term_DC, 0, count);
      tputs (buffer, 1, output_character_function);
    }
  else
    {
      if (term_dc && *term_dc)
	while (count--)
	  tputs (term_dc, 1, output_character_function);
    }
}

/* Prepare to insert by inserting COUNT blank spaces. */
static
start_insert (count)
     int count;
{
  if (term_im && *term_im)
    tputs (term_im, 1, output_character_function);

  if (term_IC && *term_IC &&
      (count > 1 || !term_ic || !*term_ic))
    {
      char *tgoto (), *buffer;
      buffer = tgoto (term_IC, 0, count);
      tputs (buffer, 1, output_character_function);
    }
  else
    {
      if (term_ic && *term_ic)
	while (count--)
	  tputs (term_ic, 1, output_character_function);
    }
}

/* We are finished doing our insertion.  Send ending string. */
static
end_insert ()
{
  if (term_ei && *term_ei)
    tputs (term_ei, 1, output_character_function);
}

/* Move the cursor back. */
backspace (count)
     int count;
{
  register int i;

  if (term_backspace)
    for (i = 0; i < count; i++)
      tputs (term_backspace, 1, output_character_function);
  else
    for (i = 0; i < count; i++)
      putc ('\b', out_stream);
}

/* Move to the start of the next line. */
crlf ()
{
  tputs (term_cr, 1, output_character_function);
  putc ('\n', out_stream);
}

/* Clear to the end of the line.  COUNT is the minimum
   number of character spaces to clear, */
clear_to_eol (count)
     int count;
{
  if (term_clreol) {
    tputs (term_clreol, 1, output_character_function);
  } else {
    register int i;
    /* Do one more character space. */
    count++;
    for (i = 0; i < count; i++)
      putc (' ', out_stream);
    backspace (count);
  }
}


/* **************************************************************** */
/*								    */
/*		      Saving and Restoring the TTY	    	    */
/*								    */
/* **************************************************************** */

#ifdef NEW_TTY_DRIVER

/* Standard flags, including ECHO. */
static int original_tty_flags = 0;

/* Local mode flags, like LPASS8. */
static int local_mode_flags = 0;

/* Terminal characters.  This has C-s and C-q in it. */
static struct tchars original_tchars;

/* Local special characters.  This has the interrupt characters in it. */
static struct ltchars original_ltchars;

/* We use this to get and set the tty_flags. */
static struct sgttyb the_ttybuff;

/* Put the terminal in CBREAK mode so that we can detect key presses. */
static
rl_prep_terminal ()
{
  int tty = fileno (rl_instream);

  /* We always get the latest tty values.  Maybe stty changed them. */

  ioctl (tty, TIOCGETP, &the_ttybuff);
  original_tty_flags = the_ttybuff.sg_flags;

  readline_echoing_p = (original_tty_flags & ECHO);

  /* If this terminal doesn't care how the 8th bit is used,
     then we can use it for the meta-key.
     We check by seeing if BOTH odd and even parity are allowed. */
  if ((the_ttybuff.sg_flags & (ODDP | EVENP)) == (ODDP | EVENP))
    {
#ifdef PASS8
      the_ttybuff.sg_flags |= PASS8;
#endif

#if defined (TIOCLGET) && defined (LPASS8)
      {
	int flags;
	ioctl (tty, TIOCLGET, &flags);
	local_mode_flags = flags;
	flags |= LPASS8;
	ioctl (tty, TIOCLSET, &flags);
      }
#endif
    }

#ifdef TIOCGETC
  {
    struct tchars temp;

    ioctl (tty, TIOCGETC, &original_tchars);
    bcopy (&original_tchars, &temp, sizeof (struct tchars));

    /* Get rid of C-s and C-q.
       We remember the value of startc (C-q) so that if the terminal is in
       xoff state, the user can xon it by pressing that character. */
    xon_char = temp.t_startc;
    temp.t_stopc = -1;
    temp.t_startc = -1;

    /* If there is an XON character, bind it to restart the output. */
    if (xon_char != -1)
      rl_bind_key (xon_char, rl_restart_output);

    /* If there is an EOF char, bind eof_char to it. */
    if (temp.t_eofc != -1)
      eof_char = temp.t_eofc;

#ifdef NEVER
    /* Get rid of C-\ and C-c. */
    temp.t_intrc = temp.t_quitc = -1;
#endif

    ioctl (tty, TIOCSETC, &temp);
  }
#endif /* TIOCGETC */

#ifdef TIOCGLTC
  {
    struct ltchars temp;

    ioctl (tty, TIOCGLTC, &original_ltchars);
    bcopy (&original_ltchars, &temp, sizeof (struct ltchars));

    /* Make the interrupt keys go away.  Just enough to make people happy. */
    temp.t_dsuspc = -1;		/* C-y */
    temp.t_lnextc = -1;		/* C-v */

    ioctl (tty, TIOCSLTC, &temp);
  }
#endif /* TIOCGLTC */

  the_ttybuff.sg_flags &= ~ECHO;
  the_ttybuff.sg_flags |= CBREAK;
  ioctl (tty, TIOCSETN, &the_ttybuff);
}

/* Restore the terminal to its original state. */
static
rl_deprep_terminal ()
{
  int tty = fileno (rl_instream);

#if defined (TIOCLGET) && defined (LPASS8)
  if ((the_ttybuff.sg_flags & (ODDP | EVENP)) == (ODDP | EVENP))
     ioctl (tty, TIOCLSET, &local_mode_flags);
#endif

#ifdef TIOCSLTC
  ioctl (tty, TIOCSLTC, &original_ltchars);
#endif

#ifdef TIOCSETC
  ioctl (tty, TIOCSETC, &original_tchars);
#endif

  the_ttybuff.sg_flags = original_tty_flags;
  ioctl (tty, TIOCSETN, &the_ttybuff);
  readline_echoing_p = 1;
}

#else  /* !defined (NEW_TTY_DRIVER) */
static struct termio otio;

static
rl_prep_terminal ()
{
  int tty = fileno (rl_instream);
  struct termio tio;

  ioctl (tty, TCGETA, &tio);
  ioctl (tty, TCGETA, &otio);

  readline_echoing_p = (tio.c_lflag & ECHO);

  tio.c_lflag &= ~(ICANON|ECHO);
  tio.c_iflag &= ~(IXON|ISTRIP|INPCK);

#ifndef HANDLE_SIGNALS
  tio.c_lflag &= ~ISIG;
#endif

  tio.c_cc[VEOF] = 1;		/* really: MIN */
  tio.c_cc[VEOL] = 0;		/* really: TIME */
  ioctl (tty, TCSETAW,&tio);
}

static
rl_deprep_terminal ()
{
  int tty = fileno (rl_instream);
  ioctl (tty, TCSETAW, &otio);
}
#endif  /* NEW_TTY_DRIVER */


/* **************************************************************** */
/*								    */
/*			Utility Functions			    */
/*								    */
/* **************************************************************** */

/* Return 0 if C is not a member of the class of characters that belong
   in words, or 1 if it is. */

int allow_pathname_alphabetic_chars = 0;
char *pathname_alphabetic_chars = "/-_=~.#$";

int
alphabetic (c)
     int c;
{
  char *rindex ();
  if (pure_alphabetic (c) || (numeric (c)))
    return (1);

  if (allow_pathname_alphabetic_chars)
    return ((int)rindex (pathname_alphabetic_chars, c));
  else
    return (0);
}

/* Return non-zero if C is a numeric character. */
int
numeric (c)
     int c;
{
  return (c >= '0' && c <= '9');
}

/* Ring the terminal bell. */
int
ding ()
{
  if (readline_echoing_p)
    {
      fprintf (stderr, "\007");
      fflush (stderr);
    }
  return (-1);
}

/* How to abort things. */
rl_abort ()
{
  ding ();
  rl_clear_message ();
  rl_init_argument ();
  rl_pending_input = 0;

  defining_kbd_macro = 0;
  while (executing_macro)
    pop_executing_macro ();

  longjmp (readline_top_level, 1);
}

/* Return a copy of the string between FROM and TO.
   FROM is inclusive, TO is not. */
char *
rl_copy (from, to)
     int from, to;
{
  register int length;
  char *copy;

  /* Fix it if the caller is confused. */
  if (from > to) {
    int t = from;
    from = to;
    to = t;
  }

  length = to - from;
  copy = (char *)xmalloc (1 + length);
  strncpy (copy, the_line + from, length);
  copy[length] = '\0';
  return (copy);
}


/* **************************************************************** */
/*								    */
/*			Insert and Delete			    */
/*								    */
/* **************************************************************** */


/* Insert a string of text into the line at point.  This is the only
   way that you should do insertion.  rl_insert () calls this
   function. */
rl_insert_text (string)
     char *string;
{
  extern int doing_an_undo;
  register int i, l = strlen (string);
  while (rl_end + l >= rl_line_buffer_len)
    {
      rl_line_buffer =
	(char *)xrealloc (rl_line_buffer,
			  rl_line_buffer_len += DEFAULT_BUFFER_SIZE);
      the_line = rl_line_buffer;
    }

  for (i = rl_end; i >= rl_point; i--)
    the_line[i + l] = the_line[i];
  strncpy (the_line + rl_point, string, l);

  /* Remember how to undo this if we aren't undoing something. */
  if (!doing_an_undo)
    {
      /* If possible and desirable, concatenate the undos. */
      if ((strlen (string) == 1) &&
	  rl_undo_list &&
	  (rl_undo_list->what == UNDO_INSERT) &&
	  (rl_undo_list->end == rl_point) &&
	  (rl_undo_list->end - rl_undo_list->start < 20))
	rl_undo_list->end++;
      else
	rl_add_undo (UNDO_INSERT, rl_point, rl_point + l, (char *)NULL);
    }
  rl_point += l;
  rl_end += l;
  the_line[rl_end] = '\0';
}

/* Delete the string between FROM and TO.  FROM is
   inclusive, TO is not. */
rl_delete_text (from, to)
     int from, to;
{
  extern int doing_an_undo;
  register char *text;

  /* Fix it if the caller is confused. */
  if (from > to) {
    int t = from;
    from = to;
    to = t;
  }
  text = rl_copy (from, to);
  strncpy (the_line + from, the_line + to, rl_end - to);

  /* Remember how to undo this delete. */
  if (!doing_an_undo)
    rl_add_undo (UNDO_DELETE, from, to, text);
  else
    free (text);

  rl_end -= (to - from);
  the_line[rl_end] = '\0';
}


/* **************************************************************** */
/*								    */
/*			Readline character functions		    */
/*								    */
/* **************************************************************** */

/* This is not a gap editor, just a stupid line input routine.  No hair
   is involved in writing any of the functions, and none should be. */

/* Note that:

   rl_end is the place in the string that we would place '\0';
   i.e., it is always safe to place '\0' there.

   rl_point is the place in the string where the cursor is.  Sometimes
   this is the same as rl_end.

   Any command that is called interactively receives two arguments.
   The first is a count: the numeric arg pased to this command.
   The second is the key which invoked this command.
*/


/* **************************************************************** */
/*								    */
/*			Movement Commands			    */
/*								    */
/* **************************************************************** */

/* Note that if you `optimize' the display for these functions, you cannot
   use said functions in other functions which do not do optimizing display.
   I.e., you will have to update the data base for rl_redisplay, and you
   might as well let rl_redisplay do that job. */

/* Move forward COUNT characters. */
rl_forward (count)
     int count;
{
  if (count < 0)
    rl_backward (-count);
  else
    while (count)
      {
#ifdef VI_MODE
	if (rl_point == (rl_end - (rl_editing_mode == vi_mode)))
#else
	if (rl_point == rl_end)
#endif
	  {
	    ding ();
	    return;
	  }
	else
	  rl_point++;
	--count;
      }
}

/* Move backward COUNT characters. */
rl_backward (count)
     int count;
{
  if (count < 0)
    rl_forward (-count);
  else
    while (count)
      {
	if (!rl_point)
	  {
	    ding ();
	    return;
	  }
	else
	  --rl_point;
	--count;
      }
}

/* Move to the beginning of the line. */
rl_beg_of_line ()
{
  rl_point = 0;
}

/* Move to the end of the line. */
rl_end_of_line ()
{
  rl_point = rl_end;
}

/* Move forward a word.  We do what Emacs does. */
rl_forward_word (count)
     int count;
{
  int c;

  if (count < 0)
    {
      rl_backward_word (-count);
      return;
    }

  while (count)
    {
      if (rl_point == rl_end)
	return;

      /* If we are not in a word, move forward until we are in one.
	 Then, move forward until we hit a non-alphabetic character. */
      c = the_line[rl_point];
      if (!alphabetic (c))
	{
	  while (++rl_point < rl_end)
	    {
	      c = the_line[rl_point];
	      if (alphabetic (c)) break;
	    }
	}
      if (rl_point == rl_end) return;
      while (++rl_point < rl_end)
	{
	  c = the_line[rl_point];
	  if (!alphabetic (c)) break;
	}
      --count;
    }
}

/* Move backward a word.  We do what Emacs does. */
rl_backward_word (count)
     int count;
{
  int c;

  if (count < 0)
    {
      rl_forward_word (-count);
      return;
    }

  while (count)
    {
      if (!rl_point)
	return;

      /* Like rl_forward_word (), except that we look at the characters
	 just before point. */

      c = the_line[rl_point - 1];
      if (!alphabetic (c))
	{
	  while (--rl_point)
	    {
	      c = the_line[rl_point - 1];
	      if (alphabetic (c)) break;
	    }
	}

      while (rl_point)
	{
	  c = the_line[rl_point - 1];
	  if (!alphabetic (c))
	    break;
	  else --rl_point;
	}
      --count;
    }
}

/* Clear the current line.  Numeric argument to C-l does this. */
rl_refresh_line ()
{
  int curr_line = last_c_pos / screenwidth;

  move_vert(curr_line);
  move_cursor_relative (0, the_line);   /* XXX is this right */
  rl_forced_update_display ();
  rl_display_fixed = 1;
}

/* C-l typed to a line without quoting clears the screen, and then reprints
   the prompt and the current input line.  Given a numeric arg, redraw only
   the current line. */
rl_clear_screen ()
{
  extern char *term_clrpag;
  static void output_character_function ();

  if (rl_explicit_arg)
    {
      rl_refresh_line ();
      return;
    }

  if (term_clrpag)
    tputs (term_clrpag, 1, output_character_function);
  else
    crlf ();

  rl_forced_update_display ();
  rl_display_fixed = 1;
}


/* **************************************************************** */
/*								    */
/*			Text commands				    */
/*								    */
/* **************************************************************** */

/* Insert the character C at the current location, moving point forward. */
rl_insert (count, c)
     int count, c;
{
  register int i;
  char *string;

  if (count <= 0)
    return;

  /* If we can optimize, then do it.  But don't let people crash
     readline because of extra large arguments. */
  if (count > 1 && count < 1024)
    {
      string = (char *)alloca (1 + count);

      for (i = 0; i < count; i++)
	string[i] = c;

      string[i] = '\0';
      rl_insert_text (string);
      return;
    }

  if (count > 1024)
    {
      int descreaser;

      string = (char *)alloca (1024 + 1);

      for (i = 0; i < 1024; i++)
	string[i] = c;

      while (count)
	{
	  descreaser = (count > 1024 ? 1024 : count);
	  string[descreaser] = '\0';
	  rl_insert_text (string);
	  count -= descreaser;
	}
      return;
    }

  /* We are inserting a single character.
     If there is pending input, then make a string of all of the
     pending characters that are bound to rl_insert, and insert
     them all. */
  if (any_typein)
    {
      int slen, key = 0, t;

      i = 0;
      string = (char *)alloca (ibuffer_len + 1);
      string[i++] = c;

      while ((key = rl_get_char()) != -2 &&
	     (keymap[key].type == ISFUNC &&
	      keymap[key].function == rl_insert))
	string[i++] = key;

      if (key != -2)
	rl_unget_char (key);

      string[i] = '\0';
      rl_insert_text (string);
      return;
    }
  else
    {
      /* Inserting a single character. */
      string = (char *)alloca (2);

      string[1] = '\0';
      string[0] = c;
      rl_insert_text (string);
    }
}

/* Insert the next typed character verbatim. */
rl_quoted_insert (count)
     int count;
{
  int c = rl_read_key (in_stream);
  rl_insert (count, c);
}

/* Insert a tab character. */
rl_tab_insert (count)
     int count;
{
  rl_insert (count, '\t');
}

#ifdef VI_MODE
/* Non-zero means enter insertion mode. */
static vi_doing_insert = 0;
#endif

/* What to do when a NEWLINE is pressed.  We accept the whole line.
   KEY is the key that invoked this command.  I guess it could have
   meaning in the future. */
rl_newline (count, key)
     int count, key;
{

  rl_done = 1;

#ifdef VI_MODE
  {
    if (vi_doing_insert)
      {
	rl_end_undo_group ();
	vi_doing_insert = 0;
      }
  }
#endif /* VI_MODE */

  if (readline_echoing_p)
    {
      move_vert (vis_botlin);
      vis_botlin = 0;
      crlf ();
      fflush (out_stream);
      rl_display_fixed++;
    }
}

rl_clean_up_for_exit ()
{
  if (readline_echoing_p)
    {
      move_vert (vis_botlin);
      vis_botlin = 0;
      fflush (out_stream);
      rl_restart_output ();
    }
}

/* What to do for some uppercase characters, like meta characters,
   and some characters appearing in emacs_ctlx_keymap.  This function
   is just a stub, you bind keys to it and the code in rl_dispatch ()
   is special cased. */
rl_do_lowercase_version (ignore1, ignore2)
     int ignore1, ignore2;
{
}

/* Rubout the character behind point. */
rl_rubout (count)
     int count;
{
  if (count < 0)
    {
      rl_delete (-count);
      return;
    }

  if (!rl_point)
    {
      ding ();
      return;
    }

  if (count > 1)
    {
      int orig_point = rl_point;
      rl_backward (count);
      rl_kill_text (orig_point, rl_point);
    }
  else
    {
      int c = the_line[--rl_point];
      rl_delete_text (rl_point, rl_point + 1);

      if (rl_point == rl_end && alphabetic (c) && last_c_pos)
	{
	  backspace (1);
	  putc (' ', out_stream);
	  backspace (1);
	  last_c_pos--;
	  rl_display_fixed++;
	}
    }
}

/* Delete the character under the cursor.  Given a numeric argument,
   kill that many characters instead. */
rl_delete (count, invoking_key)
     int count;
{
  if (count < 0)
    {
      rl_rubout (-count);
      return;
    }

  if (rl_point == rl_end)
    {
      ding ();
      return;
    }

#ifdef VI_MODE
  if ((count > 1) || ((count == 1) && (rl_editing_mode == vi_mode)))
#else
  if (count > 1)
#endif
    {
      int orig_point = rl_point;
      while (count && (rl_point < rl_end))
	{
	rl_point++;
	count--;
	}
      rl_kill_text (orig_point, rl_point);
      rl_point = orig_point;
    }
  else
    rl_delete_text (rl_point, rl_point + 1);
}


/* **************************************************************** */
/*								    */
/*			Kill commands				    */
/*								    */
/* **************************************************************** */

/* The next two functions mimic unix line editing behaviour, except they
   save the deleted text on the kill ring.  This is safer than not saving
   it, and since we have a ring, nobody should get screwed. */

/* This does what C-w does in Unix.  We can't prevent people from
   using behaviour that they expect. */
rl_unix_word_rubout ()
{
  if (!rl_point) ding ();
  else {
    int orig_point = rl_point;
    while (rl_point && whitespace (the_line[rl_point - 1]))
      rl_point--;
    while (rl_point && !whitespace (the_line[rl_point - 1]))
      rl_point--;
    rl_kill_text (rl_point, orig_point);
  }
}

/* Here is C-u doing what Unix does.  You don't *have* to use these
   key-bindings.  We have a choice of killing the entire line, or
   killing from where we are to the start of the line.  We choose the
   latter, because if you are a Unix weenie, then you haven't backspaced
   into the line at all, and if you aren't, then you know what you are
   doing. */
rl_unix_line_discard ()
{
  if (!rl_point) ding ();
  else {
    rl_kill_text (rl_point, 0);
    rl_point = 0;
  }
}



/* **************************************************************** */
/*								    */
/*			Commands For Typos			    */
/*								    */
/* **************************************************************** */

/* Random and interesting things in here.  */


/* **************************************************************** */
/*								    */
/*			Changing Case				    */
/*								    */
/* **************************************************************** */

/* The three kinds of things that we know how to do. */
#define UpCase 1
#define DownCase 2
#define CapCase 3

/* Uppercase the word at point. */
rl_upcase_word (count)
     int count;
{
  rl_change_case (count, UpCase);
}

/* Lowercase the word at point. */
rl_downcase_word (count)
     int count;
{
  rl_change_case (count, DownCase);
}

/* Upcase the first letter, downcase the rest. */
rl_capitalize_word (count)
     int count;
{
  rl_change_case (count, CapCase);
}

/* The meaty function.
   Change the case of COUNT words, performing OP on them.
   OP is one of UpCase, DownCase, or CapCase.
   If a negative argument is given, leave point where it started,
   otherwise, leave it where it moves to. */
rl_change_case (count, op)
     int count, op;
{
  register int start = rl_point, end;
  int state = 0;

  rl_forward_word (count);
  end = rl_point;

  if (count < 0)
    {
      int temp = start;
      start = end;
      end = temp;
    }

  /* We are going to modify some text, so let's prepare to undo it. */
  rl_modifying (start, end);

  for (; start < end; start++)
    {
      switch (op)
	{
	case UpCase:
	  the_line[start] = to_upper (the_line[start]);
	  break;

	case DownCase:
	  the_line[start] = to_lower (the_line[start]);
	  break;

	case CapCase:
	  if (state == 0)
	    {
	      the_line[start] = to_upper (the_line[start]);
	      state = 1;
	    }
	  else
	    {
	      the_line[start] = to_lower (the_line[start]);
	    }
	  if (!pure_alphabetic (the_line[start]))
	    state = 0;
	  break;

	default:
	  abort ();
	}
    }
  rl_point = end;
}

/* **************************************************************** */
/*								    */
/*			Transposition				    */
/*								    */
/* **************************************************************** */

/* Transpose the words at point. */
rl_transpose_words (count)
     int count;
{
  char *word1, *word2;
  int w1_beg, w1_end, w2_beg, w2_end;
  int orig_point = rl_point;

  if (!count) return;

  /* Find the two words. */
  rl_forward_word (count);
  w2_end = rl_point;
  rl_backward_word (1);
  w2_beg = rl_point;
  rl_backward_word (count);
  w1_beg = rl_point;
  rl_forward_word (1);
  w1_end = rl_point;

  /* Do some check to make sure that there really are two words. */
  if ((w1_beg == w2_beg) || (w2_beg < w1_end))
    {
      ding ();
      rl_point = orig_point;
      return;
    }

  /* Get the text of the words. */
  word1 = rl_copy (w1_beg, w1_end);
  word2 = rl_copy (w2_beg, w2_end);

  /* We are about to do many insertions and deletions.  Remember them
     as one operation. */
  rl_begin_undo_group ();

  /* Do the stuff at word2 first, so that we don't have to worry
     about word1 moving. */
  rl_point = w2_beg;
  rl_delete_text (w2_beg, w2_end);
  rl_insert_text (word1);

  rl_point = w1_beg;
  rl_delete_text (w1_beg, w1_end);
  rl_insert_text (word2);

  /* This is exactly correct since the text before this point has not
     changed in length. */
  rl_point = w2_end;

  /* I think that does it. */
  rl_end_undo_group ();
  free (word1); free (word2);
}

/* Transpose the characters at point.  If point is at the end of the line,
   then transpose the characters before point. */
rl_transpose_chars (count)
     int count;
{
  if (!count)
    return;

  if (!rl_point || rl_end < 2) {
    ding ();
    return;
  }

  while (count) {
    if (rl_point == rl_end) {
      int t = the_line[rl_point - 1];
      the_line[rl_point - 1] = the_line[rl_point - 2];
      the_line[rl_point - 2] = t;
    } else {
      int t = the_line[rl_point];
      the_line[rl_point] = the_line[rl_point - 1];
      the_line[rl_point - 1] = t;
      if (count < 0 && rl_point)
	rl_point--;
      else
	rl_point++;
    }
    if (count < 0)
      count++;
    else
      count--;
  }
}


/* **************************************************************** */
/*								    */
/*			Bogus Flow Control      		    */
/*								    */
/* **************************************************************** */

rl_restart_output (count, key)
     int count, key;
{
  int fildes = fileno (stdin);
#ifdef TIOCSTART
  ioctl (fildes, TIOCSTART, 0);
#endif /* TIOCSTART */
}

/* **************************************************************** */
/*								    */
/*	Completion matching, from readline's point of view.	    */
/*								    */
/* **************************************************************** */

/* Pointer to the generator function for completion_matches ().
   NULL means to use filename_entry_function (), the default filename
   completer. */
Function *rl_completion_entry_function = (Function *)NULL;

/* Pointer to alternative function to create matches.
   Function is called with TEXT, START, and END.
   START and END are indices in RL_LINE_BUFFER saying what the boundaries
   of TEXT are.
   If this function exists and returns NULL then call the value of
   rl_completion_entry_function to try to match, otherwise use the
   array of strings returned. */
Function *rl_attempted_completion_function = (Function *)NULL;

/* Complete the word at or before point.  You have supplied the function
   that does the initial simple matching selection algorithm (see
   completion_matches ()).  The default is to do filename completion. */
rl_complete (ignore, invoking_key)
     int ignore, invoking_key;
{
  rl_complete_internal (TAB);
  if (running_in_emacs)
    printf ("%s", the_line);
}

/* List the possible completions.  See description of rl_complete (). */
rl_possible_completions ()
{
  rl_complete_internal ('?');
}

/* The user must press "y" or "n". Non-zero return means "y" pressed. */
get_y_or_n ()
{
  int c;
 loop:
  c = rl_read_key (in_stream);
  if (c == 'y' || c == 'Y') return (1);
  if (c == 'n' || c == 'N') return (0);
  if (c == ABORT_CHAR) rl_abort ();
  ding (); goto loop;
}

/* Up to this many items will be displayed in response to a
   possible-completions call.  After that, we ask the user if
   she is sure she wants to see them all. */
int rl_completion_query_items = 100;

/* The basic list of characters that signal a break between words for the
   completer routine.  The contents of this variable is what breaks words
   in the shell, i.e. " \t\n\"\\'`@$><=" */
char *rl_basic_word_break_characters = " \t\n\"\\'`@$><=";

/* The list of characters that signal a break between words for
   rl_complete_internal.  The default list is the contents of
   rl_basic_word_break_characters.  */
char *rl_completer_word_break_characters = (char *)NULL;

/* List of characters that are word break characters, but should be left
   in TEXT when it is passed to the completion function.  The shell uses
   this to help determine what kind of completing to do. */
char *rl_special_prefixes = (char *)NULL;

/* If non-zero, then disallow duplicates in the matches. */
int rl_ignore_completion_duplicates = 1;

/* Non-zero means that the results of the matches are to be treated
   as filenames.  This is ALWAYS zero on entry, and can only be changed
   within a completion entry finder function. */
int rl_filename_completion_desired = 0;

/* Complete the word at or before point.
   WHAT_TO_DO says what to do with the completion.
   `?' means list the possible completions.
   TAB means do standard completion.
   `*' means insert all of the possible completions. */
rl_complete_internal (what_to_do)
     int what_to_do;
{
  char *filename_completion_function ();
  char **completion_matches (), **matches;
  Function *our_func;
  int start, end, delimiter = 0;
  char *text;

  if (rl_completion_entry_function)
    our_func = rl_completion_entry_function;
  else
    our_func = (int (*)())filename_completion_function;

  /* Only the completion entry function can change this. */
  rl_filename_completion_desired = 0;

  /* We now look backwards for the start of a filename/variable word. */
  end = rl_point;
  if (rl_point)
    {
      while (--rl_point &&
	     !rindex (rl_completer_word_break_characters, the_line[rl_point]));

      /* If we are at a word break, then advance past it. */
      if (rindex (rl_completer_word_break_characters,  (the_line[rl_point])))
	{
	  /* If the character that caused the word break was a quoting
	     character, then remember it as the delimiter. */
	  if (rindex ("\"'", the_line[rl_point]) && (end - rl_point) > 1)
	    delimiter = the_line[rl_point];

	  /* If the character isn't needed to determine something special
	     about what kind of completion to perform, then advance past it. */

	  if (!rl_special_prefixes ||
	      !rindex (rl_special_prefixes, the_line[rl_point]))
	    rl_point++;
	}
    }

  start = rl_point;
  rl_point = end;
  text = rl_copy (start, end);

  /* If the user wants to TRY to complete, but then wants to give
     up and use the default completion function, they set the
     variable rl_attempted_completion_function. */
  if (rl_attempted_completion_function)
    {
      matches =
	(char **)(*rl_attempted_completion_function) (text, start, end);

      if (matches)
	goto after_usual_completion;
    }

  matches = completion_matches (text, our_func, start, end);

 after_usual_completion:
  free (text);

  if (!matches)
    ding ();
  else
    {
      register int i;

    some_matches:

      /* It seems to me that in all the cases we handle we would like
	 to ignore duplicate possiblilities.  Scan for the text to
	 insert being identical to the other completions. */
      if (rl_ignore_completion_duplicates)
	{
	  char *lowest_common;
	  int j, newlen = 0;

	  /* Sort the items. */
	  /* It is safe to sort this array, because the lowest common
	     denominator found in matches[0] will remain in place. */
	  for (i = 0; matches[i]; i++);
	  qsort (matches, i, sizeof (char *), compare_strings);

	  /* Remember the lowest common denimator for it may be unique. */
	  lowest_common = savestring (matches[0]);

	  for (i = 0; matches[i + 1]; i++)
	    {
	      if (strcmp (matches[i], matches[i + 1]) == 0)
		{
		  free (matches[i]);
		  matches[i] = (char *)-1;
		}
	      else
		newlen++;
	    }

	  /* We have marked all the dead slots with (char *)-1.
	     Copy all the non-dead entries into a new array. */
	  {
	    char **temp_array =
	      (char **)malloc ((3 + newlen) * sizeof (char *));

	    for (i = 1, j = 1; matches[i]; i++)
	      if (matches[i] != (char *)-1)
		temp_array[j++] = matches[i];
	    temp_array[j] = (char *)NULL;

	    if (matches[0] != (char *)-1)
	      free (matches[0]);
	    free (matches);

	    matches = temp_array;
	  }

	  /* Place the lowest common denominator back in [0]. */
	  matches[0] = lowest_common;

	  /* If there is one string left, and it is identical to the
	     lowest common denominator, then the LCD is the string to
	     insert. */
	  if (j == 2 && strcmp (matches[0], matches[1]) == 0)
	    {
	      free (matches[1]);
	      matches[1] = (char *)NULL;
	    }
	}

      switch (what_to_do)
	{
	case TAB:
	  rl_delete_text (start, rl_point);
	  rl_point = start;
	  rl_insert_text (matches[0]);

	  /* If there are more matches, ring the bell to indicate.
	     If this was the only match, and we are hacking files,
	     check the file to see if it was a directory.  If so,
	     add a '/' to the name.  If not, and we are at the end
	     of the line, then add a space. */
	  if (matches[1])
	    {
	      ding ();		/* There are other matches remaining. */
	    }
	  else
	    {
	      char temp_string[2];

	      temp_string[0] = delimiter ? delimiter : ' ';
	      temp_string[1] = '\0';

	      if (rl_filename_completion_desired)
		{
		  struct stat finfo;
		  char *tilde_expand ();
		  char *filename = tilde_expand (matches[0]);

		  if ((stat (filename, &finfo) == 0) &&
		      ((finfo.st_mode & S_IFMT) == S_IFDIR))
		    {
		      if (the_line[rl_point] != '/')
			rl_insert_text ("/");
		    }
		  else
		    {
		      if (rl_point == rl_end)
			rl_insert_text (temp_string);
		    }
		  free (filename);
		}
	      else
		{
		  if (rl_point == rl_end)
		    rl_insert_text (temp_string);
		}
	    }
	  break;

	case '*':
	  {
	    int i = 1;

	    rl_delete_text (start, rl_point);
	    rl_point = start;
	    rl_begin_undo_group ();
	    if (matches[1])
	      {
		while (matches[i])
		  {
		    rl_insert_text (matches[i++]);
		    rl_insert_text (" ");
		  }
	      }
	    else
	      {
		rl_insert_text (matches[0]);
		rl_insert_text (" ");
	      }
	    rl_end_undo_group ();
	  }
	  break;


	case '?':
	  {
	    int len, count, limit, max = 0;
	    int j, k, l;

	    /* Handle simple case first.  What if there is only one answer? */
	    if (!matches[1])
	      {
		char *rindex (), *temp;

		if (rl_filename_completion_desired)
		  temp = rindex (matches[0], '/');
		else
		  temp = (char *)NULL;

		if (!temp)
		  temp = matches[0];
		else
		  temp++;

		crlf ();
		fprintf (out_stream, "%s", temp);
		crlf ();
		goto restart;
	      }

	    /* There is more than one answer.  Find out how many there are,
	       and find out what the maximum printed length of a single entry
	       is. */
	    for (i = 1; matches[i]; i++)
	      {
		char *rindex (), *temp = (char *)NULL;

		/* If we are hacking filenames, then only count the characters
		   after the last slash in the pathname. */
		if (rl_filename_completion_desired)
		  temp = rindex (matches[i], '/');
		else
		  temp = (char *)NULL;

		if (!temp)
		  temp = matches[i];
		else
		  temp++;

		if (strlen (temp) > max)
		  max = strlen (temp);
	      }

	    len = i;

	    /* If there are many items, then ask the user if she
	       really wants to see them all. */
	    if (len >= rl_completion_query_items)
	      {
		crlf ();
		fprintf (out_stream,
			 "There are %d possibilities.  Do you really", len);
		crlf ();
		fprintf (out_stream, "wish to see them all? (y or n)");
		fflush (out_stream);
		if (!get_y_or_n ())
		  {
		    crlf ();
		    goto restart;
		  }
	      }
	    /* How many items of MAX length can we fit in the screen window? */
	    max += 2;
	    limit = screenwidth / max;
	    if (limit != 1 && (limit * max == screenwidth))
	      limit--;

	    /* How many iterations of the printing loop? */
	    count = (len + (limit - 1)) / limit;

	    /* Watch out for special case.  If LEN is less than LIMIT, then
	       just do the inner printing loop. */
	    if (len < limit) count = 1;

	    /* Sort the items if they are not already sorted. */
	    if (!rl_ignore_completion_duplicates)
	      {
		qsort (matches, len, sizeof (char *), compare_strings);
	      }

	    /* Print the sorted items, up-and-down alphabetically, like
	       ls might. */
	    crlf ();

	    for (i = 1; i < count + 1; i++)
	      {
		for (j = 0, l = i; j < limit; j++)
		  {
		    if (l > len || !matches[l])
		      {
			break;
		      }
		    else
		      {
			char *rindex (), *temp = (char *)NULL;

			if (rl_filename_completion_desired)
			  temp = rindex (matches[l], '/');
			else
			  temp = (char *)NULL;

			if (!temp)
			  temp = matches[l];
			else
			  temp++;

			fprintf (out_stream, "%s", temp);
			for (k = 0; k < max - strlen (temp); k++)
			  putc (' ', out_stream);
		      }
		    l += count;
		  }
		crlf ();
	      }
	  restart:

	    rl_on_new_line ();
	  }
	  break;

	default:
	  abort ();
	}

      for (i = 0; matches[i]; i++)
	free (matches[i]);
      free (matches);
    }
}

/* A completion function for usernames.
   TEXT contains a partial username preceded by a random
   character (usually `~').  */
char *
username_completion_function (text, state)
     int state;
     char *text;
{
  static char *username = (char *)NULL;
  static struct passwd *entry;
  static int namelen;

  if (!state)
    {
      if (username)
	free (username);
      username = savestring (&text[1]);
      namelen = strlen (username);
      setpwent ();
    }

  while (entry = getpwent ())
    {
      if (strncmp (username, entry->pw_name, namelen) == 0)
	break;
    }

  if (!entry)
    {
      endpwent ();
      return ((char *)NULL);
    }
  else
    {
      char *value = (char *)xmalloc (2 + strlen (entry->pw_name));
      *value = *text;
      strcpy (value + 1, entry->pw_name);
      rl_filename_completion_desired = 1;
      return (value);
    }
}

/* If non-null, this contains the address of a function to call if the
   standard meaning for expanding a tilde fails.  The function is called
   with the text (sans tilde, as in "foo"), and returns a malloc()'ed string
   which is the expansion, or a NULL pointer if there is no expansion. */
Function *rl_tilde_expander = (Function *)NULL;

/* Expand FILENAME if it begins with a tilde.  This always returns
   a new string. */
char *
tilde_expand (filename)
     char *filename;
{
  char *dirname = filename ? savestring (filename) : (char *)NULL;

  if (dirname && *dirname == '~')
    {
      char *temp_name;
      if (!dirname[1] || dirname[1] == '/')
	{
	  /* Prepend $HOME to the rest of the string. */
	  char *temp_home = (char *)getenv ("HOME");

	  temp_name = (char *)alloca (1 + strlen (&dirname[1])
				      + (temp_home? strlen (temp_home) : 0));
	  temp_name[0] = '\0';
	  if (temp_home)
	    strcpy (temp_name, temp_home);
	  strcat (temp_name, &dirname[1]);
	  free (dirname);
	  dirname = savestring (temp_name);
	}
      else
	{
	  struct passwd *getpwnam (), *user_entry;
	  char *username = (char *)alloca (257);
	  int i, c;

	  for (i = 1; c = dirname[i]; i++)
	    {
	      if (c == '/') break;
	      else username[i - 1] = c;
	    }
	  username[i - 1] = '\0';

	  if (!(user_entry = getpwnam (username)))
	    {
	      /* If the calling program has a special syntax for
		 expanding tildes, and we couldn't find a standard
		 expansion, then let them try. */
	      if (rl_tilde_expander)
		{
		  char *expansion;

		  expansion = (char *)(*rl_tilde_expander) (username);

		  if (expansion)
		    {
		      temp_name = (char *)alloca (1 + strlen (expansion)
						  + strlen (&dirname[i]));
		      strcpy (temp_name, expansion);
		      strcat (temp_name, &dirname[i]);
		      free (expansion);
		      goto return_name;
		    }
		}
	      /*
	       * We shouldn't report errors.
	       */
	    }
	  else
	    {
	      temp_name = (char *)alloca (1 + strlen (user_entry->pw_dir)
					  + strlen (&dirname[i]));
	      strcpy (temp_name, user_entry->pw_dir);
	      strcat (temp_name, &dirname[i]);
	    return_name:
	      free (dirname);
	      dirname = savestring (temp_name);
	    }
	}
    }
  return (dirname);
}


/* **************************************************************** */
/*								    */
/*			Undo, and Undoing			    */
/*								    */
/* **************************************************************** */

/* Non-zero tells rl_delete_text and rl_insert_text to not add to
   the undo list. */
int doing_an_undo = 0;

/* The current undo list for THE_LINE. */
UNDO_LIST *rl_undo_list = (UNDO_LIST *)NULL;

/* Remember how to undo something.  Concatenate some undos if that
   seems right. */
rl_add_undo (what, start, end, text)
     enum undo_code what;
     int start, end;
     char *text;
{
  UNDO_LIST *temp = (UNDO_LIST *)xmalloc (sizeof (UNDO_LIST));
  temp->what = what;
  temp->start = start;
  temp->end = end;
  temp->text = text;
  temp->next = rl_undo_list;
  rl_undo_list = temp;
}

/* Free the existing undo list. */
free_undo_list ()
{
  while (rl_undo_list) {
    UNDO_LIST *release = rl_undo_list;
    rl_undo_list = rl_undo_list->next;

    if (release->what == UNDO_DELETE)
      free (release->text);

    free (release);
  }
}

/* Undo the next thing in the list.  Return 0 if there
   is nothing to undo, or non-zero if there was. */
int
rl_do_undo ()
{
  UNDO_LIST *release;
  int waiting_for_begin = 0;

undo_thing:
  if (!rl_undo_list)
    return (0);

  doing_an_undo = 1;

  switch (rl_undo_list->what) {

    /* Undoing deletes means inserting some text. */
  case UNDO_DELETE:
    rl_point = rl_undo_list->start;
    rl_insert_text (rl_undo_list->text);
    free (rl_undo_list->text);
    break;

    /* Undoing inserts means deleting some text. */
  case UNDO_INSERT:
    rl_delete_text (rl_undo_list->start, rl_undo_list->end);
    rl_point = rl_undo_list->start;
    break;

    /* Undoing an END means undoing everything 'til we get to
       a BEGIN. */
  case UNDO_END:
    waiting_for_begin++;
    break;

    /* Undoing a BEGIN means that we are done with this group. */
  case UNDO_BEGIN:
    if (waiting_for_begin)
      waiting_for_begin--;
    else
      abort ();
    break;
  }

  doing_an_undo = 0;

  release = rl_undo_list;
  rl_undo_list = rl_undo_list->next;
  free (release);

  if (waiting_for_begin)
    goto undo_thing;

  return (1);
}

/* Begin a group.  Subsequent undos are undone as an atomic operation. */
rl_begin_undo_group ()
{
  rl_add_undo (UNDO_BEGIN, 0, 0, 0);
}

/* End an undo group started with rl_begin_undo_group (). */
rl_end_undo_group ()
{
  rl_add_undo (UNDO_END, 0, 0, 0);
}

/* Save an undo entry for the text from START to END. */
rl_modifying (start, end)
     int start, end;
{
  if (start > end)
    {
      int t = start;
      start = end;
      end = t;
    }

  if (start != end)
    {
      char *temp = rl_copy (start, end);
      rl_begin_undo_group ();
      rl_add_undo (UNDO_DELETE, start, end, temp);
      rl_add_undo (UNDO_INSERT, start, end, (char *)NULL);
      rl_end_undo_group ();
    }
}

/* Revert the current line to its previous state. */
rl_revert_line ()
{
  if (!rl_undo_list) ding ();
  else {
    while (rl_undo_list)
      rl_do_undo ();
  }
}

/* Do some undoing of things that were done. */
rl_undo_command (count)
{
  if (count < 0) return;	/* Nothing to do. */

  while (count)
    {
      if (rl_do_undo ())
	{
	  count--;
	}
      else
	{
	  ding ();
	  break;
	}
    }
}

/* **************************************************************** */
/*								    */
/*			History Utilities			    */
/*								    */
/* **************************************************************** */

/* We already have a history library, and that is what we use to control
   the history features of readline.  However, this is our local interface
   to the history mechanism. */

/* While we are editing the history, this is the saved
   version of the original line. */
HIST_ENTRY *saved_line_for_history = (HIST_ENTRY *)NULL;

/* Set the history pointer back to the last entry in the history. */
start_using_history ()
{
  using_history ();
  if (saved_line_for_history)
    free_history_entry (saved_line_for_history);

  saved_line_for_history = (HIST_ENTRY *)NULL;
}

/* Free the contents (and containing structure) of a HIST_ENTRY. */
free_history_entry (entry)
     HIST_ENTRY *entry;
{
  if (!entry) return;
  if (entry->line)
    free (entry->line);
  free (entry);
}

/* Perhaps put back the current line if it has changed. */
maybe_replace_line ()
{
  HIST_ENTRY *temp = current_history ();

  /* If the current line has changed, save the changes. */
  if (temp && ((UNDO_LIST *)(temp->data) != rl_undo_list)) {
    temp = replace_history_entry (where_history (), the_line, rl_undo_list);
    free (temp->line);
    free (temp);
  }
}

/* Put back the saved_line_for_history if there is one. */
maybe_unsave_line ()
{
  if (saved_line_for_history) {
    strcpy (the_line, saved_line_for_history->line);
    rl_undo_list = (UNDO_LIST *)saved_line_for_history->data;
    free_history_entry (saved_line_for_history);
    saved_line_for_history = (HIST_ENTRY *)NULL;
    rl_end = rl_point = strlen (the_line);
  } else {
    ding ();
  }
}

/* Save the current line in saved_line_for_history. */
maybe_save_line ()
{
  if (!saved_line_for_history) {
    saved_line_for_history = (HIST_ENTRY *)xmalloc (sizeof (HIST_ENTRY));
    saved_line_for_history->line = savestring (the_line);
    saved_line_for_history->data = (char *)rl_undo_list;
  }
}



/* **************************************************************** */
/*								    */
/*			History Commands			    */
/*								    */
/* **************************************************************** */

/* Meta-< goes to the start of the history. */
rl_beginning_of_history ()
{
  rl_get_previous_history (1 + where_history ());
}

/* Meta-> goes to the end of the history.  (The current line). */
rl_end_of_history ()
{
  maybe_replace_line ();
  using_history ();
  maybe_unsave_line ();
}

/* Move down to the next history line. */
rl_get_next_history (count)
     int count;
{
  HIST_ENTRY *temp = (HIST_ENTRY *)NULL;

  if (count < 0)
    {
      rl_get_previous_history (-count);
      return;
    }

  if (!count)
    return;

  maybe_replace_line ();

  while (count)
    {
      temp = next_history ();
      if (!temp)
	break;
      --count;
    }

  if (!temp)
    maybe_unsave_line ();
  else
    {
      strcpy (the_line, temp->line);
      rl_undo_list = (UNDO_LIST *)temp->data;
      rl_end = rl_point = strlen (the_line);
    }
}

/* Get the previous item out of our interactive history, making it the current
   line.  If there is no previous history, just ding. */
rl_get_previous_history (count)
     int count;
{
  HIST_ENTRY *old_temp = (HIST_ENTRY *)NULL;
  HIST_ENTRY *temp = (HIST_ENTRY *)NULL;

  if (count < 0)
    {
      rl_get_next_history (-count);
      return;
    }

  if (!count)
    return;

  /* If we don't have a line saved, then save this one. */
  maybe_save_line ();

  /* If the current line has changed, save the changes. */
  maybe_replace_line ();

  while (count)
    {
      temp = previous_history ();
      if (!temp)
	break;
      else
	old_temp = temp;
      --count;
    }

  /* If there was a large argument, and we moved back to the start of the
     history, that is not an error.  So use the last value found. */
  if (!temp && old_temp)
    temp = old_temp;

  if (!temp)
    ding ();
  else
    {
      strcpy (the_line, temp->line);
      rl_undo_list = (UNDO_LIST *)temp->data;
      rl_end = rl_point = strlen (the_line);
#ifdef VI_MODE
      if (rl_editing_mode == vi_mode)
	rl_point = 0;
#endif /* VI_MODE */
    }
}

/* There is a command in ksh which yanks into this line, the last word
   of the previous line.  Here it is.  We left it on M-. */
rl_yank_previous_last_arg (ignore)
     int ignore;
{
}



/* **************************************************************** */
/*								    */
/*			I-Search and Searching			    */
/*								    */
/* **************************************************************** */

/* Search backwards through the history looking for a string which is typed
   interactively.  Start with the current line. */
rl_reverse_search_history (sign, key)
     int sign;
     int key;
{
  rl_search_history (-sign, key);
}

/* Search forwards through the history looking for a string which is typed
   interactively.  Start with the current line. */
rl_forward_search_history (sign, key)
     int sign;
     int key;
{
  rl_search_history (sign, key);
}

/* Display the current state of the search in the echo-area.
   SEARCH_STRING contains the string that is being searched for,
   DIRECTION is zero for forward, or 1 for reverse,
   WHERE is the history list number of the current line.  If it is
   -1, then this line is the starting one. */
rl_display_search (search_string, reverse_p, where)
     char *search_string;
     int reverse_p, where;
{
  char *message = (char *)NULL;

  message =
    (char *)alloca (1 + (search_string ? strlen (search_string) : 0) + 30);

  *message = '\0';

#ifdef NEVER
  if (where != -1)
    sprintf (message, "[%d]", where + history_base);
#endif

  strcat (message, "(");

  if (reverse_p)
    strcat (message, "reverse-");

  strcat (message, "i-search)`");

  if (search_string)
    strcat (message, search_string);

  strcat (message, "': ");
  rl_message (message, 0, 0);
  rl_redisplay ();
}

/* Search through the history looking for an interactively typed string.
   This is analogous to i-search.  We start the search in the current line.
   DIRECTION is which direction to search; > 0 means forward, < 0 means
   backwards. */
rl_search_history (direction, invoking_key)
     int direction;
     int invoking_key;
{
  /* The string that the user types in to search for. */
  char *search_string = (char *)alloca (128);

  /* The current length of SEARCH_STRING. */
  int search_string_index;

  /* The list of lines to search through. */
  char **lines;

  /* The length of LINES. */
  int hlen;

  /* Where we get LINES from. */
  HIST_ENTRY **hlist = history_list ();

  int orig_point = rl_point;
  int orig_line = where_history ();
  int last_found_line = orig_line;
  int c, done = 0;
  register int i = 0;


  /* The line currently being searched. */
  char *sline;

  /* Offset in that line. */
  int index;

  /* Non-zero if we are doing a reverse search. */
  int reverse = (direction < 0);

  /* Create an arrary of pointers to the lines that we want to search. */

  maybe_replace_line ();
  if (hlist)
    for (i = 0; hlist[i]; i++);

  /* Allocate space for this many lines, +1 for the current input line,
     and remember those lines. */
  lines = (char **)alloca ((1 + (hlen = i)) * sizeof (char *));
  for (i = 0; i < hlen; i++)
    lines[i] = hlist[i]->line;

  if (saved_line_for_history)
    lines[i] = saved_line_for_history->line;
  else
    {
      /* So I have to type it in this way instead. */
      lines[i] = (char *)alloca (1 + strlen (the_line));
      strcpy (lines[i], &the_line[0]);
    }

  hlen++;

  /* The line where we start the search. */
  i = orig_line;

  /* Initialize search parameters. */
  *search_string = '\0';
  search_string_index = 0;

  rl_display_search (search_string, reverse, -1);

  sline = the_line;
  index = rl_point;

  while (!done)
    {
      c = rl_read_key (in_stream);

      /* Hack C to Do What I Mean. */
      {
	Function *f = (Function *)NULL;

	if (keymap[c].type == ISFUNC)
	  f = keymap[c].function;

	if (f == rl_reverse_search_history)
	  c = reverse ? -1 : -2;
	else if (f == rl_forward_search_history)
	  c =  !reverse ? -1 : -2;
      }

      switch (c)
	{
	case ESC:
	  done = 1;
	  continue;

	  /* case invoking_key: */
	case -1:
	  goto search_again;

	  /* switch directions */
	case -2:
	  direction = -direction;
	  reverse = (direction < 0);

	  goto do_search;

	case CTRL ('G'):
	  strcpy (the_line, lines[orig_line]);
	  rl_point = orig_point;
	  rl_end = strlen (the_line);
	  rl_clear_message ();
	  return;

	default:
	  if (c < 32 || c > 126)
	    {
	      rl_execute_next (c);
	      done = 1;
	      continue;
	    }
	  else
	    {
	      search_string[search_string_index++] = c;
	      search_string[search_string_index] = '\0';
	      goto do_search;

	    search_again:

	      if (!search_string_index)
		continue;
	      else
		{
		  if (reverse)
		    --index;
		  else
		    if (index != strlen (sline))
		      ++index;
		    else
		      ding ();
		}
	    do_search:

	      while (1)
		{
		  if (reverse)
		    {
		      while (index >= 0)
			if (strncmp
			    (search_string,
			     sline + index,
			     search_string_index) == 0)
			  goto string_found;
			else
			  index--;
		    }
		  else
		    {
		      register int limit =
			(strlen (sline) - search_string_index) + 1;

		      while (index < limit)
			{
			  if (strncmp (search_string,
				       sline + index,
				       search_string_index) == 0)
			    goto string_found;
			  index++;
			}
		    }

		next_line:
		  i += direction;

		  /* At limit for direction? */
		  if ((reverse && i < 0) ||
		      (!reverse && i == hlen))
		    goto search_failed;

		  sline = lines[i];
		  if (reverse)
		    index = strlen (sline);
		  else
		    index = 0;

		  /* If the search string is longer than the current
		     line, no match. */
		  if (search_string_index > strlen (sline))
		    goto next_line;

		  /* Start actually searching. */
		  if (reverse)
		    index -= search_string_index;
		}

	    search_failed:
	      /* We cannot find the search string.  Ding the bell. */
	      ding ();
	      i = last_found_line;
	      break;

	    string_found:
	      /* We have found the search string.  Just display it.  But don't
		 actually move there in the history list until the user accepts
		 the location. */
	      strcpy (the_line, lines[i]);
	      rl_point = index;
	      rl_end = strlen (the_line);
	      last_found_line = i;
	      rl_display_search (search_string, reverse,
				 (i == orig_line) ? -1 : i);
	    }
	}
      continue;
    }
  /* The user has won.  They found the string that they wanted.  Now all
     we have to do is place them there. */
  {
    int now = last_found_line;

    /* First put back the original state. */
    strcpy (the_line, lines[orig_line]);

    if (now < orig_line)
      rl_get_previous_history (orig_line - now);
    else
      rl_get_next_history (now - orig_line);

    rl_point = index;
    rl_clear_message ();
  }
}

/* Make C be the next command to be executed. */
rl_execute_next (c)
     int c;
{
  rl_pending_input = c;
}

/* **************************************************************** */
/*								    */
/*			Killing Mechanism			    */
/*								    */
/* **************************************************************** */

/* What we assume for a max number of kills. */
#define DEFAULT_MAX_KILLS 10

/* The real variable to look at to find out when to flush kills. */
int rl_max_kills = DEFAULT_MAX_KILLS;

/* Where to store killed text. */
char **rl_kill_ring = (char **)NULL;

/* Where we are in the kill ring. */
int rl_kill_index = 0;

/* How many slots we have in the kill ring. */
int rl_kill_ring_length = 0;

/* How to say that you only want to save a certain amount
   of kill material. */
rl_set_retained_kills (num)
     int num;
{}

/* The way to kill something.  This appends or prepends to the last
   kill, if the last command was a kill command.  if FROM is less
   than TO, then the text is appended, otherwise prepended.  If the
   last command was not a kill command, then a new slot is made for
   this kill. */
rl_kill_text (from, to)
     int from, to;
{
  int slot;
  char *text = rl_copy (from, to);

  /* Is there anything to kill? */
  if (from == to) {
    free (text);
    last_command_was_kill++;
    return;
  }

  /* Delete the copied text from the line. */
  rl_delete_text (from, to);

  /* First, find the slot to work with. */
  if (!last_command_was_kill) {

    /* Get a new slot.  */
    if (!rl_kill_ring) {

      /* If we don't have any defined, then make one. */
      rl_kill_ring =
	(char **)xmalloc (((rl_kill_ring_length = 1) + 1) * sizeof (char *));
      slot = 1;

    } else {

      /* We have to add a new slot on the end, unless we have exceeded
	 the max limit for remembering kills. */
      slot = rl_kill_ring_length;
      if (slot == rl_max_kills) {
	register int i;
	free (rl_kill_ring[0]);
	for (i = 0; i < slot; i++)
	  rl_kill_ring[i] = rl_kill_ring[i + 1];
      } else {
	rl_kill_ring =
	  (char **)xrealloc (rl_kill_ring,
			     ((slot = (rl_kill_ring_length += 1)) + 1)
			     * sizeof (char *));
      }
    }
    slot--;
  } else {
    slot = rl_kill_ring_length - 1;
  }

  /* If the last command was a kill, prepend or append. */
  if (last_command_was_kill) {
    char *old = rl_kill_ring[slot];
    char *new = (char *)xmalloc (1 + strlen (old) + strlen (text));

    if (from < to) {
      strcpy (new, old);
      strcat (new, text);
    } else {
      strcpy (new, text);
      strcat (new, old);
    }
    free (old);
    free (text);
    rl_kill_ring[slot] = new;
  } else {
    rl_kill_ring[slot] = text;
  }
  rl_kill_index = slot;
  last_command_was_kill++;
}

/* Now REMEMBER!  In order to do prepending or appending correctly, kill
   commands always make rl_point's original position be the FROM argument,
   and rl_point's extent be the TO argument. */


/* **************************************************************** */
/*								    */
/*			Killing Commands			    */
/*								    */
/* **************************************************************** */

/* Delete the word at point, saving the text in the kill ring. */
rl_kill_word (count)
     int count;
{
  int orig_point = rl_point;

  if (count < 0)
    rl_backward_kill_word (-count);
  else
    {
      rl_forward_word (count);

      if (rl_point != orig_point)
	rl_kill_text (orig_point, rl_point);

      rl_point = orig_point;
    }
}

/* Rubout the word before point, placing it on the kill ring. */
rl_backward_kill_word (count)
     int count;
{
  int orig_point = rl_point;

  if (count < 0)
    rl_kill_word (-count);
  else
    {
      rl_backward_word (count);

      if (rl_point != orig_point)
	rl_kill_text (orig_point, rl_point);
    }
}

/* Kill from here to the end of the line.  If DIRECTION is negative, kill
   back to the line start instead. */
rl_kill_line (direction)
     int direction;
{
  int orig_point = rl_point;

  if (direction < 0)
    rl_backward_kill_line (1);
  else
    {
      rl_end_of_line ();
      if (orig_point != rl_point)
	rl_kill_text (orig_point, rl_point);
      rl_point = orig_point;
    }
}

/* Kill backwards to the start of the line.  If DIRECTION is negative, kill
   forwards to the line end instead. */
rl_backward_kill_line (direction)
     int direction;
{
  int orig_point = rl_point;

  if (direction < 0)
    rl_kill_line (1);
  else
    {
      if (!rl_point)
	ding ();
      else
	{
	  rl_beg_of_line ();
	  rl_kill_text (orig_point, rl_point);
	}
    }
}

/* Yank back the last killed text.  This ignores arguments. */
rl_yank ()
{
  if (!rl_kill_ring) rl_abort ();
  rl_insert_text (rl_kill_ring[rl_kill_index]);
}

/* If the last command was yank, or yank_pop, and the text just
   before point is identical to the current kill item, then
   delete that text from the line, rotate the index down, and
   yank back some other text. */
rl_yank_pop ()
{
  int l;

  if (((rl_last_func != rl_yank_pop) && (rl_last_func != rl_yank)) ||
      !rl_kill_ring)
    {
      rl_abort ();
    }

  l = strlen (rl_kill_ring[rl_kill_index]);
  if (((rl_point - l) >= 0) &&
      (strncmp (the_line + (rl_point - l),
		rl_kill_ring[rl_kill_index], l) == 0))
    {
      rl_delete_text ((rl_point - l), rl_point);
      rl_point -= l;
      rl_kill_index--;
      if (rl_kill_index < 0)
	rl_kill_index = rl_kill_ring_length - 1;
      rl_yank ();
    }
  else
    rl_abort ();

}

/* Yank the COUNTth argument from the previous history line. */
rl_yank_nth_arg (count, ignore)
     int count;
{
  register HIST_ENTRY *entry = previous_history ();
  char *arg;

  if (entry)
    next_history ();
  else
    {
      ding ();
      return;
    }

  arg = history_arg_extract (count, count, entry->line);
  if (!arg || !*arg)
    {
      ding ();
      return;
    }

  rl_begin_undo_group ();
  if (rl_point && the_line[rl_point - 1] != ' ')
    rl_insert_text (" ");
  rl_insert_text (arg);
  free (arg);
  rl_end_undo_group ();
}

/* Vi Mode. */
#ifdef VI_MODE
#include "vi_mode.c"
#endif /* VI_MODE */

/* How to toggle back and forth between editing modes. */
rl_vi_editing_mode ()
{
#ifdef VI_MODE
  rl_editing_mode = vi_mode;
  rl_vi_insertion_mode ();
#endif /* VI_MODE */
}

rl_emacs_editing_mode ()
{
  rl_editing_mode = emacs_mode;
  keymap = emacs_standard_keymap;
}


/* **************************************************************** */
/*								    */
/*			     Completion				    */
/*								    */
/* **************************************************************** */

/* Non-zero means that case is not significant in completion. */
int completion_case_fold = 0;

/* Return an array of (char *) which is a list of completions for TEXT.
   If there are no completions, return a NULL pointer.
   The first entry in the returned array is the substitution for TEXT.
    The remaining entries are the possible completions.
   The array is terminated with a NULL pointer.

   ENTRY_FUNCTION is a function of two args, and returns a (char *).
     The first argument is TEXT.
     The second is a state argument; it should be zero on the first call, and
     non-zero on subsequent calls.  It returns a NULL pointer to the caller
     when there are no more matches.
 */
char **
completion_matches (text, entry_function)
     char *text;
     char *(*entry_function) ();
{
  /* Number of slots in match_list. */
  int match_list_size;

  /* The list of matches. */
  char **match_list =
    (char **)xmalloc (((match_list_size = 10) + 1) * sizeof (char *));

  /* Number of matches actually found. */
  int matches = 0;

  /* Temporary string binder. */
  char *string;

  match_list[1] = (char *)NULL;

  while (string = (*entry_function) (text, matches))
    {
      if (matches + 1 == match_list_size)
	match_list =
	  (char **)xrealloc (match_list,
			     ((match_list_size += 10) + 1) * sizeof (char *));

      match_list[++matches] = string;
      match_list[matches + 1] = (char *)NULL;
    }

  /* If there were any matches, then look through them finding out the
     lowest common denominator.  That then becomes match_list[0]. */
  if (matches)
    {
      register int i = 1;
      int low = 100000;		/* Count of max-matched characters. */

      /* If only one match, just use that. */
      if (matches == 1)
	{
	  match_list[0] = match_list[1];
	  match_list[1] = (char *)NULL;
	}
      else
	{
	  /* Otherwise, compare each member of the list with
	     the next, finding out where they stop matching. */

	  while (i < matches)
	    {
	      register int c1, c2, si;

	      if (completion_case_fold)
		{
		  for (si = 0;
		       (c1 = to_lower(match_list[i][si])) &&
		       (c2 = to_lower(match_list[i + 1][si]));
		       si++)
		    if (c1 != c2) break;
		}
	      else
		{
		  for (si = 0;
		       (c1 = match_list[i][si]) &&
		       (c2 = match_list[i + 1][si]);
		       si++)
		    if (c1 != c2) break;
		}

	      if (low > si) low = si;
	      i++;
	    }
	  match_list[0] = (char *)xmalloc (low + 1);
	  strncpy (match_list[0], match_list[1], low);
	  match_list[0][low] = '\0';
	}
    }
  else				/* There were no matches. */
    {
      free (match_list);
      match_list = (char **)NULL;
    }
  return (match_list);
}

/* Okay, now we write the entry_function for filename completion.  In the
   general case.  Note that completion in the shell is a little different
   because of all the pathnames that must be followed when looking up the
   completion for a command. */
char *
filename_completion_function (text, state)
     int state;
     char *text;
{
  static DIR *directory;
  static char *filename = (char *)NULL;
  static char *dirname = (char *)NULL;
  static char *users_dirname = (char *)NULL;
  static int filename_len;

  struct direct *entry = (struct direct *)NULL;

  /* If we don't have any state, then do some initialization. */
  if (!state)
    {
      char *rindex (), *temp;

      if (dirname) free (dirname);
      if (filename) free (filename);
      if (users_dirname) free (users_dirname);

      filename = savestring (text);
      if (!*text) text = ".";
      dirname = savestring (text);

      temp = rindex (dirname, '/');

      if (temp)
	{
	  strcpy (filename, ++temp);
	  *temp = '\0';
	}
      else
	strcpy (dirname, ".");

      /* We aren't done yet.  We also support the "~user" syntax. */

      /* Save the version of the directory that the user typed. */
      users_dirname = savestring (dirname);
      {
	char *tilde_expand (), *temp_dirname = tilde_expand (dirname);
	free (dirname);
	dirname = temp_dirname;
#ifdef SHELL
	{
	  extern int follow_symbolic_links;
	  char *make_absolute ();

	  if (follow_symbolic_links && (strcmp (dirname, ".") != 0))
	    {
	      temp_dirname = make_absolute (dirname, get_working_directory (""));

	      if (temp_dirname)
		{
		  free (dirname);
		  dirname = temp_dirname;
		}
	    }
	}
#endif				/* SHELL */
      }
      directory = opendir (dirname);
      filename_len = strlen (filename);

      rl_filename_completion_desired = 1;
    }

  /* At this point we should entertain the possibility of hacking wildcarded
     filenames, like /usr/man*\/te<TAB>.  If the directory name contains
     globbing characters, then build an array of directories to glob on, and
     glob on the first one. */

  /* Now that we have some state, we can read the directory. */

  while (directory && (entry = readdir (directory)))
    {
      /* Special case for no filename.
	 All entries except "." and ".." match. */
      if (!filename_len)
	{
	  if ((strcmp (entry->d_name, ".") != 0) &&
	      (strcmp (entry->d_name, "..") != 0))
	    break;
	}
      else
	{
	  /* Otherwise, if these match upto the length of filename, then
	     it is a match. */
#ifdef TMB_SYSV
	  if ((strlen (entry->d_name) >= filename_len) &&
	      (strncmp (filename, entry->d_name, filename_len) == 0))
#else
	    if ((entry->d_namlen >= filename_len) &&
		(strncmp (filename, entry->d_name, filename_len) == 0))
#endif /* TMB_SYSV */
	      {
		break;
	      }
	}
    }

  if (!entry)
    {
      if (directory)
	{
	  closedir (directory);
	  directory = (DIR *)NULL;
	}
      return (char *)NULL;
    }
  else
    {
      char *temp;

      if (dirname && (strcmp (dirname, ".") != 0))
	{
#ifdef TMB_SYSV
	  temp = (char *)xmalloc (1 + strlen (users_dirname)
				  + strlen (entry->d_name));
#else
	  temp = (char *)xmalloc (1 + strlen (users_dirname)
				  + entry->d_namlen);
#endif /* TMB_SYSV */
	  strcpy (temp, users_dirname);
	  strcat (temp, entry->d_name);
	}
      else
	{
	  temp = (savestring (entry->d_name));
	}
      return (temp);
    }
}


/* **************************************************************** */
/*								    */
/*			Binding keys				    */
/*								    */
/* **************************************************************** */

/* rl_add_defun (char *name, Function *function, int key)
   Add NAME to the list of named functions.  Make FUNCTION
   be the function that gets called.
   If KEY is not -1, then bind it. */
rl_add_defun (name, function, key)
     char *name;
     Function *function;
     int key;
{
  if (key != -1)
    rl_bind_key (key, function);
  rl_add_funmap_entry (name, function);
}

/* Bind KEY to FUNCTION.  Returns non-zero if KEY is out of range. */
int
rl_bind_key (key, function)
     int key;
     Function *function;
{
  if (key < 0)
    return (key);

  if (key > 127 && key < 256)
    {
      if (keymap[ESC].type == ISKMAP)
	{
	  Keymap escmap = (Keymap)keymap[ESC].function;

	  key -= 128;
	  escmap[key].type = ISFUNC;
	  escmap[key].function = function;
	  return (0);
	}
      return (key);
    }

  keymap[key].type = ISFUNC;
  keymap[key].function = function;
 return (0);
}

/* Bind KEY to FUNCTION in MAP.  Returns non-zero in case of invalid
   KEY. */
int
rl_bind_key_in_map (key, function, map)
     int key;
     Function *function;
     Keymap map;
{
  int result;
  Keymap oldmap = keymap;

  keymap = map;
  result = rl_bind_key (key, function);
  keymap = oldmap;
  return (result);
}

/* Make KEY do nothing in the currently selected keymap.
   Returns non-zero in case of error. */
int
rl_unbind_key (key)
     int key;
{
  return (rl_bind_key (key, (Function *)NULL));
}

/* Make KEY do nothing in MAP.
   Returns non-zero in case of error. */
int
rl_unbind_key_in_map (key, map)
     int key;
     Keymap map;
{
  return (rl_bind_key_in_map (key, (Function *)NULL, map));
}

/* Bind the key sequence represented by the string KEYSEQ to
   FUNCTION.  This makes new keymaps as necessary.  The initial
   place to do bindings is in MAP. */
rl_set_key (keyseq, function, map)
     char *keyseq;
     Function *function;
     Keymap map;
{
  rl_generic_bind (ISFUNC, keyseq, function, map);
}

/* Bind the key sequence represented by the string KEYSEQ to
   the string of characters MACRO.  This makes new keymaps as
   necessary.  The initial place to do bindings is in MAP. */
rl_macro_bind (keyseq, macro, map)
     char *keyseq, *macro;
     Keymap map;
{
  char *macro_keys = (char *)xmalloc (2 * (strlen (macro)));
  int macro_keys_len;

  if (rl_translate_keyseq (macro, macro_keys, &macro_keys_len))
    {
      free (macro_keys);
      return;
    }
  rl_generic_bind (ISMACR, keyseq, macro_keys, map);
}

/* Bind the key sequence represented by the string KEYSEQ to
   the arbitrary pointer DATA.  TYPE says what kind of data is
   pointed to by DATA, right now this can be a function (ISFUNC),
   a macro (ISMACR), or a keymap (ISKMAP).  This makes new keymaps
   as necessary.  The initial place to do bindings is in MAP. */
rl_generic_bind (type, keyseq, data, map)
     int type;
     char *keyseq, *data;
     Keymap map;
{
  char *keys;
  int keys_len;
  register int i;
  int start;

  /* If no keys to bind to, exit right away. */
  if (!keyseq || !*keyseq)
    {
      if (type == ISMACR)
	free (data);
      return;
    }

  keys = (char *)alloca (1 + (2 * strlen (keyseq)));

  /* Translate the ASCII representation of KEYSEQ into an array
     of characters.  Stuff the characters into ARRAY, and the
     length of ARRAY into LENGTH. */
  if (rl_translate_keyseq (keyseq, keys, &keys_len))
    return;

  /* Handle mapping of the ESC Key in vi mode */
  start = 0;
#ifdef VI_MODE
  if ((rl_editing_mode == vi_mode) && (keys[0] == ESC))
    {
    start++;
    map = vi_movement_keymap;
    if(keys[1] == ESC)
      {
      extern KEYMAP_ENTRY_ARRAY vi_escape_keymap;

      start++;
      map = vi_escape_keymap;
      }
    }
#endif

  /* Bind keys, making new keymaps as necessary. */
  for (i = start; i < keys_len; i++)
    {
      if (i + 1 < keys_len)
	{
	  if (map[keys[i]].type != ISKMAP)
	    {
	      if (map[i].type == ISMACR)
		free ((char *)map[i].function);

	      map[keys[i]].type = ISKMAP;
	      map[keys[i]].function = (Function *)rl_make_bare_keymap ();
	    }
	  map = (Keymap)map[keys[i]].function;
	}
      else
	{
	  if (map[keys[i]].type == ISMACR)
	    free ((char *)map[keys[i]].function);

	  map[keys[i]].function = (Function *)data;
	  map[keys[i]].type = type;
	}
    }
}

/* Translate the ASCII representation of SEQ, stuffing the
   values into ARRAY, an array of characters.  LEN gets the
   final length of ARRAY.  Return non-zero if there was an
   error parsing SEQ. */
rl_translate_keyseq (seq, array, len)
     char *seq, *array;
     int *len;
{
  register int i, c, l = 0;

  for (i = 0; c = seq[i]; i++)
    {
      if (c == '\\')
	{
	  c = seq[++i];

	  if (!c)
	    break;

	  if (((c == 'C' || c == 'M') &&  seq[i + 1] == '-') ||
	      (c == 'e'))
	    {
	      /* Handle special case of backwards define. */
	      if (strncmp (&seq[i], "C-\\M-", 5) == 0)
		{
		  array[l++] = ESC;
		  i += 5;
		  array[l++] = CTRL (to_upper (seq[i]));
		  if (!seq[i])
		    i--;
		  continue;
		}

	      switch (c)
		{
		case 'M':
		  i++;
		  array[l++] = ESC;
		  break;

		case 'C':
		  i += 2;
		  array[l++] = CTRL (to_upper (seq[i]));
		  break;

		case 'e':
		  array[l++] = ESC;
		}

	      continue;
	    }
	}
      array[l++] = c;
    }

  array[l] = '\0';
  *len = l;
  return (0);
}

/* Return a pointer to the function that STRING represents.
   If STRING doesn't have a matching function, then a NULL pointer
   is returned. */
Function *
rl_named_function (string)
     char *string;
{
  register int i;
  static int stricmp ();

  for (i = 0; funmap[i]; i++)
    if (stricmp (funmap[i]->name, string) == 0)
      return (funmap[i]->function);
  return ((Function *)NULL);
}

/* The last key bindings file read. */
static char *last_readline_init_file = "~/.inputrc";

/* Re-read the current keybindings file. */
rl_re_read_init_file (count, ignore)
     int count, ignore;
{
  rl_read_init_file (last_readline_init_file);
}

/* Do key bindings from a file.  If FILENAME is NULL it defaults
   to `~/.inputrc'.  If the file existed and could be opened and
   read, 0 is returned, otherwise errno is returned. */
int
rl_read_init_file (filename)
     char *filename;
{
  int line_size, line_index;
  char *line = (char *)xmalloc (line_size = 100);
  char *openname;
  FILE *file;

  int c;

  /* Default the filename. */
  if (!filename)
    filename = "~/.inputrc";

  openname = tilde_expand (filename);

  /* Open the file. */
  file = fopen (openname, "r");
  free (openname);

  if (!file)
    return (errno);

  last_readline_init_file = filename;

  /* Loop reading lines from the file.  Lines that start with `#' are
     comments, all other lines are commands for readline initialization. */
  while ((c = getc(file)) != EOF)
    {
      /* If comment, flush to EOL. */
      if (c == '#')
	{
	  while ((c = getc(file)) != EOF && c != '\n');
	  if (c == EOF)
	    goto function_exit;
	  continue;
	}

      /* Otherwise, this is the start of a line.  Read the
	 line from the file. */
      line_index = 0;
      while (c != EOF && c != '\n')
	{
	  line[line_index++] = c;
	  if (line_index == line_size)
	    line = (char *)xrealloc (line, line_size += 100);
	  c = getc (file);
	}
      line[line_index] = '\0';

      /* Parse the line. */
      rl_parse_and_bind (line);
    }

function_exit:

  free (line);
  /* Close up the file and exit. */
  fclose (file);
  return (0);
}


/* **************************************************************** */
/*								    */
/*			Parser Directives       		    */
/*								    */
/* **************************************************************** */

/* Conditionals. */

/* Calling programs set this to have their argv[0]. */
char *rl_readline_name = "other";

/* Stack of previous values of parsing_conditionalized_out. */
static unsigned char *if_stack = (unsigned char *)NULL;
static int if_stack_depth = 0;
static int if_stack_size = 0;

/* Push parsing_conditionalized_out, and set parser state based on ARGS. */
parser_if (args)
     char *args;
{
  register int i;
  static int stricmp ();

  /* Push parser state. */
  if (if_stack_depth + 1 >= if_stack_size)
    {
      if (!if_stack)
	if_stack = (unsigned char *)xmalloc (if_stack_size = 20);
      else
	if_stack = (unsigned char *)xrealloc (if_stack, if_stack_size += 20);
    }
  if_stack[if_stack_depth++] = parsing_conditionalized_out;

  /* We only check to see if the first word in ARGS is the same as the
     value stored in rl_readline_name. */

  /* Isolate first argument. */
  for (i = 0; args[i] && !whitespace (args[i]); i++);

  if (args[i])
    args[i++] = '\0';

  if (stricmp (args, rl_readline_name) == 0)
    parsing_conditionalized_out = 0;
  else
    parsing_conditionalized_out = 1;
}

/* Invert the current parser state if there is anything on the stack. */
parser_else (args)
     char *args;
{
  if (if_stack_depth)
    parsing_conditionalized_out = !parsing_conditionalized_out;
  else
    {
      /* *** What, no error message? *** */
    }
}

/* Terminate a conditional, popping the value of
   parsing_conditionalized_out from the stack. */
parser_endif (args)
     char *args;
{
  if (if_stack_depth)
    parsing_conditionalized_out = if_stack[--if_stack_depth];
  else
    {
      /* *** What, no error message? *** */
    }
}

/* Associate textual names with actual functions. */
static struct {
  char *name;
  Function *function;
} parser_directives [] = {
  { "if", parser_if },
  { "endif", parser_endif },
  { "else", parser_else },
  { (char *)0x0, (Function *)0x0 }
};

/* Handle a parser directive.  STATEMENT is the line of the directive
   without any leading `$'. */
static int
handle_parser_directive (statement)
     char *statement;
{
  register int i;
  char *directive, *args;
  static int stricmp ();

  /* Isolate the actual directive. */

  /* Skip whitespace. */
  for (i = 0; whitespace (statement[i]); i++);

  directive = &statement[i];

  for (; statement[i] && !whitespace (statement[i]); i++);

  if (statement[i])
    statement[i++] = '\0';

  for (; statement[i] && whitespace (statement[i]); i++);

  args = &statement[i];

  /* Lookup the command, and act on it. */
  for (i = 0; parser_directives[i].name; i++)
    if (stricmp (directive, parser_directives[i].name) == 0)
      {
	(*parser_directives[i].function) (args);
	return (0);
      }

  /* *** Should an error message be output? */
  return (1);
}

/* Read the binding command from STRING and perform it.
   A key binding command looks like: Keyname: function-name\0,
   a variable binding command looks like: set variable value.
   A new-style keybinding looks like "\C-x\C-x": exchange-point-and-mark. */
rl_parse_and_bind (string)
     char *string;
{
  extern char *possible_control_prefixes[], *possible_meta_prefixes[];
  char *rindex (), *funname, *kname;
  static int substring_member_of_array (), stricmp ();
  register int c;
  int key, i;

  if (!string || !*string || *string == '#')
    return;

  /* If this is a parser directive, act on it. */
  if (*string == '$')
    {
      handle_parser_directive (&string[1]);
      return;
    }

  /* If we are supposed to be skipping parsing right now, then do it. */
  if (parsing_conditionalized_out)
    return;

  i = 0;
  /* If this keyname is a complex key expression surrounded by quotes,
     advance to after the matching close quote. */
  if (*string == '"')
    {
      for (i = 1; c = string[i]; i++)
	{
	  if (c == '"' && string[i - 1] != '\\')
	    break;
	}
    }

  /* Advance to the colon (:) or whitespace which separates the two objects. */
  for (; (c = string[i]) && c != ':' && c != ' ' && c != '\t'; i++ );

  /* Mark the end of the command (or keyname). */
  if (string[i])
    string[i++] = '\0';

  /* If this is a command to set a variable, then do that. */
  if (stricmp (string, "set") == 0)
    {
      char *var = string + i;
      char *value;

      /* Make VAR point to start of variable name. */
      while (*var && whitespace (*var)) var++;

      /* Make value point to start of value string. */
      value = var;
      while (*value && !whitespace (*value)) value++;
      if (*value)
	*value++ = '\0';
      while (*value && whitespace (*value)) value++;

      rl_variable_bind (var, value);
      return;
    }

  /* Skip any whitespace between keyname and funname. */
  for (; string[i] && whitespace (string[i]); i++);
  funname = &string[i];

  /* Now isolate funname.
     For straight function names just look for whitespace, since
     that will signify the end of the string.  But this could be a
     macro definition.  In that case, the string is quoted, so skip
     to the matching delimiter. */
  if (*funname == '\'' || *funname == '"')
    {
      int delimiter = string[i++];

      for (; c = string[i]; i++)
	{
	  if (c == delimiter && string[i - 1] != '\\')
	    break;
	}
      if (c)
	i++;
    }

  /* Advance to the end of the string.  */
  for (; string[i] && !whitespace (string[i]); i++);

  /* No extra whitespace at the end of the string. */
  string[i] = '\0';

  /* If this is a new-style key-binding, then do the binding with
     rl_set_key ().  Otherwise, let the older code deal with it. */
  if (*string == '"')
    {
      char *seq = (char *)alloca (1 + strlen (string));
      register int j, k = 0;

      for (j = 1; string[j]; j++)
	{
	  if (string[j] == '"' && string[j - 1] != '\\')
	    break;

	  seq[k++] = string[j];
	}
      seq[k] = '\0';

      /* Binding macro? */
      if (*funname == '\'' || *funname == '"')
	{
	  j = strlen (funname);

	  if (j && funname[j - 1] == *funname)
	    funname[j - 1] = '\0';

	  rl_macro_bind (seq, &funname[1], keymap);
	}
      else
	rl_set_key (seq, rl_named_function (funname), keymap);

      return;
    }

  /* Get the actual character we want to deal with. */
  kname = rindex (string, '-');
  if (!kname)
    kname = string;
  else
    kname++;

  key = glean_key_from_name (kname);

  /* Add in control and meta bits. */
  if (substring_member_of_array (string, possible_control_prefixes))
    key = CTRL (to_upper (key));

  if (substring_member_of_array (string, possible_meta_prefixes))
    key = META (key);

  /* Temporary.  Handle old-style keyname with macro-binding. */
  if (*funname == '\'' || *funname == '"')
    {
      char seq[2];
      int fl = strlen (funname);

      seq[0] = key; seq[1] = '\0';
      if (fl && funname[fl - 1] == *funname)
	funname[fl - 1] = '\0';

      rl_macro_bind (seq, &funname[1], keymap);
    }
  else
    rl_bind_key (key, rl_named_function (funname));
}

rl_variable_bind (name, value)
     char *name, *value;
{
  static int strnicmp (), stricmp ();

  if (stricmp (name, "editing-mode") == 0)
    {
      if (strnicmp (value, "vi", 2) == 0)
	{
#ifdef VI_MODE
	  keymap = vi_insertion_keymap;
	  rl_editing_mode = vi_mode;
#endif /* VI_MODE */
	}
      else if (strnicmp (value, "emacs", 5) == 0)
	{
	  keymap = emacs_standard_keymap;
	  rl_editing_mode = emacs_mode;
	}
    }
  else if (stricmp (name, "horizontal-scroll-mode") == 0)
    {
      if (!*value || stricmp (value, "On") == 0)
	horizontal_scroll_mode = 1;
      else
	horizontal_scroll_mode = 0;
    }
}

/* Return the character which matches NAME.
   For example, `Space' returns ' '. */

typedef struct {
  char *name;
  int value;
} assoc_list;

assoc_list name_key_alist[] = {
  { "Space", ' ' },
  { "SPC", ' ' },
  { "Rubout", 0x7f },
  { "DEL", 0x7f },
  { "Tab", 0x09 },
  { "Newline", '\n' },
  { "Return", '\r' },
  { "RET", '\r' },
  { "LFD", '\n' },
  { "Escape", '\033' },
  { "ESC", '\033' },

  { (char *)0x0, 0 }
};

int
glean_key_from_name (name)
     char *name;
{
  register int i;
  static int stricmp ();

  for (i = 0; name_key_alist[i].name; i++)
    if (stricmp (name, name_key_alist[i].name) == 0)
      return (name_key_alist[i].value);

  return (*name);
}


/* **************************************************************** */
/*								    */
/*			String Utility Functions		    */
/*								    */
/* **************************************************************** */

/* Return non-zero if any members of ARRAY are a substring in STRING. */
static int
substring_member_of_array (string, array)
     char *string, **array;
{
  static char *strindex ();

  while (*array)
    {
      if (strindex (string, *array))
	return (1);
      array++;
    }
  return (0);
}

/* Whoops, Unix doesn't have strnicmp. */

/* Compare at most COUNT characters from string1 to string2.  Case
   doesn't matter. */
static int
strnicmp (string1, string2, count)
     char *string1, *string2;
{
  register char ch1, ch2;

  while (count) {
    ch1 = *string1++;
    ch2 = *string2++;
    if (to_upper(ch1) == to_upper(ch2))
      count--;
    else break;
  }
  return (count);
}

/* strcmp (), but caseless. */
static int
stricmp (string1, string2)
     char *string1, *string2;
{
  register char ch1, ch2;

  while (*string1 && *string2) {
    ch1 = *string1++;
    ch2 = *string2++;
    if (to_upper(ch1) != to_upper(ch2))
      return (1);
  }
  return (*string1 | *string2);
}

/* Determine if s2 occurs in s1.  If so, return a pointer to the
   match in s1.  The compare is case insensitive. */
static char *
strindex (s1, s2)
     register char *s1, *s2;
{
  register int i, l = strlen (s2);
  register int len = strlen (s1);

  for (i = 0; (len - i) >= l; i++)
    if (strnicmp (&s1[i], s2, l) == 0)
      return (s1 + i);
  return ((char *)NULL);
}


#ifdef STATIC_MALLOC

/* **************************************************************** */
/*								    */
/*			xmalloc and xrealloc ()		     	    */
/*								    */
/* **************************************************************** */

static char *
xmalloc (bytes)
     int bytes;
{
  static memory_error_and_abort ();
  char *temp = (char *)malloc (bytes);

  if (!temp)
    memory_error_and_abort ();
  return (temp);
}

static char *
xrealloc (pointer, bytes)
     char *pointer;
     int bytes;
{
  static memory_error_and_abort ();
  char *temp = (char *)realloc (pointer, bytes);

  if (!temp)
    memory_error_and_abort ();
  return (temp);
}

static
memory_error_and_abort ()
{
  fprintf (stderr, "readline: Out of virtual memory!\n");
  abort ();
}
#endif /* STATIC_MALLOC */


/* **************************************************************** */
/*								    */
/*			Testing Readline			    */
/*								    */
/* **************************************************************** */

#ifdef TEST

main ()
{
  HIST_ENTRY **history_list ();
  char *temp = (char *)NULL;
  char *prompt = "readline% ";
  int done = 0;

  while (!done)
    {
      temp = readline (prompt);

      /* Test for EOF. */
      if (!temp)
	exit (1);

      /* If there is anything on the line, print it and remember it. */
      if (*temp)
	{
	  fprintf (stderr, "%s\r\n", temp);
	  add_history (temp);
	}

      /* Check for `command' that we handle. */
      if (strcmp (temp, "quit") == 0)
	done = 1;

      if (strcmp (temp, "list") == 0) {
	HIST_ENTRY **list = history_list ();
	register int i;
	if (list) {
	  for (i = 0; list[i]; i++) {
	    fprintf (stderr, "%d: %s\r\n", i, list[i]->line);
	    free (list[i]->line);
	  }
	  free (list);
	}
      }
      free (temp);
    }
}

#endif /* TEST */


/*
 * Local variables:
 * compile-command: "gcc -g -traditional -I. -I.. -DTEST -o readline readline.c keymaps.o funmap.o history.o -ltermcap"
 * end:
 */
