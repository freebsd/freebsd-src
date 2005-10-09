/* sectioning.c -- for @chapter, @section, ..., @contents ...
   $Id: sectioning.c,v 1.25 2004/07/05 22:23:23 karl Exp $

   Copyright (C) 1999, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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
#include "cmds.h"
#include "macro.h"
#include "makeinfo.h"
#include "node.h"
#include "toc.h"
#include "sectioning.h"
#include "xml.h"

/* See comment in sectioning.h.  */
section_alist_type section_alist[] = {
  { "unnumberedsubsubsec", 5, ENUM_SECT_NO,  TOC_YES },
  { "unnumberedsubsec",    4, ENUM_SECT_NO,  TOC_YES },
  { "unnumberedsec",       3, ENUM_SECT_NO,  TOC_YES },
  { "unnumbered",          2, ENUM_SECT_NO,  TOC_YES },
  { "centerchap",          2, ENUM_SECT_NO,  TOC_YES },

  { "appendixsubsubsec",   5, ENUM_SECT_APP, TOC_YES },  /* numbered like A.X.X.X */
  { "appendixsubsec",      4, ENUM_SECT_APP, TOC_YES },
  { "appendixsec",         3, ENUM_SECT_APP, TOC_YES },
  { "appendixsection",     3, ENUM_SECT_APP, TOC_YES },
  { "appendix",            2, ENUM_SECT_APP, TOC_YES },

  { "subsubsec",           5, ENUM_SECT_YES, TOC_YES },
  { "subsubsection",       5, ENUM_SECT_YES, TOC_YES },
  { "subsection",          4, ENUM_SECT_YES, TOC_YES },
  { "section",             3, ENUM_SECT_YES, TOC_YES },
  { "chapter",             2, ENUM_SECT_YES, TOC_YES },

  { "subsubheading",       5, ENUM_SECT_NO,  TOC_NO },
  { "subheading",          4, ENUM_SECT_NO,  TOC_NO },
  { "heading",             3, ENUM_SECT_NO,  TOC_NO },
  { "chapheading",         2, ENUM_SECT_NO,  TOC_NO },
  { "majorheading",        2, ENUM_SECT_NO,  TOC_NO },

  { "top",                 1, ENUM_SECT_NO,  TOC_YES },
  { NULL,                  0, 0, 0 }
};

/* The argument of @settitle, used for HTML. */
char *title = NULL;


#define APPENDIX_MAGIC   1024
#define UNNUMBERED_MAGIC 2048

/* Number memory for every level @chapter, @section,
   @subsection, @subsubsection. */
static int numbers [] = { 0, 0, 0, 0 };

/* enum_marker == APPENDIX_MAGIC then we are counting appendencies
   enum_marker == UNNUMBERED_MAGIC then we are within unnumbered area.
   Handling situations like this:
   @unnumbered ..
   @section ...   */
static int enum_marker = 0;

/* Organized by level commands.  That is, "*" == chapter, "=" == section. */
static char *scoring_characters = "*=-.";

/* Amount to offset the name of sectioning commands to levels by. */
static int section_alist_offset = 0;

/* These two variables are for @float, @cindex like commands that need to know
   in which section they are used.  */
/* Last value returned by get_sectioning_number.  */
static char *last_sectioning_number = "";
/* Last title used by sectioning_underscore, etc.  */
static char *last_sectioning_title = "";

/* num == ENUM_SECT_NO  means unnumbered (should never call this)
   num == ENUM_SECT_YES means numbered
   num == ENUM_SECT_APP means numbered like A.1 and so on */
static char *
get_sectioning_number (int level, int num)
{
  static char s[100]; /* should ever be enough for 99.99.99.99
                         Appendix A.1 */

  char *p;
  int i;

  s[0] = 0;

  /* create enumeration in front of chapter, section, subsection and so on. */
  for (i = 0; i < level; i++)
    {
      p = s + strlen (s);
      if ((i == 0) && (enum_marker == APPENDIX_MAGIC))
        sprintf (p, "%c.", numbers[i] + 64); /* Should be changed to
                                                be more portable */
      else
        sprintf (p, "%d.", numbers[i]);
    }

  /* the last number is never followed by a dot */
  p = s + strlen (s);
  if ((num == ENUM_SECT_APP)
      && (i == 0)
      && (enum_marker == APPENDIX_MAGIC))
    sprintf (p, _("Appendix %c"), numbers[i] + 64);
  else
    sprintf (p, "%d", numbers[i]);

  /* Poor man's cache :-)  */
  if (strlen (last_sectioning_number))
    free (last_sectioning_number);
  last_sectioning_number = xstrdup (s);

  return s;
}


/* Set the level of @top to LEVEL.  Return the old level of @top. */
int
set_top_section_level (int level)
{
  int i, result = -1;

  for (i = 0; section_alist[i].name; i++)
    if (strcmp (section_alist[i].name, "top") == 0)
      {
        result = section_alist[i].level;
        section_alist[i].level = level;
        break;
      }
  return result;
}


/* return the index of the given sectioning command in section_alist */
static int
search_sectioning (char *text)
{
  int i;
  char *t;

  /* ignore the optional command prefix */
  if (text[0] == COMMAND_PREFIX)
    text++;

  for (i = 0; (t = section_alist[i].name); i++)
    {
      if (strcmp (t, text) == 0)
        {
          return i;
        }
    }
  return -1;
}

/* Return an integer which identifies the type of section present in
   TEXT -- 1 for @top, 2 for chapters, ..., 5 for subsubsections (as
   specified in section_alist).  We take into account any @lowersections
   and @raisesections.  If SECNAME is non-NULL, also return the
   corresponding section name.  */
int
what_section (char *text, char **secname)
{
  int index, j;
  char *temp;
  int return_val;

 find_section_command:
  for (j = 0; text[j] && cr_or_whitespace (text[j]); j++);
  if (text[j] != COMMAND_PREFIX)
    return -1;

  text = text + j + 1;

  /* We skip @c, @comment, and @?index commands. */
  if ((strncmp (text, "comment", strlen ("comment")) == 0) ||
      (text[0] == 'c' && cr_or_whitespace (text[1])) ||
      (strcmp (text + 1, "index") == 0))
    {
      while (*text++ != '\n');
      goto find_section_command;
    }

  /* Handle italicized sectioning commands. */
  if (*text == 'i')
    text++;

  for (j = 0; text[j] && !cr_or_whitespace (text[j]); j++);

  temp = xmalloc (1 + j);
  strncpy (temp, text, j);
  temp[j] = 0;

  index = search_sectioning (temp);
  free (temp);
  if (index >= 0)
    {
      return_val = section_alist[index].level + section_alist_offset;
      if (return_val < 0)
        return_val = 0;
      else if (return_val > 5)
        return_val = 5;

      if (secname)
        {
          int i;
          int alist_size = sizeof (section_alist) / sizeof(section_alist_type);
          /* Find location of offset sectioning entry, but don't go off
             either end of the array.  */
          int index_offset = MAX (index - section_alist_offset, 0);
          index_offset = MIN (index_offset, alist_size - 1);

          /* Also make sure we don't go into the next "group" of
             sectioning changes, e.g., change from an @appendix to an
             @heading or some such.  */
#define SIGN(expr) ((expr) < 0 ? -1 : 1)
          for (i = index; i != index_offset; i -= SIGN (section_alist_offset))
            {
              /* As it happens, each group has unique .num/.toc values.  */
              if (section_alist[i].num != section_alist[index_offset].num
                  || section_alist[i].toc != section_alist[index_offset].toc)
                break;
            }
          *secname = section_alist[i].name;
        }
      return return_val;
    }
  return -1;
}

/* Returns current top level division (ie. chapter, unnumbered) number.
   - For chapters, returns the number.
   - For unnumbered sections, returns empty string.
   - For appendices, returns A, B, etc. */
char *
current_chapter_number (void)
{
  if (enum_marker == UNNUMBERED_MAGIC)
    return xstrdup ("");
  else if (enum_marker == APPENDIX_MAGIC)
    {
      char s[1];
      sprintf (s, "%c", numbers[0] + 64);
      return xstrdup (s);
    }
  else
    {
      char s[5];
      sprintf (s, "%d", numbers[0]);
      return xstrdup (s);
    }
}

/* Returns number of the last sectioning command used.  */
char *
current_sectioning_number (void)
{
  if (enum_marker == UNNUMBERED_MAGIC || !number_sections)
    return xstrdup ("");
  else
    return xstrdup (last_sectioning_number);
}

/* Returns arguments of the last sectioning command used.  */
char *
current_sectioning_name (void)
{
  return xstrdup (last_sectioning_title);
}

/* insert_and_underscore, sectioning_underscore and sectioning_html call this.  */

static char *
handle_enum_increment (int level, int index)
{
  /* Here is how TeX handles enumeration:
     - Anything starting with @unnumbered is not enumerated.
     - @majorheading and the like are not enumberated.  */
  int i;

  /* First constraint above.  */
  if (enum_marker == UNNUMBERED_MAGIC && level == 0)
    return xstrdup ("");

  /* Second constraint.  */
  if (section_alist[index].num == ENUM_SECT_NO)
    return xstrdup ("");

  /* reset all counters which are one level deeper */
  for (i = level; i < 3; i++)
    numbers [i + 1] = 0;

  numbers[level]++;
  if (section_alist[index].num == ENUM_SECT_NO || enum_marker == UNNUMBERED_MAGIC
      || !number_sections)
    return xstrdup ("");
  else
    return xstrdup (get_sectioning_number (level, section_alist[index].num));
}


void
sectioning_underscore (char *cmd)
{
  char *temp, *secname;
  int level;
  
  /* If we're not indenting the first paragraph, we shall make it behave
     like @noindent is called directly after the section heading. */
  if (! do_first_par_indent)
    cm_noindent ();

  temp = xmalloc (2 + strlen (cmd));
  temp[0] = COMMAND_PREFIX;
  strcpy (&temp[1], cmd);
  level = what_section (temp, &secname);
  level -= 2;
  if (level < 0)
    level = 0;
  free (temp);

  /* If the argument to @top is empty, we try using the one from @settitle.
     Warn if both are unusable.  */
  if (STREQ (command, "top"))
    {
      int save_input_text_offset = input_text_offset;

      get_rest_of_line (0, &temp);

      /* Due to get_rest_of_line ... */
      line_number--;

      if (strlen (temp) == 0 && (!title || strlen (title) == 0))
        warning ("Must specify a title with least one of @settitle or @top");

      input_text_offset = save_input_text_offset;
    }

  if (xml)
    {
      /* If the section appears in the toc, it means it's a real section
	 unlike majorheading, chapheading etc. */
      if (section_alist[search_sectioning (cmd)].toc == TOC_YES)
	{
	  xml_close_sections (level);
	  /* Mark the beginning of the section
	     If the next command is printindex, we will remove
	     the section and put an Index instead */
	  flush_output ();
	  xml_last_section_output_position = output_paragraph_offset;

	  get_rest_of_line (0, &temp);

          /* Use @settitle value if @top parameter is empty.  */
          if (STREQ (command, "top") && strlen(temp) == 0)
            temp = xstrdup (title ? title : "");

          /* Docbook does not support @unnumbered at all.  So we provide numbers
             that other formats use.  @appendix seems to be fine though, so we let
             Docbook handle that as usual.  */
          if (docbook && enum_marker != APPENDIX_MAGIC)
            {
              if (section_alist[search_sectioning (cmd)].num == ENUM_SECT_NO
                  && section_alist[search_sectioning (cmd)].toc == TOC_YES)
                xml_insert_element_with_attribute (xml_element (secname),
                    START, "label=\"%s\" xreflabel=\"%s\"",
                    handle_enum_increment (level, search_sectioning (cmd)),
                    text_expansion (temp));
              else
                xml_insert_element_with_attribute (xml_element (secname),
                    START, "label=\"%s\"",
                    handle_enum_increment (level, search_sectioning (cmd)));
            }
          else
            xml_insert_element (xml_element (secname), START);

	  xml_insert_element (TITLE, START);
	  xml_open_section (level, secname);
	  execute_string ("%s", temp);
	  xml_insert_element (TITLE, END);

	  free (temp);
	}
      else
        {
          if (docbook)
            {
              if (level > 0)
                xml_insert_element_with_attribute (xml_element (secname), START,
                    "renderas=\"sect%d\"", level);
              else
                xml_insert_element_with_attribute (xml_element (secname), START,
                    "renderas=\"other\"");
            }
          else
            xml_insert_element (xml_element (secname), START);

          get_rest_of_line (0, &temp);
          execute_string ("%s", temp);
          free (temp);

          xml_insert_element (xml_element (secname), END);
        }
    }
  else if (html)
    sectioning_html (level, secname);
  else
    insert_and_underscore (level, secname);
}


/* Insert the text following input_text_offset up to the end of the line
   in a new, separate paragraph.  Directly underneath it, insert a
   line of WITH_CHAR, the same length of the inserted text. */
void
insert_and_underscore (int level, char *cmd)
{
  int i, len;
  int index;
  int old_no_indent;
  unsigned char *starting_pos, *ending_pos;
  char *temp;
  char with_char = scoring_characters[level];

  close_paragraph ();
  filling_enabled =  indented_fill = 0;
  old_no_indent = no_indent;
  no_indent = 1;

  if (macro_expansion_output_stream && !executing_string)
    append_to_expansion_output (input_text_offset + 1);

  get_rest_of_line (0, &temp);

  /* Use @settitle value if @top parameter is empty.  */
  if (STREQ (command, "top") && strlen(temp) == 0)
    temp = xstrdup (title ? title : "");

  starting_pos = output_paragraph + output_paragraph_offset;

  /* Poor man's cache for section title.  */
  if (strlen (last_sectioning_title))
    free (last_sectioning_title);
  last_sectioning_title = xstrdup (temp);

  index = search_sectioning (cmd);
  if (index < 0)
    {
      /* should never happen, but a poor guy, named Murphy ... */
      warning (_("Internal error (search_sectioning) `%s'!"), cmd);
      return;
    }

  /* This is a bit tricky: we must produce "X.Y SECTION-NAME" in the
     Info output and in TOC, but only SECTION-NAME in the macro-expanded
     output.  */

  /* Step 1: produce "X.Y" and add it to Info output.  */
  add_word_args ("%s ", handle_enum_increment (level, index));

  /* Step 2: add "SECTION-NAME" to both Info and macro-expanded output.  */
  if (macro_expansion_output_stream && !executing_string)
    {
      char *temp1 = xmalloc (2 + strlen (temp));
      sprintf (temp1, "%s\n", temp);
      remember_itext (input_text, input_text_offset);
      me_execute_string (temp1);
      free (temp1);
    }
  else
    execute_string ("%s\n", temp);

  /* Step 3: pluck "X.Y SECTION-NAME" from the output buffer and
     insert it into the TOC.  */
  ending_pos = output_paragraph + output_paragraph_offset;
  if (section_alist[index].toc == TOC_YES)
    toc_add_entry (substring (starting_pos, ending_pos - 1),
                   level, current_node, NULL);

  free (temp);

  len = (ending_pos - starting_pos) - 1;
  for (i = 0; i < len; i++)
    add_char (with_char);
  insert ('\n');
  close_paragraph ();
  filling_enabled = 1;
  no_indent = old_no_indent;
}

/* Insert the text following input_text_offset up to the end of the
   line as an HTML heading element of the appropriate `level' and
   tagged as an anchor for the current node.. */

void
sectioning_html (int level, char *cmd)
{
  static int toc_ref_count = 0;
  int index;
  int old_no_indent;
  unsigned char *starting_pos, *ending_pos;
  char *temp, *toc_anchor = NULL;

  close_paragraph ();
  filling_enabled =  indented_fill = 0;
  old_no_indent = no_indent;
  no_indent = 1;

  /* level 0 (chapter) is <h2>, and we go down from there.  */
  add_html_block_elt_args ("<h%d class=\"%s\">", level + 2, cmd);

  /* If we are outside of any node, produce an anchor that
     the TOC could refer to.  */
  if (!current_node || !*current_node)
    {
      static const char a_name[] = "<a name=\"";

      starting_pos = output_paragraph + output_paragraph_offset;
      add_word_args ("%sTOC%d\">", a_name, toc_ref_count++);
      toc_anchor = substring (starting_pos + sizeof (a_name) - 1,
                              output_paragraph + output_paragraph_offset);
      /* This must be added after toc_anchor is extracted, since
         toc_anchor cannot include the closing </a>.  For details,
         see toc.c:toc_add_entry and toc.c:contents_update_html.

         Also, the anchor close must be output before the section name
         in case the name itself contains an anchor. */
      add_word ("</a>");
    }
  starting_pos = output_paragraph + output_paragraph_offset;

  if (macro_expansion_output_stream && !executing_string)
    append_to_expansion_output (input_text_offset + 1);

  get_rest_of_line (0, &temp);

  /* Use @settitle value if @top parameter is empty.  */
  if (STREQ (command, "top") && strlen(temp) == 0)
    temp = xstrdup (title ? title : "");

  index = search_sectioning (cmd);
  if (index < 0)
    {
      /* should never happen, but a poor guy, named Murphy ... */
      warning (_("Internal error (search_sectioning) \"%s\"!"), cmd);
      return;
    }

  /* Produce "X.Y" and add it to HTML output.  */
  {
    char *title_number = handle_enum_increment (level, index);
    if (strlen (title_number) > 0)
      add_word_args ("%s ", title_number);
  }

  /* add the section name to both HTML and macro-expanded output.  */
  if (macro_expansion_output_stream && !executing_string)
    {
      remember_itext (input_text, input_text_offset);
      me_execute_string (temp);
      write_region_to_macro_output ("\n", 0, 1);
    }
  else
    execute_string ("%s", temp);

  ending_pos = output_paragraph + output_paragraph_offset;

  /* Pluck ``X.Y SECTION-NAME'' from the output buffer and insert it
     into the TOC.  */
  if (section_alist[index].toc == TOC_YES)
    toc_add_entry (substring (starting_pos, ending_pos),
                   level, current_node, toc_anchor);

  free (temp);

  if (outstanding_node)
    outstanding_node = 0;

  add_word_args ("</h%d>", level + 2);
  close_paragraph();
  filling_enabled = 1;
  no_indent = old_no_indent;
}


/* Shift the meaning of @section to @chapter. */
void
cm_raisesections (void)
{
  discard_until ("\n");
  section_alist_offset--;
}

/* Shift the meaning of @chapter to @section. */
void
cm_lowersections (void)
{
  discard_until ("\n");
  section_alist_offset++;
}

/* The command still works, but prints a warning message in addition. */
void
cm_ideprecated (int arg, int start, int end)
{
  warning (_("%c%s is obsolete; use %c%s instead"),
           COMMAND_PREFIX, command, COMMAND_PREFIX, command + 1);
  sectioning_underscore (command + 1);
}


/* Treat this just like @unnumbered.  The only difference is
   in node defaulting. */
void
cm_top (void)
{
  /* It is an error to have more than one @top. */
  if (top_node_seen && strcmp (current_node, "Top") != 0)
    {
      TAG_ENTRY *tag = tag_table;

      line_error (_("Node with %ctop as a section already exists"),
                  COMMAND_PREFIX);

      while (tag)
        {
          if (tag->flags & TAG_FLAG_IS_TOP)
            {
              file_line_error (tag->filename, tag->line_no,
                               _("Here is the %ctop node"), COMMAND_PREFIX);
              return;
            }
          tag = tag->next_ent;
        }
    }
  else
    {
      top_node_seen = 1;

      /* It is an error to use @top before using @node. */
      if (!tag_table)
        {
          char *top_name;

          get_rest_of_line (0, &top_name);
          line_error (_("%ctop used before %cnode, defaulting to %s"),
                      COMMAND_PREFIX, COMMAND_PREFIX, top_name);
          execute_string ("@node Top, , (dir), (dir)\n@top %s\n", top_name);
          free (top_name);
          return;
        }

      cm_unnumbered ();

      /* The most recently defined node is the top node. */
      tag_table->flags |= TAG_FLAG_IS_TOP;

      /* Now set the logical hierarchical level of the Top node. */
      {
        int orig_offset = input_text_offset;

        input_text_offset = search_forward (node_search_string, orig_offset);

        if (input_text_offset > 0)
          {
            int this_section;

            /* We have encountered a non-top node, so mark that one exists. */
            non_top_node_seen = 1;

            /* Move to the end of this line, and find out what the
               sectioning command is here. */
            while (input_text[input_text_offset] != '\n')
              input_text_offset++;

            if (input_text_offset < input_text_length)
              input_text_offset++;

            this_section = what_section (input_text + input_text_offset,
                                         NULL);

            /* If we found a sectioning command, then give the top section
               a level of this section - 1. */
            if (this_section != -1)
              set_top_section_level (this_section - 1);
          }
        input_text_offset = orig_offset;
      }
    }
}

/* The remainder of the text on this line is a chapter heading. */
void
cm_chapter (void)
{
  enum_marker = 0;
  sectioning_underscore ("chapter");
}

/* The remainder of the text on this line is a section heading. */
void
cm_section (void)
{
  sectioning_underscore ("section");
}

/* The remainder of the text on this line is a subsection heading. */
void
cm_subsection (void)
{
  sectioning_underscore ("subsection");
}

/* The remainder of the text on this line is a subsubsection heading. */
void
cm_subsubsection (void)
{
  sectioning_underscore ("subsubsection");
}

/* The remainder of the text on this line is an unnumbered heading. */
void
cm_unnumbered (void)
{
  enum_marker = UNNUMBERED_MAGIC;
  sectioning_underscore ("unnumbered");
}

/* The remainder of the text on this line is an unnumbered section heading. */
void
cm_unnumberedsec (void)
{
  sectioning_underscore ("unnumberedsec");
}

/* The remainder of the text on this line is an unnumbered
   subsection heading. */
void
cm_unnumberedsubsec (void)
{
  sectioning_underscore ("unnumberedsubsec");
}

/* The remainder of the text on this line is an unnumbered
   subsubsection heading. */
void
cm_unnumberedsubsubsec (void)
{
  sectioning_underscore ("unnumberedsubsubsec");
}

/* The remainder of the text on this line is an appendix heading. */
void
cm_appendix (void)
{
  /* Reset top level number so we start from Appendix A */
  if (enum_marker != APPENDIX_MAGIC)
    numbers [0] = 0;
  enum_marker = APPENDIX_MAGIC;
  sectioning_underscore ("appendix");
}

/* The remainder of the text on this line is an appendix section heading. */
void
cm_appendixsec (void)
{
  sectioning_underscore ("appendixsec");
}

/* The remainder of the text on this line is an appendix subsection heading. */
void
cm_appendixsubsec (void)
{
  sectioning_underscore ("appendixsubsec");
}

/* The remainder of the text on this line is an appendix
   subsubsection heading. */
void
cm_appendixsubsubsec (void)
{
  sectioning_underscore ("appendixsubsubsec");
}

/* Compatibility functions substitute for chapter, section, etc. */
void
cm_majorheading (void)
{
  sectioning_underscore ("majorheading");
}

void
cm_chapheading (void)
{
  sectioning_underscore ("chapheading");
}

void
cm_heading (void)
{
  sectioning_underscore ("heading");
}

void
cm_subheading (void)
{
  sectioning_underscore ("subheading");
}

void
cm_subsubheading (void)
{
  sectioning_underscore ("subsubheading");
}
