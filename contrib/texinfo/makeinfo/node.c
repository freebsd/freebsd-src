/* node.c -- nodes for Texinfo.
   $Id: node.c,v 1.23 1999/09/20 12:31:21 karl Exp $

   Copyright (C) 1998, 99 Free Software Foundation, Inc.

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
#include "footnote.h"
#include "macro.h"
#include "makeinfo.h"
#include "node.h"
#include "sectioning.h"
#include "insertion.h"


/* See comments in node.h.  */
NODE_REF *node_references = NULL;
NODE_REF *node_node_references = NULL;
TAG_ENTRY *tag_table = NULL;
int node_number = -1;
int current_section = 0;
int outstanding_node = 0;

/* Adding nodes, and making tags.  */

/* Start a new tag table. */
void
init_tag_table ()
{
  while (tag_table)
    {
      TAG_ENTRY *temp = tag_table;
      free (temp->node);
      free (temp->prev);
      free (temp->next);
      free (temp->up);
      tag_table = tag_table->next_ent;
      free (temp);
    }
}

/* Write out the contents of the existing tag table.
   INDIRECT_P says how to format the output (it depends on whether the
   table is direct or indirect).  */
static void
write_tag_table_internal (indirect_p)
     int indirect_p;
{
  TAG_ENTRY *node;
  int old_indent = no_indent;

  no_indent = 1;
  filling_enabled = 0;
  must_start_paragraph = 0;
  close_paragraph ();

  if (!indirect_p)
    {
      no_indent = 1;
      insert ('\n');
    }

  add_word_args ("\037\nTag Table:\n%s", indirect_p ? "(Indirect)\n" : "");

  /* Do not collapse -- to -, etc., in node names.  */
  in_fixed_width_font++;

  for (node = tag_table; node; node = node->next_ent)
    {
      if (node->flags & TAG_FLAG_ANCHOR)
        { /* This reference is to an anchor.  */
          execute_string ("Ref: %s", node->node);
        }
      else
        { /* This reference is to a node.  */
          execute_string ("Node: %s", node->node);
        }
      add_word_args ("\177%d\n", node->position);
    }

  add_word ("\037\nEnd Tag Table\n");

  /* Do not collapse -- to -, etc., in node names.  */
  in_fixed_width_font--;

  flush_output ();
  no_indent = old_indent;
}

void
write_tag_table ()
{
  write_tag_table_internal (0); /* Not indirect. */
}

void
write_tag_table_indirect ()
{
  write_tag_table_internal (1);
}

/* Convert "top" and friends into "Top". */
static void
normalize_node_name (string)
     char *string;
{
  if (strcasecmp (string, "Top") == 0)
    strcpy (string, "Top");
}

char *
get_node_token (expand)
      int expand;
{
  char *string;

  get_until_in_line (expand, ",", &string);

  if (curchar () == ',')
    input_text_offset++;

  fix_whitespace (string);

  /* Force all versions of "top" to be "Top". */
  normalize_node_name (string);

  return string;
}

/* Expand any macros and other directives in a node name, and
   return the expanded name as an malloc'ed string.  */
char *
expand_node_name (node)
     char *node;
{
  char *result = node;

  if (node)
    {
      /* Don't expand --, `` etc., in case somebody will want
         to print the result.  */
      in_fixed_width_font++;
      result = expansion (node, 0);
      in_fixed_width_font--;
      fix_whitespace (result);
      normalize_node_name (result);
    }
  return result;
}

/* Look up NAME in the tag table, and return the associated
   tag_entry.  If the node is not in the table return NULL. */
TAG_ENTRY *
find_node (name)
     char *name;
{
  TAG_ENTRY *tag = tag_table;
  char *expanded_name;
  char n1 = name[0];

  while (tag)
    {
      if (tag->node[0] == n1 && strcmp (tag->node, name) == 0)
        return tag;
      tag = tag->next_ent;
    }

  if (!expensive_validation)
    return NULL;

  /* Try harder.  Maybe TAG_TABLE has the expanded NAME, or maybe NAME
     is expanded while TAG_TABLE has its unexpanded form.  This may
     slow down the search, but if they want this feature, let them
     pay!  If they want it fast, they should write every node name
     consistently (either always expanded or always unexpaned).  */
  expanded_name = expand_node_name (name);
  for (tag = tag_table; tag; tag = tag->next_ent)
    {
      if (STREQ (tag->node, expanded_name))
        break;
      /* If the tag name doesn't have the command prefix, there's no
         chance it could expand into anything but itself.  */
      if (strchr (tag->node, COMMAND_PREFIX))
        {
          char *expanded_node = expand_node_name (tag->node);

          if (STREQ (expanded_node, expanded_name))
            {
              free (expanded_node);
              break;
            }
          free (expanded_node);
        }
    }
  free (expanded_name);
  return tag;
}

/* Similarly for next etc. references in a @node command, where we
   don't care about most of the entries. */
static void
remember_node_node_reference (node)
     char *node;
{
  NODE_REF *temp = xmalloc (sizeof (NODE_REF));
  int number;

  if (!node) return;
  temp->next = node_node_references;
  temp->node = xstrdup (node);
  temp->type = followed_reference;
  number = number_of_node (node);
  if (number)
    temp->number = number;      /* Already assigned. */
  else
    {
      node_number++;
      temp->number = node_number;
    }
  node_node_references = temp;
}

/* Remember NODE and associates. */
void
remember_node (node, prev, next, up, position, line_no, flags)
     char *node, *prev, *next, *up;
     int position, line_no, flags;
{
  /* Check for existence of this tag already. */
  if (validating)
    {
      TAG_ENTRY *tag = find_node (node);
      if (tag)
        {
          line_error (_("Node `%s' previously defined at line %d"),
                      node, tag->line_no);
          return;
        }
    }

  if (!(flags & TAG_FLAG_ANCHOR))
    {
      /* Make this the current node. */
      current_node = node;
    }

  /* Add it to the list. */
  {
    int number = number_of_node (node);

    TAG_ENTRY *new = xmalloc (sizeof (TAG_ENTRY));
    new->node = node;
    new->prev = prev;
    new->next = next;
    new->up = up;
    new->position = position;
    new->line_no = line_no;
    new->filename = node_filename;
    new->touched = 0;
    new->flags = flags;
    if (number)
      new->number = number;     /* Already assigned. */
    else
      {
        node_number++;
        new->number = node_number;
      }
    new->next_ent = tag_table;
    tag_table = new;
  }

  if (html)
    { /* Note the references to the next etc. nodes too.  */
      remember_node_node_reference (next);
      remember_node_node_reference (prev);
      remember_node_node_reference (up);
    }
}

/* Remember this node name for later validation use.  This is used to
   remember menu references while reading the input file.  After the
   output file has been written, if validation is on, then we use the
   contents of `node_references' as a list of nodes to validate.  */
void
remember_node_reference (node, line, type)
     char *node;
     int line;
     enum reftype type;
{
  NODE_REF *temp = xmalloc (sizeof (NODE_REF));
  int number = number_of_node (node);

  temp->next = node_references;
  temp->node = xstrdup (node);
  temp->line_no = line;
  temp->section = current_section;
  temp->type = type;
  temp->containing_node = xstrdup (current_node ? current_node : "");
  temp->filename = node_filename;
  if (number)
    temp->number = number;      /* Already assigned. */
  else
    {
      node_number++;
      temp->number = node_number;
    }

  node_references = temp;
}

static void
isolate_nodename (nodename)
     char *nodename;
{
  int i, c;
  int paren_seen, paren;

  if (!nodename)
    return;

  canon_white (nodename);
  paren_seen = paren = i = 0;

  if (*nodename == '.' || !*nodename)
    {
      *nodename = 0;
      return;
    }

  if (*nodename == '(')
    {
      paren++;
      paren_seen++;
      i++;
    }

  for (; (c = nodename[i]); i++)
    {
      if (paren)
        {
          if (c == '(')
            paren++;
          else if (c == ')')
            paren--;

          continue;
        }

      /* If the character following the close paren is a space, then this
         node has no more characters associated with it. */
      if (c == '\t' ||
          c == '\n' ||
          c == ','  ||
          ((paren_seen && nodename[i - 1] == ')') &&
           (c == ' ' || c == '.')) ||
          (c == '.' &&
           ((!nodename[i + 1] ||
             (cr_or_whitespace (nodename[i + 1])) ||
             (nodename[i + 1] == ')')))))
        break;
    }
  nodename[i] = 0;
}

/* This function gets called at the start of every line while inside a
   menu.  It checks to see if the line starts with "* ", and if so and
   REMEMBER_REF is nonzero, remembers the node reference as type
   REF_TYPE that this menu refers to.  input_text_offset is at the \n
   just before the menu line.  If REMEMBER_REF is zero, REF_TYPE is unused.  */
#define MENU_STARTER "* "
char *
glean_node_from_menu (remember_ref, ref_type)
     int remember_ref;
     enum reftype ref_type;
{
  int i, orig_offset = input_text_offset;
  char *nodename;
  char *line, *expanded_line;
  char *old_input = input_text;
  size_t old_size = input_text_length;

  if (strncmp (&input_text[input_text_offset + 1],
               MENU_STARTER,
               strlen (MENU_STARTER)) != 0)
    return NULL;
  else
    input_text_offset += strlen (MENU_STARTER) + 1;

  /* The menu entry might include macro calls, so we need to expand them.  */
  get_until ("\n", &line);
  only_macro_expansion++;       /* only expand macros in menu entries */
  expanded_line = expansion (line, 0);
  only_macro_expansion--;
  free (line);
  input_text = expanded_line;
  input_text_offset = 0;
  input_text_length = strlen (expanded_line);

  get_until_in_line (0, ":", &nodename);
  if (curchar () == ':')
    input_text_offset++;

  if (curchar () != ':')
    {
      free (nodename);
      get_until_in_line (0, "\n", &nodename);
      isolate_nodename (nodename);
    }

  input_text = old_input;
  input_text_offset = orig_offset;
  input_text_length = old_size;
  free (expanded_line);
  fix_whitespace (nodename);
  normalize_node_name (nodename);
  i = strlen (nodename);
  if (i && nodename[i - 1] == ':')
    nodename[i - 1] = 0;

  if (remember_ref)
    remember_node_reference (nodename, line_number, ref_type);

  return nodename;
}

/* Set the name of the current output file.  */
void
set_current_output_filename (fname)
     const char *fname;
{
  if (current_output_filename)
    free (current_output_filename);
  current_output_filename = xstrdup (fname);
}

/* The order is: nodename, nextnode, prevnode, upnode.
   If all of the NEXT, PREV, and UP fields are empty, they are defaulted.
   You must follow a node command which has those fields defaulted
   with a sectioning command (e.g. @chapter) giving the "level" of that node.
   It is an error not to do so.
   The defaults come from the menu in this node's parent. */
void
cm_node ()
{
  char *node, *prev, *next, *up;
  int new_node_pos, defaulting, this_section;
  int no_warn = 0;

  if (strcmp (command, "nwnode") == 0)
    no_warn = TAG_FLAG_NO_WARN;

  /* Get rid of unmatched brace arguments from previous commands. */
  discard_braces ();

  /* There also might be insertions left lying around that haven't been
     ended yet.  Do that also. */
  discard_insertions (1);

  if (!html && !already_outputting_pending_notes)
    {
      close_paragraph ();
      output_pending_notes ();
    }

  if (html && splitting && top_node_seen)
    {
      /* End the current split output file. */
      close_paragraph ();
      output_pending_notes ();
      start_paragraph ();
      /* Fixme: html: need a navigation bar here. */
      add_word ("</body></html>\n");
      close_paragraph ();
      fclose (output_stream);
      output_stream = NULL;
    }

  filling_enabled = indented_fill = 0;
  new_node_pos = output_position;
  if (!html || (html && splitting))
    current_footnote_number = 1;

  if (macro_expansion_output_stream && !executing_string)
    append_to_expansion_output (input_text_offset + 1);

  /* Do not collapse -- to -, etc., in node names.  */
  in_fixed_width_font++;

  /* While expanding the @node line, leave any non-macros
     intact, so that the macro-expanded output includes them.  */
  only_macro_expansion++;
  node = get_node_token (1);
  only_macro_expansion--;
  next = get_node_token (0);
  prev = get_node_token (0);
  up = get_node_token (0);

  if (verbose_mode)
    printf (_("Formatting node %s...\n"), node);

  if (macro_expansion_output_stream && !executing_string)
    remember_itext (input_text, input_text_offset);

  no_indent = 1;
  if (!no_headers && !html)
    {
      add_word_args ("\037\nFile: %s,  Node: ", pretty_output_filename);

      if (macro_expansion_output_stream && !executing_string)
        me_execute_string (node);
      else
        execute_string ("%s", node);
      filling_enabled = indented_fill = 0;
    }

  /* Check for defaulting of this node's next, prev, and up fields. */
  defaulting = (*next == 0 && *prev == 0 && *up == 0);

  this_section = what_section (input_text + input_text_offset);

  /* If we are defaulting, then look at the immediately following
     sectioning command (error if none) to determine the node's
     level.  Find the node that contains the menu mentioning this node
     that is one level up (error if not found).  That node is the "Up"
     of this node.  Default the "Next" and "Prev" from the menu. */
  if (defaulting)
    {
      NODE_REF *last_ref = NULL;
      NODE_REF *ref = node_references;

      if (this_section < 0 && !STREQ (node, "Top"))
        {
          char *polite_section_name = "top";
          int i;

          for (i = 0; section_alist[i].name; i++)
            if (section_alist[i].level == current_section + 1)
              {
                polite_section_name = section_alist[i].name;
                break;
              }

          line_error
            (_("Node `%s' requires a sectioning command (e.g. %c%s)"),
             node, COMMAND_PREFIX, polite_section_name);
        }
      else
        {
          if (strcmp (node, "Top") == 0)
            {
              /* Default the NEXT pointer to be the first menu item in
                 this node, if there is a menu in this node.  We have to
                 try very hard to find the menu, as it may be obscured
                 by execution_strings which are on the filestack.  For
                 every member of the filestack which has a FILENAME
                 member which is identical to the current INPUT_FILENAME,
                 search forward from that offset. */
              int saved_input_text_offset = input_text_offset;
              int saved_input_text_length = input_text_length;
              char *saved_input_text = input_text;
              FSTACK *next_file = filestack;

              int orig_offset, orig_size;

              /* No matter what, make this file point back at `(dir)'. */
              free (up);
              up = xstrdup ("(dir)"); /* html fixxme */

              while (1)
                {
                  orig_offset = input_text_offset;
                  orig_size =
                    search_forward (node_search_string, orig_offset);

                  if (orig_size < 0)
                    orig_size = input_text_length;

                  input_text_offset = search_forward ("\n@menu", orig_offset);
                  if (input_text_offset > -1
                      && cr_or_whitespace (input_text[input_text_offset + 6]))
                    {
                      char *nodename_from_menu = NULL;

                      input_text_offset =
                        search_forward ("\n* ", input_text_offset);

                      if (input_text_offset != -1)
                        nodename_from_menu = glean_node_from_menu (0, 0);

                      if (nodename_from_menu)
                        {
                          free (next);
                          next = nodename_from_menu;
                          break;
                        }
                    }

                  /* We got here, so it hasn't been found yet.  Try
                     the next file on the filestack if there is one. */
                  if (next_file
                      && FILENAME_CMP (next_file->filename, input_filename)
                          == 0)
                    {
                      input_text = next_file->text;
                      input_text_offset = next_file->offset;
                      input_text_length = next_file->size;
                      next_file = next_file->next;
                    }
                  else
                    { /* No more input files to check. */
                      break;
                    }
                }

              input_text = saved_input_text;
              input_text_offset = saved_input_text_offset;
              input_text_length = saved_input_text_length;
            }
        }

      /* Fix the level of the menu references in the Top node, iff it
         was declared with @top, and no subsequent reference was found. */
      if (top_node_seen && !non_top_node_seen)
        {
          /* Then this is the first non-@top node seen. */
          int level;

          level = set_top_section_level (this_section - 1);
          non_top_node_seen = 1;

          while (ref)
            {
              if (ref->section == level)
                ref->section = this_section - 1;
              ref = ref->next;
            }

          ref = node_references;
        }

      while (ref)
        {
          if (ref->section == (this_section - 1)
              && ref->type == menu_reference
              && strcmp (ref->node, node) == 0)
            {
              char *containing_node = ref->containing_node;

              free (up);
              up = xstrdup (containing_node);

              if (last_ref
                  && last_ref->type == menu_reference
                  && strcmp (last_ref->containing_node, containing_node) == 0)
                {
                  free (next);
                  next = xstrdup (last_ref->node);
                }

              while (ref->section == this_section - 1
                     && ref->next
                     && ref->next->type != menu_reference)
                ref = ref->next;

              if (ref->next && ref->type == menu_reference
                  && strcmp (ref->next->containing_node, containing_node) == 0)
                {
                  free (prev);
                  prev = xstrdup (ref->next->node);
                }
              else if (!ref->next
                       && strcasecmp (ref->containing_node, "Top") == 0)
                {
                  free (prev);
                  prev = xstrdup (ref->containing_node);
                }
              break;
            }
          last_ref = ref;
          ref = ref->next;
        }
    }

  /* Insert the correct args if we are expanding macros, and the node's
     pointers weren't defaulted. */
  if (macro_expansion_output_stream && !executing_string && !defaulting)
    {
      char *temp;
      int op_orig = output_paragraph_offset;
      int meta_pos_orig = meta_char_pos;
      int extra = html ? strlen (node) : 0;

      temp = xmalloc (7 + extra + strlen (next) + strlen (prev) + strlen (up));
      sprintf (temp, "%s, %s, %s, %s", html ? node : "", next, prev, up);
      me_execute_string (temp);
      free (temp);

      output_paragraph_offset = op_orig;
      meta_char_pos = meta_pos_orig;
    }

  if (!*node)
    {
      line_error (_("No node name specified for `%c%s' command"),
                  COMMAND_PREFIX, command);
      free (node);
      free (next); next = NULL;
      free (prev); prev= NULL;
      free (up);   up = NULL;
      node_number++;            /* else it doesn't get bumped */
    }
  else
    {
      if (!*next) { free (next); next = NULL; }
      if (!*prev) { free (prev); prev = NULL; }
      if (!*up)   { free (up);   up = NULL;   }
      remember_node (node, prev, next, up, new_node_pos, line_number, no_warn);
      outstanding_node = 1;
    }

  if (html)
    {
      char *tem;

      if (splitting)
        { /* this code not operational, we do not currently split html */
          char filename[20];

          sprintf (filename, "node%d.html", number_of_node (node));
          output_stream = fopen (filename, "w");
          if (output_stream == NULL)
            {
              fs_error (filename);
              xexit (1);
            }
          set_current_output_filename (filename);
          /* FIXME: when this code is operational, we will need to
             expand node, next, prev, and up before output.  */
          add_word_args ("<html><head><title>%s</title>", node);
          if (next) add_link (next, "rel=next");
          if (prev) add_link (prev, "rel=previous");
          if (up) add_link (up, "rel=up");
          add_word ("</head>\n<body>\n");
        }

      if (!splitting && no_headers)
	{ /* cross refs need a name="#anchor" even if we're not writing headers*/
          add_word ("<a name=\"");
          tem = expand_node_name (node);
          add_anchor_name (tem, 0);
          add_word ("\"></a>");
          free (tem);
	}

      if (splitting || !no_headers)
        { /* Navigation bar.   The <p> avoids the links area running
             on with old Lynxen.  */
          add_word_args ("<p>%s\n", splitting ? "" : "<hr>");
          add_word_args ("%s<a name=\"", _("Node:"));
          tem = expand_node_name (node);
          add_anchor_name (tem, 0);
          add_word_args ("\">%s</a>", tem);
          free (tem);

          if (next)
            {
              add_word (",\n");
              add_word (_("Next:"));
              add_word ("<a rel=next href=\"");
              tem = expansion (next, 0);
              add_anchor_name (tem, 1);
              add_word_args ("\">%s</a>", tem);
              free (tem);
            }
          if (prev)
            {
              add_word (",\n");
              add_word (_("Previous:"));
              add_word ("<a rel=previous href=\"");
              tem = expansion (prev, 0);
              add_anchor_name (tem, 1);
              add_word_args ("\">%s</a>", tem);
              free (tem);
            }
          if (up)
            {
              add_word (",\n");
              add_word (_("Up:"));
              add_word ("<a rel=up href=\"");
              tem = expansion (up, 0);
              add_anchor_name (tem, 1);
              add_word_args ("\">%s</a>", tem);
              free (tem);
            }
          /* html fixxme: we want a `top' or `contents' link here.  */

          add_word_args ("\n%s<br>\n", splitting ? "<hr>" : "");
        }
    }

  else if (!no_headers)
    {
      if (macro_expansion_output_stream)
        me_inhibit_expansion++;

      /* These strings are not translatable.  */
      if (next)
        {
          execute_string (",  Next: %s", next);
          filling_enabled = indented_fill = 0;
        }
      if (prev)
        {
          execute_string (",  Prev: %s", prev);
          filling_enabled = indented_fill = 0;
        }
      if (up)
        {
          execute_string (",  Up: %s", up);
          filling_enabled = indented_fill = 0;
        }
      if (macro_expansion_output_stream)
        me_inhibit_expansion--;
    }

  close_paragraph ();
  no_indent = 0;

  /* Change the section only if there was a sectioning command. */
  if (this_section >= 0)
    current_section = this_section;

  if (current_node && STREQ (current_node, "Top"))
    top_node_seen = 1;

  filling_enabled = 1;
  in_fixed_width_font--;
}

/* Cross-reference target at an arbitrary spot.  */
void
cm_anchor (arg)
     int arg;
{
  char *anchor;

  if (arg == END)
    return;

  /* Parse the anchor text.  */
  anchor = get_xref_token (1);

  /* In HTML mode, need to actually produce some output.  */
  if (html)
    {
      /* If this anchor is at the beginning of a new paragraph, make
	 sure a new paragraph is indeed started.  */
      if (!paragraph_is_open)
	{
	  start_paragraph ();
	  if (!in_fixed_width_font || in_menu || in_detailmenu)
	    {
	      insert_string ("<p>");
	      in_paragraph = 1;
	    }
	}
      add_word ("<a name=\"");
      add_anchor_name (anchor, 0);
      add_word ("\"></a>");
    }

  /* Save it in the tag table.  */
  remember_node (anchor, NULL, NULL, NULL, output_position + output_column,
                 line_number, TAG_FLAG_ANCHOR);
}

/* Find NODE in REF_LIST. */
static NODE_REF *
find_node_reference (node, ref_list)
     char *node;
     NODE_REF *ref_list;
{
  NODE_REF *orig_ref_list = ref_list;
  char *expanded_node;

  while (ref_list)
    {
      if (strcmp (node, ref_list->node) == 0)
        break;
      ref_list = ref_list->next;
    }

  if (ref_list || !expensive_validation)
    return ref_list;

  /* Maybe NODE is not expanded yet.  This may be SLOW.  */
  expanded_node = expand_node_name (node);
  for (ref_list = orig_ref_list; ref_list; ref_list = ref_list->next)
    {
      if (STREQ (expanded_node, ref_list->node))
        break;
      if (strchr (ref_list->node, COMMAND_PREFIX))
        {
          char *expanded_ref = expand_node_name (ref_list->node);

          if (STREQ (expanded_node, expanded_ref))
            {
              free (expanded_ref);
              break;
            }
          free (expanded_ref);
        }
    }
  free (expanded_node);
  return ref_list;
}

void
free_node_references ()
{
  NODE_REF *list, *temp;

  list = node_references;

  while (list)
    {
      temp = list;
      free (list->node);
      free (list->containing_node);
      list = list->next;
      free (temp);
    }
  node_references = NULL;
}

void
free_node_node_references ()
{
  NODE_REF *list, *temp;

  list = node_references;

  while (list)
    {
      temp = list;
      free (list->node);
      list = list->next;
      free (temp);
    }
  node_node_references = NULL;
}

/* Return the number assigned to a named node in either the tag_table
   or node_references list or zero if no number has been assigned. */
int
number_of_node (node)
     char *node;
{
  NODE_REF *temp_ref;
  TAG_ENTRY *temp_node = find_node (node);

  if (temp_node)
    return temp_node->number;
  else if ((temp_ref = find_node_reference (node, node_references)))
    return temp_ref->number;
  else if ((temp_ref = find_node_reference (node, node_node_references)))
    return temp_ref->number;
  else
    return 0;
}

/* validation */

/* Return 1 if TAG (at LINE) correctly validated, or 0 if not.
   LABEL is the (translated) description of the type of reference --
   Menu, Cross, Next, etc.  */

static int
validate (tag, line, label)
     char *tag;
     int line;
     char *label;
{
  TAG_ENTRY *result;

  /* If there isn't a tag to verify, or if the tag is in another file,
     then it must be okay. */
  if (!tag || !*tag || *tag == '(')
    return 1;

  /* Otherwise, the tag must exist. */
  result = find_node (tag);

  if (!result)
    {
      line_number = line;
      line_error (_("%s reference to nonexistent node `%s'"), label, tag);
      return 0;
    }
  result->touched++;
  return 1;
}

/* The strings here are followed in the message by `reference to...' in
   the `validate' routine.  They are only used in messages, thus are
   translated.  */
static char *
reftype_type_string (type)
     enum reftype type;
{
  switch (type)
    {
    case menu_reference:
      return _("Menu");
    case followed_reference:
      return _("Cross");
    default:
      return "Internal-bad-reference-type";
    }
}

static void
validate_other_references (ref_list)
     NODE_REF *ref_list;
{
  char *old_input_filename = input_filename;

  while (ref_list)
    {
      input_filename = ref_list->filename;
      validate (ref_list->node, ref_list->line_no,
                reftype_type_string (ref_list->type));
      ref_list = ref_list->next;
    }
  input_filename = old_input_filename;
}

/* Validation of an info file.
   Scan through the list of tag entries touching the Prev, Next, and Up
   elements of each.  It is an error not to be able to touch one of them,
   except in the case of external node references, such as "(DIR)".

   If the Prev is different from the Up,
   then the Prev node must have a Next pointing at this node.

   Every node except Top must have an Up.
   The Up node must contain some sort of reference, other than a Next,
   to this node.

   If the Next is different from the Next of the Up,
   then the Next node must have a Prev pointing at this node. */
void
validate_file (tag_table)
     TAG_ENTRY *tag_table;
{
  char *old_input_filename = input_filename;
  TAG_ENTRY *tags = tag_table;

  while (tags)
    {
      TAG_ENTRY *temp_tag;
      char *tem1, *tem2;

      input_filename = tags->filename;
      line_number = tags->line_no;

      /* If this is a "no warn" node, don't validate it in any way. */
      if (tags->flags & TAG_FLAG_NO_WARN)
        {
          tags = tags->next_ent;
          continue;
        }

      /* If this node has a Next, then make sure that the Next exists. */
      if (tags->next)
        {
          validate (tags->next, tags->line_no, _("Next"));

          /* If the Next node exists, and there is no Up, then make sure
             that the Prev of the Next points back.  But do nothing if
             we aren't supposed to issue warnings about this node. */
          temp_tag = find_node (tags->next);
          if (temp_tag && !(temp_tag->flags & TAG_FLAG_NO_WARN))
            {
              char *prev = temp_tag->prev;
              int you_lose = !prev || !STREQ (prev, tags->node);

              if (you_lose && expensive_validation)
                {
                  tem1 = expand_node_name (prev);
                  tem2 = expand_node_name (tags->node);

                  if (STREQ (tem1, tem2))
                    you_lose = 0;
                  free (tem1);
                  free (tem2);
                }
              if (you_lose)
                {
                  line_error (_("Next field of node `%s' not pointed to"),
                              tags->node);
                  line_number = temp_tag->line_no;
                  input_filename = temp_tag->filename;
                  line_error (_("This node (%s) has the bad Prev"),
                              temp_tag->node);
                  input_filename = tags->filename;
                  line_number = tags->line_no;
                  temp_tag->flags |= TAG_FLAG_PREV_ERROR;
                }
            }
        }

      /* Validate the Prev field if there is one, and we haven't already
         complained about it in some way.  You don't have to have a Prev
         field at this stage. */
      if (!(tags->flags & TAG_FLAG_PREV_ERROR) && tags->prev)
        {
          int valid_p = validate (tags->prev, tags->line_no, _("Prev"));

          if (!valid_p)
            tags->flags |= TAG_FLAG_PREV_ERROR;
          else
            { /* If the Prev field is not the same as the Up field,
                 then the node pointed to by the Prev field must have
                 a Next field which points to this node. */
              int prev_equals_up = !tags->up || STREQ (tags->prev, tags->up);

              if (!prev_equals_up && expensive_validation)
                {
                  tem1 = expand_node_name (tags->prev);
                  tem2 = expand_node_name (tags->up);
                  prev_equals_up = STREQ (tem1, tem2);
                  free (tem1);
                  free (tem2);
                }
              if (!prev_equals_up)
                {
                  temp_tag = find_node (tags->prev);

                  /* If we aren't supposed to issue warnings about the
                     target node, do nothing. */
                  if (!temp_tag || (temp_tag->flags & TAG_FLAG_NO_WARN))
                    /* Do nothing. */ ;
                  else
                    {
                      int you_lose = !temp_tag->next
                        || !STREQ (temp_tag->next, tags->node);

                      if (temp_tag->next && you_lose && expensive_validation)
                        {
                          tem1 = expand_node_name (temp_tag->next);
                          tem2 = expand_node_name (tags->node);
                          if (STREQ (tem1, tem2))
                            you_lose = 0;
                          free (tem1);
                          free (tem2);
                        }
                      if (you_lose)
                        {
                          line_error
                            (_("Prev field of node `%s' not pointed to"),
                             tags->node);
                          line_number = temp_tag->line_no;
                          input_filename = temp_tag->filename;
                          line_error (_("This node (%s) has the bad Next"),
                                      temp_tag->node);
                          input_filename = tags->filename;
                          line_number = tags->line_no;
                          temp_tag->flags |= TAG_FLAG_NEXT_ERROR;
                        }
                    }
                }
            }
        }

      if (!tags->up
          && !(tags->flags & TAG_FLAG_ANCHOR)
          && strcasecmp (tags->node, "Top") != 0)
        line_error (_("`%s' has no Up field"), tags->node);
      else if (tags->up)
        {
          int valid_p = validate (tags->up, tags->line_no, _("Up"));

          /* If node X has Up: Y, then warn if Y fails to have a menu item
             or note pointing at X, if Y isn't of the form "(Y)". */
          if (valid_p && *tags->up != '(')
            {
              NODE_REF *nref;
              NODE_REF *tref = NULL;
              NODE_REF *list = node_references;

              for (;;)
                {
                  nref = find_node_reference (tags->node, list);
                  if (!nref)
                    break;

                  if (strcmp (nref->containing_node, tags->up) == 0)
                    {
                      if (nref->type != menu_reference)
                        {
                          tref = nref;
                          list = nref->next;
                        }
                      else
                        break;
                    }
                  list = nref->next;
                }

              if (!nref)
                {
		  if (!tref && expensive_validation)
		    {
		      /* Sigh...  This might be AWFULLY slow, but if
		         they want this feature, they'll have to pay!
		         We do all the loop again expanding each
		         containing_node reference as we go.  */
		      char *tags_up = expand_node_name (tags->up);
		      char *tem;

		      list = node_references;

		      for (;;)
			{
			  nref = find_node_reference (tags->node, list);
			  if (!nref)
			    break;
			  tem = expand_node_name (nref->containing_node);
			  if (STREQ (tem, tags_up))
			    {
			      if (nref->type != menu_reference)
				tref = nref;
			      else
				{
				  free (tem);
				  break;
				}
			    }
			  free (tem);
			  list = nref->next;
			}
		    }
                  if (!nref && !tref)
                    {
                      temp_tag = find_node (tags->up);
                      line_number = temp_tag->line_no;
                      input_filename = temp_tag->filename;
                      line_error (
           _("Node `%s' lacks menu item for `%s' despite being its Up target"),
                                  tags->up, tags->node);
                      line_number = tags->line_no;
                      input_filename = tags->filename;
                    }
                }
            }
        }
      tags = tags->next_ent;
    }

  validate_other_references (node_references);
  /* We have told the user about the references which didn't exist.
     Now tell him about the nodes which aren't referenced. */

  for (tags = tag_table; tags; tags = tags->next_ent)
    {
      /* If this node is a "no warn" node, do nothing. */
      if (tags->flags & TAG_FLAG_NO_WARN)
        {
          tags = tags->next_ent;
          continue;
        }

      /* Special hack.  If the node in question appears to have
         been referenced more than REFERENCE_WARNING_LIMIT times,
         give a warning. */
      if (tags->touched > reference_warning_limit)
        {
          input_filename = tags->filename;
          line_number = tags->line_no;
          warning (_("node `%s' has been referenced %d times"),
                   tags->node, tags->touched);
        }

      if (tags->touched == 0)
        {
          input_filename = tags->filename;
          line_number = tags->line_no;

          /* Notice that the node "Top" is special, and doesn't have to
             be referenced.   Anchors don't have to be referenced
             either, you might define them for another document.  */
          if (strcasecmp (tags->node, "Top") != 0
              && !(tags->flags & TAG_FLAG_ANCHOR))
            warning (_("unreferenced node `%s'"), tags->node);
        }
    }
  input_filename = old_input_filename;
}


/* Splitting */

/* Return true if the tag entry pointed to by TAGS is the last node.
   This means only anchors follow.  */

static int
last_node_p (tags)
     TAG_ENTRY *tags;
{
  int last = 1;
  while (tags->next_ent) {
    tags = tags->next_ent;
    if (tags->flags & TAG_FLAG_ANCHOR)
      ;
    else
      {
        last = 0;
        break;
      }
  }
  
  return last;
}


/* Split large output files into a series of smaller files.  Each file
   is pointed to in the tag table, which then gets written out as the
   original file.  The new files have the same name as the original file
   with a "-num" attached.  SIZE is the largest number of bytes to allow
   in any single split file. */
void
split_file (filename, size)
     char *filename;
     int size;
{
  char *root_filename, *root_pathname;
  char *the_file, *filename_part ();
  struct stat fileinfo;
  long file_size;
  char *the_header;
  int header_size;
  int dos_file_names = 0;       /* if nonzero, don't exceed 8+3 limits */

  /* Can only do this to files with tag tables. */
  if (!tag_table)
    return;

  if (size == 0)
    size = DEFAULT_SPLIT_SIZE;

  if ((stat (filename, &fileinfo) != 0) ||
      (((long) fileinfo.st_size) < SPLIT_SIZE_THRESHOLD))
    return;
  file_size = (long) fileinfo.st_size;

  the_file = find_and_load (filename);
  if (!the_file)
    return;

  root_filename = filename_part (filename);
  root_pathname = pathname_part (filename);

  /* Do we need to generate names of subfiles which don't exceed 8+3 limits? */
  dos_file_names = !HAVE_LONG_FILENAMES (root_pathname ? root_pathname : ".");

  if (!root_pathname)
    root_pathname = xstrdup ("");

  /* Start splitting the file.  Walk along the tag table
     outputting sections of the file.  When we have written
     all of the nodes in the tag table, make the top-level
     pointer file, which contains indirect pointers and
     tags for the nodes. */
  {
    int which_file = 1;
    TAG_ENTRY *tags = tag_table;
    char *indirect_info = NULL;

    /* Remember the `header' of this file.  The first tag in the file is
       the bottom of the header; the top of the file is the start. */
    the_header = xmalloc (1 + (header_size = tags->position));
    memcpy (the_header, the_file, header_size);

    while (tags)
      {
        int file_top, file_bot, limit;

        /* Have to include the Control-_. */
        file_top = file_bot = tags->position;
        limit = file_top + size;

        /* If the rest of this file is only one node, then
           that is the entire subfile. */
        if (last_node_p (tags))
          {
            int i = tags->position + 1;
            char last_char = the_file[i];

            while (i < file_size)
              {
                if ((the_file[i] == '\037') &&
                    ((last_char == '\n') ||
                     (last_char == '\014')))
                  break;
                else
                  last_char = the_file[i];
                i++;
              }
            file_bot = i;
            tags = tags->next_ent;
            goto write_region;
          }

        /* Otherwise, find the largest number of nodes that can fit in
           this subfile. */
        for (; tags; tags = tags->next_ent)
          {
            if (last_node_p (tags))
              {
                /* This entry is the last node.  Search forward for the end
                   of this node, and that is the end of this file. */
                int i = tags->position + 1;
                char last_char = the_file[i];

                while (i < file_size)
                  {
                    if ((the_file[i] == '\037') &&
                        ((last_char == '\n') ||
                         (last_char == '\014')))
                      break;
                    else
                      last_char = the_file[i];
                    i++;
                  }
                file_bot = i;

                if (file_bot < limit)
                  {
                    tags = tags->next_ent;
                    goto write_region;
                  }
                else
                  {
                    /* Here we want to write out everything before the last
                       node, and then write the last node out in a file
                       by itself. */
                    file_bot = tags->position;
                    goto write_region;
                  }
              }

            /* Write region only if this was a node, not an anchor.  */
            if (tags->next_ent->position > limit
                && !(tags->flags & TAG_FLAG_ANCHOR))
              {
                if (tags->position == file_top)
                  tags = tags->next_ent;

                file_bot = tags->position;

              write_region:
                {
                  int fd;
                  char *split_filename, *split_basename;
                  unsigned root_len = strlen (root_filename);

                  split_filename = xmalloc (10 + strlen (root_pathname)
                                            + root_len);
                  split_basename = xmalloc (10 + root_len);
                  sprintf (split_basename, "%s-%d", root_filename, which_file);
                  if (dos_file_names)
                    {
                      char *dot = strchr (split_basename, '.');
                      unsigned base_len = strlen (split_basename);

                      if (dot)
                        { /* Make foobar.i1, .., foobar.i99, foobar.100, ... */
                          dot[1] = 'i';
                          memmove (which_file <= 99 ? dot + 2 : dot + 1,
                                   split_basename + root_len + 1,
                                   strlen (split_basename + root_len + 1) + 1);
                        }
                      else if (base_len > 8)
                        {
                          /* Make foobar-1, .., fooba-10, .., foob-100, ... */
                          unsigned numlen = base_len - root_len;

                          memmove (split_basename + 8 - numlen,
                                   split_basename + root_len, numlen + 1);
                        }
                    }
                  sprintf (split_filename, "%s%s", root_pathname,
                           split_basename);

                  fd = open (split_filename, O_WRONLY|O_TRUNC|O_CREAT, 0666);
                  if (fd < 0
                      || write (fd, the_header, header_size) != header_size
                      || write (fd, the_file + file_top, file_bot - file_top)
                         != (file_bot - file_top)
                      || (close (fd)) < 0)
                    {
                      perror (split_filename);
                      if (fd != -1)
                        close (fd);
                      xexit (1);
                    }

                  if (!indirect_info)
                    {
                      indirect_info = the_file + file_top;
                      sprintf (indirect_info, "\037\nIndirect:\n");
                      indirect_info += strlen (indirect_info);
                    }

                  sprintf (indirect_info, "%s: %d\n",
                           split_basename, file_top);

                  free (split_basename);
                  free (split_filename);
                  indirect_info += strlen (indirect_info);
                  which_file++;
                  break;
                }
              }
          }
      }

    /* We have sucessfully created the subfiles.  Now write out the
       original again.  We must use `output_stream', or
       write_tag_table_indirect () won't know where to place the output. */
    output_stream = fopen (filename, "w");
    if (!output_stream)
      {
        perror (filename);
        xexit (1);
      }

    {
      int distance = indirect_info - the_file;
      fwrite (the_file, 1, distance, output_stream);

      /* Inhibit newlines. */
      paragraph_is_open = 0;

      write_tag_table_indirect ();
      fclose (output_stream);
      free (the_header);
      free (the_file);
      return;
    }
  }
}
