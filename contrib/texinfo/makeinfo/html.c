/* html.c -- html-related utilities.
   $Id: html.c,v 1.18 2003/06/02 12:32:29 karl Exp $

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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "system.h"
#include "cmds.h"
#include "html.h"
#include "lang.h"
#include "makeinfo.h"
#include "sectioning.h"

HSTACK *htmlstack = NULL;

static char *process_css_file (/* char * */);

/* See html.h.  */
int html_output_head_p = 0;
int html_title_written = 0;


void
html_output_head ()
{
  static const char *html_title = NULL;

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
                   encoding_table[document_encoding_code].encname);
  add_word ("\">\n");

  if (!document_description)
    document_description = html_title;

  add_word_args ("<meta name=\"description\" content=\"%s\">\n",
                 document_description);
  add_word_args ("<meta name=\"generator\" content=\"makeinfo %s\">\n",
                 VERSION);
#if 0
  /* let's not do this now, since it causes mozilla to put up a
     navigation bar.  */
  add_word ("<link href=\"http://www.gnu.org/software/texinfo/\" \
rel=\"generator-home\">\n");
#endif

  if (copying_text)
    { /* copying_text has already been fully expanded in
         begin_insertion (by full_expansion), so use insert_ rather than
         add_.  It is not ideal that we include the html markup here within
         <head>, but the alternative is to have yet more and different
         expansions of the copying text.  Yuck.  */
      insert_string ("<!--\n");
      insert_string (copying_text);
      insert_string ("-->\n");
    }

  /* Put the style definitions in a comment for the sake of browsers
     that don't support <style>.  */
  add_word ("<meta http-equiv=\"Content-Style-Type\" content=\"text/css\">\n");
  add_word ("<style type=\"text/css\"><!--\n");

  {
    char *css_inline = NULL;

    if (css_include)
      /* This writes out any @import commands from the --css-file,
         and returns any actual css code following the imports.  */
      css_inline = process_css_file (css_include);

    /* This seems cleaner than adding <br>'s at the end of each line for
       these "roman" displays.  It's hardly the end of the world if the
       browser doesn't do <style>s, in any case; they'll just come out in
       typewriter.  */
#define CSS_FONT_INHERIT "font-family:inherit"
    add_word_args ("  pre.display { %s }\n", CSS_FONT_INHERIT);
    add_word_args ("  pre.format  { %s }\n", CSS_FONT_INHERIT);

    /* Alternatively, we could do <font size=-1> in insertion.c, but this
       way makes it easier to override.  */
#define CSS_FONT_SMALLER "font-size:smaller"
    add_word_args ("  pre.smalldisplay { %s; %s }\n", CSS_FONT_INHERIT,
                   CSS_FONT_SMALLER);
    add_word_args ("  pre.smallformat  { %s; %s }\n", CSS_FONT_INHERIT,
                   CSS_FONT_SMALLER);
    add_word_args ("  pre.smallexample { %s }\n", CSS_FONT_SMALLER);
    add_word_args ("  pre.smalllisp    { %s }\n", CSS_FONT_SMALLER);

    /* Write out any css code from the user's --css-file.  */
    if (css_inline)
      add_word (css_inline);

    add_word ("--></style>\n");
  }

  add_word ("</head>\n<body>\n");

  if (title && !html_title_written && titlepage_cmd_present)
    {
      add_word_args ("<h1 class=\"settitle\">%s</h1>\n", html_title);
      html_title_written = 1;
    }
}



/* Append CHAR to BUFFER, (re)allocating as necessary.  We don't handle
   null characters.  */

typedef struct
{
  unsigned size;    /* allocated */
  unsigned length;  /* used */
  char *buffer;
} buffer_type;


static buffer_type *
init_buffer ()
{
  buffer_type *buf = xmalloc (sizeof (buffer_type));
  buf->length = 0;
  buf->size = 0;
  buf->buffer = NULL;

  return buf;
}


static void
append_char (buf, c)
    buffer_type *buf;
    int c;
{
  buf->length++;
  if (buf->length >= buf->size)
    {
      buf->size += 100;
      buf->buffer = xrealloc (buf->buffer, buf->size);
    }
  buf->buffer[buf->length - 1] = c;
  buf->buffer[buf->length] = 0;
}


/* Read the cascading style-sheet file FILENAME.  Write out any @import
   commands, which must come first, by the definition of css.  If the
   file contains any actual css code following the @imports, return it;
   else return NULL.  */

static char *
process_css_file (filename)
    char *filename;
{
  int c, lastchar;
  FILE *f;
  buffer_type *import_text = init_buffer ();
  buffer_type *inline_text = init_buffer ();
  unsigned lineno = 1;
  enum { null_state, comment_state, import_state, inline_state } state
    = null_state, prev_state;

  /* read from stdin if `-' is the filename.  */
  f = STREQ (filename, "-") ? stdin : fopen (filename, "r");
  if (!f)
    {
      error (_("%s: could not open --css-file: %s"), progname, filename);
      return NULL;
    }

  /* Read the file.  The @import statements must come at the beginning,
     with only whitespace and comments allowed before any inline css code.  */
  while ((c = getc (f)) >= 0)
    {
      if (c == '\n')
        lineno++;

      switch (state)
        {
        case null_state: /* between things */
          if (c == '@')
            {
              /* If there's some other @command, just call it an
                 import, it's all the same to us.  So don't bother
                 looking for the `import'.  */
              append_char (import_text, c);
              state = import_state;
            }
          else if (c == '/')
            { /* possible start of a comment */
              int nextchar = getc (f);
              if (nextchar == '*')
                state = comment_state;
              else
                {
                  ungetc (nextchar, f); /* wasn't a comment */
                  state = inline_state;
                }
            }
          else if (isspace (c))
            ; /* skip whitespace; maybe should use c_isspace?  */

          else
            /* not an @import, not a comment, not whitespace: we must
               have started the inline text.  */
            state = inline_state;

          if (state == inline_state)
            append_char (inline_text, c);

          if (state != null_state)
            prev_state = null_state;
          break;

        case comment_state:
          if (c == '/' && lastchar == '*')
            state = prev_state;  /* end of comment */
          break;  /* else ignore this comment char */

        case import_state:
          append_char (import_text, c);  /* include this import char */
          if (c == ';')
            { /* done with @import */
              append_char (import_text, '\n');  /* make the output nice */
              state = null_state;
              prev_state = import_state;
            }
          break;

        case inline_state:
          /* No harm in writing out comments, so don't bother parsing
             them out, just append everything.  */
          append_char (inline_text, c);
          break;
        }

      lastchar = c;
    }

  /* Reached the end of the file.  We should not be still in a comment.  */
  if (state == comment_state)
    warning (_("%s:%d: --css-file ended in comment"), filename, lineno);

  /* Write the @import text, if any.  */
  if (import_text->buffer)
    {
      add_word (import_text->buffer);
      free (import_text->buffer);
      free (import_text);
    }

  /* We're wasting the buffer struct memory, but so what.  */
  return inline_text->buffer;
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



/* Save current tag.  */
void
push_tag (tag)
     char *tag;
{
  HSTACK *newstack = xmalloc (sizeof (HSTACK));

  newstack->tag = tag;
  newstack->next = htmlstack;
  htmlstack = newstack;
}

/* Get last tag.  */
void
pop_tag ()
{
  HSTACK *tos = htmlstack;

  if (!tos)
    {
      line_error (_("[unexpected] no html tag to pop"));
      return;
    }

  htmlstack = htmlstack->next;
  free (tos);
}

/* Open or close TAG according to START_OR_END. */
void
insert_html_tag (start_or_end, tag)
     int start_or_end;
     char *tag;
{
  char *old_tag = NULL;
  int do_return = 0;

  if (!paragraph_is_open && (start_or_end == START))
    {
      /* Need to compensate for the <p> we are about to insert, or
	 else cm_xxx functions that call us will get wrong text
	 between START and END.  */
      adjust_braces_following (output_paragraph_offset, 3);
      add_word ("<p>");
    }

  if (start_or_end != START)
    pop_tag ();

  if (htmlstack)
    old_tag = htmlstack->tag;

  if (htmlstack
      && (strcmp (htmlstack->tag, tag) == 0))
    do_return = 1;

  if (start_or_end == START)
    push_tag (tag);

  if (do_return)
    return;

  /* texinfo.tex doesn't support more than one font attribute
     at the same time.  */
  if ((start_or_end == START) && old_tag && *old_tag)
    {
      add_word ("</");
      add_word (old_tag);
      add_char ('>');
    }

  if (*tag)
    {
      add_char ('<');
      if (start_or_end != START)
        add_char ('/');
      add_word (tag);
      add_char ('>');
    }

  if ((start_or_end != START) && old_tag && *old_tag)
    {
      add_char ('<');
      add_word (old_tag);
      add_char ('>');
    }
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
	      line_error (_("[unexpected] invalid node name: `%s'"), nodename);
	      xexit (1);
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
