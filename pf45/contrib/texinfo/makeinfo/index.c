/* index.c -- indexing for Texinfo.
   $Id: index.c,v 1.17 2004/11/30 02:03:23 karl Exp $

   Copyright (C) 1998, 1999, 2002, 2003, 2004 Free Software Foundation,
   Inc.

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
#include "files.h"
#include "footnote.h"
#include "html.h"
#include "index.h"
#include "lang.h"
#include "macro.h"
#include "sectioning.h"
#include "toc.h"
#include "xml.h"

INDEX_ALIST **name_index_alist = NULL;

/* An array of pointers.  Each one is for a different index.  The
   "synindex" command changes which array slot is pointed to by a
   given "index". */
INDEX_ELT **the_indices = NULL;

/* The number of defined indices. */
int defined_indices = 0;

/* This is the order of the index.  */
int index_counter = 0;

/* Stuff for defining commands on the fly. */
COMMAND **user_command_array = NULL;
int user_command_array_len = 0;

/* How to compare index entries for sorting.  May be set to strcoll.  */
int (*index_compare_fn) (const char *a, const char *b) = strcasecmp;

/* Function to compare index entries for sorting.  (Calls
   `index_compare_fn' above.)  */
int index_element_compare (const void *element1, const void *element2);

/* Find which element in the known list of indices has this name.
   Returns -1 if NAME isn't found. */
static int
find_index_offset (char *name)
{
  int i;
  for (i = 0; i < defined_indices; i++)
    if (name_index_alist[i] && STREQ (name, name_index_alist[i]->name))
      return i;
  return -1;
}

/* Return a pointer to the entry of (name . index) for this name.
   Return NULL if the index doesn't exist. */
static INDEX_ALIST *
find_index (char *name)
{
  int offset = find_index_offset (name);
  if (offset > -1)
    return name_index_alist[offset];
  else
    return NULL;
}

/* User-defined commands, which happens only from user-defined indexes.
   Used to initialize the builtin indices, too.  */
static void
define_user_command (char *name, COMMAND_FUNCTION (*proc), int needs_braces_p)
{
  int slot = user_command_array_len;
  user_command_array_len++;

  if (!user_command_array)
    user_command_array = xmalloc (1 * sizeof (COMMAND *));

  user_command_array = xrealloc (user_command_array,
                            (1 + user_command_array_len) * sizeof (COMMAND *));

  user_command_array[slot] = xmalloc (sizeof (COMMAND));
  user_command_array[slot]->name = xstrdup (name);
  user_command_array[slot]->proc = proc;
  user_command_array[slot]->argument_in_braces = needs_braces_p;
}

/* Please release me, let me go... */
static void
free_index (INDEX_ELT *index)
{
  INDEX_ELT *temp;

  while ((temp = index))
    {
      free (temp->entry);
      free (temp->entry_text);
      /* Do not free the node, because we already freed the tag table,
         which freed all the node names.  */
      /* free (temp->node); */
      index = index->next;
      free (temp);
    }
}

/* Flush an index by name.  This will delete the list of entries that
   would be written by a @printindex command for this index. */
static void
undefindex (char *name)
{
  int i;
  int which = find_index_offset (name);

  /* The index might have already been freed if this was the target of
     an @synindex.  */
  if (which < 0 || !name_index_alist[which])
    return;

  i = name_index_alist[which]->read_index;

  free_index (the_indices[i]);
  the_indices[i] = NULL;

  free (name_index_alist[which]->name);
  free (name_index_alist[which]);
  name_index_alist[which] = NULL;
}

/* Add the arguments to the current index command to the index NAME.  */
static void
index_add_arg (char *name)
{
  int which;
  char *index_entry;
  INDEX_ALIST *tem;

  tem = find_index (name);

  which = tem ? tem->write_index : -1;

  if (macro_expansion_output_stream && !executing_string)
    append_to_expansion_output (input_text_offset + 1);

  get_rest_of_line (0, &index_entry);
  ignore_blank_line ();

  if (macro_expansion_output_stream && !executing_string)
    {
      char *index_line = xmalloc (strlen (index_entry) + 2);
      sprintf (index_line, "%s\n", index_entry);
      me_execute_string_keep_state (index_line, NULL);
      free (index_line);
    }

  if (which < 0)
    {
      line_error (_("Unknown index `%s'"), name);
      free (index_entry);
    }
  else
    {
      INDEX_ELT *new = xmalloc (sizeof (INDEX_ELT));

      index_counter++;

      /* Get output line number updated before doing anything.  */
      if (!html && !xml)
        flush_output ();

      new->next = the_indices[which];
      new->entry = NULL;
      new->entry_text = index_entry;
      /* Since footnotes are handled at the very end of the document,
         node name in the non-split HTML outputs always show the last
         node.  We artificially make it ``Footnotes''.  */
      if (html && !splitting && already_outputting_pending_notes)
        new->node = xstrdup (_("Footnotes"));
      else
        new->node = current_node ? current_node : xstrdup ("");
      if (!html && !xml && no_headers)
        {
          new->section = current_sectioning_number ();
          if (strlen (new->section) == 0)
            new->section_name = current_sectioning_name ();
          else
            new->section_name = "";
        }
      else
        {
          new->section = NULL;
          new->section_name = NULL;
        }
      new->code = tem->code;
      new->defining_line = line_number - 1;
      new->output_line = no_headers ? output_line_number : node_line_number;
      /* We need to make a copy since input_filename may point to
         something that goes away, for example, inside a macro.
         (see the findexerr test).  */
      new->defining_file = xstrdup (input_filename);

      if (html && splitting)
        {
          if (current_output_filename && *current_output_filename)
            new->output_file = filename_part (current_output_filename);
          else
            new->output_file = xstrdup ("");
        }
      else
        new->output_file = NULL;        

      new->entry_number = index_counter;
      the_indices[which] = new;

#if 0
      /* The index breaks if there are colons in the entry.
         -- This is true, but it's too painful to force changing index
         entries to use `colon', and too confusing for users.  The real
         fix is to change Info support to support arbitrary characters
         in node names, and we're not ready to do that.  --karl,
         19mar02.  */
      if (strchr (new->entry_text, ':'))
        warning (_("Info cannot handle `:' in index entry `%s'"),
                 new->entry_text);
#endif

      if (html)
        {
          /* Anchor.  */
          int removed_empty_elt = 0;

          /* We must put the anchor outside the <dl> and <ul> blocks.  */
          if (rollback_empty_tag ("dl"))
            removed_empty_elt = 1;
          else if (rollback_empty_tag ("ul"))
            removed_empty_elt = 2;

          add_word ("<a name=\"index-");
          add_escaped_anchor_name (index_entry, 0);
          add_word_args ("-%d\"></a>", index_counter);

          if (removed_empty_elt == 1)
            add_html_block_elt_args ("\n<dl>");
          else if (removed_empty_elt == 2)
            add_html_block_elt_args ("\n<ul>");
        }
    }

  if (xml)
    xml_insert_indexterm (index_entry, name);
}

/* The function which user defined index commands call. */
static void
gen_index (void)
{
  char *name = xstrdup (command);
  if (strlen (name) >= strlen ("index"))
    name[strlen (name) - strlen ("index")] = 0;
  index_add_arg (name);
  free (name);
}

/* Define an index known as NAME.  We assign the slot number.
   If CODE is nonzero, make this a code index. */
static void
defindex (char *name, int code)
{
  int i, slot;

  /* If it already exists, flush it. */
  undefindex (name);

  /* Try to find an empty slot. */
  slot = -1;
  for (i = 0; i < defined_indices; i++)
    if (!name_index_alist[i])
      {
        slot = i;
        break;
      }

  if (slot < 0)
    { /* No such luck.  Make space for another index. */
      slot = defined_indices;
      defined_indices++;

      name_index_alist = (INDEX_ALIST **)
        xrealloc (name_index_alist, (1 + defined_indices)
                                    * sizeof (INDEX_ALIST *));
      the_indices = (INDEX_ELT **)
        xrealloc (the_indices, (1 + defined_indices) * sizeof (INDEX_ELT *));
    }

  /* We have a slot.  Start assigning. */
  name_index_alist[slot] = xmalloc (sizeof (INDEX_ALIST));
  name_index_alist[slot]->name = xstrdup (name);
  name_index_alist[slot]->read_index = slot;
  name_index_alist[slot]->write_index = slot;
  name_index_alist[slot]->code = code;

  the_indices[slot] = NULL;
}

/* Define an index NAME, implicitly @code if CODE is nonzero.  */
static void
top_defindex (char *name, int code)
{
  char *temp;

  temp = xmalloc (1 + strlen (name) + strlen ("index"));
  sprintf (temp, "%sindex", name);
  define_user_command (temp, gen_index, 0);
  defindex (name, code);
  free (temp);
}

/* Set up predefined indices.  */
void
init_indices (void)
{
  int i;

  /* Create the default data structures. */

  /* Initialize data space. */
  if (!the_indices)
    {
      the_indices = xmalloc ((1 + defined_indices) * sizeof (INDEX_ELT *));
      the_indices[defined_indices] = NULL;

      name_index_alist = xmalloc ((1 + defined_indices)
                                  * sizeof (INDEX_ALIST *));
      name_index_alist[defined_indices] = NULL;
    }

  /* If there were existing indices, get rid of them now. */
  for (i = 0; i < defined_indices; i++)
    {
      if (name_index_alist[i])
        { /* Suppose we're called with two input files, and the first
             does a @synindex pg cp.  Then, when we get here to start
             the second file, the "pg" element won't get freed by
             undefindex (because it's pointing to "cp").  So free it
             here; otherwise, when we try to define the pg index again
             just below, it will still point to cp.  */
          undefindex (name_index_alist[i]->name);

          /* undefindex sets all this to null in some cases.  */
          if (name_index_alist[i])
            {
              free (name_index_alist[i]->name);
              free (name_index_alist[i]);
              name_index_alist[i] = NULL;
            }
        }
    }

  /* Add the default indices. */
  top_defindex ("cp", 0);           /* cp is the only non-code index.  */
  top_defindex ("fn", 1);
  top_defindex ("ky", 1);
  top_defindex ("pg", 1);
  top_defindex ("tp", 1);
  top_defindex ("vr", 1);
}

/* Given an index name, return the offset in the_indices of this index,
   or -1 if there is no such index. */
static int
translate_index (char *name)
{
  INDEX_ALIST *which = find_index (name);

  if (which)
    return which->read_index;
  else
    return -1;
}

/* Return the index list which belongs to NAME. */
INDEX_ELT *
index_list (char *name)
{
  int which = translate_index (name);
  if (which < 0)
    return (INDEX_ELT *) -1;
  else
    return the_indices[which];
}

/* Define a new index command.  Arg is name of index. */
static void
gen_defindex (int code)
{
  char *name;
  get_rest_of_line (0, &name);

  if (find_index (name))
    {
      line_error (_("Index `%s' already exists"), name);
    }
  else
    {
      char *temp = xmalloc (strlen (name) + sizeof ("index"));
      sprintf (temp, "%sindex", name);
      define_user_command (temp, gen_index, 0);
      defindex (name, code);
      free (temp);
    }

  free (name);
}

void
cm_defindex (void)
{
  gen_defindex (0);
}

void
cm_defcodeindex (void)
{
  gen_defindex (1);
}

/* Expects 2 args, on the same line.  Both are index abbreviations.
   Make the first one be a synonym for the second one, i.e. make the
   first one have the same index as the second one. */
void
cm_synindex (void)
{
  int source, target;
  char *abbrev1, *abbrev2;

  skip_whitespace ();
  get_until_in_line (0, " ", &abbrev1);
  target = find_index_offset (abbrev1);
  skip_whitespace ();
  get_until_in_line (0, " ", &abbrev2);
  source = find_index_offset (abbrev2);
  if (source < 0 || target < 0)
    {
      line_error (_("Unknown index `%s' and/or `%s' in @synindex"),
                  abbrev1, abbrev2);
    }
  else
    {
      if (xml && !docbook)
        xml_synindex (abbrev1, abbrev2);
      else
        name_index_alist[target]->write_index
          = name_index_alist[source]->write_index;
    }

  free (abbrev1);
  free (abbrev2);
}

void
cm_pindex (void)                    /* Pinhead index. */
{
  index_add_arg ("pg");
}

void
cm_vindex (void)                    /* Variable index. */
{
  index_add_arg ("vr");
}

void
cm_kindex (void)                    /* Key index. */
{
  index_add_arg ("ky");
}

void
cm_cindex (void)                    /* Concept index. */
{
  index_add_arg ("cp");
}

void
cm_findex (void)                    /* Function index. */
{
  index_add_arg ("fn");
}

void
cm_tindex (void)                    /* Data Type index. */
{
  index_add_arg ("tp");
}

int
index_element_compare (const void *element1, const void *element2)
{
  INDEX_ELT **elt1 = (INDEX_ELT **) element1;
  INDEX_ELT **elt2 = (INDEX_ELT **) element2;

  return index_compare_fn ((*elt1)->entry, (*elt2)->entry);
}

/* Force all index entries to be unique. */
static void
make_index_entries_unique (INDEX_ELT **array, int count)
{
  int i, j;
  INDEX_ELT **copy;
  int counter = 1;

  copy = xmalloc ((1 + count) * sizeof (INDEX_ELT *));

  for (i = 0, j = 0; i < count; i++)
    {
      if (i == (count - 1)
          || array[i]->node != array[i + 1]->node
          || !STREQ (array[i]->entry, array[i + 1]->entry))
        copy[j++] = array[i];
      else
        {
          free (array[i]->entry);
          free (array[i]->entry_text);
          free (array[i]);
        }
    }
  copy[j] = NULL;

  /* Now COPY contains only unique entries.  Duplicated entries in the
     original array have been freed.  Replace the current array with
     the copy, fixing the NEXT pointers. */
  for (i = 0; copy[i]; i++)
    {
      copy[i]->next = copy[i + 1];

      /* Fix entry names which are the same.  They point to different nodes,
         so we make the entry name unique. */
      if (copy[i+1]
          && STREQ (copy[i]->entry, copy[i + 1]->entry)
          && !html)
        {
          char *new_entry_name;

          new_entry_name = xmalloc (10 + strlen (copy[i]->entry));
          sprintf (new_entry_name, "%s <%d>", copy[i]->entry, counter);
          free (copy[i]->entry);
          copy[i]->entry = new_entry_name;
          counter++;
        }
      else
        counter = 1;

      array[i] = copy[i];
    }
  array[i] = NULL;

  /* Free the storage used only by COPY. */
  free (copy);
}


/* Sort the index passed in INDEX, returning an array of pointers to
   elements.  The array is terminated with a NULL pointer.  */

static INDEX_ELT **
sort_index (INDEX_ELT *index)
{
  INDEX_ELT **array;
  INDEX_ELT *temp;
  int count = 0;
  int save_line_number = line_number;
  char *save_input_filename = input_filename;
  int save_html = html;

  /* Pretend we are in non-HTML mode, for the purpose of getting the
     expanded index entry that lacks any markup and other HTML escape
     characters which could produce a wrong sort order.  */
  /* fixme: html: this still causes some markup, such as non-ASCII
     characters @AE{} etc., to sort incorrectly.  */
  html = 0;

  for (temp = index, count = 0; temp; temp = temp->next, count++)
    ;
  /* We have the length, now we can allocate an array. */
  array = xmalloc ((count + 1) * sizeof (INDEX_ELT *));

  for (temp = index, count = 0; temp; temp = temp->next, count++)
    {
      /* Allocate new memory for the return array, since parts of the
         original INDEX get freed.  Otherwise, if the document calls
         @printindex twice on the same index, with duplicate entries,
         we'll have garbage the second time.  There are cleaner ways to
         deal, but this will suffice for now.  */
      array[count] = xmalloc (sizeof (INDEX_ELT));
      *(array[count]) = *(temp);  /* struct assignment, hope it's ok */

      /* Adjust next pointers to use the new memory.  */
      if (count > 0)
        array[count-1]->next = array[count];

      /* Set line number and input filename to the source line for this
         index entry, as this expansion finds any errors.  */
      line_number = array[count]->defining_line;
      input_filename = array[count]->defining_file;

      /* If this particular entry should be printed as a "code" index,
         then expand it as @code{entry}, i.e., as in fixed-width font.  */
      array[count]->entry = expansion (temp->entry_text, array[count]->code);
    }
  array[count] = NULL;    /* terminate the array. */

  line_number = save_line_number;
  input_filename = save_input_filename;
  html = save_html;

#ifdef HAVE_STRCOLL
  /* This is not perfect.  We should set (then restore) the locale to the
     documentlanguage, so strcoll operates according to the document's
     locale, not the user's.  For now, I'm just going to assume that
     those few new documents which use @documentlanguage will be
     processed in the appropriate locale.  In any case, don't use
     strcoll in the C (aka POSIX) locale, that is the ASCII ordering.  */
  if (language_code != en)
    {
      char *lang_env = getenv ("LANG");
      if (lang_env && !STREQ (lang_env, "C") && !STREQ (lang_env, "POSIX"))
        index_compare_fn = strcoll;
    }
#endif /* HAVE_STRCOLL */

  /* Sort the array. */
  qsort (array, count, sizeof (INDEX_ELT *), index_element_compare);

  /* Remove duplicate entries.  */
  make_index_entries_unique (array, count);

  /* Replace the original index with the sorted one, in case the
     document wants to print it again.  If the index wasn't empty.  */
  if (index)
    *index = **array;

  return array;
}

static void
insert_index_output_line_no (int line_number, int output_line_number_len)
{
  int last_column;
  int str_size = output_line_number_len + strlen (_("(line )"))
    + sizeof (NULL);
  char *out_line_no_str = (char *) xmalloc (str_size + 1);

  /* Do not translate ``(line NNN)'' below for !no_headers case (Info output),
     because it's something like the ``* Menu'' strings.  For plaintext output
     it should be translated though.  */
  sprintf (out_line_no_str,
      no_headers ? _("(line %*d)") : "(line %*d)",
      output_line_number_len, line_number);

  {
    int i = output_paragraph_offset; 
    while (0 < i && output_paragraph[i-1] != '\n')
      i--;
    last_column = output_paragraph_offset - i;
  }

  if (last_column + strlen (out_line_no_str) > fill_column)
    {
      insert ('\n');
      last_column = 0;
    }

  while (last_column + strlen (out_line_no_str) < fill_column)
    {
      insert (' ');
      last_column++;
    }

  insert_string (out_line_no_str);
  insert ('\n');

  free (out_line_no_str);
}

/* Nonzero means that we are in the middle of printing an index. */
int printing_index = 0;

/* Takes one arg, a short name of an index to print.
   Outputs a menu of the sorted elements of the index. */
void
cm_printindex (void)
{
  char *index_name;
  get_rest_of_line (0, &index_name);

  /* get_rest_of_line increments the line number by one,
     so to make warnings/errors point to the correct line,
     we decrement the line_number again.  */
  if (!handling_delayed_writes)
    line_number--;

  if (xml && !docbook)
    {
      xml_insert_element (PRINTINDEX, START);
      insert_string (index_name);
      xml_insert_element (PRINTINDEX, END);
    }
  else if (!handling_delayed_writes)
    {
      int command_len = sizeof ("@ ") + strlen (command) + strlen (index_name);
      char *index_command = xmalloc (command_len + 1);

      close_paragraph ();
      if (docbook)
        xml_begin_index ();

      sprintf (index_command, "@%s %s", command, index_name);
      register_delayed_write (index_command);
      free (index_command);
    }
  else
    {
      int item;
      INDEX_ELT *index;
      INDEX_ELT *last_index = 0;
      INDEX_ELT **array;
      unsigned line_length;
      char *line;
      int saved_inhibit_paragraph_indentation = inhibit_paragraph_indentation;
      int saved_filling_enabled = filling_enabled;
      int saved_line_number = line_number;
      char *saved_input_filename = input_filename;
      unsigned output_line_number_len;

      index = index_list (index_name);
      if (index == (INDEX_ELT *)-1)
        {
          line_error (_("Unknown index `%s' in @printindex"), index_name);
          free (index_name);
          return;
        }

      /* Do this before sorting, so execute_string is in the good environment */
      if (xml && docbook)
        xml_begin_index ();

      /* Do this before sorting, so execute_string in index_element_compare
         will give the same results as when we actually print.  */
      printing_index = 1;
      filling_enabled = 0;
      inhibit_paragraph_indentation = 1;
      xml_sort_index = 1;
      array = sort_index (index);
      xml_sort_index = 0;
      close_paragraph ();
      if (html)
        add_html_block_elt_args ("<ul class=\"index-%s\" compact>",
                                 index_name);
      else if (!no_headers && !docbook)
        { /* Info.  Add magic cookie for info readers (to treat this
             menu differently), and the usual start-of-menu.  */
          add_char ('\0');
          add_word ("\010[index");
          add_char ('\0');
          add_word ("\010]\n");
          add_word ("* Menu:\n\n");
        }

      me_inhibit_expansion++;

      /* This will probably be enough.  */
      line_length = 100;
      line = xmalloc (line_length);

      {
        char *max_output_line_number = (char *) xmalloc (25 * sizeof (char));

        if (no_headers)
          sprintf (max_output_line_number, "%d", output_line_number);
        else
          {
            INDEX_ELT *tmp_entry = index;
            unsigned tmp = 0;
            for (tmp_entry = index; tmp_entry; tmp_entry = tmp_entry->next)
              tmp = tmp_entry->output_line > tmp ? tmp_entry->output_line : tmp;
            sprintf (max_output_line_number, "%d", tmp);
          }

        output_line_number_len = strlen (max_output_line_number);
        free (max_output_line_number);
      }

      for (item = 0; (index = array[item]); item++)
        {
          /* A pathological document might have an index entry outside of any
             node.  Don't crash; try using the section name instead.  */
          char *index_node = index->node;

          line_number = index->defining_line;
          input_filename = index->defining_file;

          if ((!index_node || !*index_node) && html)
            index_node = toc_find_section_of_node (index_node);

          if (!index_node || !*index_node)
            {
              line_error (_("Entry for index `%s' outside of any node"),
                          index_name);
              if (html || !no_headers)
                index_node = (char *) _("(outside of any node)");
            }

          if (html)
            {
              /* For HTML, we need to expand and HTML-escape the
                 original entry text, at the same time.  Consider
                 @cindex J@"urgen.  We want J&uuml;urgen.  We can't
                 expand and then escape since we'll end up with
                 J&amp;uuml;rgen.  We can't escape and then expand
                 because then `expansion' will see J@&quot;urgen, and
                 @&quot;urgen is not a command.  */
              char *html_entry =
                maybe_escaped_expansion (index->entry_text, index->code, 1);

              add_html_block_elt_args ("\n<li><a href=\"%s#index-",
                  (splitting && index->output_file) ? index->output_file : "");
              add_escaped_anchor_name (index->entry_text, 0);
              add_word_args ("-%d\">%s</a>: ", index->entry_number,
                  html_entry);
              free (html_entry);

              add_word ("<a href=\"");
              if (index->node && *index->node)
                {
                  /* Ensure any non-macros in the node name are expanded.  */
                  char *expanded_index;

                  in_fixed_width_font++;
                  expanded_index = expansion (index_node, 0);
                  in_fixed_width_font--;
                  add_anchor_name (expanded_index, 1);
		  expanded_index = escape_string (expanded_index);
                  add_word_args ("\">%s</a>", expanded_index);
                  free (expanded_index);
                }
              else if (STREQ (index_node, _("(outside of any node)")))
                {
                  add_anchor_name (index_node, 1);
                  add_word_args ("\">%s</a>", index_node);
                }
              else
                /* If we use the section instead of the (missing) node, then
                   index_node already includes all we need except the #.  */
                add_word_args ("#%s</a>", index_node);

              add_html_block_elt ("</li>");
            }
          else if (xml && docbook)
            {
              /* In the DocBook case, the expanded index entry is not
                 good for us, since it was expanded for non-DocBook mode
                 inside sort_index.  So we send the original entry text
                 to be used with execute_string.  */
              xml_insert_indexentry (index->entry_text, index_node);
            }
          else
            {
              unsigned new_length = strlen (index->entry);

              if (new_length < 50) /* minimum length used below */
                new_length = 50;
              new_length += strlen (index_node) + 7; /* * : .\n\0 */

              if (new_length > line_length)
                {
                  line_length = new_length;
                  line = xrealloc (line, line_length);
                }
              /* Print the entry, nicely formatted.  We've already
                 expanded any commands in index->entry, including any
                 implicit @code.  Thus, can't call execute_string, since
                 @@ has turned into @. */
              if (!no_headers)
                {
                  sprintf (line, "* %-37s  ", index->entry);
                  line[2 + strlen (index->entry)] = ':';
                  insert_string (line);
                  /* Make sure any non-macros in the node name are expanded.  */
                  in_fixed_width_font++;
                  execute_string ("%s. ", index_node);
                  insert_index_output_line_no (index->output_line,
                      output_line_number_len);
                  in_fixed_width_font--;
                }
              else
                {
                  /* With --no-headers, the @node lines are gone, so
                     there's little sense in referring to them in the
                     index.  Instead, output the number or name of the
                     section that corresponds to that node.  */
                  sprintf (line, "%-*s ", number_sections ? 46 : 1, index->entry);
                  line[strlen (index->entry)] = ':';
                  insert_string (line);

                  if (strlen (index->section) > 0)
                    { /* We got your number.  */
                      insert_string ((char *) _("See "));
                      insert_string (index->section);
                    }
                  else
                    { /* Sigh, index in an @unnumbered. :-\  */
                      insert_string ("\n          ");
                      insert_string ((char *) _("See "));
                      insert_string ("``");
                      insert_string (expansion (index->section_name, 0));
                      insert_string ("''");
                    }

                  insert_string (". ");
                  insert_index_output_line_no (index->output_line,
                      output_line_number_len);
                }
            }

          /* Prevent `output_paragraph' from growing to the size of the
             whole index.  */
          flush_output ();
          last_index = index;
        }

      free (line);

      me_inhibit_expansion--;
      printing_index = 0;

      close_single_paragraph ();
      filling_enabled = saved_filling_enabled;
      inhibit_paragraph_indentation = saved_inhibit_paragraph_indentation;
      input_filename = saved_input_filename;
      line_number = saved_line_number;

      if (html)
        add_html_block_elt ("</ul>");
      else if (xml && docbook)
        xml_end_index ();
    }

  free (index_name);
  /* Re-increment the line number, because get_rest_of_line
     left us looking at the next line after the command.  */
  line_number++;
}
