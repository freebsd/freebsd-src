/* docbook.c -- docbook output.
   $Id: docbook.c,v 1.3 2001/12/31 16:52:17 karl Exp $

   Copyright (C) 2001 Free Software Foundation, Inc.

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
#include "docbook.h"
#include "insertion.h"
#include "lang.h"
#include "makeinfo.h"
#include "macro.h"
#include "sectioning.h"

int docbook_version_inserted = 0;
int docbook_first_chapter_found = 0;
int docbook_must_insert_node_anchor = 0;
int docbook_begin_book_p = 0;
int docbook_no_new_paragraph = 0;

static int section_level = -1;
static int in_docbook_paragraph = 0;
static int in_list = 0;

static int in_table = 0;
static int in_term = 0;
static int in_entry = 0;
static int in_varlistitem = 0;

static int in_example = 0;

void
docbook_begin_section (level, cmd)
     int level;
     char *cmd;
{
  int i, old_no_indent;
  char *temp, *tem;
  static char *last_chap = NULL;

  close_paragraph ();
  docbook_first_chapter_found = 1;
  filling_enabled =  indented_fill = 0;
  old_no_indent = no_indent;
  no_indent = 1;

  if (!docbook_begin_book_p)
    docbook_begin_book ();

  if (macro_expansion_output_stream && !executing_string)
    append_to_expansion_output (input_text_offset + 1);

  get_rest_of_line (0, &temp);

  if (in_docbook_paragraph)
    {
      insert_string ("\n</para>\n\n");
      adjust_braces_following (0, 10);
    }
  in_docbook_paragraph = 0;
  docbook_no_new_paragraph++;

  if (level > section_level + 1)
    level = section_level + 1;

  for (i = section_level;  i >= level ; i--)
    {
      if (i == 0)
         {
           if (last_chap && strcmp(last_chap, "appendix") == 0)
	     add_word ("</appendix>\n\n");
           else
	     add_word ("</chapter>\n\n");
         }
      else
	add_word_args ("</sect%d>\n\n", i);
    }

  section_level = level;

  if (level == 0)
    {
      if (strcmp(cmd, "appendix") == 0)
        add_word ("<appendix");
      else
        add_word ("<chapter");
      last_chap = cmd;
    }
  else
    add_word_args ("<sect%d", level);
  
  if (docbook_must_insert_node_anchor)
    {
      add_word (" id=\"");
      tem = expansion (current_node, 0);
      add_escaped_anchor_name (tem, 0);
      free (tem);
      add_word ("\"");
      docbook_must_insert_node_anchor = 0;
    }
   add_word (">\n");
   add_word ("<title>");

  if (macro_expansion_output_stream && !executing_string)
    {
      char *temp1 = xmalloc (2 + strlen (temp));
      sprintf (temp1, "%s", temp);
      remember_itext (input_text, input_text_offset);
      me_execute_string (temp1);
      free (temp1);
    }
  else
    execute_string ("%s", temp);

  free (temp);

  add_word ("</title>\n");

  close_paragraph ();
  filling_enabled = 1;
  no_indent = old_no_indent;  
  docbook_no_new_paragraph--;
  insert_string("\n<para>");
  in_docbook_paragraph = 1;
}

void
docbook_begin_paragraph ()
{
  if (!docbook_first_chapter_found)
    return;

  if (in_example)
    return;

  if (in_table && !in_term)
    {
      if (!in_varlistitem)
	insert_string ("\n<listitem><para>\n");
      else
	insert_string ("\n</para>\n\n<para>\n");
      in_varlistitem = 1;
      
      return;
    }
  if (in_list)
    return;
  if (in_docbook_paragraph)
    {
       insert_string ("\n</para>\n\n");
       adjust_braces_following (0, 10);
    }

#if 0
  if (docbook_must_insert_node_anchor)
    {
      char *tem;
       insert_string ("<para id=\"");
      adjust_braces_following (0, 10);
      tem = expansion (current_node, 0);
      add_escaped_anchor_name (tem, 0);
      free (tem);
      add_word ("\">\n");
      docbook_must_insert_node_anchor = 0;
    }
  else
#endif
    {
      insert_string ("<para>\n");
      adjust_braces_following (0, 7);
    }
  in_docbook_paragraph = 1;
}

void
docbook_begin_book ()
{
  if (!docbook_begin_book_p)
    docbook_begin_book_p = 1;
  else
    return;
  
  ++docbook_no_new_paragraph;
  add_word_args ("<!DOCTYPE book PUBLIC \"-//Davenport//DTD DocBook V3.0//EN\">\n\
<book>\n<title>%s</title>\n", title);
  --docbook_no_new_paragraph;
}

void
docbook_end_book ()
{
  int i;
  if (in_docbook_paragraph)
    {
      insert_string ("\n</para>\n\n");
    }
  
  for (i = section_level;  i >= 0 ; i--)
    {
      if (i == 0)
	add_word ("</chapter>\n");
      else
	add_word_args ("</sect%d>\n", i);
    }

  add_word ("</book>\n");
}

void
docbook_insert_tag (start_or_end, tag)
     int start_or_end;
     char *tag;
{
  if (!paragraph_is_open && start_or_end == START)
    docbook_begin_paragraph ();

  add_char ('<');
  if (start_or_end == START)
    add_word (tag);
  else
    {
      add_char ('/');
      for (; *tag && *tag != ' '; tag++)
	add_char(*tag);
    }
  add_meta_char ('>');
}

void 
docbook_xref1 (node_name)
     char *node_name;
{
  char *tem;
  add_word ("<xref linkend=\"");
  tem = expansion (node_name, 0);
  add_escaped_anchor_name (tem, 1);
  free (tem);
  add_word ("\"/>");
}

void 
docbook_xref2 (node_name, ref_name)
     char *node_name;
     char *ref_name;
{
  char *tem;
  add_word ("<xref linkend=\"");
  tem = expansion (node_name, 0);
  add_escaped_anchor_name (tem, 1);
  free (tem);
  add_word ("\"/>");
}

int
docbook_quote (character)
     int character;
{
  switch (language_code)
    {
    case fr:
      if (character == '`')
	{
	  add_word ("«&nbsp");
	  return ';';
	}
      else
	{
	  add_word ("&nbsp;");
	  return '»';
	}
      break;

    default:
      if (character == '`')
	{
	  add_word ("&ldquo");
	  return ';';
	}
      else
	{
	  add_word ("&rdquo");
	  return ';';
	}
      break;
    }
}

#define IS_BLANK(c) (c == ' ' || c == '\t' || c == '\n')

int
docbook_is_punctuation (character, next)
     int character;
     int next;
{
  return ( (character == ';'
	    || character == ':'
	    || character == '?'
	    || character == '!')
	   && IS_BLANK (next));
}

void
docbook_punctuation (character)
     int character;
{
  switch (language_code)
    {
    case fr:
      while (output_paragraph[output_paragraph_offset-1] == ' ')
 	output_paragraph_offset--;
      add_word ("&nbsp;");
      break;
    }
}

static int in_item = 0;

void
docbook_begin_itemize ()
{
  if (in_docbook_paragraph)
      insert_string ("\n</para>\n");

  in_docbook_paragraph = 0;
  insert_string ("\n<itemizedlist>\n");
  in_item = 0;
  in_list = 1;
}

void
docbook_end_itemize ()
{
  if (in_item)
    {
      insert_string ("\n</para></listitem>\n");
      in_item = 0;
    }
  insert_string ("\n</itemizedlist>\n\n<para>\n");
  in_docbook_paragraph = 1;
  in_list = 0;
}

void
docbook_begin_enumerate ()
{
  if (in_docbook_paragraph)
    insert_string ("\n</para>\n");
  in_docbook_paragraph = 0;
  insert_string ("\n<orderedlist>\n");
  in_item = 0;
  in_list = 1;
}

void
docbook_end_enumerate ()
{
  if (in_item)
    {
      insert_string ("\n</para></listitem>\n");
      in_item = 0;
    }
  insert_string ("\n</orderedlist>\n\n<para>\n");
  in_docbook_paragraph = 1;
  in_list = 0;
}

void
docbook_begin_table ()
{
#if 0
  if (in_docbook_paragraph)
    insert_string ("\n</para>\n\n");
  in_docbook_paragraph = 0;
#endif
  
  add_word ("\n<variablelist>\n");
  in_table ++;
  in_varlistitem = 0;
  in_entry = 0;
}

void
docbook_end_table ()
{
  if (!in_varlistitem)
    docbook_begin_paragraph ();
  insert_string ("\n</para></listitem>\n</varlistentry>\n\n</variablelist>\n");
#if 0
  if (in_table == 1)
    {
      insert_string ("\n</para>\n\n");
      in_docbook_paragraph = 0;
    }
  else
    {
      insert_string ("\n<para>\n\n");
      in_docbook_paragraph = 1;
    }
#endif
  in_table --;
  in_list = 0;
}

void 
docbook_add_item ()
{
  if (in_item)
    insert_string ("\n</para></listitem>\n");
  insert_string ("\n<listitem><para>\n");
  in_docbook_paragraph = 1;
  in_item = 1;
}

void 
docbook_add_table_item ()
{
  if (in_varlistitem)
    {
      insert_string ("\n</para></listitem>\n</varlistentry>\n\n");
      in_entry = 0;
      in_varlistitem = 0;
    }
  if (!in_entry)
    {
      insert_string ("<varlistentry>\n");
      in_entry = 1;
    }
  insert_string ("<term>");
  in_list = 1;
  in_term = 1;
}

void 
docbook_close_table_item ()
{
  insert_string ("</term>");
  in_list = 1;
  in_term = 0;
}

void
docbook_add_anchor (anchor)
     char *anchor;
{
  add_word ("<anchor id=\"");
  add_anchor_name (anchor, 0);
  add_word ("\">");
}

void
docbook_footnote (note)
     char *note;
{
  /* add_word_args ("<footnote><para>\n%s\n</para></footnote>\n", note); */
  add_word ("<footnote><para>\n");
  execute_string("%s", note);
  add_word("\n</para></footnote>\n");
}

void
docbook_begin_index ()
{
  add_word ("<variablelist>\n");
}

void 
docbook_begin_example ()
{
  add_word ("\n\n<screen>\n");
  in_example = 1;
}

void 
docbook_end_example ()
{
  in_example = 0;
  add_word ("</screen>\n\n");
}
