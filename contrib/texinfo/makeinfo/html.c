/* html.c -- html-related utilities.
   $Id: html.c,v 1.19 2002/02/23 19:12:15 karl Exp $

   Copyright (C) 1999, 2000, 01, 02 Free Software Foundation, Inc.

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
  static char *html_title = NULL;
  static int html_title_written = 0;

  if (html_output_head_p)
    return;
  html_output_head_p = 1;

  /* The <title> should not have markup, so use text_expansion.  */
  if (!html_title)
    html_title = title ? text_expansion (title) : _("Untitled");

  add_word_args ("<html lang=\"%s\">\n<head>\n<title>%s</title>\n",
                 language_table[language_code].abbrev, html_title);

  add_word ("<meta http-equiv=\"Content-Type\" content=\"text/html");
  if (document_encoding_code != no_encoding)
    add_word_args ("; charset=%s",
                   encoding_table[document_encoding_code].ecname);
  add_word ("\">\n");

  if (!document_description)
    document_description = html_title;

  add_word_args ("<meta name=description content=\"%s\">\n",
                 document_description);
  add_word_args ("<meta name=generator content=\"makeinfo %s\">\n", VERSION);
  add_word ("<link href=\"http://texinfo.org/\" rel=generator-home>\n");
  add_word ("</head>\n<body>\n");

  if (title && !html_title_written)
    {
      add_word_args ("<h1>%s</h1>\n", html_title);
      html_title_written = 1;
    }
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
    }
  while (string[i++]);

  if (newlen == i) return string; /* Already OK. */

  newstring = xmalloc (newlen);
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
  return newstring - newlen;
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
add_link (nodename, attributes)
     char *nodename, *attributes;
{
  if (nodename)
    {
      add_html_elt ("<link ");
      add_word_args ("%s", attributes);
      add_word_args (" href=\"");
      add_anchor_name (nodename, 1);
      add_word ("\"></a>\n");
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
    {
      if (splitting)
	add_url_name (nodename, href);
      add_char ('#');
    }
  /* Always add NODENAME, so that the reference would pinpoint the
     exact node on its file.  This is so several nodes could share the
     same file, in case of file-name clashes, but also for more
     accurate browser positioning.  */
  if (strcasecmp (nodename, "(dir)") == 0)
    /* Strip the parens, but keep the original letter-case.  */
    add_word_args ("%.3s", nodename + 1);
  else
    add_escaped_anchor_name (nodename);
}

/* Insert the text for the name of a reference in an HTML url, aprropriate
   for NODENAME */
void
add_url_name (nodename, href)
     char *nodename;
     int href;
{
    add_nodename_to_filename (nodename, href);
}

/* Only allow [-0-9a-zA-Z_.] when nodifying filenames.  This may
   result in filename clashes; e.g.,

   @node Foo ],,,
   @node Foo [,,,

   both map to Foo--.html.  If that happens, cm_node will put all
   the nodes whose file names clash on the same file.  */
void
fix_filename (filename)
     char *filename;
{
  char *p;
  for (p = filename; *p; p++)
    {
      if (!(isalnum (*p) || strchr ("-._", *p)))
	*p = '-';
    }
}

/* As we can't look-up a (forward-referenced) nodes' html filename
   from the tentry, we take the easy way out.  We assume that
   nodenames are unique, and generate the html filename from the
   nodename, that's always known.  */
static char *
nodename_to_filename_1 (nodename, href)
     char *nodename;
     int href;
{
  char *p;
  char *filename;
  char dirname[PATH_MAX];

  if (strcasecmp (nodename, "Top") == 0)
    {
      /* We want to convert references to the Top node into
	 "index.html#Top".  */
      if (href)
	filename = xstrdup ("index.html"); /* "#Top" is added by our callers */
      else
	filename = xstrdup ("Top");
    }
  else if (strcasecmp (nodename, "(dir)") == 0)
    /* We want to convert references to the (dir) node into
       "../index.html".  */
    filename = xstrdup ("../index.html");
  else
    {
      filename = xmalloc (PATH_MAX);
      dirname[0] = '\0';
      *filename = '\0';

      /* Check for external reference: ``(info-document)node-name''
	 Assume this node lives at: ``../info-document/node-name.html''

	 We need to handle the special case (sigh): ``(info-document)'',
	 ie, an external top-node, which should translate to:
	 ``../info-document/info-document.html'' */

      p = nodename;
      if (*nodename == '(')
	{
	  int length;

	  p = strchr (nodename, ')');
	  if (p == NULL)
	    {
	      line_error (_("Invalid node name: `%s'"), nodename);
	      exit (1);
	    }

	  length = p - nodename - 1;
	  if (length > 5 &&
	      FILENAME_CMPN (p - 5, ".info", 5) == 0)
	    length -= 5;
	  /* This is for DOS, and also for Windows and GNU/Linux
	     systems that might have Info files copied from a DOS 8+3
	     filesystem.  */
	  if (length > 4 &&
	      FILENAME_CMPN (p - 4, ".inf", 4) == 0)
	    length -= 4;
	  strcpy (filename, "../");
	  strncpy (dirname, nodename + 1, length);
	  *(dirname + length) = '\0';
	  fix_filename (dirname);
	  strcat (filename, dirname);
	  strcat (filename, "/");
	  p++;
	}

      /* In the case of just (info-document), there will be nothing
	 remaining, and we will refer to ../info-document/, which will
	 work fine.  */
      strcat (filename, p);
      if (*p)
	{
	  /* Hmm */
	  fix_filename (filename + strlen (filename) - strlen (p));
	  strcat (filename, ".html");
	}
    }

  /* Produce a file name suitable for the underlying filesystem.  */
  normalize_filename (filename);

#if 0
  /* We add ``#Nodified-filename'' anchor to external references to be
     prepared for non-split HTML support.  Maybe drop this. */
  if (href && *dirname)
    {
      strcat (filename, "#");
      strcat (filename, p);
      /* Hmm, again */
      fix_filename (filename + strlen (filename) - strlen (p));
    }
#endif

  return filename;
}

/* If necessary, ie, if current filename != filename of node, output
   the node name.  */
void
add_nodename_to_filename (nodename, href)
     char *nodename;
     int href;
{
  /* for now, don't check: always output filename */
  char *filename = nodename_to_filename_1 (nodename, href);
  add_word (filename);
  free (filename);
}

char *
nodename_to_filename (nodename)
     char *nodename;
{
  /* The callers of nodename_to_filename use the result to produce
     <a href=, so call nodename_to_filename_1 with last arg non-zero.  */
  return nodename_to_filename_1 (nodename, 1);
}
