/* float.c -- float environment functions.
   $Id: float.c,v 1.8 2004/07/05 22:23:22 karl Exp $

   Copyright (C) 2003, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Originally written by Alper Ersoy <dirt@gtk.org>.  */

#include "system.h"
#include "makeinfo.h"
#include "cmds.h"
#include "files.h"
#include "float.h"
#include "html.h"
#include "sectioning.h"
#include "xml.h"

static FLOAT_ELT *float_stack = NULL;

void
add_new_float (char *id, char *title, char *shorttitle,
    char *type, char *position)
{
  FLOAT_ELT *new = xmalloc (sizeof (FLOAT_ELT));
  unsigned long num_len;

  new->id = id;
  new->type = type;
  new->title = title;
  new->shorttitle = shorttitle;
  new->position = position;
  new->title_used = 0;
  new->defining_line = line_number - 1;

  new->number = current_chapter_number ();
  /* Append dot if not @unnumbered.  */
  num_len = strlen (new->number);
  if (num_len > 0)
    {
      new->number = xrealloc (new->number, num_len + 1 + 1);
      new->number[num_len] = '.';
      new->number[num_len+1] = '\0';
    }

  { /* Append the current float number.  */
    unsigned len = strlen (new->number) + 21;  /* that's 64 bits */
    char *s = xmalloc (len + 1);

    sprintf (s, "%s%d", new->number,
                count_floats_of_type_in_chapter (text_expansion (type),
                                                 new->number) + 1); 
    free (new->number);
    new->number = xstrdup (s);
  }

  /* Plain text output needs sectioning number and its title,
     when listing floats.  */
  if (!html && !xml && no_headers)
    {
      new->section = current_sectioning_number ();
      if (strlen (new->section) == 0)
        new->section_name = current_sectioning_name ();
      else
        new->section_name = "";
    }

  new->next = float_stack;
  float_stack = new;
}

int
count_floats_of_type_in_chapter (char *type, char *chapter)
{
  int i = 0;
  int l = strlen (chapter);
  FLOAT_ELT *temp = float_stack;

  while (temp && strncmp (temp->number, chapter, l) == 0)
    {
      if (strlen (temp->id) > 0 && STREQ (text_expansion (temp->type), type))
        i++;
      temp = temp->next;
    }

  return i;
}

char *
current_float_title (void)
{
  return float_stack->title;
}

char *
current_float_shorttitle (void)
{
  return float_stack->shorttitle;
}

char *
current_float_type (void)
{
  return float_stack->type;
}

char *
current_float_position (void)
{
  return float_stack->position;
}

char *
current_float_number (void)
{
  return float_stack->number;
}

char *
current_float_id (void)
{
  return float_stack->id;
}

char *
get_float_ref (char *id)
{
  FLOAT_ELT *temp = float_stack;

  while (temp)
    {
      if (STREQ (id, temp->id))
        {
          char *s = xmalloc (strlen (temp->type) + strlen (temp->number) + 2);
          sprintf (s, "%s %s", temp->type, temp->number);
          return s;
        }
      temp = temp->next;
    }

  return NULL;
}

static int
float_type_exists (char *check_type)
{
  /* Check if the requested float_type exists in the floats stack.  */
  FLOAT_ELT *temp;

  for (temp = float_stack; temp; temp = temp->next)
    if (STREQ (temp->type, check_type) && temp->id && *temp->id)
      return 1;

  return 0;
}

void
cm_listoffloats (void)
{
  char *float_type;
  get_rest_of_line (1, &float_type);

  /* get_rest_of_line increments the line number by one,
     so to make warnings/errors point to the correct line,
     we decrement the line_number again.  */
  if (!handling_delayed_writes)
    line_number--;

  if (handling_delayed_writes && !float_type_exists (float_type))
    warning (_("Requested float type `%s' not previously used"), float_type);

  if (xml)
    {
      xml_insert_element_with_attribute (LISTOFFLOATS, START,
          "type=\"%s\"", text_expansion (float_type));
      xml_insert_element (LISTOFFLOATS, END);
    }
  else if (!handling_delayed_writes)
    {
      int command_len = sizeof ("@ ") + strlen (command) + strlen (float_type);
      char *list_command = xmalloc (command_len + 1);

      /* These are for the text following @listoffloats command.
         Handling them with delayed writes is too late.  */
      close_paragraph ();
      cm_noindent ();

      sprintf (list_command, "@%s %s", command, float_type);
      register_delayed_write (list_command);
      free (list_command);
    }
  else if (float_type_exists (float_type))
    {
      FLOAT_ELT *temp = (FLOAT_ELT *) reverse_list
        ((GENERIC_LIST *) float_stack);
      FLOAT_ELT *new_start = temp;

      if (html)
        insert_string ("<ul class=\"listoffloats\">\n");
      else
        {
          if (!no_headers)
            insert_string ("* Menu:\n\n");
        }

      while (temp)
        {
          if (strlen (temp->id) > 0 && STREQ (float_type, temp->type))
            {
              if (html)
                {
                  /* A bit of space for HTML reabality.  */
                  insert_string ("  ");
                  add_html_block_elt ("<li>");

                  /* Simply relying on @ref command doesn't work here, because
                     commas in the caption may confuse the argument parsing.  */
                  add_word ("<a href=\"");
                  add_anchor_name (temp->id, 1);
                  add_word ("\">");

                  if (strlen (float_type) > 0)
                    execute_string ("%s", float_type);

                  if (strlen (temp->id) > 0)
                    {
                      if (strlen (float_type) > 0)
                        add_char (' ');

                      add_word (temp->number);
                    }

                  if (strlen (temp->title) > 0)
                    {
                      if (strlen (float_type) > 0
                          || strlen (temp->id) > 0)
                        insert_string (": ");

                      execute_string ("%s", temp->title);
                    }

                  add_word ("</a>");

                  add_html_block_elt ("</li>\n");
                }
              else
                {
                  char *entry;
                  char *raw_entry;
                  char *title = expansion (temp->title, 0);

                  int len;
                  int aux_chars_len; /* these are asterisk, colon, etc.  */
                  int column_width; /* width of the first column in menus.  */
                  int number_len; /* length of Figure X.Y: etc.   */
                  int i = 0;

                  /* Chosen widths are to match what @printindex produces.  */
                  if (no_headers)
                    {
                      column_width = 43;
                      /* We have only one auxiliary character, NULL.  */
                      aux_chars_len = sizeof ("");
                    }
                  else
                    {
                      column_width = 37;
                      /* We'll be adding an asterisk, followed by a space
                         and then a colon after the title, to construct a
                         proper menu item.  */
                      aux_chars_len = sizeof ("* :");
                    }

                  /* Allocate enough space for possible expansion later.  */
                  raw_entry = (char *) xmalloc (strlen (float_type)
                      + strlen (temp->number) + strlen (title)
                      + sizeof (":  "));

                  sprintf (raw_entry, "%s %s", float_type, temp->number);

                  if (strlen (title) > 0)
                    strcat (raw_entry, ": ");

                  number_len = strlen (raw_entry);

                  len = strlen (title) + strlen (raw_entry);

                  /* If we have a @shortcaption, try it if @caption is
                     too long to fit on a line.  */
                  if (len + aux_chars_len > column_width
                      && strlen (temp->shorttitle) > 0)
                    title = expansion (temp->shorttitle, 0);

                  strcat (raw_entry, title);
                  len = strlen (raw_entry);

                  if (len + aux_chars_len > column_width)
                    { /* Shorten long titles by looking for a space before
                         column_width - strlen (" ...").  */
                      /* -1 is for NULL, which is already in aux_chars_len.  */
                      aux_chars_len += sizeof ("...") - 1;
                      len = column_width - aux_chars_len;
                      while (raw_entry[len] != ' ' && len >= 0)
                        len--;

                      /* Advance to the whitespace.  */
                      len++;

                      /* If we are at the end of, say, Figure X.Y:, but
                         we have a title, then this means title does not
                         contain any whitespaces.  Or it may be that we
                         went as far as the beginning.  Just print as much
                         as possible of the title.  */
                      if (len == 0
                          || (len == number_len && strlen (title) > 0))
                        len = column_width - sizeof ("...");

                      /* Break here.  */
                      raw_entry[len] = 0;

                      entry = xmalloc (len + aux_chars_len);

                      if (!no_headers)
                        strcpy (entry, "* ");
                      else
                        entry[0] = 0;

                      strcat (entry, raw_entry);
                      strcat (entry, "...");

                      if (!no_headers)
                        strcat (entry, ":");
                    }
                  else
                    {
                      entry = xmalloc (len + aux_chars_len);

                      if (!no_headers)
                        strcpy (entry, "* ");
                      else
                        entry[0] = 0;

                      strcat (entry, raw_entry);

                      if (!no_headers)
                        strcat (entry, ":");
                    }

                  insert_string (entry);

                  i = strlen (entry);
                  /* We insert space chars until ``column_width + four spaces''
                     is reached, to make the layout the same with what we produce
                     for @printindex.  This is of course not obligatory, though
                     easier on the eye.  -1 is for NULL.  */
                  while (i < column_width + sizeof ("    ") - 1)
                    {
                      insert (' ');
                      i++;
                    }

                  if (no_headers)
                    {
                      if (strlen (temp->section) > 0)
                        { /* We got your number.  */
                          insert_string ((char *) _("See "));
                          insert_string (temp->section);
                        }
                      else
                        { /* Sigh, @float in an @unnumbered. :-\  */
                          insert_string ("\n          ");
                          insert_string ((char *) _("See "));
                          insert_string ("``");
                          insert_string (expansion (temp->section_name, 0));
                          insert_string ("''");
                        }
                    }
                  else
                    insert_string (temp->id);

                  insert_string (".\n");

                  free (entry);
                  free (title);
                }
            }
          temp = temp->next;
        }

      if (html)
        {
          inhibit_paragraph_indentation = 1;
          insert_string ("</ul>\n\n");
        }
      else
        insert ('\n');

      /* Retain the original order of float stack.  */
      temp = new_start;
      float_stack = (FLOAT_ELT *) reverse_list ((GENERIC_LIST *) temp);
    }

  free (float_type);
  /* Re-increment the line number, because get_rest_of_line
     left us looking at the next line after the command.  */
  line_number++;
}

int
current_float_used_title (void)
{
	return float_stack->title_used;
}

void current_float_set_title_used (void)
{
	float_stack->title_used = 1;
}
