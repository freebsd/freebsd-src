/* toc.c -- table of contents handling.
   $Id: toc.c,v 1.22 2002/04/01 14:07:11 karl Exp $

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Karl Heinz Marbaise <kama@hippo.fido.de>.  */

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



/* array of toc entries */
static TOC_ENTRY_ELT **toc_entry_alist = NULL;

/* toc_counter start from 0 ... n for every @chapter, @section ... */
static int toc_counter = 0;

/* the file where we found the @contents directive */
char *contents_filename;

/* the file where we found the @shortcontents directive */
char *shortcontents_filename;

static const char contents_placebo[] = "\n...Table of Contents...\n";
static const char shortcontents_placebo[] = "\n...Short Contents...\n";
static const char lots_of_stars[] =
"***************************************************************************";


/* Routine to add an entry to the table of contents */
int
toc_add_entry (tocname, level, node_name, anchor)
     char *tocname;
     int level;
     char *node_name;
     char *anchor;
{
  char *tocname_and_node, *expanded_node, *s, *d;
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
              if (*s == '&')
                {
                  strcpy (d, "&amp;");
                  d += 5;
                }
              else if (! URL_SAFE_CHAR (*s))
                {
                  sprintf (d, "%%%x", (unsigned char) *s);
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
toc_find_section_of_node (node)
     char *node;
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
toc_free ()
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
contents_update_html (fp)
     FILE *fp;
{
  int i;
  int k;
  int last_level;

  /* does exist any toc? */
  if (!toc_counter)
      /* no, so return to sender ;-) */
      return;

  flush_output ();      /* in case we are writing stdout */

  fprintf (fp, "\n<h2>%s</h2>\n<ul>\n", _("Table of Contents"));

  last_level = toc_entry_alist[0]->level;

  for (i = 0; i < toc_counter; i++)
    {
      if (toc_entry_alist[i]->level > last_level)
        {
          /* unusual, but it is possible
             @chapter ...
             @subsubsection ...      ? */
          for (k = 0; k < (toc_entry_alist[i]->level-last_level); k++)
            fputs ("<ul>\n", fp);
        }
      else if (toc_entry_alist[i]->level < last_level)
        {
          /* @subsubsection ...
             @chapter ... this IS usual.*/
          for (k = 0; k < (last_level-toc_entry_alist[i]->level); k++)
            fputs ("</ul>\n", fp);
        }

      /* No double entries in TOC.  */
      if (!(i && strcmp (toc_entry_alist[i]->name,
			 toc_entry_alist[i-1]->name) == 0))
        {
          /* each toc entry is a list item.  */
          fputs ("<li>", fp);

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
	      fprintf (fp, "<a name=\"toc_%.*s\"></a>\n    ",
		       p - toc_entry_alist[i]->name, toc_entry_alist[i]->name);
	    }

          /* Insert link -- to an external file if splitting, or
             within the current document if not splitting.  */
	  fprintf (fp, "<a href=\"%s#%s</a>\n",
		   splitting ? toc_entry_alist[i]->html_file : "",
		   toc_entry_alist[i]->name);
        }

      last_level = toc_entry_alist[i]->level;
    }

  /* Go back to start level. */
  if (toc_entry_alist[0]->level < last_level)
    for (k = 0; k < (last_level-toc_entry_alist[0]->level); k++)
      fputs ("</ul>\n", fp);

  fputs ("</ul>\n\n", fp);
}

/* print table of contents in ASCII (--no-headers)
   May be we should create a new command line switch --ascii ? */
static void
contents_update_info (fp)
     FILE *fp;
{
  int i;
  int k;

  if (!toc_counter)
      return;

  flush_output ();      /* in case we are writing stdout */

  fprintf (fp, "%s\n%.*s\n\n", _("Table of Contents"),
           (int) strlen (_("Table of Contents")), lots_of_stars);

  for (i = 0; i < toc_counter; i++)
    {
      if (toc_entry_alist[i]->level == 0)
        fputs ("\n", fp);

      /* indention with two spaces per level, should this
         changed? */
      for (k = 0; k < toc_entry_alist[i]->level; k++)
        fputs ("  ", fp);

      fprintf (fp, "%s\n", toc_entry_alist[i]->name);
    }
  fputs ("\n\n", fp);
}

/* shortcontents in HTML; Should this produce a standalone file? */
static void
shortcontents_update_html (fp)
     FILE *fp;
{
  int i;
  char *toc_file;

  /* does exist any toc? */
  if (!toc_counter)
    return;

  flush_output ();      /* in case we are writing stdout */

  fprintf (fp, "\n<h2>%s</h2>\n<ul>\n", _("Short Contents"));

  if (contents_filename)
    toc_file = filename_part (contents_filename);

  for (i = 0; i < toc_counter; i++)
    {
      char *name = toc_entry_alist[i]->name;

      if (toc_entry_alist[i]->level == 0)
	{
	  if (contents_filename)
	    fprintf (fp, "<li><a href=\"%s#toc_%s</a>\n",
		     splitting ? toc_file : "", name);
	  else
	    fprintf (fp, "<a href=\"%s#%s</a>\n",
		     splitting ? toc_entry_alist[i]->html_file : "", name);
	}
    }
  fputs ("</ul>\n\n", fp);
  if (contents_filename)
    free (toc_file);
}

/* short contents in ASCII (--no-headers).  */
static void
shortcontents_update_info (fp)
     FILE *fp;
{
  int i;

  if (!toc_counter)
      return;

  flush_output ();      /* in case we are writing stdout */

  fprintf (fp, "%s\n%.*s\n\n", _("Short Contents"),
           (int) strlen (_("Short Contents")), lots_of_stars);

  for (i = 0; i < toc_counter; i++)
    {
      if (toc_entry_alist[i]->level == 0)
        fprintf (fp, "%s\n", toc_entry_alist[i]->name);
    }
  fputs ("\n\n", fp);
}


static FILE *toc_fp;
static char *toc_buf;

static int
rewrite_top (fname, placebo)
     const char *fname, *placebo;
{
  int idx;

  /* Can't rewrite standard output or the null device.  No point in
     complaining.  */
  if (STREQ (fname, "-")
      || FILENAME_CMP (fname, NULL_DEVICE) == 0
      || FILENAME_CMP (fname, ALSO_NULL_DEVICE) == 0)
    return -1;

  toc_buf = find_and_load (fname);

  if (!toc_buf)
    {
      fs_error (fname);
      return -1;
    }

  idx = search_forward (placebo, 0);

  if (idx < 0)
    {
      error (_("%s: TOC should be here, but it was not found"), fname);
      return -1;
    }

  toc_fp = fopen (fname, "w");
  if (!toc_fp)
    {
      fs_error (fname);
      return -1;
    }

  if (fwrite (toc_buf, 1, idx, toc_fp) != idx)
    {
      fs_error (fname);
      return -1;
    }

  return idx + strlen (placebo);
}

static void
contents_update ()
{
  int cont_idx = rewrite_top (contents_filename, contents_placebo);

  if (cont_idx < 0)
    return;

  if (html)
    contents_update_html (toc_fp);
  else
    contents_update_info (toc_fp);

  if (fwrite (toc_buf + cont_idx, 1, input_text_length - cont_idx, toc_fp)
      != input_text_length - cont_idx
      || fclose (toc_fp) != 0)
    fs_error (contents_filename);
}

static void
shortcontents_update ()
{
  int cont_idx = rewrite_top (shortcontents_filename, shortcontents_placebo);

  if (cont_idx < 0)
    return;

  if (html)
    shortcontents_update_html (toc_fp);
  else
    shortcontents_update_info (toc_fp);

  if (fwrite (toc_buf + cont_idx, 1, input_text_length - cont_idx - 1, toc_fp)
      != input_text_length - cont_idx - 1
      || fclose (toc_fp) != 0)
    fs_error (shortcontents_filename);
}

void
toc_update ()
{
  if (!html && !no_headers)
    return;

  if (contents_filename)
    contents_update ();
  if (shortcontents_filename)
    shortcontents_update ();
}

void
cm_contents (arg)
     int arg;
{
  if ((html || no_headers) && arg == START)
    {
      if (contents_filename)
        {
          free (contents_filename);
          contents_filename = NULL;
        }

      if (contents_filename && STREQ (contents_filename, "-"))
        {
          if (html)
            contents_update_html (stdout);
          else
            contents_update_info (stdout);
        }
      else
        {
          if (!executing_string && html)
            html_output_head ();
          contents_filename = xstrdup (current_output_filename);
          insert_string (contents_placebo); /* just mark it, for now */
        }
    }
}

void
cm_shortcontents (arg)
     int arg;
{
  if ((html || no_headers) && arg == START)
    {
      if (shortcontents_filename)
        {
          free (shortcontents_filename);
          shortcontents_filename = NULL;
        }

      if (shortcontents_filename && STREQ (shortcontents_filename, "-"))
        {
          if (html)
            shortcontents_update_html (stdout);
          else
            shortcontents_update_info (stdout);
        }
      else
        {
          if (!executing_string && html)
            html_output_head ();
          shortcontents_filename = xstrdup (current_output_filename);
          insert_string (shortcontents_placebo); /* just mark it, for now */
        }
    }
}
