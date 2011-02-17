/* defun.c -- @defun and friends.
   $Id: defun.c,v 1.11 2004/04/11 17:56:46 karl Exp $

   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "system.h"
#include "defun.h"
#include "xml.h"
#include "insertion.h"
#include "makeinfo.h"
#include "cmds.h"
#include "html.h"


#define DEFUN_SELF_DELIMITING(c) \
  ((c) == '(' || (c) == ')' || (c) == '[' || (c) == ']')

struct token_accumulator
{
  unsigned int length;
  unsigned int index;
  char **tokens;
};

static void
initialize_token_accumulator (struct token_accumulator *accumulator)
{
  accumulator->length = 0;
  accumulator->index = 0;
  accumulator->tokens = NULL;
}

static void
accumulate_token (struct token_accumulator *accumulator, char *token)
{
  if (accumulator->index >= accumulator->length)
    {
      accumulator->length += 10;
      accumulator->tokens = xrealloc (accumulator->tokens,
                                      (accumulator->length * sizeof (char *)));
    }
  accumulator->tokens[accumulator->index] = token;
  accumulator->index += 1;
}

/* Given STRING_POINTER pointing at an open brace, skip forward and return a
   pointer to just past the matching close brace. */
static int
scan_group_in_string (char **string_pointer)
{
  char *scan_string = (*string_pointer) + 1;
  unsigned int level = 1;
  int started_command = 0;

  for (;;)
    {
      int c;
      if (level == 0)
        {
          *string_pointer = scan_string;
          return 1;
        }
      c = *scan_string++;
      if (c == 0)
        {
          /* Tweak line_number to compensate for fact that
             we gobbled the whole line before coming here. */
          line_number--;
          line_error (_("Missing `}' in @def arg"));
          line_number++;
          *string_pointer = scan_string - 1;
          return 0;
        }

      if (c == '{' && !started_command)
        level++;
      if (c == '}' && !started_command)
        level--;

      /* remember if at @.  */
      started_command = (c == '@' && !started_command);
    }
}

/* Return a list of tokens from the contents of STRING.
   Commands and brace-delimited groups count as single tokens.
   Contiguous whitespace characters are converted to a token
   consisting of a single space. */
static char **
args_from_string (char *string)
{
  struct token_accumulator accumulator;
  char *token_start, *token_end;
  char *scan_string = string;

  initialize_token_accumulator (&accumulator);

  while (*scan_string)
    { /* Replace arbitrary whitespace by a single space. */
      if (whitespace (*scan_string))
        {
          scan_string += 1;
          while (whitespace (*scan_string))
            scan_string += 1;
          accumulate_token ((&accumulator), (xstrdup (" ")));
          continue;
        }

      /* Commands count as single tokens. */
      if (*scan_string == COMMAND_PREFIX)
        {
          token_start = scan_string;
          scan_string += 1;
          if (self_delimiting (*scan_string))
            scan_string += 1;
          else
            {
              int c;
              while (1)
                {
                  c = *scan_string++;

                  if ((c == 0) || (c == '{') || (whitespace (c)))
                    {
                      scan_string -= 1;
                      break;
                    }
                }

              if (*scan_string == '{')
                {
                  char *s = scan_string;
                  (void) scan_group_in_string (&s);
                  scan_string = s;
                }
            }
          token_end = scan_string;
        }

      /* Parentheses and brackets are self-delimiting. */
      else if (DEFUN_SELF_DELIMITING (*scan_string))
        {
          token_start = scan_string;
          scan_string += 1;
          token_end = scan_string;
        }

      /* Open brace introduces a group that is a single token. */
      else if (*scan_string == '{')
        {
          char *s = scan_string;
          int balanced = scan_group_in_string (&s);

          token_start = scan_string + 1;
          scan_string = s;
          token_end = balanced ? (scan_string - 1) : scan_string;
        }

      /* Make commas separate tokens so to differentiate them from
         parameter types in XML output. */
      else if (*scan_string == ',')
	{
          token_start = scan_string;
          scan_string += 1;
          token_end = scan_string;
	}

      /* Otherwise a token is delimited by whitespace, parentheses,
         brackets, or braces.  A token is also ended by a command. */
      else
        {
          token_start = scan_string;

          for (;;)
            {
              int c;

              c = *scan_string++;

              /* Do not back up if we're looking at a }; since the only
                 valid }'s are those matched with {'s, we want to give
                 an error.  If we back up, we go into an infinite loop.  */
              if (!c || whitespace (c) || DEFUN_SELF_DELIMITING (c)
                  || c == '{')
                {
                  scan_string--;
                  break;
                }

	      /* End token if we are looking at a comma, as commas are
		 delimiters too. */
	      if (c == ',')
		{
		  scan_string--;
		  break;
		}

              /* If we encounter a command embedded within a token,
                 then end the token. */
              if (c == COMMAND_PREFIX)
                {
                  scan_string--;
                  break;
                }
            }
          token_end = scan_string;
        }

      accumulate_token (&accumulator, substring (token_start, token_end));
    }
  accumulate_token (&accumulator, NULL);
  return accumulator.tokens;
}

static void
process_defun_args (char **defun_args, int auto_var_p)
{
  int pending_space = 0;

  if (xml)
    {
      xml_process_defun_args (defun_args, auto_var_p);
      return;
    }

  for (;;)
    {
      char *defun_arg = *defun_args++;

      if (defun_arg == NULL)
        break;

      if (defun_arg[0] == ' ')
        {
          pending_space = 1;
          continue;
        }

      if (pending_space)
        {
          add_char (' ');
          pending_space = 0;
        }

      if (DEFUN_SELF_DELIMITING (defun_arg[0]))
        {
          /* Within @deffn and friends, texinfo.tex makes parentheses
             sans serif and brackets bold.  We use roman instead.  */
          if (html)
            insert_html_tag (START, "");
            
          add_char (defun_arg[0]);
          
          if (html)
            insert_html_tag (END, "");
        }
      /* else if (defun_arg[0] == '&' || defun_arg[0] == COMMAND_PREFIX) */
        /* execute_string ("%s", defun_arg); */
      /* else if (auto_var_p) */
        /* execute_string ("%s", defun_arg); */
      else
        execute_string ("%s", defun_arg);
    }
}

static char *
next_nonwhite_defun_arg (char ***arg_pointer)
{
  char **scan = (*arg_pointer);
  char *arg = (*scan++);

  if ((arg != 0) && (*arg == ' '))
    arg = *scan++;

  if (arg == 0)
    scan -= 1;

  *arg_pointer = scan;

  return (arg == 0) ? "" : arg;
}


/* This is needed also in insertion.c.  */

enum insertion_type
get_base_type (int type)
{
  int base_type;
  switch (type)
    {
    case defivar:	base_type = defcv; break;
    case defmac:	base_type = deffn; break;
    case defmethod:	base_type = defop; break;
    case defopt:	base_type = defvr; break;
    case defspec:	base_type = deffn; break;
    case deftypecv:	base_type = deftypecv; break;
    case deftypefun:	base_type = deftypefn; break;
    case deftypeivar:	base_type = deftypeivar; break;
    case deftypemethod:	base_type = deftypemethod; break;
    case deftypeop:	base_type = deftypeop; break;
    case deftypevar:	base_type = deftypevr; break;
    case defun:		base_type = deffn; break;
    case defvar:	base_type = defvr; break;
    default:
      base_type = type;
      break;
    }

  return base_type;
}

/* Make the defun type insertion.
   TYPE says which insertion this is.
   X_P, if nonzero, says not to start a new insertion. */
static void
defun_internal (int type, int x_p)
{
  int base_type;
  char **defun_args, **scan_args;
  const char *category;
  char *defined_name;
  char *type_name = NULL;
  char *type_name2 = NULL;

  {
    char *line;

    /* The @def.. line is the only place in Texinfo where you are
       allowed to use unquoted braces that don't delimit arguments of
       a command or a macro; in any other place it will trigger an
       error message from the reader loop.  The special handling of
       this case inside `args_from_string' is an extra special hack
       which allows this.  The side effect is that if we try to expand
       the rest of the line below, the recursive reader loop will
       signal an error if there are brace-delimited arguments on that line.

       The best solution to this would be to change the syntax of
       @def.. commands so that it doesn't violate Texinfo's own rules.
       But it's probably too late for this now, as it will break a lot
       of existing manuals.

       Unfortunately, this means that you can't call macros, use @value, etc.
       inside @def.. commands, sigh.  */
    get_rest_of_line (0, &line);

    /* Basic line continuation.  If a line ends with \s*@\s* concatanate
       the next line. */
    {
      char *next_line, *new_line;
      int i;

      line_continuation:
        i = strlen (line) - 1;

        if (line[i] == '@' && line[i-1] != '@')
          {
            get_rest_of_line (0, &next_line);
            new_line = (char *) xmalloc (i + strlen (next_line) + 2);
            strncpy (new_line, line, i);
            new_line[i] = '\0';
            free (line);
            strcat (new_line, " ");
            strcat (new_line, next_line);
            line = xstrdup (new_line);
            free (next_line);
            free (new_line);

            goto line_continuation;
          }
    }

    defun_args = (args_from_string (line));
    free (line);
  }

  scan_args = defun_args;

  /* Get base type and category string.  */
  base_type = get_base_type (type);

  /* xx all these const strings should be determined upon
     documentlanguage argument and NOT via gettext  (kama).  */
  switch (type)
    {
    case defun:
    case deftypefun:
      category = _("Function");
      break;
    case defmac:
      category = _("Macro");
      break;
    case defspec:
      category = _("Special Form");
      break;
    case defvar:
    case deftypevar:
      category = _("Variable");
      break;
    case defopt:
      category = _("User Option");
      break;
    case defivar:
    case deftypeivar:
      category = _("Instance Variable");
      break;
    case defmethod:
    case deftypemethod:
      category = _("Method");
      break;
    default:
      category = next_nonwhite_defun_arg (&scan_args);
      break;
    }

  /* The class name.  */
  if ((base_type == deftypecv)
      || (base_type == deftypefn)
      || (base_type == deftypevr)
      || (base_type == defcv)
      || (base_type == defop)
      || (base_type == deftypeivar)
      || (base_type == deftypemethod)
      || (base_type == deftypeop)
     )
    type_name = next_nonwhite_defun_arg (&scan_args);

  /* The type name for typed languages.  */
  if ((base_type == deftypecv)
      || (base_type == deftypeivar)
      || (base_type == deftypemethod)
      || (base_type == deftypeop)
     )
    type_name2 = next_nonwhite_defun_arg (&scan_args);

  /* The function or whatever that's actually being defined.  */
  defined_name = next_nonwhite_defun_arg (&scan_args);

  /* This hack exists solely for the purposes of formatting the Texinfo
     manual.  I couldn't think of a better way.  The token might be a
     simple @@ followed immediately by more text.  If this is the case,
     then the next defun arg is part of this one, and we should
     concatenate them. */
  if (*scan_args && **scan_args && !whitespace (**scan_args)
       && STREQ (defined_name, "@@"))
    {
      char *tem = xmalloc (3 + strlen (scan_args[0]));

      sprintf (tem, "@@%s", scan_args[0]);

      free (scan_args[0]);
      scan_args[0] = tem;
      scan_args++;
      defined_name = tem;
    }

  /* It's easy to write @defun foo(arg1 arg2), but a following ( is
     misparsed by texinfo.tex and this is next to impossible to fix.
     Warn about it.  */
  if (*scan_args && **scan_args && **scan_args == '(')
    warning ("`%c' follows defined name `%s' instead of whitespace",
             **scan_args, defined_name);

  if (!x_p)
    begin_insertion (type);

  /* Write the definition header line.
     This should start at the normal indentation.  */
  current_indent -= default_indentation_increment;
  start_paragraph ();

  if (!html && !xml)
    switch (base_type)
      {
      case deffn:
      case defvr:
      case deftp:
        execute_string (" --- %s: %s", category, defined_name);
        break;
      case deftypefn:
      case deftypevr:
        execute_string (" --- %s: %s %s", category, type_name, defined_name);
        break;
      case defcv:
        execute_string (" --- %s %s %s: %s", category, _("of"), type_name,
                        defined_name);
        break;
      case deftypecv:
      case deftypeivar:
        execute_string (" --- %s %s %s: %s %s", category, _("of"), type_name,
                        type_name2, defined_name);
        break;
      case defop:
        execute_string (" --- %s %s %s: %s", category, _("on"), type_name,
                        defined_name);
        break;
      case deftypeop:
        execute_string (" --- %s %s %s: %s %s", category, _("on"), type_name,
                        type_name2, defined_name);
        break;
      case deftypemethod:
        execute_string (" --- %s %s %s: %s %s", category, _("on"), type_name,
                        type_name2, defined_name);
        break;
      }
  else if (html)
    {
      /* If this is not a @def...x version, it could only
         be a normal version @def.... So start the table here.  */
      if (!x_p)
        insert_string ("<div class=\"defun\">\n");
      else
        rollback_empty_tag ("blockquote");

      /* xx The single words (on, off) used here, should depend on
         documentlanguage and NOT on gettext  --kama.  */
      switch (base_type)
        {
        case deffn:
        case defvr:
        case deftp:
        case deftypefn:
        case deftypevr:
          execute_string ("--- %s: ", category);
          break;

        case defcv:
        case deftypecv:
        case deftypeivar:
	  execute_string ("--- %s %s %s: ", category, _("of"), type_name);
	  break;

        case defop:
        case deftypemethod:
        case deftypeop:
	  execute_string ("--- %s %s %s: ", category, _("on"), type_name);
	  break;
	} /* switch (base_type)... */

      switch (base_type)
        {
        case deffn:
        case defvr:
        case deftp:
          /* <var> is for the following function arguments.  */
          insert_html_tag (START, "b");
          execute_string ("%s", defined_name);
          insert_html_tag (END, "b");
          insert_html_tag (START, "var");
          break;
        case deftypefn:
        case deftypevr:
          execute_string ("%s ", type_name);
          insert_html_tag (START, "b");
          execute_string ("%s", defined_name);
          insert_html_tag (END, "b");
          insert_html_tag (START, "var");
          break;
        case defcv:
        case defop:
          insert_html_tag (START, "b");
          execute_string ("%s", defined_name);
          insert_html_tag (END, "b");
          insert_html_tag (START, "var");
          break;
        case deftypecv:
        case deftypeivar:
        case deftypemethod:
        case deftypeop:
          execute_string ("%s ", type_name2);
          insert_html_tag (START, "b");
          execute_string ("%s", defined_name);
          insert_html_tag (END, "b");
          insert_html_tag (START, "var");
          break;
        }
    }
  else if (xml)
    xml_begin_def_term (base_type, category, defined_name, type_name,
	type_name2);

  current_indent += default_indentation_increment;

  /* Now process the function arguments, if any.  If these carry onto
     the next line, they should be indented by two increments to
     distinguish them from the body of the definition, which is indented
     by one increment.  */
  current_indent += default_indentation_increment;

  switch (base_type)
    {
    case deffn:
    case defop:
      process_defun_args (scan_args, 1);
      break;

      /* Through Makeinfo 1.67 we processed remaining args only for deftp,
         deftypefn, and deftypemethod.  But the libc manual, for example,
         needs to say:
            @deftypevar {char *} tzname[2]
         And simply allowing the extra text seems far simpler than trying
         to invent yet more defn commands.  In any case, we should either
         output it or give an error, not silently ignore it.  */
    default:
      process_defun_args (scan_args, 0);
      break;
    }

  current_indent -= default_indentation_increment;
  if (!html)
    close_single_paragraph ();

  /* Make an entry in the appropriate index.  (XML and
     Docbook already got their entries, so skip them.)  */
  if (!xml)
    switch (base_type)
      {
      case deffn:
      case deftypefn:
	execute_string ("@findex %s\n", defined_name);
	break;
      case defcv:
      case deftypecv:
      case deftypevr:
      case defvr:
	execute_string ("@vindex %s\n", defined_name);
	break;
      case deftypeivar:
	execute_string ("@vindex %s %s %s\n", defined_name, _("of"),
                        type_name);
	break;
      case defop:
      case deftypeop:
      case deftypemethod:
	execute_string ("@findex %s %s %s\n", defined_name, _("on"),
                        type_name);
	break;
      case deftp:
	execute_string ("@tindex %s\n", defined_name);
	break;
      }

  if (xml)
    xml_end_def_term ();
  else if (html)
    {
      inhibit_paragraph_indentation = 1;
      no_indent = 1;
      insert_html_tag (END, "var");
      insert_string ("<br>\n");
      /* Indent the definition a bit.  */
      add_html_block_elt ("<blockquote>");
      no_indent = 0;
      inhibit_paragraph_indentation = 0;
      paragraph_is_open = 0;
    }

  /* Deallocate the token list. */
  scan_args = defun_args;
  while (1)
    {
      char * arg = (*scan_args++);
      if (arg == NULL)
        break;
      free (arg);
    }
  free (defun_args);
}

/* Add an entry for a function, macro, special form, variable, or option.
   If the name of the calling command ends in `x', then this is an extra
   entry included in the body of an insertion of the same type. */
void
cm_defun (void)
{
  int type;
  char *base_command = xstrdup (command);  /* command with any `x' removed */
  int x_p = (command[strlen (command) - 1] == 'x');

  if (x_p)
    base_command[strlen (base_command) - 1] = 0;

  type = find_type_from_name (base_command);

  /* If we are adding to an already existing insertion, then make sure
     that we are already in an insertion of type TYPE. */
  if (x_p)
    {
      INSERTION_ELT *i = insertion_stack;
      /* Skip over ifclear and ifset conditionals.  */
      while (i && (i->insertion == ifset || i->insertion == ifclear))
        i = i->next;
        
      if (!i || i->insertion != type)
        {
          line_error (_("Must be in `@%s' environment to use `@%s'"),
                      base_command, command);
          discard_until ("\n");
          return;
        }
    }

  defun_internal (type, x_p);
  free (base_command);
}
