/* footnote.c -- footnotes for Texinfo.
   $Id: footnote.c,v 1.13 2002/03/02 15:05:21 karl Exp $

   Copyright (C) 1998, 99, 2002 Free Software Foundation, Inc.

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
#include "footnote.h"
#include "macro.h"
#include "makeinfo.h"
#include "xml.h"

/* Nonzero means that the footnote style for this document was set on
   the command line, which overrides any other settings. */
int footnote_style_preset = 0;

/* The current footnote number in this node.  Each time a new node is
   started this is reset to 1. */
int current_footnote_number = 1;

/* Nonzero means we automatically number footnotes with no specified marker. */
int number_footnotes = 1;

/* Nonzero means we are currently outputting footnotes. */
int already_outputting_pending_notes = 0;


/* Footnotes can be handled in one of two ways:

   separate_node:
        Make them look like followed references, with the reference
        destinations in a makeinfo manufactured node or,
   end_node:
        Make them appear at the bottom of the node that they originally
        appeared in. */

#define separate_node 0
#define end_node 1

int footnote_style = end_node;
int first_footnote_this_node = 1;
int footnote_count = 0;

/* Set the footnote style based on the style identifier in STRING. */
int
set_footnote_style (string)
     char *string;
{
  if (strcasecmp (string, "separate") == 0)
    footnote_style = separate_node;
  else if (strcasecmp (string, "end") == 0)
    footnote_style = end_node;
  else
    return -1;

 return 0;
}

void
cm_footnotestyle ()
{
  char *arg;

  get_rest_of_line (1, &arg);

  /* If set on command line, do not change the footnote style.  */
  if (!footnote_style_preset && set_footnote_style (arg) != 0)
    line_error (_("Bad argument to %c%s"), COMMAND_PREFIX, command);

  free (arg);
}

typedef struct fn
{
  struct fn *next;
  char *marker;
  char *note;
  int number;
}  FN;

FN *pending_notes = NULL;

/* A method for remembering footnotes.  Note that this list gets output
   at the end of the current node. */
void
remember_note (marker, note)
     char *marker, *note;
{
  FN *temp = xmalloc (sizeof (FN));

  temp->marker = xstrdup (marker);
  temp->note = xstrdup (note);
  temp->next = pending_notes;
  temp->number = current_footnote_number;
  pending_notes = temp;
  footnote_count++;
}

/* How to get rid of existing footnotes. */
static void
free_pending_notes ()
{
  FN *temp;

  while ((temp = pending_notes))
    {
      free (temp->marker);
      free (temp->note);
      pending_notes = pending_notes->next;
      free (temp);
    }
  first_footnote_this_node = 1;
  footnote_count = 0;
  current_footnote_number = 1;	/* for html */
}

/* What to do when you see a @footnote construct. */

 /* Handle a "footnote".
    footnote *{this is a footnote}
    where "*" is the (optional) marker character for this note. */
void
cm_footnote ()
{
  char *marker;
  char *note;

  get_until ("{", &marker);
  canon_white (marker);

  if (macro_expansion_output_stream && !executing_string)
    append_to_expansion_output (input_text_offset + 1); /* include the { */

  /* Read the argument in braces. */
  if (curchar () != '{')
    {
      line_error (_("`%c%s' needs an argument `{...}', not just `%s'"),
                  COMMAND_PREFIX, command, marker);
      free (marker);
      return;
    }
  else
    {
      int len;
      int braces = 1;
      int loc = ++input_text_offset;

      while (braces)
        {
          if (loc == input_text_length)
            {
              line_error (_("No closing brace for footnote `%s'"), marker);
              return;
            }

          if (input_text[loc] == '{')
            braces++;
          else if (input_text[loc] == '}')
            braces--;
          else if (input_text[loc] == '\n')
            line_number++;

          loc++;
        }

      len = (loc - input_text_offset) - 1;
      note = xmalloc (len + 1);
      memcpy (note, &input_text[input_text_offset], len);
      note[len] = 0;
      input_text_offset = loc;
    }

  /* Must write the macro-expanded argument to the macro expansion
     output stream.  This is like the case in index_add_arg.  */
  if (macro_expansion_output_stream && !executing_string)
    {
      /* Calling me_execute_string on a lone } provokes an error, since
         as far as the reader knows there is no matching {.  We wrote
         the { above in the call to append_to_expansion_output. */
      me_execute_string_keep_state (note, "}");
    }

  if (!current_node || !*current_node)
    {
      line_error (_("Footnote defined without parent node"));
      free (marker);
      free (note);
      return;
    }

  /* output_pending_notes is non-reentrant (it uses a global data
     structure pending_notes, which it frees before it returns), and
     TeX doesn't grok footnotes inside footnotes anyway.  Disallow
     that.  */
  if (already_outputting_pending_notes)
    {
      line_error (_("Footnotes inside footnotes are not allowed"));
      free (marker);
      free (note);
      return;
    }

  if (!*marker)
    {
      free (marker);

      if (number_footnotes)
        {
          marker = xmalloc (10);
          sprintf (marker, "%d", current_footnote_number);
        }
      else
        marker = xstrdup ("*");
    }

  if (xml)
    xml_insert_footnote (note);
  else 
    {
  remember_note (marker, note);

  /* fixme: html: footnote processing needs work; we currently ignore
     the style requested; we could clash with a node name of the form
     `fn-<n>', though that's unlikely. */
  if (html)
    {
      add_html_elt ("<a rel=footnote href=");
      add_word_args ("\"#fn-%d\"><sup>%s</sup></a>",
		     current_footnote_number, marker);
    }
  else
    /* Your method should at least insert MARKER. */
    switch (footnote_style)
      {
      case separate_node:
        add_word_args ("(%s)", marker);
        execute_string (" (*note %s-Footnote-%d::)",
                        current_node, current_footnote_number);
        if (first_footnote_this_node)
          {
            char *temp_string, *expanded_ref;

            temp_string = xmalloc (strlen (current_node)
                                   + strlen ("-Footnotes") + 1);

            strcpy (temp_string, current_node);
            strcat (temp_string, "-Footnotes");
            expanded_ref = expansion (temp_string, 0);
            remember_node_reference (expanded_ref, line_number,
                                     followed_reference);
            free (temp_string);
            free (expanded_ref);
            first_footnote_this_node = 0;
          }
        break;

      case end_node:
        add_word_args ("(%s)", marker);
        break;

      default:
        break;
      }
  current_footnote_number++;
    }
  free (marker);
  free (note);
}

/* Output the footnotes.  We are at the end of the current node. */
void
output_pending_notes ()
{
  FN *footnote = pending_notes;

  if (!pending_notes)
    return;

  if (html)
    { /* The type= attribute is used just in case some weirdo browser
         out there doesn't use numbers by default.  Since we rely on the
         browser to produce the footnote numbers, we need to make sure
         they ARE indeed numbers.  Pre-HTML4 browsers seem to not care.  */
      add_word ("<hr><h4>");
      add_word (_("Footnotes"));
      add_word ("</h4>\n<ol type=\"1\">\n");
    }
  else
    switch (footnote_style)
      {
      case separate_node:
        {
          char *old_current_node = current_node;
          char *old_command = xstrdup (command);

          already_outputting_pending_notes++;
          execute_string ("%cnode %s-Footnotes,,,%s\n",
                          COMMAND_PREFIX, current_node, current_node);
          already_outputting_pending_notes--;
          current_node = old_current_node;
          free (command);
          command = old_command;
        }
      break;

      case end_node:
        close_paragraph ();
        in_fixed_width_font++;
        /* This string should be translated according to the
           @documentlanguage, not the current LANG.  We can't do that
           yet, so leave it in English.  */
        execute_string ("---------- Footnotes ----------\n\n");
        in_fixed_width_font--;
        break;
      }

  /* Handle the footnotes in reverse order. */
  {
    FN **array = xmalloc ((footnote_count + 1) * sizeof (FN *));
    array[footnote_count] = NULL;

    while (--footnote_count > -1)
      {
        array[footnote_count] = footnote;
        footnote = footnote->next;
      }

    filling_enabled = 1;
    indented_fill = 1;

    while ((footnote = array[++footnote_count]))
      {
        if (html)
          {
	    /* Make the text of every footnote begin a separate paragraph.  */
            add_word_args ("<li><a name=\"fn-%d\"></a>\n<p>",
			   footnote->number);
            already_outputting_pending_notes++;
            execute_string ("%s", footnote->note);
            already_outputting_pending_notes--;
            add_word ("</p>\n");
          }
        else
          {
            char *old_current_node = current_node;
            char *old_command = xstrdup (command);

            already_outputting_pending_notes++;
            execute_string ("%canchor{%s-Footnote-%d}(%s) %s",
                            COMMAND_PREFIX, current_node, footnote->number,
                            footnote->marker, footnote->note);
            already_outputting_pending_notes--;
            current_node = old_current_node;
            free (command);
            command = old_command;
          }

        close_paragraph ();
      }

    if (html)
      add_word ("</ol><hr>");
    close_paragraph ();
    free (array);
  }
  
  free_pending_notes ();
}
