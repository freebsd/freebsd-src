/* index.c -- indexing for Texinfo.
   $Id: index.c,v 1.24 2002/01/22 14:28:07 karl Exp $

   Copyright (C) 1998, 99, 2002 Free Software Foundation, Inc.

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
#include "index.h"
#include "lang.h"
#include "macro.h"
#include "toc.h"
#include "xml.h"

/* An index element... */
typedef struct index_elt
{
  struct index_elt *next;
  char *entry;                  /* The index entry itself, after expansion. */
  char *entry_text;             /* The original, non-expanded entry text. */
  char *node;                   /* The node from whence it came. */
  int code;                     /* Nonzero means add `@code{...}' when
                                   printing this element. */
  int defining_line;            /* Line number where this entry was written. */
  char *defining_file;          /* Source file for defining_line. */
} INDEX_ELT;


/* A list of short-names for each index.
   There are two indices into the the_indices array.
   * read_index is the index that points to the list of index
     entries that we will find if we ask for the list of entries for
     this name.
   * write_index is the index that points to the list of index entries
     that we will add new entries to.

   Initially, read_index and write_index are the same, but the
   @syncodeindex and @synindex commands can change the list we add
   entries to.

   For example, after the commands
     @cindex foo
     @defindex ii
     @synindex cp ii
     @cindex bar

   the cp index will contain the entry `foo', and the new ii
   index will contain the entry `bar'.  This is consistent with the
   way texinfo.tex handles the same situation.

   In addition, for each index, it is remembered whether that index is
   a code index or not.  Code indices have @code{} inserted around the
   first word when they are printed with printindex. */
typedef struct
{
  char *name;
  int read_index;   /* index entries for `name' */
  int write_index;  /* store index entries here, @synindex can change it */
  int code;
} INDEX_ALIST;

INDEX_ALIST **name_index_alist = NULL;

/* An array of pointers.  Each one is for a different index.  The
   "synindex" command changes which array slot is pointed to by a
   given "index". */
INDEX_ELT **the_indices = NULL;

/* The number of defined indices. */
int defined_indices = 0;

/* Stuff for defining commands on the fly. */
COMMAND **user_command_array = NULL;
int user_command_array_len = 0;

/* How to compare index entries for sorting.  May be set to strcoll.  */
int (*index_compare_fn) () = strcasecmp;

/* Find which element in the known list of indices has this name.
   Returns -1 if NAME isn't found. */
static int
find_index_offset (name)
     char *name;
{
  int i;
  for (i = 0; i < defined_indices; i++)
    if (name_index_alist[i] && STREQ (name, name_index_alist[i]->name))
      return i;
  return -1;
}

/* Return a pointer to the entry of (name . index) for this name.
   Return NULL if the index doesn't exist. */
INDEX_ALIST *
find_index (name)
     char *name;
{
  int offset = find_index_offset (name);
  if (offset > -1)
    return name_index_alist[offset];
  else
    return NULL;
}

/* User-defined commands, which happens only from user-defined indexes.
   Used to initialize the builtin indices, too.  */
void
define_user_command (name, proc, needs_braces_p)
     char *name;
     COMMAND_FUNCTION *proc;
     int needs_braces_p;
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
free_index (index)
     INDEX_ELT *index;
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
undefindex (name)
     char *name;
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

/* Add the arguments to the current index command to the index NAME.
   html fixxme generate specific html anchor */
static void
index_add_arg (name)
     char *name;
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
      new->next = the_indices[which];
      new->entry_text = index_entry;
      new->entry = NULL;
      new->node = current_node ? current_node : xstrdup ("");
      new->code = tem->code;
      new->defining_line = line_number - 1;
      /* We need to make a copy since input_filename may point to
         something that goes away, for example, inside a macro.
         (see the findexerr test).  */
      new->defining_file = xstrdup (input_filename);
      the_indices[which] = new;
      /* The index breaks if there are colons in the entry. */
      if (strchr (new->entry_text, ':'))
        warning (_("Info cannot handle `:' in index entry `%s'"),
                 new->entry_text);
    }
  if (xml)
    xml_insert_indexterm (index_entry, name);
}

/* The function which user defined index commands call. */
static void
gen_index ()
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
defindex (name, code)
     char *name;
     int code;
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
top_defindex (name, code)
     char *name;
     int code;
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
init_indices ()
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
      undefindex (name_index_alist[i]->name);
      if (name_index_alist[i])
        { /* Suppose we're called with two input files, and the first
             does a @synindex pg cp.  Then, when we get here to start
             the second file, the "pg" element won't get freed by
             undefindex (because it's pointing to "cp").  So free it
             here; otherwise, when we try to define the pg index again
             just below, it will still point to cp.  */
          free (name_index_alist[i]->name);
          free (name_index_alist[i]);
          name_index_alist[i] = NULL;
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
int
translate_index (name)
     char *name;
{
  INDEX_ALIST *which = find_index (name);

  if (which)
    return which->read_index;
  else
    return -1;
}

/* Return the index list which belongs to NAME. */
INDEX_ELT *
index_list (name)
     char *name;
{
  int which = translate_index (name);
  if (which < 0)
    return (INDEX_ELT *) -1;
  else
    return the_indices[which];
}

/* Define a new index command.  Arg is name of index. */
static void
gen_defindex (code)
     int code;
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
cm_defindex ()
{
  gen_defindex (0);
}

void
cm_defcodeindex ()
{
  gen_defindex (1);
}

/* Expects 2 args, on the same line.  Both are index abbreviations.
   Make the first one be a synonym for the second one, i.e. make the
   first one have the same index as the second one. */
void
cm_synindex ()
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
      name_index_alist[target]->write_index
        = name_index_alist[source]->write_index;
    }

  free (abbrev1);
  free (abbrev2);
}

void
cm_pindex ()                    /* Pinhead index. */
{
  index_add_arg ("pg");
}

void
cm_vindex ()                    /* Variable index. */
{
  index_add_arg ("vr");
}

void
cm_kindex ()                    /* Key index. */
{
  index_add_arg ("ky");
}

void
cm_cindex ()                    /* Concept index. */
{
  index_add_arg ("cp");
}

void
cm_findex ()                    /* Function index. */
{
  index_add_arg ("fn");
}

void
cm_tindex ()                    /* Data Type index. */
{
  index_add_arg ("tp");
}

int
index_element_compare (element1, element2)
     INDEX_ELT **element1, **element2;
{
  return index_compare_fn ((*element1)->entry, (*element2)->entry);
}

/* Force all index entries to be unique. */
void
make_index_entries_unique (array, count)
     INDEX_ELT **array;
     int count;
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

/* Sort the index passed in INDEX, returning an array of
   pointers to elements.  The array is terminated with a NULL
   pointer.  We call qsort because it's supposed to be fast.
   I think this looks bad. */
INDEX_ELT **
sort_index (index)
     INDEX_ELT *index;
{
  INDEX_ELT **array;
  INDEX_ELT *temp = index;
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

  while (temp)
    {
      count++;
      temp = temp->next;
    }

  /* We have the length.  Make an array. */

  array = xmalloc ((count + 1) * sizeof (INDEX_ELT *));
  count = 0;
  temp = index;

  while (temp)
    {
      array[count++] = temp;

      /* Set line number and input filename to the source line for this
         index entry, as this expansion finds any errors.  */
      line_number = array[count - 1]->defining_line;
      input_filename = array[count - 1]->defining_file;

      /* If this particular entry should be printed as a "code" index,
         then expand it as @code{entry}, i.e. as in fixed-width font.  */
      array[count-1]->entry = expansion (temp->entry_text,
				         array[count-1]->code);

      temp = temp->next;
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
  make_index_entries_unique (array, count);
  return array;
}

/* Nonzero means that we are in the middle of printing an index. */
int printing_index = 0;

/* Takes one arg, a short name of an index to print.
   Outputs a menu of the sorted elements of the index. */
void
cm_printindex ()
{
  if (xml && !docbook)
    {
      char *index_name;
      get_rest_of_line (0, &index_name);
      xml_insert_element (PRINTINDEX, START);
      insert_string (index_name);
      xml_insert_element (PRINTINDEX, END);
    }
  else
    {
      int item;
      INDEX_ELT *index;
      INDEX_ELT *last_index = 0;
      INDEX_ELT **array;
      char *index_name;
      unsigned line_length;
      char *line;
      int saved_inhibit_paragraph_indentation = inhibit_paragraph_indentation;
      int saved_filling_enabled = filling_enabled;
      int saved_line_number = line_number;
      char *saved_input_filename = input_filename;

      close_paragraph ();
      get_rest_of_line (0, &index_name);

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
	add_word ("<ul compact>");
      else if (!no_headers && !docbook)
	add_word ("* Menu:\n\n");
      
      me_inhibit_expansion++;
      
      /* This will probably be enough.  */
      line_length = 100;
      line = xmalloc (line_length);
      
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
		index_node = _("(outside of any node)");
	    }
	  
	  if (html)
	    /* fixme: html: we should use specific index anchors pointing
           to the actual location of the indexed position (but then we
           have to find something to wrap the anchor around). */
	    {
	      if (last_index
		  && STREQ (last_index->entry_text, index->entry_text))
		add_word (", ");  /* Don't repeat the previous entry. */
	      else
		{
		  /* In the HTML case, the expanded index entry is not
		     good for us, since it was expanded for non-HTML mode
		     inside sort_index.  So we need to HTML-escape and
		     expand the original entry text here.  */
		  char *escaped_entry = xstrdup (index->entry_text);
		  char *expanded_entry;
		  
		  /* expansion() doesn't HTML-escape the argument, so need
		     to do it separately.  */
		  escaped_entry = escape_string (escaped_entry);
		  expanded_entry = expansion (escaped_entry, index->code);
		  add_word_args ("\n<li>%s: ", expanded_entry);
		  free (escaped_entry);
		  free (expanded_entry);
		}
	      add_word ("<a href=\"");
	      if (index->node && *index->node)
		{
		  /* Make sure any non-macros in the node name are expanded.  */
		  in_fixed_width_font++;
		  index_node = expansion (index_node, 0);
		  in_fixed_width_font--;
		  add_anchor_name (index_node, 1);
		  add_word_args ("\">%s</a>", index_node);
		  free (index_node);
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
	    }
	  else if (xml && docbook)
	    {
	      xml_insert_indexentry (index->entry, index_node);
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
		  execute_string ("%s.\n", index_node);
		  in_fixed_width_font--;
		}
	      else
		{
		  /* With --no-headers, the @node lines are gone, so
		     there's little sense in referring to them in the
		     index.  Instead, output the number or name of the
		     section that corresponds to that node.  */
		  char *section_name = toc_find_section_of_node (index_node);
		  
		  sprintf (line, "%-*s ", number_sections ? 50 : 1, index->entry);
		  line[strlen (index->entry)] = ':';
		  insert_string (line);
		  if (section_name)
		    {
		      int idx = 0;
		      unsigned ref_len = strlen (section_name) + 30;
		      
		      if (ref_len > line_length)
			{
			  line_length = ref_len;
			  line = xrealloc (line, line_length);
			}
		      
		      if (number_sections)
			{
			  while (section_name[idx]
				 && (isdigit (section_name[idx])
				     || (idx && section_name[idx] == '.')))
			    idx++;
			}
		      if (idx)
			sprintf (line, " See %.*s.\n", idx, section_name);
		      else
			sprintf (line, "\n          See ``%s''.\n", section_name);
		      insert_string (line);
		    }
		  else
		    {
		      insert_string (" "); /* force a blank */
		      execute_string ("See node %s.\n", index_node);
		    }
		}
	    }
	  
	  /* Prevent `output_paragraph' from growing to the size of the
	     whole index.  */
	  flush_output ();
	  last_index = index;
	}

      free (line);
      free (index_name);
      
      me_inhibit_expansion--;
      
      printing_index = 0;
      free (array);
      close_single_paragraph ();
      filling_enabled = saved_filling_enabled;
      inhibit_paragraph_indentation = saved_inhibit_paragraph_indentation;
      input_filename = saved_input_filename;
      line_number = saved_line_number;
      
      if (html)
	add_word ("</ul>");
      else if (xml && docbook)
	xml_end_index ();
    }
}
