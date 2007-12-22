/* macro.c -- user-defined macros for Texinfo.
   $Id: macro.c,v 1.6 2004/04/11 17:56:47 karl Exp $

   Copyright (C) 1998, 1999, 2002, 2003 Free Software Foundation, Inc.

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
#include "cmds.h"
#include "files.h"
#include "macro.h"
#include "makeinfo.h"
#include "insertion.h"

/* If non-NULL, this is an output stream to write the full macro expansion
   of the input text to.  The result is another texinfo file, but
   missing @include, @infoinclude, @macro, and macro invocations.  Instead,
   all of the text is placed within the file. */
FILE *macro_expansion_output_stream = NULL;

/* Output file for -E.  */
char *macro_expansion_filename;

/* Nonzero means a macro string is in execution, as opposed to a file. */
int me_executing_string = 0;

/* Nonzero means we want only to expand macros and
   leave everything else intact.  */
int only_macro_expansion = 0;

static ITEXT **itext_info = NULL;
static int itext_size = 0;

/* Return the arglist on the current line.  This can behave in two different
   ways, depending on the variable BRACES_REQUIRED_FOR_MACRO_ARGS. */
int braces_required_for_macro_args = 0;

/* Array of macros and definitions. */
MACRO_DEF **macro_list = NULL;

int macro_list_len = 0;         /* Number of elements. */
int macro_list_size = 0;        /* Number of slots in total. */

/* Return the length of the array in ARRAY. */
int
array_len (char **array)
{
  int i = 0;

  if (array)
    for (i = 0; array[i]; i++);

  return i;
}

void
free_array (char **array)
{
  if (array)
    {
      int i;
      for (i = 0; array[i]; i++)
        free (array[i]);

      free (array);
    }
}

/* Return the macro definition of NAME or NULL if NAME is not defined. */
MACRO_DEF *
find_macro (char *name)
{
  int i;
  MACRO_DEF *def;

  def = NULL;
  for (i = 0; macro_list && (def = macro_list[i]); i++)
    {
      if ((!def->inhibited) && (strcmp (def->name, name) == 0))
        break;
    }
  return def;
}

/* Add the macro NAME with ARGLIST and BODY to the list of defined macros.
   SOURCE_FILE is the name of the file where this definition can be found,
   and SOURCE_LINENO is the line number within that file.  If a macro already
   exists with NAME, then a warning is produced, and that previous
   definition is overwritten. */
static void
add_macro (char *name, char **arglist, char *body, char *source_file,
    int source_lineno, int flags)
{
  MACRO_DEF *def;

  def = find_macro (name);

  if (!def)
    {
      if (macro_list_len + 2 >= macro_list_size)
        macro_list = xrealloc
          (macro_list, ((macro_list_size += 10) * sizeof (MACRO_DEF *)));

      macro_list[macro_list_len] = xmalloc (sizeof (MACRO_DEF));
      macro_list[macro_list_len + 1] = NULL;

      def = macro_list[macro_list_len];
      macro_list_len += 1;
      def->name = name;
    }
  else
    {
      char *temp_filename = input_filename;
      int temp_line = line_number;

      warning (_("macro `%s' previously defined"), name);

      input_filename = def->source_file;
      line_number = def->source_lineno;
      warning (_("here is the previous definition of `%s'"), name);

      input_filename = temp_filename;
      line_number = temp_line;

      if (def->arglist)
        {
          int i;

          for (i = 0; def->arglist[i]; i++)
            free (def->arglist[i]);

          free (def->arglist);
        }
      free (def->source_file);
      free (def->body);
    }

  def->source_file = xstrdup (source_file);
  def->source_lineno = source_lineno;
  def->body = body;
  def->arglist = arglist;
  def->inhibited = 0;
  def->flags = flags;
}


char **
get_brace_args (int quote_single)
{
  char **arglist, *word;
  int arglist_index, arglist_size;
  int character, escape_seen, start;
  int depth = 1;

  /* There is an arglist in braces here, so gather the args inside of it. */
  skip_whitespace_and_newlines ();
  input_text_offset++;
  arglist = NULL;
  arglist_index = arglist_size = 0;

 get_arg:
  skip_whitespace_and_newlines ();
  start = input_text_offset;
  escape_seen = 0;

  while ((character = curchar ()))
    {
      if (character == '\\')
        {
          input_text_offset += 2;
          escape_seen = 1;
        }
      else if (character == '{')
        {
          depth++;
          input_text_offset++;
        }
      else if ((character == ',' && !quote_single) ||
               ((character == '}') && depth == 1))
        {
          int len = input_text_offset - start;

          if (len || (character != '}'))
            {
              word = xmalloc (1 + len);
              memcpy (word, input_text + start, len);
              word[len] = 0;

              /* Clean up escaped characters. */
              if (escape_seen)
                {
                  int i;
                  for (i = 0; word[i]; i++)
                    if (word[i] == '\\')
                      memmove (word + i, word + i + 1,
                               1 + strlen (word + i + 1));
                }

              if (arglist_index + 2 >= arglist_size)
                arglist = xrealloc
                  (arglist, (arglist_size += 10) * sizeof (char *));

              arglist[arglist_index++] = word;
              arglist[arglist_index] = NULL;
            }

          input_text_offset++;
          if (character == '}')
            break;
          else
            goto get_arg;
        }
      else if (character == '}')
        {
          depth--;
          input_text_offset++;
        }
      else
        {
          input_text_offset++;
          if (character == '\n') line_number++;
        }
    }
  return arglist;
}

static char **
get_macro_args (MACRO_DEF *def)
{
  int i;
  char *word;

  /* Quickly check to see if this macro has been invoked with any arguments.
     If not, then don't skip any of the following whitespace. */
  for (i = input_text_offset; i < input_text_length; i++)
    if (!cr_or_whitespace (input_text[i]))
      break;

  if (input_text[i] != '{')
    {
      if (braces_required_for_macro_args)
        {
          return NULL;
        }
      else
        {
          /* Braces are not required to fill out the macro arguments.  If
             this macro takes one argument, it is considered to be the
             remainder of the line, sans whitespace. */
          if (def->arglist && def->arglist[0] && !def->arglist[1])
            {
              char **arglist;

              get_rest_of_line (0, &word);
              if (input_text[input_text_offset - 1] == '\n')
                {
                  input_text_offset--;
                  line_number--;
                }
              /* canon_white (word); */
              arglist = xmalloc (2 * sizeof (char *));
              arglist[0] = word;
              arglist[1] = NULL;
              return arglist;
            }
          else
            {
              /* The macro either took no arguments, or took more than
                 one argument.  In that case, it must be invoked with
                 arguments surrounded by braces. */
              return NULL;
            }
        }
    }
  return get_brace_args (def->flags & ME_QUOTE_ARG);
}

/* Substitute actual parameters for named parameters in body.
   The named parameters which appear in BODY must by surrounded
   reverse slashes, as in \foo\. */
static char *
apply (char **named, char **actuals, char *body)
{
  int i;
  int new_body_index, new_body_size;
  char *new_body, *text;
  int length_of_actuals;

  length_of_actuals = array_len (actuals);
  new_body_size = strlen (body);
  new_body = xmalloc (1 + new_body_size);

  /* Copy chars from BODY into NEW_BODY. */
  i = 0;
  new_body_index = 0;

  while (body[i])
    { /* Anything but a \ is easy.  */
      if (body[i] != '\\')
        new_body[new_body_index++] = body[i++];
      else
        { /* Snarf parameter name, check against named parameters. */
          char *param;
          int param_start, len;

          param_start = ++i;
          while (body[i] && body[i] != '\\')
            i++;

          len = i - param_start;
          param = xmalloc (1 + len);
          memcpy (param, body + param_start, len);
          param[len] = 0;

          if (body[i]) /* move past \ */
            i++;

          if (len == 0)
            { /* \\ always means \, even if macro has no args.  */
              len++;
              text = xmalloc (1 + len);
              sprintf (text, "\\%s", param);
            }
          else
            {
              int which;
              
              /* Check against named parameters. */
              for (which = 0; named && named[which]; which++)
                if (STREQ (named[which], param))
                  break;

              if (named && named[which])
                {
                  text = which < length_of_actuals ? actuals[which] : NULL;
                  if (!text)
                    text = "";
                  len = strlen (text);
                  text = xstrdup (text);  /* so we can free it */
                }
              else
                { /* not a parameter, so it's an error.  */
                  warning (_("\\ in macro expansion followed by `%s' instead of parameter name"),
                             param); 
                  len++;
                  text = xmalloc (1 + len);
                  sprintf (text, "\\%s", param);
                }
            }

          if (strlen (param) + 2 < len)
            {
              new_body_size += len + 1;
              new_body = xrealloc (new_body, new_body_size);
            }

          free (param);

          strcpy (new_body + new_body_index, text);
          new_body_index += len;

          free (text);
        }
    }

  new_body[new_body_index] = 0;
  return new_body;
}

/* Expand macro passed in DEF, a pointer to a MACRO_DEF, and
   return its expansion as a string.  */
char *
expand_macro (MACRO_DEF *def)
{
  char **arglist;
  int num_args;
  char *execution_string = NULL;
  int start_line = line_number;

  /* Find out how many arguments this macro definition takes. */
  num_args = array_len (def->arglist);

  /* Gather the arguments present on the line if there are any. */
  arglist = get_macro_args (def);

  if (num_args < array_len (arglist))
    {
      free_array (arglist);
      line_error (_("Macro `%s' called on line %d with too many args"),
                  def->name, start_line);
      return execution_string;
    }

  if (def->body)
    execution_string = apply (def->arglist, arglist, def->body);

  free_array (arglist);
  return execution_string;
}

/* Execute the macro passed in DEF, a pointer to a MACRO_DEF.  */
void
execute_macro (MACRO_DEF *def)
{
  char *execution_string;
  int start_line = line_number, end_line;

  if (macro_expansion_output_stream && !executing_string && !me_inhibit_expansion)
    me_append_before_this_command ();

  execution_string = expand_macro (def);
  if (!execution_string)
    return;

  if (def->body)
    {
      /* Reset the line number to where the macro arguments began.
         This makes line numbers reported in error messages correct in
         case the macro arguments span several lines and the expanded
         arguments invoke other commands.  */
      end_line = line_number;
      line_number = start_line;

      if (macro_expansion_output_stream
          && !executing_string && !me_inhibit_expansion)
        {
          remember_itext (input_text, input_text_offset);
          me_execute_string (execution_string);
        }
      else
        execute_string ("%s", execution_string);

      free (execution_string);
      line_number = end_line;
    }
}


/* Read and remember the definition of a macro.  If RECURSIVE is set,
   set the ME_RECURSE flag.  MACTYPE is either "macro" or "rmacro", and
   tells us what the matching @end should be.  */
static void
define_macro (char *mactype, int recursive)
{
  int i, start;
  char *name, *line;
  char *last_end = NULL;
  char *body = NULL;
  char **arglist = NULL;
  int body_size = 0, body_index = 0;
  int depth = 1;
  int flags = 0;
  int defining_line = line_number;

  if (macro_expansion_output_stream && !executing_string)
    me_append_before_this_command ();

  skip_whitespace ();

  /* Get the name of the macro.  This is the set of characters which are
     not whitespace and are not `{' immediately following the @macro. */
  start = input_text_offset;
  {
    int len;

    for (i = start; i < input_text_length && input_text[i] != '{'
                    && !cr_or_whitespace (input_text[i]);
         i++) ;

    len = i - start;
    name = xmalloc (1 + len);
    memcpy (name, input_text + start, len);
    name[len] = 0;
    input_text_offset = i;
  }

  skip_whitespace ();

  /* It is not required that the definition of a macro includes an arglist.
     If not, don't try to get the named parameters, just use a null list. */
  if (curchar () == '{')
    {
      int character;
      int arglist_index = 0, arglist_size = 0;
      int gathering_words = 1;
      char *word = NULL;

      /* Read the words inside of the braces which determine the arglist.
         These words will be replaced within the body of the macro at
         execution time. */

      input_text_offset++;
      skip_whitespace_and_newlines ();

      while (gathering_words)
        {
          int len;

          for (i = input_text_offset;
               (character = input_text[i]);
               i++)
            {
              switch (character)
                {
                case '\n':
                  line_number++;
                case ' ':
                case '\t':
                case ',':
                case '}':
                  /* Found the end of the current arglist word.  Save it. */
                  len = i - input_text_offset;
                  word = xmalloc (1 + len);
                  memcpy (word, input_text + input_text_offset, len);
                  word[len] = 0;
                  input_text_offset = i;

                  /* Advance to the comma or close-brace that signified
                     the end of the argument. */
                  while ((character = curchar ())
                         && character != ','
                         && character != '}')
                    {
                      input_text_offset++;
                      if (character == '\n')
                        line_number++;
                    }

                  /* Add the word to our list of words. */
                  if (arglist_index + 2 >= arglist_size)
                    {
                      arglist_size += 10;
                      arglist = xrealloc (arglist,
                                          arglist_size * sizeof (char *));
                    }

                  arglist[arglist_index++] = word;
                  arglist[arglist_index] = NULL;
                  break;
                }

              if (character == '}')
                {
                  input_text_offset++;
                  gathering_words = 0;
                  break;
                }

              if (character == ',')
                {
                  input_text_offset++;
                  skip_whitespace_and_newlines ();
                  i = input_text_offset - 1;
                }
            }
        }
      
      /* If we have exactly one argument, do @quote-arg implicitly.  Not
         only does this match TeX's behavior (which can't feasibly be
         changed), but it's a good idea.  */
      if (arglist_index == 1)
        flags |= ME_QUOTE_ARG;
    }

  /* Read the text carefully until we find an "@end macro" which
     matches this one.  The text in between is the body of the macro. */
  skip_whitespace_and_newlines ();

  while (depth)
    {
      if ((input_text_offset + 9) > input_text_length)
        {
          file_line_error (input_filename, defining_line,
			   _("%cend macro not found"), COMMAND_PREFIX);
          return;
        }

      get_rest_of_line (0, &line);

      /* Handle commands only meaningful within a macro. */
      if ((*line == COMMAND_PREFIX) && (depth == 1) &&
          (strncmp (line + 1, "allow-recursion", 15) == 0) &&
          (line[16] == 0 || whitespace (line[16])))
        {
          for (i = 16; whitespace (line[i]); i++);
          strcpy (line, line + i);
          flags |= ME_RECURSE;
          if (!*line)
            {
              free (line);
              continue;
            }
        }

      if ((*line == COMMAND_PREFIX) && (depth == 1) &&
          (strncmp (line + 1, "quote-arg", 9) == 0) &&
          (line[10] == 0 || whitespace (line[10])))
        {
          for (i = 10; whitespace (line[i]); i++);
          strcpy (line, line + i);

          if (arglist && arglist[0] && !arglist[1])
            {
              flags |= ME_QUOTE_ARG;
              if (!*line)
                {
                  free (line);
                  continue;
                }
            }
          else
           line_error (_("@quote-arg only useful for single-argument macros"));
        }

      if (*line == COMMAND_PREFIX
          && (strncmp (line + 1, "macro ", 6) == 0
              || strncmp (line + 1, "rmacro ", 7) == 0))
        depth++;

      /* Incorrect implementation of nesting -- just check that the last
         @end matches what we started with.  Since nested macros don't
         work in TeX anyway, this isn't worth the trouble to get right.  */
      if (*line == COMMAND_PREFIX && strncmp (line + 1, "end macro", 9) == 0)
        {
          depth--;
          last_end = "macro";
        }
      if (*line == COMMAND_PREFIX && strncmp (line + 1, "end rmacro", 10) == 0)
        {
          depth--;
          last_end = "rmacro";
        }

      if (depth)
        {
          if ((body_index + strlen (line) + 3) >= body_size)
            body = xrealloc (body, body_size += 3 + strlen (line));
          strcpy (body + body_index, line);
          body_index += strlen (line);
          body[body_index++] = '\n';
          body[body_index] = 0;
        }
      free (line);
    }

  /* Check that @end matched the macro command.  */
  if (!STREQ (last_end, mactype))
    warning (_("mismatched @end %s with @%s"), last_end, mactype);

  /* If it was an empty macro like
     @macro foo
     @end macro
     create an empty body.  (Otherwise, the macro is not expanded.)  */
  if (!body)
    {
      body = (char *)malloc(1);
      *body = 0;
    }

  /* We now have the name, the arglist, and the body.  However, BODY
     includes the final newline which preceded the `@end macro' text.
     Delete it. */
  if (body && strlen (body))
    body[strlen (body) - 1] = 0;

  if (recursive)
    flags |= ME_RECURSE;
    
  add_macro (name, arglist, body, input_filename, defining_line, flags);

  if (macro_expansion_output_stream && !executing_string)
    {
      /* Remember text for future expansions.  */
      remember_itext (input_text, input_text_offset);

      /* Bizarrely, output the @macro itself.  This is so texinfo.tex
         will have a chance to read it when texi2dvi calls makeinfo -E.
         The problem is that we don't really expand macros in all
         contexts; a @table's @item is one.  And a fix is not obvious to
         me, since it appears virtually identical to any other internal
         expansion.  Just setting a variable in cm_item caused other
         strange expansion problems.  */
      write_region_to_macro_output ("@", 0, 1);
      write_region_to_macro_output (mactype, 0, strlen (mactype));
      write_region_to_macro_output (" ", 0, 1);
      write_region_to_macro_output (input_text, start, input_text_offset);
    }
}

void 
cm_macro (void)
{
  define_macro ("macro", 0);
}

void 
cm_rmacro (void)
{
  define_macro ("rmacro", 1);
}

/* Delete the macro with name NAME.  The macro is deleted from the list,
   but it is also returned.  If there was no macro defined, NULL is
   returned. */

static MACRO_DEF *
delete_macro (char *name)
{
  int i;
  MACRO_DEF *def;

  def = NULL;

  for (i = 0; macro_list && (def = macro_list[i]); i++)
    if (strcmp (def->name, name) == 0)
      {
        memmove (macro_list + i, macro_list + i + 1,
               ((macro_list_len + 1) - i) * sizeof (MACRO_DEF *));
        macro_list_len--;
        break;
      }
  return def;
}

void
cm_unmacro (void)
{
  int i;
  char *line, *name;
  MACRO_DEF *def;

  if (macro_expansion_output_stream && !executing_string)
    me_append_before_this_command ();

  get_rest_of_line (0, &line);

  for (i = 0; line[i] && !whitespace (line[i]); i++);
  name = xmalloc (i + 1);
  memcpy (name, line, i);
  name[i] = 0;

  def = delete_macro (name);

  if (def)
    {
      free (def->source_file);
      free (def->name);
      free (def->body);

      if (def->arglist)
        {
          int i;

          for (i = 0; def->arglist[i]; i++)
            free (def->arglist[i]);

          free (def->arglist);
        }

      free (def);
    }

  free (line);
  free (name);

  if (macro_expansion_output_stream && !executing_string)
    remember_itext (input_text, input_text_offset);
}

/* How to output sections of the input file verbatim. */

/* Set the value of POINTER's offset to OFFSET. */
ITEXT *
remember_itext (char *pointer, int offset)
{
  int i;
  ITEXT *itext = NULL;

  /* If we have no info, initialize a blank list. */
  if (!itext_info)
    {
      itext_info = xmalloc ((itext_size = 10) * sizeof (ITEXT *));
      for (i = 0; i < itext_size; i++)
        itext_info[i] = NULL;
    }

  /* If the pointer is already present in the list, then set the offset. */
  for (i = 0; i < itext_size; i++)
    if ((itext_info[i]) &&
        (itext_info[i]->pointer == pointer))
      {
        itext = itext_info[i];
        itext_info[i]->offset = offset;
        break;
      }

  if (i == itext_size)
    {
      /* Find a blank slot (or create a new one), and remember the
         pointer and offset. */
      for (i = 0; i < itext_size; i++)
        if (itext_info[i] == NULL)
          break;

      /* If not found, then add some slots. */
      if (i == itext_size)
        {
          int j;

          itext_info = xrealloc
            (itext_info, (itext_size += 10) * sizeof (ITEXT *));

          for (j = i; j < itext_size; j++)
            itext_info[j] = NULL;
        }

      /* Now add the pointer and the offset. */
      itext_info[i] = xmalloc (sizeof (ITEXT));
      itext_info[i]->pointer = pointer;
      itext_info[i]->offset = offset;
      itext = itext_info[i];
    }
  return itext;
}

/* Forget the input text associated with POINTER. */
void
forget_itext (char *pointer)
{
  int i;

  for (i = 0; i < itext_size; i++)
    if (itext_info[i] && (itext_info[i]->pointer == pointer))
      {
        free (itext_info[i]);
        itext_info[i] = NULL;
        break;
      }
}

/* Append the text which appeared in input_text from the last offset to
   the character just before the command that we are currently executing. */
void
me_append_before_this_command (void)
{
  int i;

  for (i = input_text_offset; i && (input_text[i] != COMMAND_PREFIX); i--)
    ;
  maybe_write_itext (input_text, i);
}

/* Similar to execute_string, but only takes a single string argument,
   and remembers the input text location, etc. */
void
me_execute_string (char *execution_string)
{
  int saved_escape_html = escape_html;
  int saved_in_paragraph = in_paragraph;
  escape_html = me_executing_string == 0;
  in_paragraph = 0;
  
  pushfile ();
  input_text_offset = 0;
  /* The following xstrdup is so we can relocate input_text at will.  */
  input_text = xstrdup (execution_string);
  input_filename = xstrdup (input_filename);
  input_text_length = strlen (execution_string);

  remember_itext (input_text, 0);

  me_executing_string++;
  reader_loop ();
  free (input_text);
  free (input_filename);
  popfile ();
  me_executing_string--;

  in_paragraph = saved_in_paragraph;
  escape_html = saved_escape_html;
}

/* A wrapper around me_execute_string which saves and restores
   variables important for output generation.  This is called
   when we need to produce macro-expanded output for input which
   leaves no traces in the Info output.  */
void
me_execute_string_keep_state (char *execution_string, char *append_string)
{
  int op_orig, opcol_orig, popen_orig;
  int fill_orig, newline_orig, indent_orig, meta_pos_orig;

  remember_itext (input_text, input_text_offset);
  op_orig = output_paragraph_offset;
  meta_pos_orig = meta_char_pos;
  opcol_orig = output_column;
  popen_orig = paragraph_is_open;
  fill_orig = filling_enabled;
  newline_orig = last_char_was_newline;
  filling_enabled = 0;
  indent_orig = no_indent;
  no_indent = 1;
  me_execute_string (execution_string);
  if (append_string)
    write_region_to_macro_output (append_string, 0, strlen (append_string));
  output_paragraph_offset = op_orig;
  meta_char_pos = meta_pos_orig;
  output_column = opcol_orig;
  paragraph_is_open = popen_orig;
  filling_enabled = fill_orig;
  last_char_was_newline = newline_orig;
  no_indent = indent_orig;
}

/* Append the text which appears in input_text from the last offset to
   the current OFFSET. */
void
append_to_expansion_output (int offset)
{
  int i;
  ITEXT *itext = NULL;

  for (i = 0; i < itext_size; i++)
    if (itext_info[i] && itext_info[i]->pointer == input_text)
      {
        itext = itext_info[i];
        break;
      }

  if (!itext)
    return;

  if (offset > itext->offset)
    {
      write_region_to_macro_output (input_text, itext->offset, offset);
      remember_itext (input_text, offset);
    }
}

/* Only write this input text iff it appears in our itext list. */
void
maybe_write_itext (char *pointer, int offset)
{
  int i;
  ITEXT *itext = NULL;

  for (i = 0; i < itext_size; i++)
    if (itext_info[i] && (itext_info[i]->pointer == pointer))
      {
        itext = itext_info[i];
        break;
      }

  if (itext && (itext->offset < offset))
    {
      write_region_to_macro_output (itext->pointer, itext->offset, offset);
      remember_itext (pointer, offset);
    }
}

void
write_region_to_macro_output (char *string, int start, int end)
{
  if (macro_expansion_output_stream)
    fwrite (string + start, 1, end - start, macro_expansion_output_stream);
}

/* Aliases. */

typedef struct alias_struct
{
  char *alias;
  char *mapto;
  struct alias_struct *next;
} alias_type;

static alias_type *aliases; 

/* @alias aname = cmdname */

void
cm_alias (void)
{
  alias_type *a = xmalloc (sizeof (alias_type));

  skip_whitespace ();
  get_until_in_line (0, "=", &(a->alias));
  canon_white (a->alias);

  discard_until ("=");
  skip_whitespace ();
  get_until_in_line (0, " ", &(a->mapto));

  a->next = aliases;
  aliases = a;
}

/* Perform an alias expansion.  Called from read_command.  */
char *
alias_expand (char *tok)
{
  alias_type *findit = aliases;

  while (findit)
    if (strcmp (findit->alias, tok) == 0)
      {
	free (tok);
	return alias_expand (xstrdup (findit->mapto));
      }
    else
      findit = findit->next;

  return tok;
}

/* definfoenclose implementation.  */

/* This structure is used to track enclosure macros.  When an enclosure
   macro is recognized, a pointer to the enclosure block corresponding 
   to its name is saved in the brace element for its argument. */
typedef struct enclose_struct
{
  char *enclose;
  char *before;
  char *after;
  struct enclose_struct *next;
} enclosure_type;

static enclosure_type *enclosures; 

typedef struct enclosure_stack_struct
{
    enclosure_type *current;
    struct enclosure_stack_struct *next;
} enclosure_stack_type;

static enclosure_stack_type *enclosure_stack;

/* @definfoenclose */
void
cm_definfoenclose (void)
{
  enclosure_type *e = xmalloc (sizeof (enclosure_type));

  skip_whitespace ();
  get_until_in_line (1, ",", &(e->enclose));
  discard_until (",");
  get_until_in_line (0, ",", &(e->before));
  discard_until (",");
  get_until_in_line (0, "\n", &(e->after));

  e->next = enclosures;
  enclosures = e;
}

/* If TOK is an enclosure command, push it on the enclosure stack and
   return 1.  Else return 0.  */

int
enclosure_command (char *tok)
{
  enclosure_type *findit = enclosures;

  while (findit)
    if (strcmp (findit->enclose, tok) == 0)
      {
        enclosure_stack_type *new = xmalloc (sizeof (enclosure_stack_type));
        new->current = findit;
        new->next = enclosure_stack;
        enclosure_stack = new;

        return 1;
      }
    else
      findit = findit->next;

  return 0;
}

/* actually perform the enclosure expansion */
void
enclosure_expand (int arg, int start, int end)
{
  if (arg == START)
    add_word (enclosure_stack->current->before);
  else
    {
      enclosure_stack_type *temp;

      add_word (enclosure_stack->current->after);

      temp = enclosure_stack;
      enclosure_stack = enclosure_stack->next;
      free (temp);
    }
}
