/* xref.c -- cross references for Texinfo.
   $Id: xref.c,v 1.4 2004/12/21 17:28:35 karl Exp $

   Copyright (C) 2004 Free Software Foundation, Inc.

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
#include "float.h"
#include "html.h"
#include "index.h"
#include "macro.h"
#include "makeinfo.h"
#include "node.h"
#include "xml.h"
#include "xref.h"

/* Flags which control initial output string for xrefs. */
int px_ref_flag = 0;
int ref_flag = 0;

/* Called in the multiple-argument case to make sure we generate a valid
   Info reference.  In the single-argument case, the :: we output
   suffices for the Info readers to find the end of the reference.  */
static void
add_xref_punctuation (void)
{
  if (px_ref_flag || ref_flag)  /* user inserts punct after @xref */
    {
      /* Check if there's already punctuation.  */
      int next_char = next_nonwhitespace_character ();

      if (next_char == -1)
        /* EOF while looking for punctuation, let's
           insert a period instead of crying.  */
        add_char ('.');
      else if (next_char != ',' && next_char != '.')
        /* period and comma terminate xrefs, and nothing else.  Instead
           of generating an Info reference that can't be followed,
           though, just insert a period.  Not pretty, but functional.  */
        add_char ('.');
    }
}

/* Return next comma-delimited argument, but do not cross a close-brace
   boundary.  Clean up whitespace, too.  If EXPAND is nonzero, replace
   the entire brace-delimited argument list with its expansion before
   looking for the next comma.  */
char *
get_xref_token (int expand)
{
  char *string = 0;

  if (docbook)
    xml_in_xref_token = 1;

  if (expand)
    {
      int old_offset = input_text_offset;
      int old_lineno = line_number;

      get_until_in_braces ("}", &string);
      if (curchar () == '}')    /* as opposed to end of text */
        input_text_offset++;
      if (input_text_offset > old_offset)
        {
          int limit = input_text_offset;

          input_text_offset = old_offset;
          line_number = old_lineno;
          only_macro_expansion++;
          replace_with_expansion (input_text_offset, &limit);
          only_macro_expansion--;
        }
      free (string);
    }

  get_until_in_braces (",", &string);
  if (curchar () == ',')
    input_text_offset++;
  fix_whitespace (string);

  if (docbook)
    xml_in_xref_token = 0;

  return string;
}


/* NOTE: If you wonder why the HTML output is produced with such a
   peculiar mix of calls to add_word and execute_string, here's the
   reason.  get_xref_token (1) expands all macros in a reference, but
   any other commands, like @value, @@, etc., are left intact.  To
   expand them, we need to run the arguments through execute_string.
   However, characters like <, &, > and others cannot be let into
   execute_string, because they will be escaped.  See the mess?  */

/* Make a cross reference. */
void
cm_xref (int arg)
{
  if (arg == START)
    {
      char *arg1 = get_xref_token (1); /* expands all macros in xref */
      char *arg2 = get_xref_token (0);
      char *arg3 = get_xref_token (0);
      char *arg4 = get_xref_token (0);
      char *arg5 = get_xref_token (0);
      char *tem;

      /* "@xref{,Foo,, Bar, Baz} is not valid usage of @xref.  The
         first argument must never be blank." --rms.
         We hereby comply by disallowing such constructs.  */
      if (!*arg1)
        line_error (_("First argument to cross-reference may not be empty"));

      if (docbook)
        {
          if (!ref_flag)
            add_word (px_ref_flag || printing_index
                ? (char *) _("see ") : (char *) _("See "));

          if (!*arg4 && !*arg5)
            {
              char *arg1_id = xml_id (arg1);

              if (*arg2 || *arg3)
                {
                  xml_insert_element_with_attribute (XREFNODENAME, START,
                                                     "linkend=\"%s\"", arg1_id);
                  free (arg1_id);
                  execute_string ("%s", *arg3 ? arg3 : arg2);
                  xml_insert_element (XREFNODENAME, END);
                }
              else
                {
                  xml_insert_element_with_attribute (XREF, START,
                                                     "linkend=\"%s\"", arg1_id);
                  xml_insert_element (XREF, END);
                  free (arg1_id);
                }
            }
          else if (*arg5)
            {
              add_word_args (_("See section ``%s'' in "), *arg3 ? arg3 : arg1);
              xml_insert_element (CITE, START);
              add_word (arg5);
              xml_insert_element (CITE, END);
            }
          else if (*arg4)
            {
              /* Very sad, we are losing xrefs made to ``info only'' books.  */
            }
        }
      else if (xml)
        {
          if (!ref_flag)
            add_word_args ("%s", px_ref_flag ? _("see ") : _("See "));

          xml_insert_element (XREF, START);
          xml_insert_element (XREFNODENAME, START);
          execute_string ("%s", arg1);
          xml_insert_element (XREFNODENAME, END);
          if (*arg2)
            {
              xml_insert_element (XREFINFONAME, START);
              execute_string ("%s", arg2);
              xml_insert_element (XREFINFONAME, END);
            }
          if (*arg3)
            {
              xml_insert_element (XREFPRINTEDDESC, START);
              execute_string ("%s", arg3);
              xml_insert_element (XREFPRINTEDDESC, END);
            }
          if (*arg4)
            {
              xml_insert_element (XREFINFOFILE, START);
              execute_string ("%s", arg4);
              xml_insert_element (XREFINFOFILE, END);
            }
          if (*arg5)
            {
              xml_insert_element (XREFPRINTEDNAME, START);
              execute_string ("%s", arg5);
              xml_insert_element (XREFPRINTEDNAME, END);
            }
          xml_insert_element (XREF, END);
        }
      else if (html)
        {
          if (!ref_flag)
            add_word_args ("%s", px_ref_flag ? _("see ") : _("See "));
        }
      else
        add_word_args ("%s", px_ref_flag ? "*note " : "*Note ");

      if (!xml)
        {
          if (*arg5 || *arg4)
            {
              /* arg1 - node name
                 arg2 - reference name
                 arg3 - title or topic (and reference name if arg2 is NULL)
                 arg4 - info file name
                 arg5 - printed manual title  */
              char *ref_name;

              if (!*arg2)
                {
                  if (*arg3)
                    ref_name = arg3;
                  else
                    ref_name = arg1;
                }
              else
                ref_name = arg2;

              if (html)
                { /* More to do eventually, down to Unicode
                     Normalization Form C.  See the HTML Xref nodes in
                     the manual.  */
                  char *file_arg = arg4;
                  add_html_elt ("<a href=");

                  {
                    /* If there's a directory part, ignore it.  */
                    char *p = strrchr (file_arg, '/');
                    if (p)
                      file_arg = p + 1;

                  /* If there's a dot, make it a NULL terminator, so the
                     extension does not get into the way.  */
                    p = strrchr (file_arg , '.');
                    if (p != NULL)
                      *p = 0;
                  }
                  
                  if (! *file_arg)
                warning (_("Empty file name for HTML cross reference in `%s'"),
                           arg4);

                  /* Note that if we are splitting, and the referenced
                     tag is an anchor rather than a node, we will
                     produce a reference to a file whose name is
                     derived from the anchor name.  However, only
                     nodes create files, so we are referencing a
                     non-existent file.  cm_anchor, which see, deals
                     with that problem.  */
                  if (splitting)
                    execute_string ("\"../%s/", file_arg);
                  else
                    execute_string ("\"%s.html", file_arg);
                  /* Do not collapse -- to -, etc., in references.  */
                  in_fixed_width_font++;
                  tem = expansion (arg1, 0); /* expand @-commands in node */
                  in_fixed_width_font--;
                  add_anchor_name (tem, 1);
                  free (tem);
                  add_word ("\">");
                  execute_string ("%s",ref_name);
                  add_word ("</a>");
                }
              else
                {
                  execute_string ("%s:", ref_name);
                  in_fixed_width_font++;
                  execute_string (" (%s)%s", arg4, arg1);
                  add_xref_punctuation ();
                  in_fixed_width_font--;
                }

              /* Free all of the arguments found. */
              if (arg1) free (arg1);
              if (arg2) free (arg2);
              if (arg3) free (arg3);
              if (arg4) free (arg4);
              if (arg5) free (arg5);
              return;
            }
          else
            remember_node_reference (arg1, line_number, followed_reference);

          if (*arg3)
            {
              if (html)
                {
                  add_html_elt ("<a href=\"");
                  in_fixed_width_font++;
                  tem = expansion (arg1, 0);
                  in_fixed_width_font--;
                  add_anchor_name (tem, 1);
                  free (tem);
                  add_word ("\">");
                  execute_string ("%s", *arg2 ? arg2 : arg3);
                  add_word ("</a>");
                }
              else
                {
                  execute_string ("%s:", *arg2 ? arg2 : arg3);
                  in_fixed_width_font++;
                  execute_string (" %s", arg1);
                  add_xref_punctuation ();
                  in_fixed_width_font--;
                }
            }
          else
            {
              if (html)
                {
                  add_html_elt ("<a href=\"");
                  in_fixed_width_font++;
                  tem = expansion (arg1, 0);
                  in_fixed_width_font--;
                  add_anchor_name (tem, 1);
                  free (tem);
                  add_word ("\">");
                  if (*arg2)
                    execute_string ("%s", arg2);
                  else
                    {
                      char *fref = get_float_ref (arg1);
                      execute_string ("%s", fref ? fref : arg1);
                      free (fref);
                    }
                  add_word ("</a>");
                }
              else
                {
                  if (*arg2)
                    {
                      execute_string ("%s:", arg2);
                      in_fixed_width_font++;
                      execute_string (" %s", arg1);
                      add_xref_punctuation ();
                      in_fixed_width_font--;
                    }
                  else
                    {
                      char *fref = get_float_ref (arg1);
                      if (fref)
                        { /* Reference is being made to a float.  */
                          execute_string ("%s:", fref);
                          in_fixed_width_font++;
                          execute_string (" %s", arg1);
                          add_xref_punctuation ();
                          in_fixed_width_font--;
                        }
                      else
                        {
                          in_fixed_width_font++;
                          execute_string ("%s::", arg1);
                          in_fixed_width_font--;
                        }
                    }
                }
            }
        }
      /* Free all of the arguments found. */
      if (arg1) free (arg1);
      if (arg2) free (arg2);
      if (arg3) free (arg3);
      if (arg4) free (arg4);
      if (arg5) free (arg5);
    }
  else
    { /* Check that the next non-whitespace character is valid to follow
         an xref (so Info readers can find the node names).
         `input_text_offset' is pointing at the "}" which ended the xref
         command.  This is not used for @pxref or @ref, since we insert
         the necessary punctuation above, if needed.  */
      int temp = next_nonwhitespace_character ();

      if (temp == -1)
        warning (_("End of file reached while looking for `.' or `,'"));
      else if (temp != '.' && temp != ',')
        warning (_("`.' or `,' must follow @%s, not `%c'"), command, temp);
    }
}

void
cm_pxref (int arg)
{
  if (arg == START)
    {
      px_ref_flag++;
      cm_xref (arg);
      px_ref_flag--;
    }
  /* cm_xref isn't called with arg == END, which disables the code near
     the end of cm_xref that checks for `.' or `,' after the
     cross-reference.  This is because cm_xref generates the required
     character itself (when needed) if px_ref_flag is set.  */
}

void
cm_ref (int arg)
{
  /* See the comments in cm_pxref about the checks for punctuation.  */
  if (arg == START)
    {
      ref_flag++;
      cm_xref (arg);
      ref_flag--;
    }
}

void
cm_inforef (int arg)
{
  if (arg == START)
    {
      char *node = get_xref_token (1); /* expands all macros in inforef */
      char *pname = get_xref_token (0);
      char *file = get_xref_token (0);

      /* (see comments at cm_xref).  */
      if (!*node)
        line_error (_("First argument to @inforef may not be empty"));

      if (xml && !docbook)
        {
          xml_insert_element (INFOREF, START);
          xml_insert_element (INFOREFNODENAME, START);
          execute_string ("%s", node);
          xml_insert_element (INFOREFNODENAME, END);
          if (*pname)
            {
              xml_insert_element (INFOREFREFNAME, START);
              execute_string ("%s", pname);
              xml_insert_element (INFOREFREFNAME, END);
            }
          xml_insert_element (INFOREFINFONAME, START);
          execute_string ("%s", file);
          xml_insert_element (INFOREFINFONAME, END);

          xml_insert_element (INFOREF, END);
        }
      else if (html)
        {
          char *tem;

          add_word ((char *) _("see "));
          /* html fixxme: revisit this */
          add_html_elt ("<a href=");
          if (splitting)
            execute_string ("\"../%s/", file);
          else
            execute_string ("\"%s.html", file);
          tem = expansion (node, 0);
          add_anchor_name (tem, 1);
          add_word ("\">");
          execute_string ("%s", *pname ? pname : tem);
          add_word ("</a>");
          free (tem);
        }
      else
        {
          if (*pname)
            execute_string ("*note %s: (%s)%s", pname, file, node);
          else
            execute_string ("*note (%s)%s::", file, node);
        }

      free (node);
      free (pname);
      free (file);
    }
}

/* A URL reference.  */
void
cm_uref (int arg)
{
  if (arg == START)
    {
      extern int printing_index;
      char *url  = get_xref_token (1); /* expands all macros in uref */
      char *desc = get_xref_token (0);
      char *replacement = get_xref_token (0);

      if (docbook)
        {
          xml_insert_element_with_attribute (UREF, START, "url=\"%s\"",
              text_expansion (url));
          if (*replacement)
            execute_string ("%s", replacement);
          else if (*desc)
            execute_string ("%s", desc);
          else
            execute_string ("%s", url);
          xml_insert_element (UREF, END);
        }
      else if (xml)
        {
          xml_insert_element (UREF, START);
          xml_insert_element (UREFURL, START);
          execute_string ("%s", url);
          xml_insert_element (UREFURL, END);
          if (*desc)
            {
              xml_insert_element (UREFDESC, START);
              execute_string ("%s", desc);
              xml_insert_element (UREFDESC, END);
            }
          if (*replacement)
            {
              xml_insert_element (UREFREPLACEMENT, START);
              execute_string ("%s", replacement);
              xml_insert_element (UREFREPLACEMENT, END);
            }
          xml_insert_element (UREF, END);
        }
      else if (html)
        { /* never need to show the url */
          add_html_elt ("<a href=");
          /* don't collapse `--' etc. in the url */
          in_fixed_width_font++;
          execute_string ("\"%s\"", url);
          in_fixed_width_font--;
          add_word (">");
          execute_string ("%s", *replacement ? replacement
                                : (*desc ? desc : url));
          add_word ("</a>");
        }
      else if (*replacement) /* do not show the url */
        execute_string ("%s", replacement);
      else if (*desc)        /* show both text and url */
        {
          execute_string ("%s ", desc);
          in_fixed_width_font++;
          execute_string ("(%s)", url);
          in_fixed_width_font--;
        }
      else /* no text at all, so have the url to show */
        {
          in_fixed_width_font++;
          execute_string ("%s%s%s",
                          printing_index ? "" : "`",
                          url,
                          printing_index ? "" : "'");
          in_fixed_width_font--;
        }
      if (url)
        free (url);
      if (desc)
        free (desc);
      if (replacement)
        free (replacement);
    }
}

/* An email reference.  */
void
cm_email (int arg)
{
  if (arg == START)
    {
      char *addr = get_xref_token (1); /* expands all macros in email */
      char *name = get_xref_token (0);

      if (xml && docbook)
        {
          xml_insert_element_with_attribute (EMAIL, START, "url=\"mailto:%s\"", addr);
          if (*name)
              execute_string ("%s", name);
          xml_insert_element (EMAIL, END);
        }
      else if (xml)
        {
          xml_insert_element (EMAIL, START);
          xml_insert_element (EMAILADDRESS, START);
          execute_string ("%s", addr);
          xml_insert_element (EMAILADDRESS, END);
          if (*name)
            {
              xml_insert_element (EMAILNAME, START);
              execute_string ("%s", name);
              xml_insert_element (EMAILNAME, END);
            }
          xml_insert_element (EMAIL, END);
        }
      else if (html)
        {
          add_html_elt ("<a href=");
          /* don't collapse `--' etc. in the address */
          in_fixed_width_font++;
          execute_string ("\"mailto:%s\"", addr);
          in_fixed_width_font--;
          add_word (">");
          execute_string ("%s", *name ? name : addr);
          add_word ("</a>");
        }
      else
        {
          execute_string ("%s%s", name, *name ? " "  : "");
          in_fixed_width_font++;
          execute_string ("<%s>", addr);
          in_fixed_width_font--;
        }

      if (addr)
        free (addr);
      if (name)
        free (name);
    }
}
