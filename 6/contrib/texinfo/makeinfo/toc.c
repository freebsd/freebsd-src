/* toc.c -- table of contents handling.
   $Id: toc.c,v 1.6 2004/04/11 17:56:47 karl Exp $

   Copyright (C) 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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

   Originally written by Karl Heinz Marbaise <kama@hippo.fido.de>.  */

#include "system.h"
#include "makeinfo.h"
#include "cmds.h"
#include "files.h"
#include "macro.h"
#include "node.h"
#include "html.h"
#include "lang.h"
#include "makeinfo.h"
#include "sectioning.h"
#include "toc.h"
#include "xml.h"

/* array of toc entries */
static TOC_ENTRY_ELT **toc_entry_alist = NULL;

/* toc_counter start from 0 ... n for every @chapter, @section ... */
static int toc_counter = 0;

/* Routine to add an entry to the table of contents */
int
toc_add_entry (char *tocname, int level, char *node_name, char *anchor)
{
  char *tocname_and_node, *expanded_node, *d;
  char *s = NULL;
  char *filename = NULL;

  if (!node_name)
    node_name = "";

  /* I assume that xrealloc behaves like xmalloc if toc_entry_alist is
     NULL */
  toc_entry_alist = xrealloc (toc_entry_alist,
                              (toc_counter + 1) * sizeof (TOC_ENTRY_ELT *));

  toc_entry_alist[toc_counter] = xmalloc (sizeof (TOC_ENTRY_ELT));

  if (html)
    {
      /* We need to insert the expanded node name into the toc, so
         that when we eventually output the toc, its <a ref= link will
         point to the <a name= tag created by cm_node in the navigation
         bar.  We cannot expand the containing_node member, for the
         reasons explained in the WARNING below.  We also cannot wait
         with the node name expansion until the toc is actually output,
         since by that time the macro definitions may have been changed.
         So instead we store in the tocname member the expanded node
         name and the toc name concatenated together (with the necessary
         html markup), since that's how they are output.  */
      if (!anchor)
        s = expanded_node = expand_node_name (node_name);
      else
        expanded_node = anchor;
      if (splitting)
	{
	  if (!anchor)
	    filename = nodename_to_filename (expanded_node);
	  else
	    filename = filename_part (current_output_filename);
	}
      /* Sigh...  Need to HTML-escape the expanded node name like
         add_anchor_name does, except that we are not writing this to
         the output, so can't use add_anchor_name...  */
      /* The factor 5 in the next allocation is because the maximum
         expansion of HTML-escaping is for the & character, which is
         output as "&amp;".  2 is for "> that separates node from tocname.  */
      d = tocname_and_node = (char *)xmalloc (2 + 5 * strlen (expanded_node)
                                              + strlen (tocname) + 1);
      if (!anchor)
        {
          for (; *s; s++)
            {
              if (cr_or_whitespace (*s))
                *d++ = '-';
              else if (! URL_SAFE_CHAR (*s))
                {
                  sprintf (d, "_00%x", (unsigned char) *s);
                  /* do this manually since sprintf returns char * on
                     SunOS 4 and other old systems.  */
                  while (*d)
                    d++;
                }
              else
                *d++ = *s;
            }
          strcpy (d, "\">");
        }
      else
        /* Section outside any node, they provided explicit anchor.  */
        strcpy (d, anchor);
      strcat (d, tocname);
      free (tocname);       /* it was malloc'ed by substring() */
      free (expanded_node);
      toc_entry_alist[toc_counter]->name = tocname_and_node;
    }
  else
    toc_entry_alist[toc_counter]->name = tocname;
  /* WARNING!  The node name saved in containing_node member must
     be the node name with _only_ macros expanded (the macros in
     the node name are expanded by cm_node when it grabs the name
     from the @node directive).  Non-macros, like @value, @@ and
     other @-commands must NOT be expanded in containing_node,
     because toc_find_section_of_node looks up the node name where
     they are also unexpanded.  You *have* been warned!  */
  toc_entry_alist[toc_counter]->containing_node = xstrdup (node_name);
  toc_entry_alist[toc_counter]->level = level;
  toc_entry_alist[toc_counter]->number = toc_counter;
  toc_entry_alist[toc_counter]->html_file = filename;

  /* have to be done at least */
  return toc_counter++;
}

/* Return the name of a chapter/section/subsection etc. that
   corresponds to the node NODE.  If the node isn't found,
   return NULL.

   WARNING!  This function relies on NODE being unexpanded
   except for macros (i.e., @value, @@, and other non-macros
   should NOT be expanded), because the containing_node member
   stores unexpanded node names.

   Note that this function returns the first section whose
   containing node is NODE.  Thus, they will lose if they use
   more than a single chapter structioning command in a node,
   or if they have a node without any structuring commands.  */
char *
toc_find_section_of_node (char *node)
{
  int i;

  if (!node)
    node = "";
  for (i = 0; i < toc_counter; i++)
    if (STREQ (node, toc_entry_alist[i]->containing_node))
      return toc_entry_alist[i]->name;

  return NULL;
}

/* free up memory used by toc entries */
void
toc_free (void)
{
  int i;

  if (toc_counter)
    {
      for (i = 0; i < toc_counter; i++)
        {
          free (toc_entry_alist[i]->name);
          free (toc_entry_alist[i]->containing_node);
          free (toc_entry_alist[i]);
        }

      free (toc_entry_alist);
      toc_entry_alist = NULL; /* to be sure ;-) */
      toc_counter = 0; /* to be absolutley sure ;-) */
    }
}

/* Print table of contents in HTML.  */

static void
contents_update_html (void)
{
  int i;
  int k;
  int last_level;

  /* does exist any toc? */
  if (!toc_counter)
      /* no, so return to sender ;-) */
      return;

  add_html_block_elt_args ("\n<div class=\"contents\">\n<h2>%s</h2>\n<ul>\n", _("Table of Contents"));

  last_level = toc_entry_alist[0]->level;

  for (i = 0; i < toc_counter; i++)
    {
      if (toc_entry_alist[i]->level > last_level)
        {
          /* unusual, but it is possible
             @chapter ...
             @subsubsection ...      ? */
          for (k = 0; k < (toc_entry_alist[i]->level-last_level); k++)
            add_html_block_elt ("<ul>\n");
        }
      else if (toc_entry_alist[i]->level < last_level)
        {
          /* @subsubsection ...
             @chapter ... this IS usual.*/
          for (k = 0; k < (last_level-toc_entry_alist[i]->level); k++)
            add_word ("</li></ul>\n");
        }

      /* No double entries in TOC.  */
      if (!(i && strcmp (toc_entry_alist[i]->name,
			 toc_entry_alist[i-1]->name) == 0))
        {
          /* each toc entry is a list item.  */
          add_word ("<li>");

          /* Insert link -- to an external file if splitting, or
             within the current document if not splitting.  */
	  add_word ("<a ");
          /* For chapters (only), insert an anchor that the short contents
             will link to.  */
          if (toc_entry_alist[i]->level == 0)
	    {
	      char *p = toc_entry_alist[i]->name;

	      /* toc_entry_alist[i]->name has the form `foo">bar',
		 that is, it includes both the node name and anchor
		 text.  We need to find where `foo', the node name,
		 ends, and use that in toc_FOO.  */
	      while (*p && *p != '"')
		p++;
	      add_word_args ("name=\"toc_%.*s\" ",
		       p - toc_entry_alist[i]->name, toc_entry_alist[i]->name);
	    }
	  add_word_args ("href=\"%s#%s</a>\n",
		   splitting ? toc_entry_alist[i]->html_file : "",
		   toc_entry_alist[i]->name);
        }

      last_level = toc_entry_alist[i]->level;
    }

  /* Go back to start level. */
  if (toc_entry_alist[0]->level < last_level)
    for (k = 0; k < (last_level-toc_entry_alist[0]->level); k++)
      add_word ("</li></ul>\n");

  add_word ("</li></ul>\n</div>\n\n");
}

/* print table of contents in ASCII (--no-headers)
   May be we should create a new command line switch --ascii ? */
static void
contents_update_info (void)
{
  int i;
  int k;

  if (!toc_counter)
      return;

  insert_string ((char *) _("Table of Contents"));
  insert ('\n');
  for (i = 0; i < strlen (_("Table of Contents")); i++)
    insert ('*');
  insert_string ("\n\n");

  for (i = 0; i < toc_counter; i++)
    {
      if (toc_entry_alist[i]->level == 0)
        add_char ('\n');

      /* indention with two spaces per level, should this
         changed? */
      for (k = 0; k < toc_entry_alist[i]->level; k++)
        insert_string ("  ");

      insert_string (toc_entry_alist[i]->name);
      insert ('\n');
    }
  insert_string ("\n\n");
}

/* shortcontents in HTML; Should this produce a standalone file? */
static void
shortcontents_update_html (char *contents_filename)
{
  int i;
  char *toc_file = NULL;

  /* does exist any toc? */
  if (!toc_counter)
    return;

  add_html_block_elt_args ("\n<div class=\"shortcontents\">\n<h2>%s</h2>\n<ul>\n", _("Short Contents"));

  if (contents_filename)
    toc_file = filename_part (contents_filename);

  for (i = 0; i < toc_counter; i++)
    {
      char *name = toc_entry_alist[i]->name;

      if (toc_entry_alist[i]->level == 0)
	{
	  if (contents_filename)
	    add_word_args ("<li><a href=\"%s#toc_%s</a></li>\n",
		     splitting ? toc_file : "", name);
	  else
	    add_word_args ("<a href=\"%s#%s</a>\n",
		     splitting ? toc_entry_alist[i]->html_file : "", name);
	}
    }
  add_word ("</ul>\n</div>\n\n");
  if (contents_filename)
    free (toc_file);
}

/* short contents in ASCII (--no-headers).  */
static void
shortcontents_update_info (void)
{
  int i;

  if (!toc_counter)
      return;

  insert_string ((char *) _("Short Contents"));
  insert ('\n');
  for (i = 0; i < strlen (_("Short Contents")); i++)
    insert ('*');
  insert_string ("\n\n");

  for (i = 0; i < toc_counter; i++)
    {
      if (toc_entry_alist[i]->level == 0)
        {
          insert_string (toc_entry_alist[i]->name);
          insert ('\n');
        }
    }
  insert_string ("\n\n");
}

void
cm_contents (int arg)
{
  /* the file where we found the @contents directive */
  static char *contents_filename;

  /* No need to mess with delayed stuff for XML and Docbook.  */
  if (xml)
    {
      if (arg == START)
        {
          int elt = STREQ (command, "contents") ? CONTENTS : SHORTCONTENTS;
          xml_insert_element (elt, START);
          xml_insert_element (elt, END);
        }
    }
  else if (!handling_delayed_writes)
    {
      register_delayed_write (STREQ (command, "contents")
          ? "@contents" : "@shortcontents");

      if (html && STREQ (command, "contents"))
        {
          if (contents_filename)
            free (contents_filename);
          contents_filename = xstrdup (current_output_filename);
        }
    }
  else if (html)
    STREQ (command, "contents")
      ? contents_update_html () : shortcontents_update_html (contents_filename);
  else if (no_headers)
    STREQ (command, "contents")
      ? contents_update_info () : shortcontents_update_info ();
}
