/* readline.c -- a general facility for reading lines of input
   with emacs style editing and completion. */

/* Copyright (C) 1987, 1989, 1992 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 1, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */
#define READLINE_LIBRARY

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#if !defined (NO_SYS_FILE)
#  include <sys/file.h>
#endif /* !NO_SYS_FILE */
#include <signal.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif /* HAVE_STDLIB_H */

#include <errno.h>
/* Not all systems declare ERRNO in errno.h... and some systems #define it! */
#if !defined (errno)
extern int errno;
#endif /* !errno */

#include <setjmp.h>

#include "posixstat.h"

/* System-specific feature definitions and include files. */
#include "rldefs.h"

#if defined (GWINSZ_IN_SYS_IOCTL) || (defined (VSTATUS) && !defined (SunOS4))
#  include <sys/ioctl.h>
#endif /* GWINSZ_IN_SYS_IOCTL || VSTATUS */

/* Some standard library routines. */
#include "readline.h"
#include "history.h"

/* NOTE: Functions and variables prefixed with `_rl_' are
   pseudo-global: they are global so they can be shared
   between files in the readline library, but are not intended
   to be visible to readline callers. */

/* Functions imported from other files in the library. */
extern char *tgetstr ();
extern void rl_prep_terminal (), rl_deprep_terminal ();

extern void _rl_bind_if_unbound ();

/* External redisplay functions and variables from display.c */
extern void _rl_move_vert ();
extern void _rl_update_final ();

extern void _rl_erase_at_end_of_line ();
extern void _rl_move_cursor_relative ();

extern int _rl_vis_botlin;
extern int _rl_last_c_pos;
extern int _rl_horizontal_scroll_mode;
extern int rl_display_fixed;
extern char *rl_display_prompt;

/* Variables imported from complete.c. */
extern char *rl_completer_word_break_characters;
extern char *rl_basic_word_break_characters;
extern int rl_completion_query_items;
extern int rl_complete_with_tilde_expansion;

#if defined (VI_MODE)
extern void _rl_vi_set_last ();
extern void _rl_vi_reset_last ();
extern void _rl_vi_done_inserting ();
#endif /* VI_MODE */

/* Forward declarations used in this file. */
void _rl_free_history_entry ();

int _rl_dispatch ();
void _rl_set_screen_size ();
int _rl_output_character_function ();

static char *readline_internal ();
static void readline_initialize_everything ();
static int init_terminal_io ();
static void start_using_history ();
static void bind_arrow_keys ();

#if !defined (__GO32__)
static void readline_default_bindings ();
#endif /* !__GO32__ */

#if defined (__GO32__)
#  include <sys/pc.h>
#  undef HANDLE_SIGNALS
#endif /* __GO32__ */

#if defined (STATIC_MALLOC)
static char *xmalloc (), *xrealloc ();
#else
extern char *xmalloc (), *xrealloc ();
#endif /* STATIC_MALLOC */


/* **************************************************************** */
/*								    */
/*			Line editing input utility		    */
/*								    */
/* **************************************************************** */

static char *LibraryVersion = "2.0";

/* A pointer to the keymap that is currently in use.
   By default, it is the standard emacs keymap. */
Keymap _rl_keymap = emacs_standard_keymap;

/* The current style of editing. */
int rl_editing_mode = emacs_mode;

/* Non-zero if the previous command was a kill command. */
static int last_command_was_kill = 0;

/* The current value of the numeric argument specified by the user. */
int rl_numeric_arg = 1;

/* Non-zero if an argument was typed. */
int rl_explicit_arg = 0;

/* Temporary value used while generating the argument. */
int rl_arg_sign = 1;

/* Non-zero means we have been called at least once before. */
static int rl_initialized = 0;

/* If non-zero, this program is running in an EMACS buffer. */
static int running_in_emacs = 0;

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
FILE *rl_instream = (FILE *)NULL;
FILE *rl_outstream = (FILE *)NULL;

/* Non-zero means echo characters as they are read. */
int readline_echoing_p = 1;

/* Current prompt. */
char *rl_prompt;
int rl_visible_prompt_length = 0;

/* The number of characters read in order to type this complete command. */
int rl_key_sequence_length = 0;

/* If non-zero, then this is the address of a function to call just
   before readline_internal () prints the first prompt. */
Function *rl_startup_hook = (Function *)NULL;

/* What we use internally.  You should always refer to RL_LINE_BUFFER. */
static char *the_line;

/* The character that can generate an EOF.  Really read from
   the terminal driver... just defaulted here. */
int _rl_eof_char = CTRL ('D');

/* Non-zero makes this the next keystroke to read. */
int rl_pending_input = 0;

/* Pointer to a useful terminal name. */
char *rl_terminal_name = (char *)NULL;

/* Non-zero means to always use horizontal scrolling in line display. */
int _rl_horizontal_scroll_mode = 0;

/* Non-zero means to display an asterisk at the starts of history lines
   which have been modified. */
int _rl_mark_modified_lines = 0;  

/* The style of `bell' notification preferred.  This can be set to NO_BELL,
   AUDIBLE_BELL, or VISIBLE_BELL. */
int _rl_bell_preference = AUDIBLE_BELL;
     
/* Line buffer and maintenence. */
char *rl_line_buffer = (char *)NULL;
int rl_line_buffer_len = 0;
#define DEFAULT_BUFFER_SIZE 256

/* Forward declarations used by the display and termcap code. */
int term_xn;
int screenwidth, screenheight, screenchars;


/* **************************************************************** */
/*								    */
/*			`Forward' declarations  		    */
/*								    */
/* **************************************************************** */

/* Non-zero means do not parse any lines other than comments and
   parser directives. */
unsigned char _rl_parsing_conditionalized_out = 0;

/* Non-zero means to save keys that we dispatch on in a kbd macro. */
static int defining_kbd_macro = 0;

/* Non-zero means to convert characters with the meta bit set to
   escape-prefixed characters so we can indirect through
   emacs_meta_keymap or vi_escape_keymap. */
int _rl_convert_meta_chars_to_ascii = 1;

/* Non-zero means to output characters with the meta bit set directly
   rather than as a meta-prefixed escape sequence. */
int _rl_output_meta_chars = 0;

/* Non-zero tells rl_delete_text and rl_insert_text to not add to
   the undo list. */
static int doing_an_undo = 0;

/* **************************************************************** */
/*								    */
/*			Top Level Functions			    */
/*								    */
/* **************************************************************** */

/* Non-zero means treat 0200 bit in terminal input as Meta bit. */
int _rl_meta_flag = 0;	/* Forward declaration */

/* Read a line of input.  Prompt with PROMPT.  A NULL PROMPT means
   none.  A return value of NULL means that EOF was encountered. */
char *
readline (prompt)
     char *prompt;
{
  char *value;

  rl_prompt = prompt;

  /* If we are at EOF return a NULL string. */
  if (rl_pending_input == EOF)
    {
      rl_pending_input = 0;
      return ((char *)NULL);
    }

  rl_visible_prompt_length = rl_expand_prompt (rl_prompt);

  rl_initialize ();
  rl_prep_terminal (_rl_meta_flag);

#if defined (HANDLE_SIGNALS)
  rl_set_signals ();
#endif

  value = readline_internal ();
  rl_deprep_terminal ();

#if defined (HANDLE_SIGNALS)
  rl_clear_signals ();
#endif

  return (value);
}

/* Read a line of input from the global rl_instream, doing output on
   the global rl_outstream.
   If rl_prompt is non-null, then that is our prompt. */
static char *
readline_internal ()
{
  int lastc, c, eof_found;

  in_stream  = rl_instream;
  out_stream = rl_outstream;

  lastc = -1;
  eof_found = 0;

  if (rl_startup_hook)
    (*rl_startup_hook) ();

  if (!readline_echoing_p)
    {
      if (rl_prompt)
	{
	  fprintf (out_stream, "%s", rl_prompt);
	  fflush (out_stream);
	}
    }
  else
    {
      rl_on_new_line ();
      rl_redisplay ();
#if defined (VI_MODE)
      if (rl_editing_mode == vi_mode)
	rl_vi_insertion_mode ();
#endif /* VI_MODE */
    }

  while (!rl_done)
    {
      int lk = last_command_was_kill;
      int code;

      code = setjmp (readline_top_level);

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

      /* The character _rl_eof_char typed to blank line, and not as the
	 previous character is interpreted as EOF. */
      if (((c == _rl_eof_char && lastc != c) || c == EOF) && !rl_end)
	{
	  eof_found = 1;
	  break;
	}

      lastc = c;
      _rl_dispatch (c, _rl_keymap);

      /* If there was no change in last_command_was_kill, then no kill
	 has taken place.  Note that if input is pending we are reading
	 a prefix command, so nothing has changed yet. */
      if (!rl_pending_input)
	{
	  if (lk == last_command_was_kill)
	    last_command_was_kill = 0;
	}

#if defined (VI_MODE)
      /* In vi mode, when you exit insert mode, the cursor moves back
	 over the previous character.  We explicitly check for that here. */
      if (rl_editing_mode == vi_mode && _rl_keymap == vi_movement_keymap)
	rl_vi_check ();
#endif /* VI_MODE */

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
	_rl_free_history_entry (entry);

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

/* **************************************************************** */
/*								    */
/*			Character Input Buffering       	    */
/*								    */
/* **************************************************************** */

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
  return push_index;
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
   Return the key in KEY.
   Result is KEY if there was a key, or 0 if there wasn't. */
int
rl_get_char (key)
     int *key;
{
  if (push_index == pop_index)
    return (0);

  *key = ibuffer[pop_index++];

  if (pop_index >= ibuffer_len)
    pop_index = 0;

  return (1);
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

/* If a character is available to be read, then read it
   and stuff it into IBUFFER.  Otherwise, just return. */
void
rl_gather_tyi ()
{
#if defined (__GO32__)
  char input;

  if (isatty (0))
    {
      int i = rl_getc ();

      if (i != EOF)
	rl_stuff_char (i);
    }
  else if (kbhit () && ibuffer_space ())
    rl_stuff_char (getkey ());
#else /* !__GO32__ */

  int tty = fileno (in_stream);
  register int tem, result = -1;
  int chars_avail;
  char input;

#if defined (FIONREAD)
  result = ioctl (tty, FIONREAD, &chars_avail);
#endif

#if defined (O_NDELAY)
  if (result == -1)
    {
      int flags;

      flags = fcntl (tty, F_GETFL, 0);

      fcntl (tty, F_SETFL, (flags | O_NDELAY));
      chars_avail = read (tty, &input, 1);

      fcntl (tty, F_SETFL, flags);
      if (chars_avail == -1 && errno == EAGAIN)
	return;
    }
#endif /* O_NDELAY */

  /* If there's nothing available, don't waste time trying to read
     something. */
  if (chars_avail == 0)
    return;

  tem = ibuffer_space ();

  if (chars_avail > tem)
    chars_avail = tem;

  /* One cannot read all of the available input.  I can only read a single
     character at a time, or else programs which require input can be
     thwarted.  If the buffer is larger than one character, I lose.
     Damn! */
  if (tem < ibuffer_len)
    chars_avail = 0;

  if (result != -1)
    {
      while (chars_avail--)
	rl_stuff_char (rl_getc (in_stream));
    }
  else
    {
      if (chars_avail)
	rl_stuff_char (input);
    }
#endif /* !__GO32__ */
}

static int next_macro_key ();
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
      /* If input is coming from a macro, then use that. */
      if (c = next_macro_key ())
	return (c);

      /* If the user has an event function, then call it periodically. */
      if (rl_event_hook)
	{
	  while (rl_event_hook && !rl_get_char (&c))
	    {
	      (*rl_event_hook) ();
	      rl_gather_tyi ();
	    }
	}
      else
	{
	  if (!rl_get_char (&c))
	    c = rl_getc (in_stream);
	}
    }

  return (c);
}

/* Found later in this file. */
static void add_macro_char (), with_macro_input ();

/* Do the command associated with KEY in MAP.
   If the associated command is really a keymap, then read
   another key, and dispatch into that map. */
int
_rl_dispatch (key, map)
     register int key;
     Keymap map;
{
  int r = 0;

  if (META_CHAR (key) && _rl_convert_meta_chars_to_ascii)
    {
      if (map[ESC].type == ISKMAP)
	{
	  if (defining_kbd_macro)
	    add_macro_char (ESC);
	  map = FUNCTION_TO_KEYMAP (map, ESC);
	  key = UNMETA (key);
	  rl_key_sequence_length += 2;
	  return (_rl_dispatch (key, map));
	}
      else
	ding ();
      return 0;
    }

  if (defining_kbd_macro)
    add_macro_char (key);

  switch (map[key].type)
    {
    case ISFUNC:
      {
	Function *func = map[key].function;

	if (func != (Function *)NULL)
	  {
	    /* Special case rl_do_lowercase_version (). */
	    if (func == rl_do_lowercase_version)
	      return (_rl_dispatch (to_lower (key), map));

	    r = (*map[key].function)(rl_numeric_arg * rl_arg_sign, key);

	    /* If we have input pending, then the last command was a prefix
	       command.  Don't change the state of rl_last_func.  Otherwise,
	       remember the last command executed in this variable. */
	    if (!rl_pending_input)
	      rl_last_func = map[key].function;
	  }
	else
	  {
	    rl_abort ();
	    return -1;
	  }
      }
      break;

    case ISKMAP:
      if (map[key].function != (Function *)NULL)
	{
	  int newkey;

	  rl_key_sequence_length++;
	  newkey = rl_read_key ();
	  r = _rl_dispatch (newkey, FUNCTION_TO_KEYMAP (map, key));
	}
      else
	{
	  rl_abort ();
	  return -1;
	}
      break;

    case ISMACR:
      if (map[key].function != (Function *)NULL)
	{
	  char *macro;

	  macro = savestring ((char *)map[key].function);
	  with_macro_input (macro);
	  return 0;
	}
      break;
    }
#if defined (VI_MODE)
  if (rl_editing_mode == vi_mode && _rl_keymap == vi_movement_keymap &&
      rl_vi_textmod_command (key))
    _rl_vi_set_last (key, rl_numeric_arg, rl_arg_sign);
#endif
  return (r);
}


/* **************************************************************** */
/*								    */
/*			Hacking Keyboard Macros 		    */
/*								    */
/* **************************************************************** */

/* The currently executing macro string.  If this is non-zero,
   then it is a malloc ()'ed string where input is coming from. */
static char *executing_macro = (char *)NULL;

/* The offset in the above string to the next character to be read. */
static int executing_macro_index = 0;

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
  int sindex;
};

/* The list of saved macros. */
struct saved_macro *macro_list = (struct saved_macro *)NULL;

/* Forward declarations of static functions.  Thank you C. */
static void push_executing_macro (), pop_executing_macro ();

/* This one has to be declared earlier in the file. */
/* static void add_macro_char (); */

/* Set up to read subsequent input from STRING.
   STRING is free ()'ed when we are done with it. */
static void
with_macro_input (string)
     char *string;
{
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
      pop_executing_macro ();
      return (next_macro_key ());
    }

  return (executing_macro[executing_macro_index++]);
}

/* Save the currently executing macro on a stack of saved macros. */
static void
push_executing_macro ()
{
  struct saved_macro *saver;

  saver = (struct saved_macro *)xmalloc (sizeof (struct saved_macro));
  saver->next = macro_list;
  saver->sindex = executing_macro_index;
  saver->string = executing_macro;

  macro_list = saver;
}

/* Discard the current macro, replacing it with the one
   on the top of the stack of saved macros. */
static void
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
      executing_macro_index = macro_list->sindex;
      macro_list = macro_list->next;
      free (disposer);
    }
}

/* Add a character to the macro being built. */
static void
add_macro_char (c)
     int c;
{
  if (current_macro_index + 1 >= current_macro_size)
    {
      if (!current_macro)
	current_macro = xmalloc (current_macro_size = 25);
      else
	current_macro = xrealloc (current_macro, current_macro_size += 25);
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
    {
      rl_abort ();
      return -1;
    }

  if (rl_explicit_arg)
    {
      if (current_macro)
	with_macro_input (savestring (current_macro));
    }
  else
    current_macro_index = 0;

  defining_kbd_macro = 1;
  return 0;
}

/* Stop defining a keyboard macro.
   A numeric argument says to execute the macro right now,
   that many times, counting the definition as the first time. */
rl_end_kbd_macro (count, ignore)
     int count, ignore;
{
  if (!defining_kbd_macro)
    {
      rl_abort ();
      return -1;
    }

  current_macro_index -= (rl_key_sequence_length - 1);
  current_macro[current_macro_index] = '\0';

  defining_kbd_macro = 0;

  return (rl_call_last_kbd_macro (--count, 0));
}

/* Execute the most recently defined keyboard macro.
   COUNT says how many times to execute it. */
rl_call_last_kbd_macro (count, ignore)
     int count, ignore;
{
  if (!current_macro)
    rl_abort ();

  if (defining_kbd_macro)
    {
      ding ();		/* no recursive macros */
      current_macro[--current_macro_index] = '\0';	/* erase this char */
      return 0;
    }

  while (count--)
    with_macro_input (savestring (current_macro));
  return 0;
}

void
_rl_kill_kbd_macro ()
{
  if (current_macro)
    {
      free (current_macro);
      current_macro = (char *) NULL;
    }
  current_macro_size = current_macro_index = 0;

  if (executing_macro)
    {
      free (executing_macro);
      executing_macro = (char *) NULL;
    }
  executing_macro_index = 0;

  defining_kbd_macro = 0;
}

/* **************************************************************** */
/*								    */
/*			Initializations 			    */
/*								    */
/* **************************************************************** */

/* Initliaze readline (and terminal if not already). */
rl_initialize ()
{
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
  rl_reset_line_state ();

  /* No such function typed yet. */
  rl_last_func = (Function *)NULL;

  /* Parsing of key-bindings begins in an enabled state. */
  _rl_parsing_conditionalized_out = 0;

  return 0;
}

/* Initialize the entire state of the world. */
static void
readline_initialize_everything ()
{
  char *t, *t1;

  /* Find out if we are running in Emacs. */
  running_in_emacs = getenv ("EMACS") != (char *)0;

  /* Set up input and output if they are not already set up. */
  if (!rl_instream)
    rl_instream = stdin;

  if (!rl_outstream)
    rl_outstream = stdout;

  /* Bind in_stream and out_stream immediately.  These values may change,
     but they may also be used before readline_internal () is called. */
  in_stream = rl_instream;
  out_stream = rl_outstream;

  /* Allocate data structures. */
  if (!rl_line_buffer)
    rl_line_buffer = xmalloc (rl_line_buffer_len = DEFAULT_BUFFER_SIZE);

  /* Initialize the terminal interface. */
  init_terminal_io ((char *)NULL);

#if !defined (__GO32__)
  /* Bind tty characters to readline functions. */
  readline_default_bindings ();
#endif /* !__GO32__ */

  /* Initialize the function names. */
  rl_initialize_funmap ();

  /* Check for LC_CTYPE and use its value to decide the defaults for
     8-bit character input and output. */
  t = getenv ("LC_CTYPE");
  t1 = getenv ("LANG");
  if (t && (strstr (t, "8859-1") != NULL || strstr (t, "8859_1") != NULL ||
	  strstr (t, "KOI8-R") != NULL || strstr (t, "koi8-r") != NULL) ||
      t1 && (strstr (t1, "8859-1") != NULL || strstr (t1, "8859_1") != NULL ||
	   strstr (t1, "KOI8-R") != NULL || strstr (t1, "koi8-r") != NULL))
    {
      _rl_meta_flag = 1;
      _rl_convert_meta_chars_to_ascii = 0;
      _rl_output_meta_chars = 1;
    }
      
  /* Read in the init file. */
  rl_read_init_file ((char *)NULL);

  /* XXX */
  if (_rl_horizontal_scroll_mode && term_xn)
    {
      screenwidth--;
      screenchars -= screenheight;
    }

  /* Override the effect of any `set keymap' assignments in the
     inputrc file. */
  rl_set_keymap_from_edit_mode ();

  /* Try to bind a common arrow key prefix, if not already bound. */
  bind_arrow_keys ();

  /* If the completion parser's default word break characters haven't
     been set yet, then do so now. */
  if (rl_completer_word_break_characters == (char *)NULL)
    rl_completer_word_break_characters = rl_basic_word_break_characters;
}

/* If this system allows us to look at the values of the regular
   input editing characters, then bind them to their readline
   equivalents, iff the characters are not bound to keymaps. */
static void
readline_default_bindings ()
{
  rltty_set_default_bindings (_rl_keymap);
}

static void
bind_arrow_keys_internal ()
{
  Function *f;

  f = rl_function_of_keyseq ("\033[A", _rl_keymap, (int *)NULL);
  if (!f || f == rl_do_lowercase_version)
    {
      _rl_bind_if_unbound ("\033[A", rl_get_previous_history);
      _rl_bind_if_unbound ("\033[B", rl_get_next_history);
      _rl_bind_if_unbound ("\033[C", rl_forward);
      _rl_bind_if_unbound ("\033[D", rl_backward);
    }

  f = rl_function_of_keyseq ("\033OA", _rl_keymap, (int *)NULL);
  if (!f || f == rl_do_lowercase_version)
    {
      _rl_bind_if_unbound ("\033OA", rl_get_previous_history);
      _rl_bind_if_unbound ("\033OB", rl_get_next_history);
      _rl_bind_if_unbound ("\033OC", rl_forward);
      _rl_bind_if_unbound ("\033OD", rl_backward);
    }
}

/* Try and bind the common arrow key prefix after giving termcap and
   the inputrc file a chance to bind them and create `real' keymaps
   for the arrow key prefix. */
static void
bind_arrow_keys ()
{
  Keymap xkeymap;

  xkeymap = _rl_keymap;

  _rl_keymap = emacs_standard_keymap;
  bind_arrow_keys_internal ();

#if defined (VI_MODE)
  _rl_keymap = vi_movement_keymap;
  bind_arrow_keys_internal ();
#endif

  _rl_keymap = xkeymap;
}


/* **************************************************************** */
/*								    */
/*			Numeric Arguments			    */
/*								    */
/* **************************************************************** */

/* Handle C-u style numeric args, as well as M--, and M-digits. */
static int
rl_digit_loop ()
{
  int key, c;

  while (1)
    {
      rl_message ("(arg: %d) ", rl_arg_sign * rl_numeric_arg);
      key = c = rl_read_key ();

      if (_rl_keymap[c].type == ISFUNC &&
	  _rl_keymap[c].function == rl_universal_argument)
	{
	  rl_numeric_arg *= 4;
	  continue;
	}
      c = UNMETA (c);
      if (digit_p (c))
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
	      rl_arg_sign = -1;
	    }
	  else
	    {
	      rl_clear_message ();
	      return (_rl_dispatch (key, _rl_keymap));
	    }
	}
    }
  return 0;
}

/* Add the current digit to the argument in progress. */
rl_digit_argument (ignore, key)
     int ignore, key;
{
  rl_pending_input = key;
  return (rl_digit_loop ());
}

/* What to do when you abort reading an argument. */
rl_discard_argument ()
{
  ding ();
  rl_clear_message ();
  rl_init_argument ();
  return 0;
}

/* Create a default argument. */
rl_init_argument ()
{
  rl_numeric_arg = rl_arg_sign = 1;
  rl_explicit_arg = 0;
  return 0;
}

/* C-u, universal argument.  Multiply the current argument by 4.
   Read a key.  If the key has nothing to do with arguments, then
   dispatch on it.  If the key is the abort character then abort. */
rl_universal_argument ()
{
  rl_numeric_arg *= 4;
  return (rl_digit_loop ());
}

/* **************************************************************** */
/*								    */
/*			Terminal and Termcap			    */
/*								    */
/* **************************************************************** */

static char *term_buffer = (char *)NULL;
static char *term_string_buffer = (char *)NULL;

static int tcap_initialized = 0;

/* Non-zero means this terminal can't really do anything. */
int dumb_term = 0;
/* On Solaris2, sys/types.h #includes sys/reg.h, which #defines PC.
   Unfortunately, PC is a global variable used by the termcap library. */
#undef PC

#if !defined (__linux__)
/* If this causes problems, add back the `extern'. */
/*extern*/ char PC, *BC, *UP;
#endif /* __linux__ */

/* Some strings to control terminal actions.  These are output by tputs (). */
char *term_goto, *term_clreol, *term_cr, *term_clrpag, *term_backspace;
char *term_pc;

/* Non-zero if we determine that the terminal can do character insertion. */
int terminal_can_insert = 0;

/* How to insert characters. */
char *term_im, *term_ei, *term_ic, *term_ip, *term_IC;

/* How to delete characters. */
char *term_dc, *term_DC;

#if defined (HACK_TERMCAP_MOTION)
char *term_forward_char;
#endif  /* HACK_TERMCAP_MOTION */

/* How to go up a line. */
char *term_up;

/* A visible bell, if the terminal can be made to flash the screen. */
char *visible_bell;

/* Non-zero means that this terminal has a meta key. */
int term_has_meta;

/* The string to write to turn on the meta key, if this term has one. */
char *term_mm;

/* The string to write to turn off the meta key, if this term has one. */
char *term_mo;

/* The key sequences output by the arrow keys, if this terminal has any. */
char *term_ku, *term_kd, *term_kr, *term_kl;

/* How to initialize and reset the arrow keys, if this terminal has any. */
char *term_ks, *term_ke;

/* Re-initialize the terminal considering that the TERM/TERMCAP variable
   has changed. */
rl_reset_terminal (terminal_name)
     char *terminal_name;
{
  init_terminal_io (terminal_name);
  return 0;
}

/* Set readline's idea of the screen size.  TTY is a file descriptor open
   to the terminal.  If IGNORE_ENV is true, we do not pay attention to the
   values of $LINES and $COLUMNS.  The tests for TERM_STRING_BUFFER being
   non-null serve to check whether or not we have initialized termcap. */
void
_rl_set_screen_size (tty, ignore_env)
     int tty, ignore_env;
{
#if defined (TIOCGWINSZ)
  struct winsize window_size;
#endif /* TIOCGWINSZ */

#if defined (TIOCGWINSZ)
  if (ioctl (tty, TIOCGWINSZ, &window_size) == 0)
    {
      screenwidth = (int) window_size.ws_col;
      screenheight = (int) window_size.ws_row;
    }
#endif /* TIOCGWINSZ */

  /* Environment variable COLUMNS overrides setting of "co" if IGNORE_ENV
     is unset. */
  if (screenwidth <= 0)
    {
      char *sw;

      if (!ignore_env && (sw = getenv ("COLUMNS")))
	screenwidth = atoi (sw);

      if (screenwidth <= 0 && term_string_buffer)
	screenwidth = tgetnum ("co");
    }

  /* Environment variable LINES overrides setting of "li" if IGNORE_ENV
     is unset. */
  if (screenheight <= 0)
    {
      char *sh;

      if (!ignore_env && (sh = getenv ("LINES")))
	screenheight = atoi (sh);

      if (screenheight <= 0 && term_string_buffer)
	screenheight = tgetnum ("li");
    }

  /* If all else fails, default to 80x24 terminal. */
  if (screenwidth <= 1)
    screenwidth = 80;

  if (screenheight <= 0)
    screenheight = 24;

#if defined (SHELL)
  /* If we're being compiled as part of bash, set the environment
     variables $LINES and $COLUMNS to new values. */
  set_lines_and_columns (screenheight, screenwidth);
#endif

  if (!term_xn)
    screenwidth--;

  screenchars = screenwidth * screenheight;
}

struct _tc_string {
     char *tc_var;
     char **tc_value;
};

/* This should be kept sorted, just in case we decide to change the
   search algorithm to something smarter. */
static struct _tc_string tc_strings[] =
{
  "DC", &term_DC,
  "IC", &term_IC,
  "ce", &term_clreol,
  "cl", &term_clrpag,
  "cr", &term_cr,
  "dc", &term_dc,
  "ei", &term_ei,
  "ic", &term_ic,
  "im", &term_im,
  "kd", &term_kd,
  "kl", &term_kl,
  "kr", &term_kr,
  "ku", &term_ku,
  "ks", &term_ks,
  "ke", &term_ke,
  "le", &term_backspace,
  "mm", &term_mm,
  "mo", &term_mo,
#if defined (HACK_TERMCAP_MOTION)
  "nd", &term_forward_char,
#endif
  "pc", &term_pc,
  "up", &term_up,
  "vb", &visible_bell,
};

#define NUM_TC_STRINGS (sizeof (tc_strings) / sizeof (struct _tc_string))

/* Read the desired terminal capability strings into BP.  The capabilities
   are described in the TC_STRINGS table. */
static void
get_term_capabilities (bp)
     char **bp;
{
  register int i;

  for (i = 0; i < NUM_TC_STRINGS; i++)
    *(tc_strings[i].tc_value) = tgetstr (tc_strings[i].tc_var, bp);
  tcap_initialized = 1;
}

static int
init_terminal_io (terminal_name)
     char *terminal_name;
{
#if defined (__GO32__)
  screenwidth = ScreenCols ();
  screenheight = ScreenRows ();
  screenchars = screenwidth * screenheight;
  term_cr = "\r";
  term_im = term_ei = term_ic = term_IC = (char *)NULL;
  term_up = term_dc = term_DC = visible_bell = (char *)NULL;

  /* Does the __GO32__ have a meta key?  I don't know. */
  term_has_meta = 0;
  term_mm = term_mo = (char *)NULL;

  /* It probably has arrow keys, but I don't know what they are. */
  term_ku = term_kd = term_kr = term_kl = (char *)NULL;

#if defined (HACK_TERMCAP_MOTION)
  term_forward_char = (char *)NULL;
#endif /* HACK_TERMCAP_MOTION */
  terminal_can_insert = term_xn = 0;
  return;
#else /* !__GO32__ */

  char *term, *buffer;
  int tty;
  Keymap xkeymap;

  term = terminal_name ? terminal_name : getenv ("TERM");

  if (!term_string_buffer)
    term_string_buffer = xmalloc (2048);

  if (!term_buffer)
    term_buffer = xmalloc (2048);

  buffer = term_string_buffer;

  term_clrpag = term_cr = term_clreol = (char *)NULL;

  if (!term)
    term = "dumb";

  if (tgetent (term_buffer, term) <= 0)
    {
      dumb_term = 1;
      screenwidth = 79;
      screenheight = 24;
      screenchars = 79 * 24;
      term_cr = "\r";
      term_im = term_ei = term_ic = term_IC = (char *)NULL;
      term_up = term_dc = term_DC = visible_bell = (char *)NULL;
      term_ku = term_kd = term_kl = term_kr = (char *)NULL;
#if defined (HACK_TERMCAP_MOTION)
      term_forward_char = (char *)NULL;
#endif
      terminal_can_insert = 0;
      return 0;
    }

  get_term_capabilities (&buffer);

  /* Set up the variables that the termcap library expects the application
     to provide. */
  PC = term_pc ? *term_pc : 0;
  BC = term_backspace;
  UP = term_up;

  if (!term_cr)
    term_cr =  "\r";

  if (rl_instream)
    tty = fileno (rl_instream);
  else
    tty = 0;

  screenwidth = screenheight = 0;

  term_xn = tgetflag ("am") && tgetflag ("xn");

  _rl_set_screen_size (tty, 0);

  /* "An application program can assume that the terminal can do
      character insertion if *any one of* the capabilities `IC',
      `im', `ic' or `ip' is provided."  But we can't do anything if
      only `ip' is provided, so... */
  terminal_can_insert = (term_IC || term_im || term_ic);

  /* Check to see if this terminal has a meta key and clear the capability
     variables if there is none. */
  term_has_meta = (tgetflag ("km") || tgetflag ("MT"));
  if (!term_has_meta)
    {
      term_mm = (char *)NULL;
      term_mo = (char *)NULL;
    }

  /* Attempt to find and bind the arrow keys.  Do not override already
     bound keys in an overzealous attempt, however. */
  xkeymap = _rl_keymap;

  _rl_keymap = emacs_standard_keymap;
  _rl_bind_if_unbound (term_ku, rl_get_previous_history);
  _rl_bind_if_unbound (term_kd, rl_get_next_history);
  _rl_bind_if_unbound (term_kr, rl_forward);
  _rl_bind_if_unbound (term_kl, rl_backward);

#if defined (VI_MODE)
  _rl_keymap = vi_movement_keymap;
  _rl_bind_if_unbound (term_ku, rl_get_previous_history);
  _rl_bind_if_unbound (term_kd, rl_get_next_history);
  _rl_bind_if_unbound (term_kr, rl_forward);
  _rl_bind_if_unbound (term_kl, rl_backward);
#endif /* VI_MODE */

  _rl_keymap = xkeymap;

#endif /* !__GO32__ */
  return 0;
}

char *
rl_get_termcap (cap)
     char *cap;
{
  register int i;

  if (tcap_initialized == 0)
    return ((char *)NULL);
  for (i = 0; i < NUM_TC_STRINGS; i++)
    {
      if (tc_strings[i].tc_var[0] == cap[0] && strcmp (tc_strings[i].tc_var, cap) == 0)
        return *(tc_strings[i].tc_value);
    }
  return ((char *)NULL);
}

/* A function for the use of tputs () */
int
_rl_output_character_function (c)
     int c;
{
  return putc (c, out_stream);
}

/* Write COUNT characters from STRING to the output stream. */
void
_rl_output_some_chars (string, count)
     char *string;
     int count;
{
  fwrite (string, 1, count, out_stream);
}

/* Move the cursor back. */
backspace (count)
     int count;
{
  register int i;

#if !defined (__GO32__)
  if (term_backspace)
    for (i = 0; i < count; i++)
      tputs (term_backspace, 1, _rl_output_character_function);
  else
#endif /* !__GO32__ */
    for (i = 0; i < count; i++)
      putc ('\b', out_stream);
  return 0;
}

/* Move to the start of the next line. */
crlf ()
{
#if defined (NEW_TTY_DRIVER)
  tputs (term_cr, 1, _rl_output_character_function);
#endif /* NEW_TTY_DRIVER */
  putc ('\n', out_stream);
  return 0;
}

rl_tty_status (count, key)
     int count, key;
{
#if defined (TIOCSTAT)
  ioctl (1, TIOCSTAT, (char *)0);
  rl_refresh_line ();
#else
  ding ();
#endif
  return 0;
}


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
  if (pure_alphabetic (c) || (digit_p (c)))
    return (1);

  if (allow_pathname_alphabetic_chars)
    return (strchr (pathname_alphabetic_chars, c) != NULL);
  else
    return (0);
}

/* Ring the terminal bell. */
int
ding ()
{
  if (readline_echoing_p)
    {
#if !defined (__GO32__)
      switch (_rl_bell_preference)
        {
	case NO_BELL:
	default:
	  break;
	case VISIBLE_BELL:
	  if (visible_bell)
	    {
	      tputs (visible_bell, 1, _rl_output_character_function);
	      break;
	    }
	  /* FALLTHROUGH */
	case AUDIBLE_BELL:
	  fprintf (stderr, "\007");
	  fflush (stderr);
	  break;
        }
#else /* __GO32__ */
      fprintf (stderr, "\007");
      fflush (stderr);
#endif /* __GO32__ */
      return (0);
    }
  return (-1);
}

/* How to abort things. */
rl_abort (count, key)
     int count, key;
{
  ding ();
  rl_clear_message ();
  rl_init_argument ();
  rl_pending_input = 0;

  defining_kbd_macro = 0;
  while (executing_macro)
    pop_executing_macro ();

  rl_last_func = (Function *)NULL;
  longjmp (readline_top_level, 1);
}

/* Return a copy of the string between FROM and TO.
   FROM is inclusive, TO is not. */
char *
rl_copy_text (from, to)
     int from, to;
{
  register int length;
  char *copy;

  /* Fix it if the caller is confused. */
  if (from > to)
    {
      int t = from;
      from = to;
      to = t;
    }

  length = to - from;
  copy = xmalloc (1 + length);
  strncpy (copy, the_line + from, length);
  copy[length] = '\0';
  return (copy);
}

/* Increase the size of RL_LINE_BUFFER until it has enough space to hold
   LEN characters. */
void
rl_extend_line_buffer (len)
     int len;
{
  while (len >= rl_line_buffer_len)
    {
      rl_line_buffer_len += DEFAULT_BUFFER_SIZE;
      rl_line_buffer = xrealloc (rl_line_buffer, rl_line_buffer_len);
    }

  the_line = rl_line_buffer;
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
  register int i, l = strlen (string);

  if (rl_end + l >= rl_line_buffer_len)
    rl_extend_line_buffer (rl_end + l);

  for (i = rl_end; i >= rl_point; i--)
    the_line[i + l] = the_line[i];
  strncpy (the_line + rl_point, string, l);

  /* Remember how to undo this if we aren't undoing something. */
  if (!doing_an_undo)
    {
      /* If possible and desirable, concatenate the undos. */
      if ((l == 1) &&
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
  return l;
}

/* Delete the string between FROM and TO.  FROM is
   inclusive, TO is not. */
rl_delete_text (from, to)
     int from, to;
{
  register char *text;
  register int diff, i;

  /* Fix it if the caller is confused. */
  if (from > to)
    {
      int t = from;
      from = to;
      to = t;
    }

  if (to > rl_end)
    to = rl_end;

  text = rl_copy_text (from, to);

  /* Some versions of strncpy() can't handle overlapping arguments. */
  diff = to - from;
  for (i = from; i < rl_end - diff; i++)
    the_line[i] = the_line[i + diff];

  /* Remember how to undo this delete. */
  if (!doing_an_undo)
    rl_add_undo (UNDO_DELETE, from, to, text);
  else
    free (text);

  rl_end -= diff;
  the_line[rl_end] = '\0';
  return (diff);
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
rl_forward (count, key)
     int count, key;
{
  if (count < 0)
    rl_backward (-count);
  else if (count > 0)
    {
      int end = rl_point + count;
#if defined (VI_MODE)
      int lend = rl_end - (rl_editing_mode == vi_mode);
#else
      int lend = rl_end;
#endif

      if (end > lend)
	{
	  rl_point = lend;
	  ding ();
	}
      else
	rl_point = end;
    }
  return 0;
}

/* Move backward COUNT characters. */
rl_backward (count, key)
     int count, key;
{
  if (count < 0)
    rl_forward (-count);
  else if (count > 0)
    {
      if (rl_point < count)
	{
	  rl_point = 0;
	  ding ();
	}
      else
        rl_point -= count;
    }
  return 0;
}

/* Move to the beginning of the line. */
rl_beg_of_line (count, key)
     int count, key;
{
  rl_point = 0;
  return 0;
}

/* Move to the end of the line. */
rl_end_of_line (count, key)
     int count, key;
{
  rl_point = rl_end;
  return 0;
}

/* Move forward a word.  We do what Emacs does. */
rl_forward_word (count, key)
     int count, key;
{
  int c;

  if (count < 0)
    {
      rl_backward_word (-count);
      return 0;
    }

  while (count)
    {
      if (rl_point == rl_end)
	return 0;

      /* If we are not in a word, move forward until we are in one.
	 Then, move forward until we hit a non-alphabetic character. */
      c = the_line[rl_point];
      if (!alphabetic (c))
	{
	  while (++rl_point < rl_end)
	    {
	      c = the_line[rl_point];
	      if (alphabetic (c))
		break;
	    }
	}
      if (rl_point == rl_end)
	return 0;
      while (++rl_point < rl_end)
	{
	  c = the_line[rl_point];
	  if (!alphabetic (c))
	    break;
	}
      --count;
    }
  return 0;
}

/* Move backward a word.  We do what Emacs does. */
rl_backward_word (count, key)
     int count, key;
{
  int c;

  if (count < 0)
    {
      rl_forward_word (-count);
      return 0;
    }

  while (count)
    {
      if (!rl_point)
	return 0;

      /* Like rl_forward_word (), except that we look at the characters
	 just before point. */

      c = the_line[rl_point - 1];
      if (!alphabetic (c))
	{
	  while (--rl_point)
	    {
	      c = the_line[rl_point - 1];
	      if (alphabetic (c))
		break;
	    }
	}

      while (rl_point)
	{
	  c = the_line[rl_point - 1];
	  if (!alphabetic (c))
	    break;
	  else
	    --rl_point;
	}
      --count;
    }
  return 0;
}

/* Clear the current line.  Numeric argument to C-l does this. */
rl_refresh_line ()
{
  int curr_line, nleft;

  /* Find out whether or not there might be invisible characters in the
     editing buffer. */
  if (rl_display_prompt == rl_prompt)
    nleft = _rl_last_c_pos - screenwidth - rl_visible_prompt_length;
  else
    nleft = _rl_last_c_pos - screenwidth;

  if (nleft > 0)
    curr_line = 1 + nleft / screenwidth;
  else
    curr_line = 0;

  _rl_move_vert (curr_line);
  _rl_move_cursor_relative (0, the_line);   /* XXX is this right */

#if defined (__GO32__)
  {
    int row, col, width, row_start;

    ScreenGetCursor (&row, &col);
    width = ScreenCols ();
    row_start = ScreenPrimary + (row * width);
    memset (row_start + col, 0, (width - col) * 2);
  }
#else /* !__GO32__ */
  if (term_clreol)
    tputs (term_clreol, 1, _rl_output_character_function);
#endif /* !__GO32__ */

  rl_forced_update_display ();
  rl_display_fixed = 1;

  return 0;
}

/* C-l typed to a line without quoting clears the screen, and then reprints
   the prompt and the current input line.  Given a numeric arg, redraw only
   the current line. */
rl_clear_screen (count, key)
     int count, key;
{
  if (rl_explicit_arg)
    {
      rl_refresh_line ();
      return 0;
    }

#if !defined (__GO32__)
  if (term_clrpag)
    tputs (term_clrpag, 1, _rl_output_character_function);
  else
#endif /* !__GO32__ */
    crlf ();

  rl_forced_update_display ();
  rl_display_fixed = 1;

  return 0;
}

rl_arrow_keys (count, c)
     int count, c;
{
  int ch;

  ch = rl_read_key ();

  switch (to_upper (ch))
    {
    case 'A':
      rl_get_previous_history (count);
      break;

    case 'B':
      rl_get_next_history (count);
      break;

    case 'C':
      rl_forward (count);
      break;

    case 'D':
      rl_backward (count);
      break;

    default:
      ding ();
    }
  return 0;
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
    return 0;

  /* If we can optimize, then do it.  But don't let people crash
     readline because of extra large arguments. */
  if (count > 1 && count < 1024)
    {
      string = xmalloc (1 + count);

      for (i = 0; i < count; i++)
	string[i] = c;

      string[i] = '\0';
      rl_insert_text (string);
      free (string);

      return 0;
    }

  if (count > 1024)
    {
      int decreaser;
      char str[1024+1];

      for (i = 0; i < 1024; i++)
	str[i] = c;

      while (count)
	{
	  decreaser = (count > 1024 ? 1024 : count);
	  str[decreaser] = '\0';
	  rl_insert_text (str);
	  count -= decreaser;
	}

      return 0;
    }

  /* We are inserting a single character.
     If there is pending input, then make a string of all of the
     pending characters that are bound to rl_insert, and insert
     them all. */
  if (any_typein)
    {
      int key = 0, t;

      i = 0;
      string = xmalloc (ibuffer_len + 1);
      string[i++] = c;

      while ((t = rl_get_char (&key)) &&
	     (_rl_keymap[key].type == ISFUNC &&
	      _rl_keymap[key].function == rl_insert))
	string[i++] = key;

      if (t)
	rl_unget_char (key);

      string[i] = '\0';
      rl_insert_text (string);
      free (string);
    }
  else
    {
      /* Inserting a single character. */
      char str[2];

      str[1] = '\0';
      str[0] = c;
      rl_insert_text (str);
    }
  return 0;
}

/* Insert the next typed character verbatim. */
rl_quoted_insert (count, key)
     int count, key;
{
  int c;

  c = rl_read_key ();
  return (rl_insert (count, c));  
}

/* Insert a tab character. */
rl_tab_insert (count, key)
     int count, key;
{
  return (rl_insert (count, '\t'));
}

/* What to do when a NEWLINE is pressed.  We accept the whole line.
   KEY is the key that invoked this command.  I guess it could have
   meaning in the future. */
rl_newline (count, key)
     int count, key;
{
  rl_done = 1;

#if defined (VI_MODE)
  _rl_vi_done_inserting ();
  _rl_vi_reset_last ();

#endif /* VI_MODE */

  if (readline_echoing_p)
    _rl_update_final ();
  return 0;
}

rl_clean_up_for_exit ()
{
  if (readline_echoing_p)
    {
      _rl_move_vert (_rl_vis_botlin);
      _rl_vis_botlin = 0;
      fflush (out_stream);
      rl_restart_output ();
    }
  return 0;
}

/* What to do for some uppercase characters, like meta characters,
   and some characters appearing in emacs_ctlx_keymap.  This function
   is just a stub, you bind keys to it and the code in _rl_dispatch ()
   is special cased. */
rl_do_lowercase_version (ignore1, ignore2)
     int ignore1, ignore2;
{
  return 0;
}

/* Rubout the character behind point. */
rl_rubout (count, key)
     int count, key;
{
  if (count < 0)
    {
      rl_delete (-count);
      return 0;
    }

  if (!rl_point)
    {
      ding ();
      return -1;
    }

  if (count > 1 || rl_explicit_arg)
    {
      int orig_point = rl_point;
      rl_backward (count);
      rl_kill_text (orig_point, rl_point);
    }
  else
    {
      int c = the_line[--rl_point];
      rl_delete_text (rl_point, rl_point + 1);

      if (rl_point == rl_end && isprint (c) && _rl_last_c_pos)
	{
	  int l;
	  l = rl_character_len (c, rl_point);
	  _rl_erase_at_end_of_line (l);
	}
    }
  return 0;
}

/* Delete the character under the cursor.  Given a numeric argument,
   kill that many characters instead. */
rl_delete (count, invoking_key)
     int count, invoking_key;
{
  if (count < 0)
    {
      return (rl_rubout (-count));
    }

  if (rl_point == rl_end)
    {
      ding ();
      return -1;
    }

  if (count > 1 || rl_explicit_arg)
    {
      int orig_point = rl_point;
      rl_forward (count);
      rl_kill_text (orig_point, rl_point);
      rl_point = orig_point;
      return 0;
    }
  else
    return (rl_delete_text (rl_point, rl_point + 1));
  
}

/* Delete all spaces and tabs around point. */
rl_delete_horizontal_space (count, ignore)
     int count, ignore;
{
  int start = rl_point;

  while (rl_point && whitespace (the_line[rl_point - 1]))
    rl_point--;

  start = rl_point;

  while (rl_point < rl_end && whitespace (the_line[rl_point]))
    rl_point++;

  if (start != rl_point)
    {
      rl_delete_text (start, rl_point);
      rl_point = start;
    }
  return 0;
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
rl_unix_word_rubout (count, key)
     int count, key;
{
  if (!rl_point)
    ding ();
  else
    {
      int orig_point = rl_point;
      if (count <= 0)
	count = 1;

      while (count--)
	{
	  while (rl_point && whitespace (the_line[rl_point - 1]))
	    rl_point--;

	  while (rl_point && !whitespace (the_line[rl_point - 1]))
	    rl_point--;
	}

      rl_kill_text (orig_point, rl_point);
    }
  return 0;
}

/* Here is C-u doing what Unix does.  You don't *have* to use these
   key-bindings.  We have a choice of killing the entire line, or
   killing from where we are to the start of the line.  We choose the
   latter, because if you are a Unix weenie, then you haven't backspaced
   into the line at all, and if you aren't, then you know what you are
   doing. */
rl_unix_line_discard (count, key)
     int count, key;
{
  if (!rl_point)
    ding ();
  else
    {
      rl_kill_text (rl_point, 0);
      rl_point = 0;
    }
  return 0;
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

static int rl_change_case ();

/* Uppercase the word at point. */
rl_upcase_word (count, key)
     int count, key;
{
  return (rl_change_case (count, UpCase));
}

/* Lowercase the word at point. */
rl_downcase_word (count, key)
     int count, key;
{
  return (rl_change_case (count, DownCase));
}

/* Upcase the first letter, downcase the rest. */
rl_capitalize_word (count, key)
     int count, key;
{
 return (rl_change_case (count, CapCase));
}

/* The meaty function.
   Change the case of COUNT words, performing OP on them.
   OP is one of UpCase, DownCase, or CapCase.
   If a negative argument is given, leave point where it started,
   otherwise, leave it where it moves to. */
static int
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
	  ding ();
	  return -1;
	}
    }
  rl_point = end;
  return 0;
}

/* **************************************************************** */
/*								    */
/*			Transposition				    */
/*								    */
/* **************************************************************** */

/* Transpose the words at point. */
rl_transpose_words (count, key)
     int count, key;
{
  char *word1, *word2;
  int w1_beg, w1_end, w2_beg, w2_end;
  int orig_point = rl_point;

  if (!count)
    return 0;

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
      return -1;
    }

  /* Get the text of the words. */
  word1 = rl_copy_text (w1_beg, w1_end);
  word2 = rl_copy_text (w2_beg, w2_end);

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
  free (word1);
  free (word2);

  return 0;
}

/* Transpose the characters at point.  If point is at the end of the line,
   then transpose the characters before point. */
rl_transpose_chars (count, key)
     int count, key;
{
  char dummy[2];

  if (!count)
    return 0;

  if (!rl_point || rl_end < 2)
    {
      ding ();
      return -1;
    }

  rl_begin_undo_group ();

  if (rl_point == rl_end)
    {
      --rl_point;
      count = 1;
    }
  rl_point--;

  dummy[0] = the_line[rl_point];
  dummy[1] = '\0';

  rl_delete_text (rl_point, rl_point + 1);

  rl_point += count;
  if (rl_point > rl_end)
    rl_point = rl_end;
  else if (rl_point < 0)
    rl_point = 0;
  rl_insert_text (dummy);

  rl_end_undo_group ();
  return 0;
}

/* **************************************************************** */
/*								    */
/*			Undo, and Undoing			    */
/*								    */
/* **************************************************************** */

/* The current undo list for THE_LINE. */
UNDO_LIST *rl_undo_list = (UNDO_LIST *)NULL;

/* Remember how to undo something.  Concatenate some undos if that
   seems right. */
void
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
void
free_undo_list ()
{
  while (rl_undo_list)
    {
      UNDO_LIST *release = rl_undo_list;
      rl_undo_list = rl_undo_list->next;

      if (release->what == UNDO_DELETE)
	free (release->text);

      free (release);
    }
  rl_undo_list = (UNDO_LIST *)NULL;
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
      ding ();
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
int
rl_begin_undo_group ()
{
  rl_add_undo (UNDO_BEGIN, 0, 0, 0);
  return 0;
}

/* End an undo group started with rl_begin_undo_group (). */
int
rl_end_undo_group ()
{
  rl_add_undo (UNDO_END, 0, 0, 0);
  return 0;
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
      char *temp = rl_copy_text (start, end);
      rl_begin_undo_group ();
      rl_add_undo (UNDO_DELETE, start, end, temp);
      rl_add_undo (UNDO_INSERT, start, end, (char *)NULL);
      rl_end_undo_group ();
    }
  return 0;
}

/* Revert the current line to its previous state. */
int
rl_revert_line (count, key)
     int count, key;
{
  if (!rl_undo_list)
    ding ();
  else
    {
      while (rl_undo_list)
	rl_do_undo ();
    }
  return 0;
}

/* Do some undoing of things that were done. */
int
rl_undo_command (count, key)
     int count, key;
{
  if (count < 0)
    return 0;	/* Nothing to do. */

  while (count)
    {
      if (rl_do_undo ())
	count--;
      else
	{
	  ding ();
	  break;
	}
    }
  return 0;
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
static void
start_using_history ()
{
  using_history ();
  if (saved_line_for_history)
    _rl_free_history_entry (saved_line_for_history);

  saved_line_for_history = (HIST_ENTRY *)NULL;
}

/* Free the contents (and containing structure) of a HIST_ENTRY. */
void
_rl_free_history_entry (entry)
     HIST_ENTRY *entry;
{
  if (!entry)
    return;
  if (entry->line)
    free (entry->line);
  free (entry);
}

/* Perhaps put back the current line if it has changed. */
maybe_replace_line ()
{
  HIST_ENTRY *temp = current_history ();

  /* If the current line has changed, save the changes. */
  if (temp && ((UNDO_LIST *)(temp->data) != rl_undo_list))
    {
      temp = replace_history_entry (where_history (), the_line, rl_undo_list);
      free (temp->line);
      free (temp);
    }
  return 0;
}

/* Put back the saved_line_for_history if there is one. */
maybe_unsave_line ()
{
  if (saved_line_for_history)
    {
      int line_len;

      line_len = strlen (saved_line_for_history->line);

      if (line_len >= rl_line_buffer_len)
	rl_extend_line_buffer (line_len);

      strcpy (the_line, saved_line_for_history->line);
      rl_undo_list = (UNDO_LIST *)saved_line_for_history->data;
      _rl_free_history_entry (saved_line_for_history);
      saved_line_for_history = (HIST_ENTRY *)NULL;
      rl_end = rl_point = strlen (the_line);
    }
  else
    ding ();
  return 0;
}

/* Save the current line in saved_line_for_history. */
maybe_save_line ()
{
  if (!saved_line_for_history)
    {
      saved_line_for_history = (HIST_ENTRY *)xmalloc (sizeof (HIST_ENTRY));
      saved_line_for_history->line = savestring (the_line);
      saved_line_for_history->data = (char *)rl_undo_list;
    }
  return 0;
}

/* **************************************************************** */
/*								    */
/*			History Commands			    */
/*								    */
/* **************************************************************** */

/* Meta-< goes to the start of the history. */
rl_beginning_of_history (count, key)
     int count, key;
{
  return (rl_get_previous_history (1 + where_history ()));
}

/* Meta-> goes to the end of the history.  (The current line). */
rl_end_of_history (count, key)
     int count, key;
{
  maybe_replace_line ();
  using_history ();
  maybe_unsave_line ();
  return 0;
}

/* Move down to the next history line. */
rl_get_next_history (count, key)
     int count, key;
{
  HIST_ENTRY *temp = (HIST_ENTRY *)NULL;

  if (count < 0)
    return (rl_get_previous_history (-count));

  if (!count)
    return 0;

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
      int line_len;

      line_len = strlen (temp->line);

      if (line_len >= rl_line_buffer_len)
	rl_extend_line_buffer (line_len);

      strcpy (the_line, temp->line);
      rl_undo_list = (UNDO_LIST *)temp->data;
      rl_end = rl_point = strlen (the_line);
#if defined (VI_MODE)
      if (rl_editing_mode == vi_mode)
	rl_point = 0;
#endif /* VI_MODE */
    }
  return 0;
}

/* Get the previous item out of our interactive history, making it the current
   line.  If there is no previous history, just ding. */
rl_get_previous_history (count, key)
     int count, key;
{
  HIST_ENTRY *old_temp = (HIST_ENTRY *)NULL;
  HIST_ENTRY *temp = (HIST_ENTRY *)NULL;

  if (count < 0)
    return (rl_get_next_history (-count));

  if (!count)
    return 0;

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
      int line_len;

      line_len = strlen (temp->line);

      if (line_len >= rl_line_buffer_len)
	rl_extend_line_buffer (line_len);

      strcpy (the_line, temp->line);
      rl_undo_list = (UNDO_LIST *)temp->data;
      rl_end = rl_point = line_len;

#if defined (VI_MODE)
      if (rl_editing_mode == vi_mode)
	rl_point = 0;
#endif /* VI_MODE */
    }
  return 0;
}

/* Make C be the next command to be executed. */
rl_execute_next (c)
     int c;
{
  rl_pending_input = c;
  return 0;
}

/* **************************************************************** */
/*								    */
/*		   The Mark and the Region.			    */
/*								    */
/* **************************************************************** */

/* Set the mark at POSITION. */
rl_set_mark (position)
     int position;
{
  if (position > rl_end)
    return -1;

  rl_mark = position;
  return 0;
}

/* Exchange the position of mark and point. */
rl_exchange_mark_and_point (count, key)
     int count, key;
{
  if (rl_mark > rl_end)
    rl_mark = -1;

  if (rl_mark == -1)
    {
      ding ();
      return -1;
    }
  else
    {
      int temp = rl_point;

      rl_point = rl_mark;
      rl_mark = temp;
    }
  return 0;
}


/* **************************************************************** */
/*								    */
/*			Killing Mechanism			    */
/*								    */
/* **************************************************************** */

/* What we assume for a max number of kills. */
#define DEFAULT_MAX_KILLS 10

/* The real variable to look at to find out when to flush kills. */
int rl_max_kills =  DEFAULT_MAX_KILLS;

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
{
  return 0;
}

/* The way to kill something.  This appends or prepends to the last
   kill, if the last command was a kill command.  if FROM is less
   than TO, then the text is appended, otherwise prepended.  If the
   last command was not a kill command, then a new slot is made for
   this kill. */
rl_kill_text (from, to)
     int from, to;
{
  int slot;
  char *text;

  /* Is there anything to kill? */
  if (from == to)
    {
      last_command_was_kill++;
      return 0;
    }

  text = rl_copy_text (from, to);

  /* Delete the copied text from the line. */
  rl_delete_text (from, to);

  /* First, find the slot to work with. */
  if (!last_command_was_kill)
    {
      /* Get a new slot.  */
      if (!rl_kill_ring)
	{
	  /* If we don't have any defined, then make one. */
	  rl_kill_ring = (char **)
	    xmalloc (((rl_kill_ring_length = 1) + 1) * sizeof (char *));
	  rl_kill_ring[slot = 0] = (char *)NULL;
	}
      else
	{
	  /* We have to add a new slot on the end, unless we have
	     exceeded the max limit for remembering kills. */
	  slot = rl_kill_ring_length;
	  if (slot == rl_max_kills)
	    {
	      register int i;
	      free (rl_kill_ring[0]);
	      for (i = 0; i < slot; i++)
		rl_kill_ring[i] = rl_kill_ring[i + 1];
	    }
	  else
	    {
	      slot = rl_kill_ring_length += 1;
	      rl_kill_ring = (char **)xrealloc (rl_kill_ring, slot * sizeof (char *));
	    }
	  rl_kill_ring[--slot] = (char *)NULL;
	}
    }
  else
    slot = rl_kill_ring_length - 1;

  /* If the last command was a kill, prepend or append. */
  if (last_command_was_kill && rl_editing_mode != vi_mode)
    {
      char *old = rl_kill_ring[slot];
      char *new = xmalloc (1 + strlen (old) + strlen (text));

      if (from < to)
	{
	  strcpy (new, old);
	  strcat (new, text);
	}
      else
	{
	  strcpy (new, text);
	  strcat (new, old);
	}
      free (old);
      free (text);
      rl_kill_ring[slot] = new;
    }
  else
    {
      rl_kill_ring[slot] = text;
    }
  rl_kill_index = slot;
  last_command_was_kill++;
  return 0;
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
rl_kill_word (count, key)
     int count, key;
{
  int orig_point = rl_point;

  if (count < 0)
    return (rl_backward_kill_word (-count));
  else
    {
      rl_forward_word (count);

      if (rl_point != orig_point)
	rl_kill_text (orig_point, rl_point);

      rl_point = orig_point;
    }
  return 0;
}

/* Rubout the word before point, placing it on the kill ring. */
rl_backward_kill_word (count, ignore)
     int count, ignore;
{
  int orig_point = rl_point;

  if (count < 0)
    return (rl_kill_word (-count));
  else
    {
      rl_backward_word (count);

      if (rl_point != orig_point)
	rl_kill_text (orig_point, rl_point);
    }
  return 0;
}

/* Kill from here to the end of the line.  If DIRECTION is negative, kill
   back to the line start instead. */
rl_kill_line (direction, ignore)
     int direction, ignore;
{
  int orig_point = rl_point;

  if (direction < 0)
    return (rl_backward_kill_line (1));
  else
    {
      rl_end_of_line (1, ignore);
      if (orig_point != rl_point)
	rl_kill_text (orig_point, rl_point);
      rl_point = orig_point;
    }
  return 0;
}

/* Kill backwards to the start of the line.  If DIRECTION is negative, kill
   forwards to the line end instead. */
rl_backward_kill_line (direction, ignore)
     int direction, ignore;
{
  int orig_point = rl_point;

  if (direction < 0)
    return (rl_kill_line (1));
  else
    {
      if (!rl_point)
	ding ();
      else
	{
	  rl_beg_of_line (1, ignore);
	  rl_kill_text (orig_point, rl_point);
	}
    }
  return 0;
}

/* Kill the whole line, no matter where point is. */
rl_kill_full_line (count, ignore)
     int count, ignore;
{
  rl_begin_undo_group ();
  rl_point = 0;
  rl_kill_text (rl_point, rl_end);
  rl_end_undo_group ();
  return 0;
}

/* Yank back the last killed text.  This ignores arguments. */
rl_yank (count, ignore)
     int count, ignore;
{
  if (!rl_kill_ring)
    {
      rl_abort (count, ignore);
      return -1;
    }

  rl_set_mark (rl_point);
  rl_insert_text (rl_kill_ring[rl_kill_index]);
  return 0;
}

/* If the last command was yank, or yank_pop, and the text just
   before point is identical to the current kill item, then
   delete that text from the line, rotate the index down, and
   yank back some other text. */
rl_yank_pop (count, key)
     int count, key;
{
  int l;

  if (((rl_last_func != rl_yank_pop) && (rl_last_func != rl_yank)) ||
      !rl_kill_ring)
    {
      rl_abort (1, key);
      return -1;
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
      rl_yank (1, 0);
      return 0;
    }
  else
    {
      rl_abort (1, key);
      return -1;
    }
}

/* Yank the COUNTth argument from the previous history line. */
rl_yank_nth_arg (count, ignore)
     int count, ignore;
{
  register HIST_ENTRY *entry = previous_history ();
  char *arg;

  if (entry)
    next_history ();
  else
    {
      ding ();
      return -1;
    }

  arg = history_arg_extract (count, count, entry->line);
  if (!arg || !*arg)
    {
      ding ();
      return -1;
    }

  rl_begin_undo_group ();

#if defined (VI_MODE)
  /* Vi mode always inserts a space before yanking the argument, and it
     inserts it right *after* rl_point. */
  if (rl_editing_mode == vi_mode)
    {
      rl_vi_append_mode ();
      rl_insert_text (" ");
    }
#endif /* VI_MODE */

  rl_insert_text (arg);
  free (arg);

  rl_end_undo_group ();
  return 0;
}

/* Yank the last argument from the previous history line.  This `knows'
   how rl_yank_nth_arg treats a count of `$'.  With an argument, this
   behaves the same as rl_yank_nth_arg. */
int
rl_yank_last_arg (count, key)
     int count, key;
{
  if (rl_explicit_arg)
    return (rl_yank_nth_arg (count, key));
  else
    return (rl_yank_nth_arg ('$', key));
}

/* How to toggle back and forth between editing modes. */
rl_vi_editing_mode (count, key)
     int count, key;
{
#if defined (VI_MODE)
  rl_editing_mode = vi_mode;
  rl_vi_insertion_mode ();
  return 0;
#endif /* VI_MODE */
}

rl_emacs_editing_mode (count, key)
     int count, key;
{
  rl_editing_mode = emacs_mode;
  _rl_keymap = emacs_standard_keymap;
  return 0;
}


/* **************************************************************** */
/*								    */
/*			USG (System V) Support			    */
/*								    */
/* **************************************************************** */

int
rl_getc (stream)
     FILE *stream;
{
  int result;
  unsigned char c;

#if defined (__GO32__)
  if (isatty (0))
    return (getkey () & 0x7F);
#endif /* __GO32__ */

  while (1)
    {
      result = read (fileno (stream), &c, sizeof (unsigned char));

      if (result == sizeof (unsigned char))
	return (c);

      /* If zero characters are returned, then the file that we are
	 reading from is empty!  Return EOF in that case. */
      if (result == 0)
	return (EOF);

#if defined (EWOULDBLOCK)
      if (errno == EWOULDBLOCK)
	{
	  int flags;

	  if ((flags = fcntl (fileno (stream), F_GETFL, 0)) < 0)
	    return (EOF);
	  if (flags & O_NDELAY)
	    {
	      flags &= ~O_NDELAY;
	      fcntl (fileno (stream), F_SETFL, flags);
	      continue;
	    }
	  continue;
	}
#endif /* EWOULDBLOCK */

#if defined (_POSIX_VERSION) && defined (EAGAIN) && defined (O_NONBLOCK)
      if (errno == EAGAIN)
	{
	  int flags;

	  if ((flags = fcntl (fileno (stream), F_GETFL, 0)) < 0)
	    return (EOF);
	  if (flags & O_NONBLOCK)
	    {
	      flags &= ~O_NONBLOCK;
	      fcntl (fileno (stream), F_SETFL, flags);
	      continue;
	    }
	}
#endif /* _POSIX_VERSION && EAGAIN && O_NONBLOCK */

#if !defined (__GO32__)
      /* If the error that we received was SIGINT, then try again,
	 this is simply an interrupted system call to read ().
	 Otherwise, some error ocurred, also signifying EOF. */
      if (errno != EINTR)
	return (EOF);
#endif /* !__GO32__ */
    }
}

#if !defined (SHELL)
/* Backwards compatibilty, now that savestring has been removed from
   all `public' readline header files. */
char *
rl_savestring (s)
     char *s;
{
  return ((char *)strcpy (xmalloc (1 + (int)strlen (s)), (s)));
}
#endif

/* Function equivalents for the macros defined in chartypes.h. */
#undef uppercase_p
int
uppercase_p (c)
     int c;
{
  return (isupper (c));
}

#undef lowercase_p
int
lowercase_p (c)
     int c;
{
  return (islower (c));
}

#undef pure_alphabetic
int
pure_alphabetic (c)
     int c;
{
  return (isupper (c) || islower (c));
}

#undef digit_p
int
digit_p (c)
     int c;
{
  return (isdigit (c));
}

#undef to_lower
int
to_lower (c)
     int c;
{
  return (isupper (c) ? tolower (c) : c);
}

#undef to_upper
int
to_upper (c)
     int c;
{
  return (islower (c) ? toupper (c) : c);
}

#undef digit_value
int
digit_value (c)
     int c;
{
  return (isdigit (c) ? c - '0' : c);
}

#if defined (STATIC_MALLOC)

/* **************************************************************** */
/*								    */
/*			xmalloc and xrealloc ()		     	    */
/*								    */
/* **************************************************************** */

static void memory_error_and_abort ();

static char *
xmalloc (bytes)
     int bytes;
{
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
  char *temp;

  if (!pointer)
    temp = (char *)malloc (bytes);
  else
    temp = (char *)realloc (pointer, bytes);

  if (!temp)
    memory_error_and_abort ();

  return (temp);
}

static void
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

#if defined (TEST)

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

      if (strcmp (temp, "list") == 0)
	{
	  HIST_ENTRY **list = history_list ();
	  register int i;
	  if (list)
	    {
	      for (i = 0; list[i]; i++)
		{
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
