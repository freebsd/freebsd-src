/* bind.c -- key binding and startup file support for the readline library. */

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

#include "posixstat.h"

/* System-specific feature definitions and include files. */
#include "rldefs.h"

/* Some standard library routines. */
#include "readline.h"
#include "history.h"

#if !defined (strchr) && !defined (__STDC__)
extern char *strchr (), *strrchr ();
#endif /* !strchr && !__STDC__ */

extern int _rl_horizontal_scroll_mode;
extern int _rl_mark_modified_lines;
extern int _rl_bell_preference;
extern int _rl_meta_flag;
extern int rl_blink_matching_paren;
extern int _rl_convert_meta_chars_to_ascii;
extern int _rl_output_meta_chars;
extern int _rl_complete_show_all;
#if defined (VISIBLE_STATS)
extern int rl_visible_stats;
#endif /* VISIBLE_STATS */
extern int rl_complete_with_tilde_expansion;
extern int rl_completion_query_items;
#if defined (VI_MODE)
extern char *rl_vi_comment_begin;
#endif

extern int rl_explicit_arg;
extern int rl_editing_mode;
extern unsigned short _rl_parsing_conditionalized_out;
extern Keymap _rl_keymap;

extern char *possible_control_prefixes[], *possible_meta_prefixes[];

extern char **rl_funmap_names ();

/* Forward declarations */
void rl_set_keymap_from_edit_mode ();

static int glean_key_from_name ();
#if !defined (BSD386) && !defined (NetBSD) && \
    !defined (FreeBSD) && !defined (_386BSD)
static int stricmp (), strnicmp ();
#else
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#if defined (STATIC_MALLOC)
static char *xmalloc (), *xrealloc ();
#else
extern char *xmalloc (), *xrealloc ();
#endif /* STATIC_MALLOC */

/* **************************************************************** */
/*								    */
/*			Binding keys				    */
/*								    */
/* **************************************************************** */

/* rl_add_defun (char *name, Function *function, int key)
   Add NAME to the list of named functions.  Make FUNCTION be the function
   that gets called.  If KEY is not -1, then bind it. */
rl_add_defun (name, function, key)
     char *name;
     Function *function;
     int key;
{
  if (key != -1)
    rl_bind_key (key, function);
  rl_add_funmap_entry (name, function);
  return 0;
}

/* Bind KEY to FUNCTION.  Returns non-zero if KEY is out of range. */
int
rl_bind_key (key, function)
     int key;
     Function *function;
{
  if (key < 0)
    return (key);

  if (META_CHAR (key) && _rl_convert_meta_chars_to_ascii)
    {
      if (_rl_keymap[ESC].type == ISKMAP)
	{
	  Keymap escmap;

	  escmap = FUNCTION_TO_KEYMAP (_rl_keymap, ESC);
	  key = UNMETA (key);
	  escmap[key].type = ISFUNC;
	  escmap[key].function = function;
	  return (0);
	}
      return (key);
    }

  _rl_keymap[key].type = ISFUNC;
  _rl_keymap[key].function = function;
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
  Keymap oldmap = _rl_keymap;

  _rl_keymap = map;
  result = rl_bind_key (key, function);
  _rl_keymap = oldmap;
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
  return (rl_generic_bind (ISFUNC, keyseq, function, map));
}

/* Bind the key sequence represented by the string KEYSEQ to
   the string of characters MACRO.  This makes new keymaps as
   necessary.  The initial place to do bindings is in MAP. */
rl_macro_bind (keyseq, macro, map)
     char *keyseq, *macro;
     Keymap map;
{
  char *macro_keys;
  int macro_keys_len;

  macro_keys = (char *)xmalloc ((2 * strlen (macro)) + 1);

  if (rl_translate_keyseq (macro, macro_keys, &macro_keys_len))
    {
      free (macro_keys);
      return -1;
    }
  rl_generic_bind (ISMACR, keyseq, macro_keys, map);
  return 0;
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

  /* If no keys to bind to, exit right away. */
  if (!keyseq || !*keyseq)
    {
      if (type == ISMACR)
	free (data);
      return -1;
    }

  keys = xmalloc (1 + (2 * strlen (keyseq)));

  /* Translate the ASCII representation of KEYSEQ into an array of
     characters.  Stuff the characters into KEYS, and the length of
     KEYS into KEYS_LEN. */
  if (rl_translate_keyseq (keyseq, keys, &keys_len))
    {
      free (keys);
      return -1;
    }

  /* Bind keys, making new keymaps as necessary. */
  for (i = 0; i < keys_len; i++)
    {
      int ic = (int) ((unsigned char)keys[i]);

      if (_rl_convert_meta_chars_to_ascii && META_CHAR (ic))
	{
	  ic = UNMETA (ic);
	  if (map[ESC].type == ISKMAP)
	    map = FUNCTION_TO_KEYMAP (map, ESC);
	}

      if ((i + 1) < keys_len)
	{
	  if (map[ic].type != ISKMAP)
	    {
	      if (map[ic].type == ISMACR)
		free ((char *)map[ic].function);

	      map[ic].type = ISKMAP;
	      map[ic].function = KEYMAP_TO_FUNCTION (rl_make_bare_keymap());
	    }
	  map = FUNCTION_TO_KEYMAP (map, ic);
	}
      else
	{
	  if (map[ic].type == ISMACR)
	    free ((char *)map[ic].function);

	  map[ic].function = KEYMAP_TO_FUNCTION (data);
	  map[ic].type = type;
	}
    }
  free (keys);
  return 0;
}

/* Translate the ASCII representation of SEQ, stuffing the values into ARRAY,
   an array of characters.  LEN gets the final length of ARRAY.  Return
   non-zero if there was an error parsing SEQ. */
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
		  /* Special hack for C-?... */
		  if (seq[i] == '?')
		    array[l++] = RUBOUT;
		  else
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

  *len = l;
  array[l] = '\0';
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

  rl_initialize_funmap ();

  for (i = 0; funmap[i]; i++)
    if (stricmp (funmap[i]->name, string) == 0)
      return (funmap[i]->function);
  return ((Function *)NULL);
}

/* Return the function (or macro) definition which would be invoked via
   KEYSEQ if executed in MAP.  If MAP is NULL, then the current keymap is
   used.  TYPE, if non-NULL, is a pointer to an int which will receive the
   type of the object pointed to.  One of ISFUNC (function), ISKMAP (keymap),
   or ISMACR (macro). */
Function *
rl_function_of_keyseq (keyseq, map, type)
     char *keyseq;
     Keymap map;
     int *type;
{
  register int i;

  if (!map)
    map = _rl_keymap;

  for (i = 0; keyseq && keyseq[i]; i++)
    {
      int ic = keyseq[i];

      if (META_CHAR (ic) && _rl_convert_meta_chars_to_ascii)
	{
	  if (map[ESC].type != ISKMAP)
	    {
	      if (type)
		*type = map[ESC].type;

	      return (map[ESC].function);
	    }
	  else
	    {
	      map = FUNCTION_TO_KEYMAP (map, ESC);
	      ic = UNMETA (ic);
	    }
	}

      if (map[ic].type == ISKMAP)
	{
	  /* If this is the last key in the key sequence, return the
	     map. */
	  if (!keyseq[i + 1])
	    {
	      if (type)
		*type = ISKMAP;

	      return (map[ic].function);
	    }
	  else
	    map = FUNCTION_TO_KEYMAP (map, ic);
	}
      else
	{
	  if (type)
	    *type = map[ic].type;

	  return (map[ic].function);
	}
    }
  return ((Function *) NULL);
}

/* The last key bindings file read. */
static char *last_readline_init_file = (char *)NULL;

/* Re-read the current keybindings file. */
rl_re_read_init_file (count, ignore)
     int count, ignore;
{
  int r;
  r = rl_read_init_file ((char *)NULL);
  rl_set_keymap_from_edit_mode ();
  return r;
}

/* Do key bindings from a file.  If FILENAME is NULL it defaults
   to the first non-null filename from this list:
     1. the filename used for the previous call
     2. the value of the shell variable `INPUTRC'
     3. ~/.inputrc
   If the file existed and could be opened and read, 0 is returned,
   otherwise errno is returned. */
int
rl_read_init_file (filename)
     char *filename;
{
  register int i;
  char *buffer, *openname, *line, *end;
  struct stat finfo;
  int file;

  /* Default the filename. */
  if (!filename)
    {
      filename = last_readline_init_file;
      if (!filename)
        filename = getenv ("INPUTRC");
      if (!filename)
	filename = DEFAULT_INPUTRC;
    }

  openname = tilde_expand (filename);

  if ((stat (openname, &finfo) < 0) ||
      (file = open (openname, O_RDONLY, 0666)) < 0)
    {
      free (openname);
      return (errno);
    }
  else
    free (openname);

  if (filename != last_readline_init_file)
    {
      if (last_readline_init_file)
	free (last_readline_init_file);

      last_readline_init_file = savestring (filename);
    }

  /* Read the file into BUFFER. */
  buffer = (char *)xmalloc ((int)finfo.st_size + 1);
  i = read (file, buffer, finfo.st_size);
  close (file);

  if (i != finfo.st_size)
    return (errno);

  /* Loop over the lines in the file.  Lines that start with `#' are
     comments; all other lines are commands for readline initialization. */
  line = buffer;
  end = buffer + finfo.st_size;
  while (line < end)
    {
      /* Find the end of this line. */
      for (i = 0; line + i != end && line[i] != '\n'; i++);

      /* Mark end of line. */
      line[i] = '\0';

      /* Skip leading whitespace. */
      while (*line && whitespace (*line))
	line++;

      /* If the line is not a comment, then parse it. */
      if (*line && *line != '#')
	rl_parse_and_bind (line);

      /* Move to the next line. */
      line += i + 1;
    }
  free (buffer);
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

/* Push _rl_parsing_conditionalized_out, and set parser state based
   on ARGS. */
static int
parser_if (args)
     char *args;
{
  register int i;

  /* Push parser state. */
  if (if_stack_depth + 1 >= if_stack_size)
    {
      if (!if_stack)
	if_stack = (unsigned char *)xmalloc (if_stack_size = 20);
      else
	if_stack = (unsigned char *)xrealloc (if_stack, if_stack_size += 20);
    }
  if_stack[if_stack_depth++] = _rl_parsing_conditionalized_out;

  /* If parsing is turned off, then nothing can turn it back on except
     for finding the matching endif.  In that case, return right now. */
  if (_rl_parsing_conditionalized_out)
    return 0;

  /* Isolate first argument. */
  for (i = 0; args[i] && !whitespace (args[i]); i++);

  if (args[i])
    args[i++] = '\0';

  /* Handle "if term=foo" and "if mode=emacs" constructs.  If this
     isn't term=foo, or mode=emacs, then check to see if the first
     word in ARGS is the same as the value stored in rl_readline_name. */
  if (rl_terminal_name && strnicmp (args, "term=", 5) == 0)
    {
      char *tem, *tname;

      /* Terminals like "aaa-60" are equivalent to "aaa". */
      tname = savestring (rl_terminal_name);
      tem = strchr (tname, '-');
      if (tem)
	*tem = '\0';

      /* Test the `long' and `short' forms of the terminal name so that
	 if someone has a `sun-cmd' and does not want to have bindings
	 that will be executed if the terminal is a `sun', they can put
	 `$if term=sun-cmd' into their .inputrc. */
      if ((stricmp (args + 5, tname) == 0) ||
	  (stricmp (args + 5, rl_terminal_name) == 0))
	_rl_parsing_conditionalized_out = 0;
      else
	_rl_parsing_conditionalized_out = 1;

      free (tname);
    }
#if defined (VI_MODE)
  else if (strnicmp (args, "mode=", 5) == 0)
    {
      int mode;

      if (stricmp (args + 5, "emacs") == 0)
	mode = emacs_mode;
      else if (stricmp (args + 5, "vi") == 0)
	mode = vi_mode;
      else
	mode = no_mode;

      if (mode == rl_editing_mode)
	_rl_parsing_conditionalized_out = 0;
      else
	_rl_parsing_conditionalized_out = 1;
    }
#endif /* VI_MODE */
  /* Check to see if the first word in ARGS is the same as the
     value stored in rl_readline_name. */
  else if (stricmp (args, rl_readline_name) == 0)
    _rl_parsing_conditionalized_out = 0;
  else
    _rl_parsing_conditionalized_out = 1;
  return 0;
}

/* Invert the current parser state if there is anything on the stack. */
static int
parser_else (args)
     char *args;
{
  register int i;

  if (!if_stack_depth)
    {
      /* Error message? */
      return 0;
    }

  /* Check the previous (n - 1) levels of the stack to make sure that
     we haven't previously turned off parsing. */
  for (i = 0; i < if_stack_depth - 1; i++)
    if (if_stack[i] == 1)
      return 0;

  /* Invert the state of parsing if at top level. */
  _rl_parsing_conditionalized_out = !_rl_parsing_conditionalized_out;
  return 0;
}

/* Terminate a conditional, popping the value of
   _rl_parsing_conditionalized_out from the stack. */
static int
parser_endif (args)
     char *args;
{
  if (if_stack_depth)
    _rl_parsing_conditionalized_out = if_stack[--if_stack_depth];
  else
    {
      /* *** What, no error message? *** */
    }
  return 0;
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

static int substring_member_of_array ();

/* Read the binding command from STRING and perform it.
   A key binding command looks like: Keyname: function-name\0,
   a variable binding command looks like: set variable value.
   A new-style keybinding looks like "\C-x\C-x": exchange-point-and-mark. */
rl_parse_and_bind (string)
     char *string;
{
  char *funname, *kname;
  register int c, i;
  int key, equivalency;

  while (string && whitespace (*string))
    string++;

  if (!string || !*string || *string == '#')
    return 0;

  /* If this is a parser directive, act on it. */
  if (*string == '$')
    {
      handle_parser_directive (&string[1]);
      return 0;
    }

  /* If we aren't supposed to be parsing right now, then we're done. */
  if (_rl_parsing_conditionalized_out)
    return 0;

  i = 0;
  /* If this keyname is a complex key expression surrounded by quotes,
     advance to after the matching close quote.  This code allows the
     backslash to quote characters in the key expression. */
  if (*string == '"')
    {
      int passc = 0;

      for (i = 1; c = string[i]; i++)
	{
	  if (passc)
	    {
	      passc = 0;
	      continue;
	    }

	  if (c == '\\')
	    {
	      passc++;
	      continue;
	    }

	  if (c == '"')
	    break;
	}
    }

  /* Advance to the colon (:) or whitespace which separates the two objects. */
  for (; (c = string[i]) && c != ':' && c != ' ' && c != '\t'; i++ );

  equivalency = (c == ':' && string[i + 1] == '=');

  /* Mark the end of the command (or keyname). */
  if (string[i])
    string[i++] = '\0';

  /* If doing assignment, skip the '=' sign as well. */
  if (equivalency)
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
      return 0;
    }

  /* Skip any whitespace between keyname and funname. */
  for (; string[i] && whitespace (string[i]); i++);
  funname = &string[i];

  /* Now isolate funname.
     For straight function names just look for whitespace, since
     that will signify the end of the string.  But this could be a
     macro definition.  In that case, the string is quoted, so skip
     to the matching delimiter.  We allow the backslash to quote the
     delimiter characters in the macro body. */
  /* This code exists to allow whitespace in macro expansions, which
     would otherwise be gobbled up by the next `for' loop.*/
  /* XXX - it may be desirable to allow backslash quoting only if " is
     the quoted string delimiter, like the shell. */
  if (*funname == '\'' || *funname == '"')
    {
      int delimiter = string[i++];
      int passc = 0;

      for (; c = string[i]; i++)
	{
	  if (passc)
	    {
	      passc = 0;
	      continue;
	    }

	  if (c == '\\')
	    {
	      passc = 1;
	      continue;
	    }

	  if (c == delimiter)
	    break;
	}
      if (c)
	i++;
    }

  /* Advance to the end of the string.  */
  for (; string[i] && !whitespace (string[i]); i++);

  /* No extra whitespace at the end of the string. */
  string[i] = '\0';

  /* Handle equivalency bindings here.  Make the left-hand side be exactly
     whatever the right-hand evaluates to, including keymaps. */
  if (equivalency)
    {
      return 0;
    }

  /* If this is a new-style key-binding, then do the binding with
     rl_set_key ().  Otherwise, let the older code deal with it. */
  if (*string == '"')
    {
      char *seq = xmalloc (1 + strlen (string));
      register int j, k = 0;
      int passc = 0;

      for (j = 1; string[j]; j++)
	{
	  /* Allow backslash to quote characters, but leave them in place.
	     This allows a string to end with a backslash quoting another
	     backslash, or with a backslash quoting a double quote.  The
	     backslashes are left in place for rl_translate_keyseq (). */
	  if (passc || (string[j] == '\\'))
	    {
	      seq[k++] = string[j];
	      passc = !passc;
	      continue;
	    }

	  if (string[j] == '"')
	    break;

	  seq[k++] = string[j];
	}
      seq[k] = '\0';

      /* Binding macro? */
      if (*funname == '\'' || *funname == '"')
	{
	  j = strlen (funname);

	  /* Remove the delimiting quotes from each end of FUNNAME. */
	  if (j && funname[j - 1] == *funname)
	    funname[j - 1] = '\0';

	  rl_macro_bind (seq, &funname[1], _rl_keymap);
	}
      else
	rl_set_key (seq, rl_named_function (funname), _rl_keymap);

      free (seq);
      return 0;
    }

  /* Get the actual character we want to deal with. */
  kname = strrchr (string, '-');
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

      rl_macro_bind (seq, &funname[1], _rl_keymap);
    }
#if defined (PREFIX_META_HACK)
  /* Ugly, but working hack to keep prefix-meta around. */
  else if (stricmp (funname, "prefix-meta") == 0)
    {
      char seq[2];

      seq[0] = key;
      seq[1] = '\0';
      rl_generic_bind (ISKMAP, seq, (char *)emacs_meta_keymap, _rl_keymap);
    }
#endif /* PREFIX_META_HACK */
  else
    rl_bind_key (key, rl_named_function (funname));
  return 0;
}

/* Simple structure for boolean readline variables (i.e., those that can
   have one of two values; either "On" or 1 for truth, or "Off" or 0 for
   false. */

static struct {
  char *name;
  int *value;
} boolean_varlist [] = {
  { "horizontal-scroll-mode",	&_rl_horizontal_scroll_mode },
  { "mark-modified-lines",	&_rl_mark_modified_lines },
  { "meta-flag",		&_rl_meta_flag },
  { "blink-matching-paren",	&rl_blink_matching_paren },
  { "convert-meta",		&_rl_convert_meta_chars_to_ascii },
  { "show-all-if-ambiguous",	&_rl_complete_show_all },
  { "output-meta",		&_rl_output_meta_chars },
#if defined (VISIBLE_STATS)
  { "visible-stats",		&rl_visible_stats },
#endif /* VISIBLE_STATS */
  { "expand-tilde",		&rl_complete_with_tilde_expansion },
  { (char *)NULL, (int *)NULL }
};

rl_variable_bind (name, value)
     char *name, *value;
{
  register int i;

  /* Check for simple variables first. */
  for (i = 0; boolean_varlist[i].name; i++)
    {
      if (stricmp (name, boolean_varlist[i].name) == 0)
	{
	  /* A variable is TRUE if the "value" is "on", "1" or "". */
	  if ((!*value) ||
	      (stricmp (value, "On") == 0) ||
	      (value[0] == '1' && value[1] == '\0'))
	    *boolean_varlist[i].value = 1;
	  else
	    *boolean_varlist[i].value = 0;
	  return 0;
	}
    }

  /* Not a boolean variable, so check for specials. */

  /* Editing mode change? */
  if (stricmp (name, "editing-mode") == 0)
    {
      if (strnicmp (value, "vi", 2) == 0)
	{
#if defined (VI_MODE)
	  _rl_keymap = vi_insertion_keymap;
	  rl_editing_mode = vi_mode;
#endif /* VI_MODE */
	}
      else if (strnicmp (value, "emacs", 5) == 0)
	{
	  _rl_keymap = emacs_standard_keymap;
	  rl_editing_mode = emacs_mode;
	}
    }

  /* Comment string change? */
  else if (stricmp (name, "comment-begin") == 0)
    {
#if defined (VI_MODE)
      if (*value)
	{
	  if (rl_vi_comment_begin)
	    free (rl_vi_comment_begin);

	  rl_vi_comment_begin = savestring (value);
	}
#endif /* VI_MODE */
    }
  else if (stricmp (name, "completion-query-items") == 0)
    {
      int nval = 100;
      if (*value)
	{
	  nval = atoi (value);
	  if (nval < 0)
	    nval = 0;
	}
      rl_completion_query_items = nval;
    }
  else if (stricmp (name, "keymap") == 0)
    {
      Keymap kmap;
      kmap = rl_get_keymap_by_name (value);
      if (kmap)
        rl_set_keymap (kmap);
    }
  else if (stricmp (name, "bell-style") == 0)
    {
      if (!*value)
        _rl_bell_preference = AUDIBLE_BELL;
      else
        {
          if (stricmp (value, "none") == 0 || stricmp (value, "off") == 0)
            _rl_bell_preference = NO_BELL;
          else if (stricmp (value, "audible") == 0 || stricmp (value, "on") == 0)
            _rl_bell_preference = AUDIBLE_BELL;
          else if (stricmp (value, "visible") == 0)
            _rl_bell_preference = VISIBLE_BELL;
        }
    }
  else if (stricmp (name, "prefer-visible-bell") == 0)
    {
      /* Backwards compatibility. */
      if (*value && (stricmp (value, "on") == 0 ||
		     (*value == '1' && !value[1])))
        _rl_bell_preference = VISIBLE_BELL;
      else
        _rl_bell_preference = AUDIBLE_BELL;
    }

  return 0;
}

/* Return the character which matches NAME.
   For example, `Space' returns ' '. */

typedef struct {
  char *name;
  int value;
} assoc_list;

static assoc_list name_key_alist[] = {
  { "DEL", 0x7f },
  { "ESC", '\033' },
  { "Escape", '\033' },
  { "LFD", '\n' },
  { "Newline", '\n' },
  { "RET", '\r' },
  { "Return", '\r' },
  { "Rubout", 0x7f },
  { "SPC", ' ' },
  { "Space", ' ' },
  { "Tab", 0x09 },
  { (char *)0x0, 0 }
};

static int
glean_key_from_name (name)
     char *name;
{
  register int i;

  for (i = 0; name_key_alist[i].name; i++)
    if (stricmp (name, name_key_alist[i].name) == 0)
      return (name_key_alist[i].value);

  return (*(unsigned char *)name);	/* XXX was return (*name) */
}

/* Auxiliary functions to manage keymaps. */
static struct {
  char *name;
  Keymap map;
} keymap_names[] = {
  { "emacs", emacs_standard_keymap },
  { "emacs-standard", emacs_standard_keymap },
  { "emacs-meta", emacs_meta_keymap },
  { "emacs-ctlx", emacs_ctlx_keymap },
#if defined (VI_MODE)
  { "vi", vi_movement_keymap },
  { "vi-move", vi_movement_keymap },
  { "vi-command", vi_movement_keymap },
  { "vi-insert", vi_insertion_keymap },
#endif /* VI_MODE */
  { (char *)0x0, (Keymap)0x0 }
};

Keymap
rl_get_keymap_by_name (name)
     char *name;
{
  register int i;

  for (i = 0; keymap_names[i].name; i++)
    if (strcmp (name, keymap_names[i].name) == 0)
      return (keymap_names[i].map);
  return ((Keymap) NULL);
}

void
rl_set_keymap (map)
     Keymap map;
{
  if (map)
    _rl_keymap = map;
}

Keymap
rl_get_keymap ()
{
  return (_rl_keymap);
}

void
rl_set_keymap_from_edit_mode ()
{
  if (rl_editing_mode == emacs_mode)
    _rl_keymap = emacs_standard_keymap;
#if defined (VI_MODE)
  else if (rl_editing_mode == vi_mode)
    _rl_keymap = vi_insertion_keymap;
#endif /* VI_MODE */
}

/* **************************************************************** */
/*								    */
/*		  Key Binding and Function Information		    */
/*								    */
/* **************************************************************** */

/* Each of the following functions produces information about the
   state of keybindings and functions known to Readline.  The info
   is always printed to rl_outstream, and in such a way that it can
   be read back in (i.e., passed to rl_parse_and_bind (). */

/* Print the names of functions known to Readline. */
void
rl_list_funmap_names (ignore)
     int ignore;
{
  register int i;
  char **funmap_names;

  funmap_names = rl_funmap_names ();

  if (!funmap_names)
    return;

  for (i = 0; funmap_names[i]; i++)
    fprintf (rl_outstream, "%s\n", funmap_names[i]);

  free (funmap_names);
}

/* Return a NULL terminated array of strings which represent the key
   sequences that are used to invoke FUNCTION in MAP. */
char **
rl_invoking_keyseqs_in_map (function, map)
     Function *function;
     Keymap map;
{
  register int key;
  char **result;
  int result_index, result_size;

  result = (char **)NULL;
  result_index = result_size = 0;

  for (key = 0; key < 128; key++)
    {
      switch (map[key].type)
	{
	case ISMACR:
	  /* Macros match, if, and only if, the pointers are identical.
	     Thus, they are treated exactly like functions in here. */
	case ISFUNC:
	  /* If the function in the keymap is the one we are looking for,
	     then add the current KEY to the list of invoking keys. */
	  if (map[key].function == function)
	    {
	      char *keyname = (char *)xmalloc (5);

	      if (CTRL_P (key))
		sprintf (keyname, "\\C-%c", to_lower (UNCTRL (key)));
	      else if (key == RUBOUT)
		sprintf (keyname, "\\C-?");
	      else if (key == '\\' || key == '"')
		{
		  keyname[0] = '\\';
		  keyname[1] = (char) key;
		  keyname[2] = '\0';
		}
	      else
		{
		  keyname[0] = (char) key;
		  keyname[1] = '\0';
		}

	      if (result_index + 2 > result_size)
		result = (char **) xrealloc
		  (result, (result_size += 10) * sizeof (char *));

	      result[result_index++] = keyname;
	      result[result_index] = (char *)NULL;
	    }
	  break;

	case ISKMAP:
	  {
	    char **seqs = (char **)NULL;

	    /* Find the list of keyseqs in this map which have FUNCTION as
	       their target.  Add the key sequences found to RESULT. */
	    if (map[key].function)
	      seqs =
	        rl_invoking_keyseqs_in_map (function, FUNCTION_TO_KEYMAP (map, key));

	    if (seqs)
	      {
		register int i;

		for (i = 0; seqs[i]; i++)
		  {
		    char *keyname = (char *)xmalloc (6 + strlen (seqs[i]));

		    if (key == ESC)
		      sprintf (keyname, "\\e");
		    else if (CTRL_P (key))
		      sprintf (keyname, "\\C-%c", to_lower (UNCTRL (key)));
		    else if (key == RUBOUT)
		      sprintf (keyname, "\\C-?");
		    else if (key == '\\' || key == '"')
		      {
			keyname[0] = '\\';
			keyname[1] = (char) key;
			keyname[2] = '\0';
		      }
		    else
		      {
			keyname[0] = (char) key;
			keyname[1] = '\0';
		      }

		    strcat (keyname, seqs[i]);
		    free (seqs[i]);

		    if (result_index + 2 > result_size)
		      result = (char **) xrealloc
			(result, (result_size += 10) * sizeof (char *));

		    result[result_index++] = keyname;
		    result[result_index] = (char *)NULL;
		  }

		free (seqs);
	      }
	  }
	  break;
	}
    }
  return (result);
}

/* Return a NULL terminated array of strings which represent the key
   sequences that can be used to invoke FUNCTION using the current keymap. */
char **
rl_invoking_keyseqs (function)
     Function *function;
{
  return (rl_invoking_keyseqs_in_map (function, _rl_keymap));
}

/* Print all of the current functions and their bindings to
   rl_outstream.  If an explicit argument is given, then print
   the output in such a way that it can be read back in. */
int
rl_dump_functions (count, key)
     int count, key;
{
  rl_function_dumper (rl_explicit_arg);
  rl_on_new_line ();
  return (0);
}

/* Print all of the functions and their bindings to rl_outstream.  If
   PRINT_READABLY is non-zero, then print the output in such a way
   that it can be read back in. */
void
rl_function_dumper (print_readably)
     int print_readably;
{
  register int i;
  char **names;
  char *name;

  names = rl_funmap_names ();

  fprintf (rl_outstream, "\n");

  for (i = 0; name = names[i]; i++)
    {
      Function *function;
      char **invokers;

      function = rl_named_function (name);
      invokers = rl_invoking_keyseqs_in_map (function, _rl_keymap);

      if (print_readably)
	{
	  if (!invokers)
	    fprintf (rl_outstream, "# %s (not bound)\n", name);
	  else
	    {
	      register int j;

	      for (j = 0; invokers[j]; j++)
		{
		  fprintf (rl_outstream, "\"%s\": %s\n",
			   invokers[j], name);
		  free (invokers[j]);
		}

	      free (invokers);
	    }
	}
      else
	{
	  if (!invokers)
	    fprintf (rl_outstream, "%s is not bound to any keys\n",
		     name);
	  else
	    {
	      register int j;

	      fprintf (rl_outstream, "%s can be found on ", name);

	      for (j = 0; invokers[j] && j < 5; j++)
		{
		  fprintf (rl_outstream, "\"%s\"%s", invokers[j],
			   invokers[j + 1] ? ", " : ".\n");
		}

	      if (j == 5 && invokers[j])
		fprintf (rl_outstream, "...\n");

	      for (j = 0; invokers[j]; j++)
		free (invokers[j]);

	      free (invokers);
	    }
	}
    }
}

/* Bind key sequence KEYSEQ to DEFAULT_FUNC if KEYSEQ is unbound. */
void
_rl_bind_if_unbound (keyseq, default_func)
     char *keyseq;
     Function *default_func;
{
  Function *func;

  if (keyseq)
    {
      func = rl_function_of_keyseq (keyseq, _rl_keymap, (int *)NULL);
      if (!func || func == rl_do_lowercase_version)
	rl_set_key (keyseq, default_func, _rl_keymap);
    }
}

/* **************************************************************** */
/*								    */
/*			String Utility Functions		    */
/*								    */
/* **************************************************************** */

static char *strindex ();

/* Return non-zero if any members of ARRAY are a substring in STRING. */
static int
substring_member_of_array (string, array)
     char *string, **array;
{
  while (*array)
    {
      if (strindex (string, *array))
	return (1);
      array++;
    }
  return (0);
}

#if !defined (BSD386) && !defined (NetBSD) && \
    !defined (FreeBSD) && !defined (_386BSD)
/* Whoops, Unix doesn't have strnicmp. */

/* Compare at most COUNT characters from string1 to string2.  Case
   doesn't matter. */
static int
strnicmp (string1, string2, count)
     char *string1, *string2;
     int count;
{
  register char ch1, ch2;

  while (count)
    {
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

  while (*string1 && *string2)
    {
      ch1 = *string1++;
      ch2 = *string2++;
      if (to_upper(ch1) != to_upper(ch2))
	return (1);
    }
  return (*string1 | *string2);
}
#endif

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
