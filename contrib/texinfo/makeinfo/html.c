/* html.c -- html-related utilities.
   $Id: html.c,v 1.28 2004/12/06 01:13:06 karl Exp $

   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004 Free Software
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
#include "cmds.h"
#include "files.h"
#include "html.h"
#include "lang.h"
#include "makeinfo.h"
#include "node.h"
#include "sectioning.h"


/* Append CHAR to BUFFER, (re)allocating as necessary.  We don't handle
   null characters.  */

typedef struct
{
  unsigned size;    /* allocated */
  unsigned length;  /* used */
  char *buffer;
} buffer_type;

static buffer_type *
init_buffer (void)
{
  buffer_type *buf = xmalloc (sizeof (buffer_type));
  buf->length = 0;
  buf->size = 0;
  buf->buffer = NULL;

  return buf;
}

static void
append_char (buffer_type *buf, int c)
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
process_css_file (char *filename)
{
  int c;
  int lastchar = 0;
  FILE *f;
  buffer_type *import_text = init_buffer ();
  buffer_type *inline_text = init_buffer ();
  unsigned lineno = 1;
  enum { null_state, comment_state, import_state, inline_state } state
    = null_state, prev_state;

  prev_state = null_state;

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
            { /* Only @import and @charset should switch into
                 import_state, other @-commands, such as @media, should
                 put us into inline_state.  I don't think any other css
                 @-commands start with `i' or `c', although of course
                 this will break when such a command is defined.  */
              int nextchar = getc (f);
              if (nextchar == 'i' || nextchar == 'c')
                {
                  append_char (import_text, c);
                  state = import_state;
                }
              else
                {
                  ungetc (nextchar, f);  /* wasn't an @import */
                  state = inline_state;
                }
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

HSTACK *htmlstack = NULL;

/* See html.h.  */
int html_output_head_p = 0;
int html_title_written = 0;

void
html_output_head (void)
{
  static const char *html_title = NULL;
  char *encoding;

  if (html_output_head_p)
    return;
  html_output_head_p = 1;

  encoding = current_document_encoding ();

  /* The <title> should not have markup, so use text_expansion.  */
  if (!html_title)
    html_title = escape_string (title ?
        text_expansion (title) : (char *) _("Untitled"));

  /* Make sure this is the very first string of the output document.  */
  output_paragraph_offset = 0;

  add_html_block_elt_args ("<html lang=\"%s\">\n<head>\n",
      language_table[language_code].abbrev);

  /* When splitting, add current node's name to title if it's available and not
     Top.  */
  if (splitting && current_node && !STREQ (current_node, "Top"))
    add_word_args ("<title>%s - %s</title>\n",
        escape_string (xstrdup (current_node)), html_title);
  else
    add_word_args ("<title>%s</title>\n",  html_title);

  add_word ("<meta http-equiv=\"Content-Type\" content=\"text/html");
  if (encoding && *encoding)
    add_word_args ("; charset=%s", encoding);
                   
  add_word ("\">\n");

  if (!document_description)
    document_description = html_title;

  add_word_args ("<meta name=\"description\" content=\"%s\">\n",
                 document_description);
  add_word_args ("<meta name=\"generator\" content=\"makeinfo %s\">\n",
                 VERSION);

  /* Navigation bar links.  */
  if (!splitting)
    add_word ("<link title=\"Top\" rel=\"top\" href=\"#Top\">\n");
  else if (tag_table)
    {
      /* Always put a top link.  */
      add_word ("<link title=\"Top\" rel=\"start\" href=\"index.html#Top\">\n");

      /* We already have a top link, avoid duplication.  */
      if (tag_table->up && !STREQ (tag_table->up, "Top"))
        add_link (tag_table->up, "rel=\"up\"");

      if (tag_table->prev)
        add_link (tag_table->prev, "rel=\"prev\"");

      if (tag_table->next)
        add_link (tag_table->next, "rel=\"next\"");

      /* fixxme: Look for a way to put links to various indices in the
         document.  Also possible candidates to be added here are First and
         Last links.  */
    }
  else
    {
      /* We are splitting, but we neither have a tag_table.  So this must be
         index.html.  So put a link to Top. */
      add_word ("<link title=\"Top\" rel=\"start\" href=\"#Top\">\n");
    }

  add_word ("<link href=\"http://www.gnu.org/software/texinfo/\" \
rel=\"generator-home\" title=\"Texinfo Homepage\">\n");

  if (copying_text)
    { /* It is not ideal that we include the html markup here within
         <head>, so we use text_expansion.  */
      insert_string ("<!--\n");
      insert_string (text_expansion (copying_text));
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

    /* Since HTML doesn't have a sc element, we use span with a bit of
       CSS spice instead.  */
#define CSS_FONT_SMALL_CAPS "font-variant:small-caps"
    add_word_args ("  span.sc    { %s }\n", CSS_FONT_SMALL_CAPS);

    /* Roman (default) font class, closest we can come.  */
#define CSS_FONT_ROMAN "font-family:serif; font-weight:normal;"
    add_word_args ("  span.roman { %s } \n", CSS_FONT_ROMAN);

    /* Sans serif font class.  */
#define CSS_FONT_SANSSERIF "font-family:sans-serif; font-weight:normal;"
    add_word_args ("  span.sansserif { %s } \n", CSS_FONT_SANSSERIF);

    /* Write out any css code from the user's --css-file.  */
    if (css_inline)
      insert_string (css_inline);

    add_word ("--></style>\n");
  }

  add_word ("</head>\n<body>\n");

  if (title && !html_title_written && titlepage_cmd_present)
    {
      add_word_args ("<h1 class=\"settitle\">%s</h1>\n", html_title);
      html_title_written = 1;
    }

  free (encoding);
}

/* Escape HTML special characters in the string if necessary,
   returning a pointer to a possibly newly-allocated one. */
char *
escape_string (char *string)
{
  char *newstring;
  int i = 0, newlen = 0;

  do
    {
      /* Find how much to allocate. */
      switch (string[i])
        {
        case '"':
          newlen += 6;          /* `&quot;' */
          break;
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
        case '"':
          strcpy (newstring, "&quot;");
          newstring += 6;
          break;
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
static void
push_tag (char *tag, char *attribs)
{
  HSTACK *newstack = xmalloc (sizeof (HSTACK));

  newstack->tag = tag;
  newstack->attribs = xstrdup (attribs);
  newstack->next = htmlstack;
  htmlstack = newstack;
}

/* Get last tag.  */
static void
pop_tag (void)
{
  HSTACK *tos = htmlstack;

  if (!tos)
    {
      line_error (_("[unexpected] no html tag to pop"));
      return;
    }

  free (htmlstack->attribs);

  htmlstack = htmlstack->next;
  free (tos);
}

/* Check if tag is an empty or a whitespace only element.
   If so, remove it, keeping whitespace intact.  */
int
rollback_empty_tag (char *tag)
{
  int check_position = output_paragraph_offset;
  int taglen = strlen (tag);
  int rollback_happened = 0;
  char *contents = "";
  char *contents_canon_white = "";

  /* If output_paragraph is empty, we cannot rollback :-\  */
  if (output_paragraph_offset <= 0)
    return 0;

  /* Find the end of the previous tag.  */
  while (output_paragraph[check_position-1] != '>' && check_position > 0)
    check_position--;

  /* Save stuff between tag's end to output_paragraph's end.  */
  if (check_position != output_paragraph_offset)
    {
      contents = xmalloc (output_paragraph_offset - check_position + 1);
      memcpy (contents, output_paragraph + check_position,
          output_paragraph_offset - check_position);

      contents[output_paragraph_offset - check_position] = '\0';

      contents_canon_white = xstrdup (contents);
      canon_white (contents_canon_white);
    }

  /* Find the start of the previous tag.  */
  while (output_paragraph[check_position-1] != '<' && check_position > 0)
    check_position--;

  /* Check to see if this is the tag.  */
  if (strncmp ((char *) output_paragraph + check_position, tag, taglen) == 0
      && (whitespace (output_paragraph[check_position + taglen])
          || output_paragraph[check_position + taglen] == '>'))
    {
      if (!contents_canon_white || !*contents_canon_white)
        {
          /* Empty content after whitespace removal, so roll it back.  */
          output_paragraph_offset = check_position - 1;
          rollback_happened = 1;

          /* Original contents may not be empty (whitespace.)  */
          if (contents && *contents)
            {
              insert_string (contents);
              free (contents);
            }
        }
    }

  return rollback_happened;
}

/* Open or close TAG according to START_OR_END. */
void
#if defined (VA_FPRINTF) && __STDC__
insert_html_tag_with_attribute (int start_or_end, char *tag, char *format, ...)
#else
insert_html_tag_with_attribute (start_or_end, tag, format, va_alist)
     int start_or_end;
     char *tag;
     char *format;
     va_dcl
#endif
{
  char *old_tag = NULL;
  char *old_attribs = NULL;
  char formatted_attribs[2000]; /* xx no fixed limits */
  int do_return = 0;
  extern int in_html_elt;

  if (start_or_end != START)
    pop_tag ();

  if (htmlstack)
    {
      old_tag = htmlstack->tag;
      old_attribs = htmlstack->attribs;
    }
  
  if (format)
    {
#ifdef VA_SPRINTF
      va_list ap;
#endif

      VA_START (ap, format);
#ifdef VA_SPRINTF
      VA_SPRINTF (formatted_attribs, format, ap);
#else
      sprintf (formatted_attribs, format, a1, a2, a3, a4, a5, a6, a7, a8);
#endif
      va_end (ap);
    }
  else
    formatted_attribs[0] = '\0';

  /* Exception: can nest multiple spans.  */
  if (htmlstack
      && STREQ (htmlstack->tag, tag)
      && !(STREQ (tag, "span") && STREQ (old_attribs, formatted_attribs)))
    do_return = 1;

  if (start_or_end == START)
    push_tag (tag, formatted_attribs);

  if (do_return)
    return;

  in_html_elt++;

  /* texinfo.tex doesn't support more than one font attribute
     at the same time.  */
  if ((start_or_end == START) && old_tag && *old_tag
      && !rollback_empty_tag (old_tag))
    add_word_args ("</%s>", old_tag);

  if (*tag)
    {
      if (start_or_end == START)
        add_word_args (format ? "<%s %s>" : "<%s>", tag, formatted_attribs);
      else if (!rollback_empty_tag (tag))
        /* Insert close tag only if we didn't rollback,
           in which case the opening tag is removed.  */
        add_word_args ("</%s>", tag);
    }

  if ((start_or_end != START) && old_tag && *old_tag)
    add_word_args (strlen (old_attribs) > 0 ? "<%s %s>" : "<%s>",
        old_tag, old_attribs);

  in_html_elt--;
}

void
insert_html_tag (int start_or_end, char *tag)
{
  insert_html_tag_with_attribute (start_or_end, tag, NULL);
}

/* Output an HTML <link> to the filename for NODE, including the
   other string as extra attributes. */
void
add_link (char *nodename, char *attributes)
{
  if (nodename)
    {
      add_html_elt ("<link ");
      add_word_args ("%s", attributes);
      add_word_args (" href=\"");
      add_anchor_name (nodename, 1);
      add_word_args ("\" title=\"%s\">\n", nodename);
    }
}

/* Output NAME with characters escaped as appropriate for an anchor
   name, i.e., escape URL special characters with our _00hh convention
   if OLD is zero.  (See the manual for details on the new scheme.)
   
   If OLD is nonzero, generate the node name with the 4.6-and-earlier
   convention of %hh (and more special characters output as-is, notably
   - and *).  This is only so that external references to old names can
   still work with HTML generated by the new makeinfo; the gcc folks
   needed this.  Our own HTML does not refer to these names.  */

void
add_escaped_anchor_name (char *name, int old)
{
  canon_white (name);

  if (!old && !strchr ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
                       *name))
    { /* XHTML does not allow anything but an ASCII letter to start an
         identifier.  Therefore kludge in this constant string if we
         have a nonletter.  */
      add_word ("g_t");
    }

  for (; *name; name++)
    {
      if (cr_or_whitespace (*name))
        add_char ('-');

      else if (!old && !URL_SAFE_CHAR (*name))
        /* Cast so characters with the high bit set are treated as >128,
           for example o-umlaut should be 246, not -10.  */
        add_word_args ("_00%x", (unsigned char) *name);

      else if (old && !URL_SAFE_CHAR (*name) && !OLD_URL_SAFE_CHAR (*name))
        /* Different output convention, but still cast as above.  */
        add_word_args ("%%%x", (unsigned char) *name);

      else
        add_char (*name);
    }
}

/* Insert the text for the name of a reference in an HTML anchor
   appropriate for NODENAME.
   
   If HREF is zero, generate text for name= in the new node name
     conversion convention.
   If HREF is negative, generate text for name= in the old convention.
   If HREF is positive, generate the name for an href= attribute, i.e.,
     including the `#' if it's an internal reference.   */
void
add_anchor_name (char *nodename, int href)
{
  if (href > 0)
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
  else if (strcasecmp (nodename, "top") == 0)
    add_word ("Top");
  else
    add_escaped_anchor_name (nodename, href < 0);
}

/* Insert the text for the name of a reference in an HTML url, aprropriate
   for NODENAME */
void
add_url_name (char *nodename, int href)
{
    add_nodename_to_filename (nodename, href);
}

/* Convert non [A-Za-z0-9] to _00xx, where xx means the hexadecimal
   representation of the ASCII character.  Also convert spaces and
   newlines to dashes.  */
static void
fix_filename (char *filename)
{
  int i;
  int len = strlen (filename);
  char *oldname = xstrdup (filename);

  *filename = '\0';

  for (i = 0; i < len; i++)
    {
      if (cr_or_whitespace (oldname[i]))
        strcat (filename, "-");
      else if (URL_SAFE_CHAR (oldname[i]))
        strncat (filename, (char *) oldname + i, 1);
      else
        {
          char *hexchar = xmalloc (6 * sizeof (char));
          sprintf (hexchar, "_00%x", (unsigned char) oldname[i]);
          strcat (filename, hexchar);
          free (hexchar);
        }

      /* Check if we are nearing boundaries.  */
      if (strlen (filename) >= PATH_MAX - 20)
        break;
    }

  free (oldname);
}

/* As we can't look-up a (forward-referenced) nodes' html filename
   from the tentry, we take the easy way out.  We assume that
   nodenames are unique, and generate the html filename from the
   nodename, that's always known.  */
static char *
nodename_to_filename_1 (char *nodename, int href)
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
add_nodename_to_filename (char *nodename, int href)
{
  /* for now, don't check: always output filename */
  char *filename = nodename_to_filename_1 (nodename, href);
  add_word (filename);
  free (filename);
}

char *
nodename_to_filename (char *nodename)
{
  /* The callers of nodename_to_filename use the result to produce
     <a href=, so call nodename_to_filename_1 with last arg non-zero.  */
  return nodename_to_filename_1 (nodename, 1);
}
