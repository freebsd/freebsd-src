/* html.c -- html-related utilities.
   $Id: html.c,v 1.5 1999/09/18 19:27:41 karl Exp $

   Copyright (C) 1999 Free Software Foundation, Inc.

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
#include "html.h"
#include "lang.h"
#include "makeinfo.h"
#include "sectioning.h"

/* See html.h.  */
int html_output_head_p = 0;

void
html_output_head ()
{
  char *html_title;
  
  if (html_output_head_p)
    return;
  html_output_head_p = 1;

  /* The <title> should not have markup.  */
  html_title = title ? text_expansion (title) : _("Untitled");

  add_word_args ("<html lang=\"%s\"><head>\n<title>%s</title>\n",
                 language_table[language_code].abbrev, html_title);

  add_word ("<meta http-equiv=\"Content-Type\" content=\"text/html");
  if (document_encoding)
    add_word_args ("; charset=%s", document_encoding);
  add_word ("\">\n");
  
  add_word_args ("<meta name=description content=\"%s\">\n", html_title);
  add_word_args ("<meta name=generator content=\"makeinfo %s\">\n", VERSION);
  add_word ("<link href=\"http://texinfo.org/\" rel=generator-home>\n");
  add_word ("</head><body>\n\n");
}


/* Escape HTML special characters in the string if necessary,
   returning a pointer to a possibly newly-allocated one. */
char *
escape_string (string)
     char * string;
{
  int i=0, newlen=0;
  char * newstring;

  do
    {
      /* Find how much to allocate. */
      switch (string[i])
        {
        case '&':
          newlen += 5;          /* `&amp;' */
          break;
        case '<':
        case '>':
          newlen += 4;          /* `&lt;', `&gt;' */
          break;
        default:
          newlen++;
        }
      i++;
    }
  while (string[i]);

  if (newlen == i) return string; /* Already OK. */

  newstring = xmalloc (newlen + 2);
  i = 0;
  do
    {
      switch (string[i])
        {
        case '&':
          strcpy (newstring, "&amp;");
          newstring += 5;
          break;
        case '<':
          strcpy (newstring, "&lt;");
          newstring += 4;
          break;
        case '>':
          strcpy (newstring, "&gt;");
          newstring += 4;
          break;
        default:
          newstring[0] = string[i];
          newstring++;
        }
    }
  while (string[i++]);
  free (string);
  return newstring - newlen -1;
}

/* Open or close TAG according to START_OR_END. */
void
insert_html_tag (start_or_end, tag)
     int start_or_end;
     char *tag;
{
  if (!paragraph_is_open && (start_or_end == START))
    {
      /* Need to compensate for the <p> we are about to insert, or
	 else cm_xxx functions that call us will get wrong text
	 between START and END.  */
      adjust_braces_following (output_paragraph_offset, 3);
      add_word ("<p>");
    }
  add_char ('<');
  if (start_or_end != START)
    add_char ('/');
  add_word (tag);
  add_char ('>');
}

/* Output an HTML <link> to the filename for NODE, including the
   other string as extra attributes. */
void
add_link (node, attributes)
     char *node, *attributes;
{
  if (node)
    {
      add_word_args ("<link %s href=\"", attributes);
      add_anchor_name (node, 1);
      add_word ("\">\n");
    }
}

/* Output NAME with characters escaped as appropriate for an anchor
   name, i.e., escape URL special characters as %<n>.  */
void
add_escaped_anchor_name (name)
     char *name;
{
  for (; *name; name++)
    {
      if (*name == '&')
        add_word ("&amp;");
      else if (! URL_SAFE_CHAR (*name))
        /* Cast so characters with the high bit set are treated as >128,
           for example o-umlaut should be 246, not -10.  */
        add_word_args ("%%%x", (unsigned char) *name);
      else
        add_char (*name);
    }
}

/* Insert the text for the name of a reference in an HTML anchor
   appropriate for NODENAME.  If HREF is nonzero, it will be
   appropriate for a href= attribute, rather than name= i.e., including
   the `#' if it's an internal reference. */
void
add_anchor_name (nodename, href)
     char *nodename;
     int href;
{
  if (href)
    add_char ('#');

  add_escaped_anchor_name (nodename);
}
