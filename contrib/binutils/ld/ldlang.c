/* Linker command language support.
   Copyright (C) 1991, 92, 93, 94, 95, 96, 97, 98, 99, 2000
   Free Software Foundation, Inc.

This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libiberty.h"
#include "obstack.h"
#include "bfdlink.h"

#include "ld.h"
#include "ldmain.h"
#include "ldgram.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldemul.h"
#include "ldlex.h"
#include "ldmisc.h"
#include "ldctor.h"
#include "ldfile.h"
#include "fnmatch.h"
#include "demangle.h"

#include <ctype.h>

/* FORWARDS */
static lang_statement_union_type *new_statement PARAMS ((enum statement_enum,
							 size_t,
							 lang_statement_list_type*));


/* LOCALS */
static struct obstack stat_obstack;

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
static CONST char *startup_file;
static lang_statement_list_type input_file_chain;
static boolean placed_commons = false;
static lang_output_section_statement_type *default_common_section;
static boolean map_option_f;
static bfd_vma print_dot;
static lang_input_statement_type *first_file;
static CONST char *current_target;
static CONST char *output_target;
static lang_statement_list_type statement_list;
static struct lang_phdr *lang_phdr_list;

static void lang_for_each_statement_worker
  PARAMS ((void (*func) (lang_statement_union_type *),
	   lang_statement_union_type *s));
static lang_input_statement_type *new_afile
  PARAMS ((const char *name, lang_input_file_enum_type file_type,
	   const char *target, boolean add_to_list));
static void init_os PARAMS ((lang_output_section_statement_type *s));
static void exp_init_os PARAMS ((etree_type *));
static void section_already_linked PARAMS ((bfd *, asection *, PTR));
static struct bfd_hash_entry *already_linked_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *,
	   const char *string));
static void already_linked_table_init PARAMS ((void));
static void already_linked_table_free PARAMS ((void));
static boolean wildcardp PARAMS ((const char *));
static lang_statement_union_type *wild_sort
  PARAMS ((lang_wild_statement_type *, lang_input_statement_type *,
	   asection *));
static lang_input_statement_type *lookup_name PARAMS ((const char *name));
static void load_symbols PARAMS ((lang_input_statement_type *entry,
				  lang_statement_list_type *));
static void wild PARAMS ((lang_wild_statement_type *s,
			  const char *section, const char *file,
			  const char *target,
			  lang_output_section_statement_type *output));
static bfd *open_output PARAMS ((const char *name));
static void ldlang_open_output PARAMS ((lang_statement_union_type *statement));
static void open_input_bfds
  PARAMS ((lang_statement_union_type *statement, boolean));
static void lang_reasonable_defaults PARAMS ((void));
static void lang_place_undefineds PARAMS ((void));
static void map_input_to_output_sections
  PARAMS ((lang_statement_union_type *s,
	   const char *target,
	   lang_output_section_statement_type *output_section_statement));
static void print_output_section_statement
  PARAMS ((lang_output_section_statement_type *output_section_statement));
static void print_assignment
  PARAMS ((lang_assignment_statement_type *assignment,
	   lang_output_section_statement_type *output_section));
static void print_input_statement PARAMS ((lang_input_statement_type *statm));
static boolean print_one_symbol PARAMS ((struct bfd_link_hash_entry *, PTR));
static void print_input_section PARAMS ((lang_input_section_type *in));
static void print_fill_statement PARAMS ((lang_fill_statement_type *fill));
static void print_data_statement PARAMS ((lang_data_statement_type *data));
static void print_address_statement PARAMS ((lang_address_statement_type *));
static void print_reloc_statement PARAMS ((lang_reloc_statement_type *reloc));
static void print_padding_statement PARAMS ((lang_padding_statement_type *s));
static void print_wild_statement
  PARAMS ((lang_wild_statement_type *w,
	   lang_output_section_statement_type *os));
static void print_group
  PARAMS ((lang_group_statement_type *, lang_output_section_statement_type *));
static void print_statement PARAMS ((lang_statement_union_type *s,
				     lang_output_section_statement_type *os));
static void print_statement_list PARAMS ((lang_statement_union_type *s,
					  lang_output_section_statement_type *os));
static void print_statements PARAMS ((void));
static bfd_vma insert_pad PARAMS ((lang_statement_union_type **this_ptr,
				   fill_type fill, unsigned int power,
				   asection *output_section_statement,
				   bfd_vma dot));
static bfd_vma size_input_section
  PARAMS ((lang_statement_union_type **this_ptr,
	   lang_output_section_statement_type *output_section_statement,
	   fill_type fill, bfd_vma dot, boolean relax));
static void lang_finish PARAMS ((void));
static void ignore_bfd_errors PARAMS ((const char *, ...));
static void lang_check PARAMS ((void));
static void lang_common PARAMS ((void));
static boolean lang_one_common PARAMS ((struct bfd_link_hash_entry *, PTR));
static void lang_place_orphans PARAMS ((void));
static int topower PARAMS ((int));
static void lang_set_startof PARAMS ((void));
static void reset_memory_regions PARAMS ((void));
static void lang_record_phdrs PARAMS ((void));
static void lang_gc_wild
  PARAMS ((lang_wild_statement_type *, const char *, const char *));
static void lang_gc_sections_1 PARAMS ((lang_statement_union_type *));
static void lang_gc_sections PARAMS ((void));
static void lang_do_version_exports_section PARAMS ((void));
static void lang_check_section_addresses PARAMS ((void));

typedef void (*callback_t) PARAMS ((lang_wild_statement_type *,
				    asection *, lang_input_statement_type *,
				    void *));
static void walk_wild_section
  PARAMS ((lang_wild_statement_type *, const char *,
	   lang_input_statement_type *, callback_t, void *));
static void walk_wild_file
  PARAMS ((lang_wild_statement_type *, const char *,
	   lang_input_statement_type *, callback_t, void *));

static int    get_target PARAMS ((const bfd_target *, void *));
static void   stricpy PARAMS ((char *, char *));
static void   strcut PARAMS ((char *, char *));
static int    name_compare PARAMS ((char *, char *));
static int    closest_target_match PARAMS ((const bfd_target *, void *));
static char * get_first_input_target PARAMS ((void));
					
/* EXPORTS */
lang_output_section_statement_type *abs_output_section;
lang_statement_list_type lang_output_section_statement;
lang_statement_list_type *stat_ptr = &statement_list;
lang_statement_list_type file_chain = { NULL, NULL };
const char *entry_symbol = NULL;
boolean entry_from_cmdline;
boolean lang_has_input_file = false;
boolean had_output_filename = false;
boolean lang_float_flag = false;
boolean delete_output_file_on_failure = false;
struct lang_nocrossrefs *nocrossref_list;

etree_type *base; /* Relocation base - or null */


#if defined(__STDC__) || defined(ALMOST_STDC)
#define cat(a,b) a##b
#else
#define cat(a,b) a/**/b
#endif

#define new_stat(x,y) (cat(x,_type)*) new_statement(cat(x,_enum), sizeof(cat(x,_type)),y)

#define outside_section_address(q) ( (q)->output_offset + (q)->output_section->vma)

#define outside_symbol_address(q) ((q)->value +   outside_section_address(q->section))

#define SECTION_NAME_MAP_LENGTH (16)

PTR
stat_alloc (size)
     size_t size;
{
  return obstack_alloc (&stat_obstack, size);
}

/*----------------------------------------------------------------------
  Generic traversal routines for finding matching sections.
*/

static void
walk_wild_section (ptr, section, file, callback, data)
     lang_wild_statement_type *ptr;
     const char *section;
     lang_input_statement_type *file;
     callback_t callback;
     void *data;
{
  /* Don't process sections from files which were excluded. */
  if (ptr->exclude_filename_list != NULL)
    {
      struct name_list *list_tmp;
      for (list_tmp = ptr->exclude_filename_list; list_tmp; list_tmp = list_tmp->next)
        {
	  boolean match;

	  if (wildcardp (list_tmp->name))
	    match = fnmatch (list_tmp->name, file->filename, 0) == 0 ? true : false;
	  else
	    match = strcmp (list_tmp->name, file->filename) == 0 ? true : false;

	  if (match)
	    return;
	}
    }

  if (file->just_syms_flag == false)
    {
      register asection *s;
      boolean wildcard;

      if (section == NULL)
	wildcard = false;
      else
	wildcard = wildcardp (section);

      for (s = file->the_bfd->sections; s != NULL; s = s->next)
	{
	  boolean match;

	  if (section == NULL)
	    match = true;
	  else
	    {
	      const char *name;

	      name = bfd_get_section_name (file->the_bfd, s);
	      if (wildcard)
		match = fnmatch (section, name, 0) == 0 ? true : false;
	      else
		match = strcmp (section, name) == 0 ? true : false;
	    }

	  if (match)
	    (*callback) (ptr, s, file, data);
	}
    }
}

/* Handle a wild statement for a single file F.  */

static void
walk_wild_file (s, section, f, callback, data)
     lang_wild_statement_type *s;
     const char *section;
     lang_input_statement_type *f;
     callback_t callback;
     void *data;
{
  if (f->the_bfd == NULL
      || ! bfd_check_format (f->the_bfd, bfd_archive))
    walk_wild_section (s, section, f, callback, data);
  else
    {
      bfd *member;

      /* This is an archive file.  We must map each member of the
	 archive separately.  */
      member = bfd_openr_next_archived_file (f->the_bfd, (bfd *) NULL);
      while (member != NULL)
	{
	  /* When lookup_name is called, it will call the add_symbols
	     entry point for the archive.  For each element of the
	     archive which is included, BFD will call ldlang_add_file,
	     which will set the usrdata field of the member to the
	     lang_input_statement.  */
	  if (member->usrdata != NULL)
	    {
	      walk_wild_section (s, section,
				 (lang_input_statement_type *) member->usrdata,
				 callback, data);
	    }

	  member = bfd_openr_next_archived_file (f->the_bfd, member);
	}
    }
}

static void
walk_wild (s, section, file, callback, data)
     lang_wild_statement_type *s;
     const char *section;
     const char *file;
     callback_t callback;
     void *data;
{
  if (file == (char *) NULL)
    {
      /* Perform the iteration over all files in the list.  */
      LANG_FOR_EACH_INPUT_STATEMENT (f)
	{
	  walk_wild_file (s, section, f, callback, data);
	}
    }
  else if (wildcardp (file))
    {
      LANG_FOR_EACH_INPUT_STATEMENT (f)
	{
	  if (fnmatch (file, f->filename, FNM_FILE_NAME) == 0)
	    walk_wild_file (s, section, f, callback, data);
	}
    }
  else
    {
      lang_input_statement_type *f;

      /* Perform the iteration over a single file.  */
      f = lookup_name (file);
      walk_wild_file (s, section, f, callback, data);
    }
}  
     
/*----------------------------------------------------------------------
  lang_for_each_statement walks the parse tree and calls the provided
  function for each node
*/

static void
lang_for_each_statement_worker (func, s)
     void (*func) PARAMS ((lang_statement_union_type *));
     lang_statement_union_type *s;
{
  for (; s != (lang_statement_union_type *) NULL; s = s->next)
    {
      func (s);

      switch (s->header.type)
	{
	case lang_constructors_statement_enum:
	  lang_for_each_statement_worker (func, constructor_list.head);
	  break;
	case lang_output_section_statement_enum:
	  lang_for_each_statement_worker
	    (func,
	     s->output_section_statement.children.head);
	  break;
	case lang_wild_statement_enum:
	  lang_for_each_statement_worker
	    (func,
	     s->wild_statement.children.head);
	  break;
	case lang_group_statement_enum:
	  lang_for_each_statement_worker (func,
					  s->group_statement.children.head);
	  break;
	case lang_data_statement_enum:
	case lang_reloc_statement_enum:
	case lang_object_symbols_statement_enum:
	case lang_output_statement_enum:
	case lang_target_statement_enum:
	case lang_input_section_enum:
	case lang_input_statement_enum:
	case lang_assignment_statement_enum:
	case lang_padding_statement_enum:
	case lang_address_statement_enum:
	case lang_fill_statement_enum:
	  break;
	default:
	  FAIL ();
	  break;
	}
    }
}

void
lang_for_each_statement (func)
     void (*func) PARAMS ((lang_statement_union_type *));
{
  lang_for_each_statement_worker (func,
				  statement_list.head);
}

/*----------------------------------------------------------------------*/
void
lang_list_init (list)
     lang_statement_list_type *list;
{
  list->head = (lang_statement_union_type *) NULL;
  list->tail = &list->head;
}

/*----------------------------------------------------------------------

  build a new statement node for the parse tree

 */

static
lang_statement_union_type *
new_statement (type, size, list)
     enum statement_enum type;
     size_t size;
     lang_statement_list_type * list;
{
  lang_statement_union_type *new = (lang_statement_union_type *)
  stat_alloc (size);

  new->header.type = type;
  new->header.next = (lang_statement_union_type *) NULL;
  lang_statement_append (list, new, &new->header.next);
  return new;
}

/*
  Build a new input file node for the language. There are several ways
  in which we treat an input file, eg, we only look at symbols, or
  prefix it with a -l etc.

  We can be supplied with requests for input files more than once;
  they may, for example be split over serveral lines like foo.o(.text)
  foo.o(.data) etc, so when asked for a file we check that we havn't
  got it already so we don't duplicate the bfd.

 */
static lang_input_statement_type *
new_afile (name, file_type, target, add_to_list)
     CONST char *name;
     lang_input_file_enum_type file_type;
     CONST char *target;
     boolean add_to_list;
{
  lang_input_statement_type *p;

  if (add_to_list)
    p = new_stat (lang_input_statement, stat_ptr);
  else
    {
      p = ((lang_input_statement_type *)
	   stat_alloc (sizeof (lang_input_statement_type)));
      p->header.next = NULL;
    }

  lang_has_input_file = true;
  p->target = target;
  switch (file_type)
    {
    case lang_input_file_is_symbols_only_enum:
      p->filename = name;
      p->is_archive = false;
      p->real = true;
      p->local_sym_name = name;
      p->just_syms_flag = true;
      p->search_dirs_flag = false;
      break;
    case lang_input_file_is_fake_enum:
      p->filename = name;
      p->is_archive = false;
      p->real = false;
      p->local_sym_name = name;
      p->just_syms_flag = false;
      p->search_dirs_flag = false;
      break;
    case lang_input_file_is_l_enum:
      p->is_archive = true;
      p->filename = name;
      p->real = true;
      p->local_sym_name = concat ("-l", name, (const char *) NULL);
      p->just_syms_flag = false;
      p->search_dirs_flag = true;
      break;
    case lang_input_file_is_marker_enum:
      p->filename = name;
      p->is_archive = false;
      p->real = false;
      p->local_sym_name = name;
      p->just_syms_flag = false;
      p->search_dirs_flag = true;
      break;
    case lang_input_file_is_search_file_enum:
      p->filename = name;
      p->is_archive = false;
      p->real = true;
      p->local_sym_name = name;
      p->just_syms_flag = false;
      p->search_dirs_flag = true;
      break;
    case lang_input_file_is_file_enum:
      p->filename = name;
      p->is_archive = false;
      p->real = true;
      p->local_sym_name = name;
      p->just_syms_flag = false;
      p->search_dirs_flag = false;
      break;
    default:
      FAIL ();
    }
  p->the_bfd = (bfd *) NULL;
  p->asymbols = (asymbol **) NULL;
  p->next_real_file = (lang_statement_union_type *) NULL;
  p->next = (lang_statement_union_type *) NULL;
  p->symbol_count = 0;
  p->dynamic = config.dynamic_link;
  p->whole_archive = whole_archive;
  p->loaded = false;
  lang_statement_append (&input_file_chain,
			 (lang_statement_union_type *) p,
			 &p->next_real_file);
  return p;
}

lang_input_statement_type *
lang_add_input_file (name, file_type, target)
     CONST char *name;
     lang_input_file_enum_type file_type;
     CONST char *target;
{
  lang_has_input_file = true;
  return new_afile (name, file_type, target, true);
}

/* Build enough state so that the parser can build its tree */
void
lang_init ()
{
  obstack_begin (&stat_obstack, 1000);

  stat_ptr = &statement_list;

  lang_list_init (stat_ptr);

  lang_list_init (&input_file_chain);
  lang_list_init (&lang_output_section_statement);
  lang_list_init (&file_chain);
  first_file = lang_add_input_file ((char *) NULL,
				    lang_input_file_is_marker_enum,
				    (char *) NULL);
  abs_output_section = lang_output_section_statement_lookup (BFD_ABS_SECTION_NAME);

  abs_output_section->bfd_section = bfd_abs_section_ptr;

}

/*----------------------------------------------------------------------
 A region is an area of memory declared with the
 MEMORY {  name:org=exp, len=exp ... }
 syntax.

 We maintain a list of all the regions here

 If no regions are specified in the script, then the default is used
 which is created when looked up to be the entire data space
*/

static lang_memory_region_type *lang_memory_region_list;
static lang_memory_region_type **lang_memory_region_list_tail = &lang_memory_region_list;

lang_memory_region_type *
lang_memory_region_lookup (name)
     CONST char *CONST name;
{
  lang_memory_region_type *p;

  for (p = lang_memory_region_list;
       p != (lang_memory_region_type *) NULL;
       p = p->next)
    {
      if (strcmp (p->name, name) == 0)
	{
	  return p;
	}
    }

#if 0
  /* This code used to always use the first region in the list as the
     default region.  I changed it to instead use a region
     encompassing all of memory as the default region.  This permits
     NOLOAD sections to work reasonably without requiring a region.
     People should specify what region they mean, if they really want
     a region.  */
  if (strcmp (name, "*default*") == 0)
    {
      if (lang_memory_region_list != (lang_memory_region_type *) NULL)
	{
	  return lang_memory_region_list;
	}
    }
#endif

  {
    lang_memory_region_type *new =
    (lang_memory_region_type *) stat_alloc (sizeof (lang_memory_region_type));

    new->name = buystring (name);
    new->next = (lang_memory_region_type *) NULL;

    *lang_memory_region_list_tail = new;
    lang_memory_region_list_tail = &new->next;
    new->origin = 0;
    new->flags = 0;
    new->not_flags = 0;
    new->length = ~(bfd_size_type)0;
    new->current = 0;
    new->had_full_message = false;

    return new;
  }
}


lang_memory_region_type *
lang_memory_default (section)
     asection *section;
{
  lang_memory_region_type *p;

  flagword sec_flags = section->flags;

  /* Override SEC_DATA to mean a writable section.  */
  if ((sec_flags & (SEC_ALLOC | SEC_READONLY | SEC_CODE)) == SEC_ALLOC)
    sec_flags |= SEC_DATA;

  for (p = lang_memory_region_list;
       p != (lang_memory_region_type *) NULL;
       p = p->next)
    {
      if ((p->flags & sec_flags) != 0
	  && (p->not_flags & sec_flags) == 0)
	{
	  return p;
	}
    }
  return lang_memory_region_lookup ("*default*");
}

lang_output_section_statement_type *
lang_output_section_find (name)
     CONST char *CONST name;
{
  lang_statement_union_type *u;
  lang_output_section_statement_type *lookup;

  for (u = lang_output_section_statement.head;
       u != (lang_statement_union_type *) NULL;
       u = lookup->next)
    {
      lookup = &u->output_section_statement;
      if (strcmp (name, lookup->name) == 0)
	{
	  return lookup;
	}
    }
  return (lang_output_section_statement_type *) NULL;
}

lang_output_section_statement_type *
lang_output_section_statement_lookup (name)
     CONST char *CONST name;
{
  lang_output_section_statement_type *lookup;

  lookup = lang_output_section_find (name);
  if (lookup == (lang_output_section_statement_type *) NULL)
    {

      lookup = (lang_output_section_statement_type *)
	new_stat (lang_output_section_statement, stat_ptr);
      lookup->region = (lang_memory_region_type *) NULL;
      lookup->lma_region = (lang_memory_region_type *) NULL;
      lookup->fill = 0;
      lookup->block_value = 1;
      lookup->name = name;

      lookup->next = (lang_statement_union_type *) NULL;
      lookup->bfd_section = (asection *) NULL;
      lookup->processed = false;
      lookup->sectype = normal_section;
      lookup->addr_tree = (etree_type *) NULL;
      lang_list_init (&lookup->children);

      lookup->memspec = (CONST char *) NULL;
      lookup->flags = 0;
      lookup->subsection_alignment = -1;
      lookup->section_alignment = -1;
      lookup->load_base = (union etree_union *) NULL;
      lookup->phdrs = NULL;

      lang_statement_append (&lang_output_section_statement,
			     (lang_statement_union_type *) lookup,
			     &lookup->next);
    }
  return lookup;
}

static void
lang_map_flags (flag)
     flagword flag;
{
  if (flag & SEC_ALLOC)
    minfo ("a");

  if (flag & SEC_CODE)
    minfo ("x");

  if (flag & SEC_READONLY)
    minfo ("r");

  if (flag & SEC_DATA)
    minfo ("w");

  if (flag & SEC_LOAD)
    minfo ("l");
}

void
lang_map ()
{
  lang_memory_region_type *m;

  minfo (_("\nMemory Configuration\n\n"));
  fprintf (config.map_file, "%-16s %-18s %-18s %s\n",
	   _("Name"), _("Origin"), _("Length"), _("Attributes"));

  for (m = lang_memory_region_list;
       m != (lang_memory_region_type *) NULL;
       m = m->next)
    {
      char buf[100];
      int len;

      fprintf (config.map_file, "%-16s ", m->name);

      sprintf_vma (buf, m->origin);
      minfo ("0x%s ", buf);
      len = strlen (buf);
      while (len < 16)
	{
	  print_space ();
	  ++len;
	}

      minfo ("0x%V", m->length);
      if (m->flags || m->not_flags)
	{
#ifndef BFD64
	  minfo ("        ");
#endif
	  if (m->flags)
	    {
	      print_space ();
	      lang_map_flags (m->flags);
	    }

	  if (m->not_flags)
	    {
	      minfo (" !");
	      lang_map_flags (m->not_flags);
	    }
	}

      print_nl ();
    }

  fprintf (config.map_file, _("\nLinker script and memory map\n\n"));

  print_statements ();
}

/* Initialize an output section.  */

static void
init_os (s)
     lang_output_section_statement_type *s;
{
  section_userdata_type *new;

  if (s->bfd_section != NULL)
    return;

  if (strcmp (s->name, DISCARD_SECTION_NAME) == 0)
    einfo (_("%P%F: Illegal use of `%s' section"), DISCARD_SECTION_NAME);

  new = ((section_userdata_type *)
	 stat_alloc (sizeof (section_userdata_type)));

  s->bfd_section = bfd_get_section_by_name (output_bfd, s->name);
  if (s->bfd_section == (asection *) NULL)
    s->bfd_section = bfd_make_section (output_bfd, s->name);
  if (s->bfd_section == (asection *) NULL)
    {
      einfo (_("%P%F: output format %s cannot represent section called %s\n"),
	     output_bfd->xvec->name, s->name);
    }
  s->bfd_section->output_section = s->bfd_section;

  /* We initialize an output sections output offset to minus its own */
  /* vma to allow us to output a section through itself */
  s->bfd_section->output_offset = 0;
  get_userdata (s->bfd_section) = (PTR) new;

  /* If there is a base address, make sure that any sections it might
     mention are initialized.  */
  if (s->addr_tree != NULL)
    exp_init_os (s->addr_tree);
}

/* Make sure that all output sections mentioned in an expression are
   initialized.  */

static void
exp_init_os (exp)
     etree_type *exp;
{
  switch (exp->type.node_class)
    {
    case etree_assign:
      exp_init_os (exp->assign.src);
      break;

    case etree_binary:
      exp_init_os (exp->binary.lhs);
      exp_init_os (exp->binary.rhs);
      break;

    case etree_trinary:
      exp_init_os (exp->trinary.cond);
      exp_init_os (exp->trinary.lhs);
      exp_init_os (exp->trinary.rhs);
      break;

    case etree_unary:
      exp_init_os (exp->unary.child);
      break;

    case etree_name:
      switch (exp->type.node_code)
	{
	case ADDR:
	case LOADADDR:
	case SIZEOF:
	  {
	    lang_output_section_statement_type *os;

	    os = lang_output_section_find (exp->name.name);
	    if (os != NULL && os->bfd_section == NULL)
	      init_os (os);
	  }
	}
      break;

    default:
      break;
    }
}

/* Sections marked with the SEC_LINK_ONCE flag should only be linked
   once into the output.  This routine checks each section, and
   arrange to discard it if a section of the same name has already
   been linked.  If the section has COMDAT information, then it uses
   that to decide whether the section should be included.  This code
   assumes that all relevant sections have the SEC_LINK_ONCE flag set;
   that is, it does not depend solely upon the section name.
   section_already_linked is called via bfd_map_over_sections.  */

/* This is the shape of the elements inside the already_linked hash
   table. It maps a name onto a list of already_linked elements with
   the same name.  It's possible to get more than one element in a
   list if the COMDAT sections have different names.  */

struct already_linked_hash_entry 
{
  struct bfd_hash_entry root;
  struct already_linked *entry;
};

struct already_linked 
{
  struct already_linked *next;
  asection *sec;
};

/* The hash table.  */

static struct bfd_hash_table already_linked_table;

/*ARGSUSED*/
static void
section_already_linked (abfd, sec, data)
     bfd *abfd;
     asection *sec;
     PTR data;
{
  lang_input_statement_type *entry = (lang_input_statement_type *) data;
  flagword flags;
  const char *name;
  struct already_linked *l;
  struct already_linked_hash_entry *already_linked_list;

  /* If we are only reading symbols from this object, then we want to
     discard all sections.  */
  if (entry->just_syms_flag)
    {
      sec->output_section = bfd_abs_section_ptr;
      sec->output_offset = sec->vma;
      return;
    }

  flags = bfd_get_section_flags (abfd, sec);

  if ((flags & SEC_LINK_ONCE) == 0)
    return;

  /* FIXME: When doing a relocateable link, we may have trouble
     copying relocations in other sections that refer to local symbols
     in the section being discarded.  Those relocations will have to
     be converted somehow; as of this writing I'm not sure that any of
     the backends handle that correctly.

     It is tempting to instead not discard link once sections when
     doing a relocateable link (technically, they should be discarded
     whenever we are building constructors).  However, that fails,
     because the linker winds up combining all the link once sections
     into a single large link once section, which defeats the purpose
     of having link once sections in the first place.

     Also, not merging link once sections in a relocateable link
     causes trouble for MIPS ELF, which relies in link once semantics
     to handle the .reginfo section correctly.  */

  name = bfd_get_section_name (abfd, sec);

  already_linked_list = 
    ((struct already_linked_hash_entry *)
     bfd_hash_lookup (&already_linked_table, name, true, false));

  for (l = already_linked_list->entry;  l != NULL; l = l->next)
    {
      if (sec->comdat == NULL
	  || l->sec->comdat == NULL
	  || strcmp (sec->comdat->name, l->sec->comdat->name) == 0)
	{
	  /* The section has already been linked.  See if we should
             issue a warning.  */
	  switch (flags & SEC_LINK_DUPLICATES)
	    {
	    default:
	      abort ();

	    case SEC_LINK_DUPLICATES_DISCARD:
	      break;

	    case SEC_LINK_DUPLICATES_ONE_ONLY:
	      if (sec->comdat == NULL)
		einfo (_("%P: %B: warning: ignoring duplicate section `%s'\n"),
		       abfd, name);
	      else
		einfo (_("%P: %B: warning: ignoring duplicate `%s' section symbol `%s'\n"),
		       abfd, name, sec->comdat->name);
	      break;

	    case SEC_LINK_DUPLICATES_SAME_CONTENTS:
	      /* FIXME: We should really dig out the contents of both
                 sections and memcmp them.  The COFF/PE spec says that
                 the Microsoft linker does not implement this
                 correctly, so I'm not going to bother doing it
                 either.  */
	      /* Fall through.  */
	    case SEC_LINK_DUPLICATES_SAME_SIZE:
	      if (bfd_section_size (abfd, sec)
		  != bfd_section_size (l->sec->owner, l->sec))
		einfo (_("%P: %B: warning: duplicate section `%s' has different size\n"),
		       abfd, name);
	      break;
	    }

	  /* Set the output_section field so that wild_doit does not
	     create a lang_input_section structure for this section.
	     Since there might be a symbol in the section being
	     discarded, we must retain a pointer to the section which
	     we are really going to use.  */
	  sec->output_section = bfd_abs_section_ptr;
	  if (sec->comdat != NULL)
	    sec->comdat->sec = l->sec;

	  return;
	}
    }

  /* This is the first section with this name.  Record it.  Allocate
     the memory from the same obstack as the hash table is kept in.  */

  l = ((struct already_linked *) 
       bfd_hash_allocate (&already_linked_table, sizeof *l));

  l->sec = sec;
  l->next = already_linked_list->entry;
  already_linked_list->entry = l;
}

/* Support routines for the hash table used by section_already_linked,
   initialize the table, fill in an entry and remove the table.  */

static struct bfd_hash_entry *
already_linked_newfunc (entry, table, string)
     struct bfd_hash_entry *entry ATTRIBUTE_UNUSED;
     struct bfd_hash_table *table;
     const char *string ATTRIBUTE_UNUSED;
{
  struct already_linked_hash_entry *ret = 
    bfd_hash_allocate (table, sizeof (struct already_linked_hash_entry));

  ret->entry = NULL;

  return (struct bfd_hash_entry *) ret;
}

static void
already_linked_table_init ()
{
  if (! bfd_hash_table_init_n (&already_linked_table,
			       already_linked_newfunc,
			       42))
    einfo (_("%P%F: Failed to create hash table\n"));
}

static void
already_linked_table_free ()
{
  bfd_hash_table_free (&already_linked_table);
}

/* The wild routines.

   These expand statements like *(.text) and foo.o to a list of
   explicit actions, like foo.o(.text), bar.o(.text) and
   foo.o(.text, .data).  */

/* Return true if the PATTERN argument is a wildcard pattern.
   Although backslashes are treated specially if a pattern contains
   wildcards, we do not consider the mere presence of a backslash to
   be enough to cause the the pattern to be treated as a wildcard.
   That lets us handle DOS filenames more naturally.  */

static boolean
wildcardp (pattern)
     const char *pattern;
{
  const char *s;

  for (s = pattern; *s != '\0'; ++s)
    if (*s == '?'
	|| *s == '*'
	|| *s == '[')
      return true;
  return false;
}

/* Add SECTION to the output section OUTPUT.  Do this by creating a
   lang_input_section statement which is placed at PTR.  FILE is the
   input file which holds SECTION.  */

void
wild_doit (ptr, section, output, file)
     lang_statement_list_type *ptr;
     asection *section;
     lang_output_section_statement_type *output;
     lang_input_statement_type *file;
{
  flagword flags;
  boolean discard;

  flags = bfd_get_section_flags (section->owner, section);

  discard = false;

  /* If we are doing a final link, discard sections marked with
     SEC_EXCLUDE.  */
  if (! link_info.relocateable
      && (flags & SEC_EXCLUDE) != 0)
    discard = true;

  /* Discard input sections which are assigned to a section named
     DISCARD_SECTION_NAME.  */
  if (strcmp (output->name, DISCARD_SECTION_NAME) == 0)
    discard = true;

  /* Discard debugging sections if we are stripping debugging
     information.  */
  if ((link_info.strip == strip_debugger || link_info.strip == strip_all)
      && (flags & SEC_DEBUGGING) != 0)
    discard = true;

  if (discard)
    {
      if (section->output_section == NULL)
	{
	  /* This prevents future calls from assigning this section.  */
	  section->output_section = bfd_abs_section_ptr;
	}
      return;
    }

  if (section->output_section == NULL)
    {
      boolean first;
      lang_input_section_type *new;
      flagword flags;

      if (output->bfd_section == NULL)
	{
	  init_os (output);
	  first = true;
	}
      else
	first = false;

      /* Add a section reference to the list */
      new = new_stat (lang_input_section, ptr);

      new->section = section;
      new->ifile = file;
      section->output_section = output->bfd_section;

      flags = section->flags;

      /* We don't copy the SEC_NEVER_LOAD flag from an input section
	 to an output section, because we want to be able to include a
	 SEC_NEVER_LOAD section in the middle of an otherwise loaded
	 section (I don't know why we want to do this, but we do).
	 build_link_order in ldwrite.c handles this case by turning
	 the embedded SEC_NEVER_LOAD section into a fill.  */

      flags &= ~ SEC_NEVER_LOAD;

      /* If final link, don't copy the SEC_LINK_ONCE flags, they've
	 already been processed.  One reason to do this is that on pe
	 format targets, .text$foo sections go into .text and it's odd
	 to see .text with SEC_LINK_ONCE set.  */

      if (! link_info.relocateable)
	flags &= ~ (SEC_LINK_ONCE | SEC_LINK_DUPLICATES);

      /* If this is not the first input section, and the SEC_READONLY
         flag is not currently set, then don't set it just because the
         input section has it set.  */

      if (! first && (section->output_section->flags & SEC_READONLY) == 0)
	flags &= ~ SEC_READONLY;

      section->output_section->flags |= flags;

      /* If SEC_READONLY is not set in the input section, then clear
         it from the output section.  */
      if ((section->flags & SEC_READONLY) == 0)
	section->output_section->flags &= ~SEC_READONLY;

      switch (output->sectype)
	{
	case normal_section:
	  break;
	case dsect_section:
	case copy_section:
	case info_section:
	case overlay_section:
	  output->bfd_section->flags &= ~SEC_ALLOC;
	  break;
	case noload_section:
	  output->bfd_section->flags &= ~SEC_LOAD;
	  output->bfd_section->flags |= SEC_NEVER_LOAD;
	  break;
	}

      /* Copy over SEC_SMALL_DATA.  */
      if (section->flags & SEC_SMALL_DATA)
	section->output_section->flags |= SEC_SMALL_DATA;

      if (section->alignment_power > output->bfd_section->alignment_power)
	output->bfd_section->alignment_power = section->alignment_power;

      /* If supplied an aligment, then force it.  */
      if (output->section_alignment != -1)
	output->bfd_section->alignment_power = output->section_alignment;
    }
}

/* Handle wildcard sorting.  This returns the lang_input_section which
   should follow the one we are going to create for SECTION and FILE,
   based on the sorting requirements of WILD.  It returns NULL if the
   new section should just go at the end of the current list.  */

static lang_statement_union_type *
wild_sort (wild, file, section)
     lang_wild_statement_type *wild;
     lang_input_statement_type *file;
     asection *section;
{
  const char *section_name;
  lang_statement_union_type *l;

  if (! wild->filenames_sorted && ! wild->sections_sorted)
    return NULL;

  section_name = bfd_get_section_name (file->the_bfd, section);
  for (l = wild->children.head; l != NULL; l = l->next)
    {
      lang_input_section_type *ls;

      if (l->header.type != lang_input_section_enum)
	continue;
      ls = &l->input_section;

      /* Sorting by filename takes precedence over sorting by section
         name.  */

      if (wild->filenames_sorted)
	{
	  const char *fn, *ln;
	  boolean fa, la;
	  int i;

	  /* The PE support for the .idata section as generated by
             dlltool assumes that files will be sorted by the name of
             the archive and then the name of the file within the
             archive.  */

	  if (file->the_bfd != NULL
	      && bfd_my_archive (file->the_bfd) != NULL)
	    {
	      fn = bfd_get_filename (bfd_my_archive (file->the_bfd));
	      fa = true;
	    }
	  else
	    {
	      fn = file->filename;
	      fa = false;
	    }

	  if (ls->ifile->the_bfd != NULL
	      && bfd_my_archive (ls->ifile->the_bfd) != NULL)
	    {
	      ln = bfd_get_filename (bfd_my_archive (ls->ifile->the_bfd));
	      la = true;
	    }
	  else
	    {
	      ln = ls->ifile->filename;
	      la = false;
	    }

	  i = strcmp (fn, ln);
	  if (i > 0)
	    continue;
	  else if (i < 0)
	    break;

	  if (fa || la)
	    {
	      if (fa)
		fn = file->filename;
	      if (la)
		ln = ls->ifile->filename;

	      i = strcmp (fn, ln);
	      if (i > 0)
		continue;
	      else if (i < 0)
		break;
	    }
	}

      /* Here either the files are not sorted by name, or we are
         looking at the sections for this file.  */

      if (wild->sections_sorted)
	{
	  if (strcmp (section_name,
		      bfd_get_section_name (ls->ifile->the_bfd,
					    ls->section))
	      < 0)
	    break;
	}
    }

  return l;
}

/* Expand a wild statement for a particular FILE.  SECTION may be
   NULL, in which case it is a wild card.  */

static void
output_section_callback (ptr, section, file, output)
     lang_wild_statement_type *ptr;
     asection *section;
     lang_input_statement_type *file;
     void *output;
{
  lang_statement_union_type *before;
  
  /* If the wild pattern was marked KEEP, the member sections
     should be as well.  */
  if (ptr->keep_sections)
    section->flags |= SEC_KEEP;
  
  before = wild_sort (ptr, file, section);
  
  /* Here BEFORE points to the lang_input_section which
     should follow the one we are about to add.  If BEFORE
     is NULL, then the section should just go at the end
     of the current list.  */
  
  if (before == NULL)
    wild_doit (&ptr->children, section, 
	       (lang_output_section_statement_type *) output, 
	       file);
  else
    {
      lang_statement_list_type list;
      lang_statement_union_type **pp;
      
      lang_list_init (&list);
      wild_doit (&list, section, 
		 (lang_output_section_statement_type *) output, 
		 file);
      
      /* If we are discarding the section, LIST.HEAD will
	 be NULL.  */
      if (list.head != NULL)
	{
	  ASSERT (list.head->next == NULL);
	  
	  for (pp = &ptr->children.head;
	       *pp != before;
	       pp = &(*pp)->next)
	    ASSERT (*pp != NULL);
	  
	  list.head->next = *pp;
	  *pp = list.head;
	}
    }
}

/* This is passed a file name which must have been seen already and
   added to the statement tree.  We will see if it has been opened
   already and had its symbols read.  If not then we'll read it.  */

static lang_input_statement_type *
lookup_name (name)
     const char *name;
{
  lang_input_statement_type *search;

  for (search = (lang_input_statement_type *) input_file_chain.head;
       search != (lang_input_statement_type *) NULL;
       search = (lang_input_statement_type *) search->next_real_file)
    {
      if (search->filename == (char *) NULL && name == (char *) NULL)
	return search;
      if (search->filename != (char *) NULL
	  && name != (char *) NULL
	  && strcmp (search->filename, name) == 0)
	break;
    }

  if (search == (lang_input_statement_type *) NULL)
    search = new_afile (name, lang_input_file_is_file_enum, default_target,
			false);

  /* If we have already added this file, or this file is not real
     (FIXME: can that ever actually happen?) or the name is NULL
     (FIXME: can that ever actually happen?) don't add this file.  */
  if (search->loaded
      || ! search->real
      || search->filename == (const char *) NULL)
    return search;

  load_symbols (search, (lang_statement_list_type *) NULL);

  return search;
}

/* Get the symbols for an input file.  */

static void
load_symbols (entry, place)
     lang_input_statement_type *entry;
     lang_statement_list_type *place;
{
  char **matching;

  if (entry->loaded)
    return;

  ldfile_open_file (entry);

  if (! bfd_check_format (entry->the_bfd, bfd_archive)
      && ! bfd_check_format_matches (entry->the_bfd, bfd_object, &matching))
    {
      bfd_error_type err;
      lang_statement_list_type *hold;

      err = bfd_get_error ();
      if (err == bfd_error_file_ambiguously_recognized)
	{
	  char **p;

	  einfo (_("%B: file not recognized: %E\n"), entry->the_bfd);
	  einfo (_("%B: matching formats:"), entry->the_bfd);
	  for (p = matching; *p != NULL; p++)
	    einfo (" %s", *p);
	  einfo ("%F\n");
	}
      else if (err != bfd_error_file_not_recognized
	       || place == NULL)
	einfo (_("%F%B: file not recognized: %E\n"), entry->the_bfd);

      bfd_close (entry->the_bfd);
      entry->the_bfd = NULL;

      /* See if the emulation has some special knowledge.  */

      if (ldemul_unrecognized_file (entry))
	return;

      /* Try to interpret the file as a linker script.  */

      ldfile_open_command_file (entry->filename);

      hold = stat_ptr;
      stat_ptr = place;

      ldfile_assumed_script = true;
      parser_input = input_script;
      yyparse ();
      ldfile_assumed_script = false;

      stat_ptr = hold;

      return;
    }

  if (ldemul_recognized_file (entry))
    return;

  /* We don't call ldlang_add_file for an archive.  Instead, the
     add_symbols entry point will call ldlang_add_file, via the
     add_archive_element callback, for each element of the archive
     which is used.  */
  switch (bfd_get_format (entry->the_bfd))
    {
    default:
      break;

    case bfd_object:
      ldlang_add_file (entry);
      if (trace_files || trace_file_tries)
	info_msg ("%I\n", entry);
      break;

    case bfd_archive:
      if (entry->whole_archive)
	{
	  bfd *member = bfd_openr_next_archived_file (entry->the_bfd,
						      (bfd *) NULL);
	  while (member != NULL)
	    {
	      if (! bfd_check_format (member, bfd_object))
		einfo (_("%F%B: object %B in archive is not object\n"),
		       entry->the_bfd, member);
	      if (! ((*link_info.callbacks->add_archive_element)
		     (&link_info, member, "--whole-archive")))
		abort ();
	      if (! bfd_link_add_symbols (member, &link_info))
		einfo (_("%F%B: could not read symbols: %E\n"), member);
	      member = bfd_openr_next_archived_file (entry->the_bfd,
						     member);
	    }

	  entry->loaded = true;

	  return;
	}
    }

  if (! bfd_link_add_symbols (entry->the_bfd, &link_info))
    einfo (_("%F%B: could not read symbols: %E\n"), entry->the_bfd);

  entry->loaded = true;
}

     

/* Handle a wild statement.  SECTION or FILE or both may be NULL,
   indicating that it is a wildcard.  Separate lang_input_section
   statements are created for each part of the expansion; they are
   added after the wild statement S.  OUTPUT is the output section.  */

static void
wild (s, section, file, target, output)
     lang_wild_statement_type *s;
     const char *section;
     const char *file;
     const char *target ATTRIBUTE_UNUSED;
     lang_output_section_statement_type *output;
{
  walk_wild (s, section, file, output_section_callback, (void *) output);

  if (section != (char *) NULL
      && strcmp (section, "COMMON") == 0
      && default_common_section == NULL)
    {
      /* Remember the section that common is going to in case we later
         get something which doesn't know where to put it.  */
      default_common_section = output;
    }
}

/* Return true iff target is the sought target.  */
static int
get_target (target, data)
     const bfd_target * target;
     void * data;
{
  const char * sought = (const char *) data;
  
  return strcmp (target->name, sought) == 0;
}

/* Like strcpy() but convert to lower case as well.  */
static void
stricpy (dest, src)
     char * dest;
     char * src;
{
  char c;
  
  while ((c = * src ++) != 0)
    {
      if (isupper ((unsigned char) c))
	c = tolower (c);

      * dest ++ = c;
    }

  * dest = 0;
}

/* Remove the first occurance of needle (if any) in haystack
   from haystack.  */
static void
strcut (haystack, needle)
     char * haystack;
     char * needle;
{
  haystack = strstr (haystack, needle);
  
  if (haystack)
    {
      char * src;

      for (src = haystack + strlen (needle); * src;)
	* haystack ++ = * src ++;
      
      * haystack = 0;
    }
}

/* Compare two target format name strings.
   Return a value indicating how "similar" they are.  */
static int
name_compare (first, second)
     char * first;
     char * second;
{
  char * copy1;
  char * copy2;
  int    result;
  
  copy1 = xmalloc (strlen (first) + 1);
  copy2 = xmalloc (strlen (second) + 1);

  /* Convert the names to lower case.  */
  stricpy (copy1, first);
  stricpy (copy2, second);

  /* Remove and endian strings from the name.  */
  strcut (copy1, "big");
  strcut (copy1, "little");
  strcut (copy2, "big");
  strcut (copy2, "little");

  /* Return a value based on how many characters match,
     starting from the beginning.   If both strings are
     the same then return 10 * their length.  */
  for (result = 0; copy1 [result] == copy2 [result]; result ++)
    if (copy1 [result] == 0)
      {
	result *= 10;
	break;
      }
  
  free (copy1);
  free (copy2);

  return result;
}

/* Set by closest_target_match() below.  */
static const bfd_target * winner;

/* Scan all the valid bfd targets looking for one that has the endianness
   requirement that was specified on the command line, and is the nearest
   match to the original output target.  */
static int
closest_target_match (target, data)
     const bfd_target * target;
     void * data;
{
  const bfd_target * original = (const bfd_target *) data;
  
  if (command_line.endian == ENDIAN_BIG && target->byteorder != BFD_ENDIAN_BIG)
    return 0;
  
  if (command_line.endian == ENDIAN_LITTLE && target->byteorder != BFD_ENDIAN_LITTLE)
    return 0;

  /* Must be the same flavour.  */
  if (target->flavour != original->flavour)
    return 0;

  /* If we have not found a potential winner yet, then record this one.  */
  if (winner == NULL)
    {
      winner = target;
      return 0;
    }

  /* Oh dear, we now have two potential candidates for a successful match.
     Compare their names and choose the better one. */
  if (name_compare (target->name, original->name) > name_compare (winner->name, original->name))
    winner = target;

  /* Keep on searching until wqe have checked them all.  */
  return 0;
}

/* Return the BFD target format of the first input file.  */
static char *
get_first_input_target ()
{
  char * target = NULL;

  LANG_FOR_EACH_INPUT_STATEMENT (s)
    {
      if (s->header.type == lang_input_statement_enum
	  && s->real)
	{
	  ldfile_open_file (s);
	  
	  if (s->the_bfd != NULL
	      && bfd_check_format (s->the_bfd, bfd_object))
	    {
	      target = bfd_get_target (s->the_bfd);
	  
	      if (target != NULL)
		break;
	    }
	}
    }
  
  return target;
}

/* Open the output file.  */

static bfd *
open_output (name)
     const char * name;
{
  bfd * output;

  /* Has the user told us which output format to use ?  */
  if (output_target == (char *) NULL)
    {
      /* No - has the current target been set to something other than the default ?  */
      if (current_target != default_target)
	output_target = current_target;

      /* No - can we determine the format of the first input file ? */
      else
	{
	  output_target = get_first_input_target ();

	  /* Failed - use the default output target.  */
	  if (output_target == NULL)
	    output_target = default_target;
	}
    }
  
  /* Has the user requested a particular endianness on the command line ?  */
  if (command_line.endian != ENDIAN_UNSET)
    {
      const bfd_target * target;
      enum bfd_endian desired_endian;

      /* Get the chosen target.  */
      target = bfd_search_for_target (get_target, (void *) output_target);

      if (command_line.endian == ENDIAN_BIG)
	desired_endian = BFD_ENDIAN_BIG;
      else
	desired_endian = BFD_ENDIAN_LITTLE;
      
      /* See if the target has the wrong endianness.  This should not happen
	 if the linker script has provided big and little endian alternatives,
	 but some scrips don't do this.  */
      if (target->byteorder != desired_endian)
	{
	  /* If it does, then see if the target provides
	     an alternative with the correct endianness.  */
	  if (target->alternative_target != NULL
	      && (target->alternative_target->byteorder == desired_endian))
	    output_target = target->alternative_target->name;
	  else
	    {
	      /* Try to find a target as similar as possible to the default
		 target, but which has the desired endian characteristic.  */
	      (void) bfd_search_for_target (closest_target_match, (void *) target);
	      
	      /* Oh dear - we could not find any targets that satisfy our requirements.  */
	      if (winner == NULL)
		einfo (_("%P: warning: could not find any targets that match endianness requirement\n"));
	      else
		output_target = winner->name;
	    }
	}
    }
      
  output = bfd_openw (name, output_target);

  if (output == (bfd *) NULL)
    {
      if (bfd_get_error () == bfd_error_invalid_target)
	einfo (_("%P%F: target %s not found\n"), output_target);

      einfo (_("%P%F: cannot open output file %s: %E\n"), name);
    }

  delete_output_file_on_failure = true;

  /*  output->flags |= D_PAGED;*/

  if (! bfd_set_format (output, bfd_object))
    einfo (_("%P%F:%s: can not make object file: %E\n"), name);
  if (! bfd_set_arch_mach (output,
			   ldfile_output_architecture,
			   ldfile_output_machine))
    einfo (_("%P%F:%s: can not set architecture: %E\n"), name);

  link_info.hash = bfd_link_hash_table_create (output);
  if (link_info.hash == (struct bfd_link_hash_table *) NULL)
    einfo (_("%P%F: can not create link hash table: %E\n"));

  bfd_set_gp_size (output, g_switch_value);
  return output;
}

static void
ldlang_open_output (statement)
     lang_statement_union_type * statement;
{
  switch (statement->header.type)
    {
    case lang_output_statement_enum:
      ASSERT (output_bfd == (bfd *) NULL);
      output_bfd = open_output (statement->output_statement.name);
      ldemul_set_output_arch ();
      if (config.magic_demand_paged && !link_info.relocateable)
	output_bfd->flags |= D_PAGED;
      else
	output_bfd->flags &= ~D_PAGED;
      if (config.text_read_only)
	output_bfd->flags |= WP_TEXT;
      else
	output_bfd->flags &= ~WP_TEXT;
      if (link_info.traditional_format)
	output_bfd->flags |= BFD_TRADITIONAL_FORMAT;
      else
	output_bfd->flags &= ~BFD_TRADITIONAL_FORMAT;
      break;

    case lang_target_statement_enum:
      current_target = statement->target_statement.target;
      break;
    default:
      break;
    }
}

/* Open all the input files.  */

static void
open_input_bfds (s, force)
     lang_statement_union_type *s;
     boolean force;
{
  for (; s != (lang_statement_union_type *) NULL; s = s->next)
    {
      switch (s->header.type)
	{
	case lang_constructors_statement_enum:
	  open_input_bfds (constructor_list.head, force);
	  break;
	case lang_output_section_statement_enum:
	  open_input_bfds (s->output_section_statement.children.head, force);
	  break;
	case lang_wild_statement_enum:
	  /* Maybe we should load the file's symbols */
	  if (s->wild_statement.filename
	      && ! wildcardp (s->wild_statement.filename))
	    (void) lookup_name (s->wild_statement.filename);
	  open_input_bfds (s->wild_statement.children.head, force);
	  break;
	case lang_group_statement_enum:
	  {
	    struct bfd_link_hash_entry *undefs;

	    /* We must continually search the entries in the group
               until no new symbols are added to the list of undefined
               symbols.  */

	    do
	      {
		undefs = link_info.hash->undefs_tail;
		open_input_bfds (s->group_statement.children.head, true);
	      }
	    while (undefs != link_info.hash->undefs_tail);
	  }
	  break;
	case lang_target_statement_enum:
	  current_target = s->target_statement.target;
	  break;
	case lang_input_statement_enum:
	  if (s->input_statement.real)
	    {
	      lang_statement_list_type add;

	      s->input_statement.target = current_target;

	      /* If we are being called from within a group, and this
                 is an archive which has already been searched, then
                 force it to be researched.  */
	      if (force
		  && s->input_statement.loaded
		  && bfd_check_format (s->input_statement.the_bfd,
				       bfd_archive))
		s->input_statement.loaded = false;

	      lang_list_init (&add);

	      load_symbols (&s->input_statement, &add);

	      if (add.head != NULL)
		{
		  *add.tail = s->next;
		  s->next = add.head;
		}
	    }
	  break;
	default:
	  break;
	}
    }
}

/* If there are [COMMONS] statements, put a wild one into the bss section */

static void
lang_reasonable_defaults ()
{
#if 0
  lang_output_section_statement_lookup (".text");
  lang_output_section_statement_lookup (".data");

  default_common_section =
    lang_output_section_statement_lookup (".bss");


  if (placed_commons == false)
    {
      lang_wild_statement_type *new =
      new_stat (lang_wild_statement,
		&default_common_section->children);

      new->section_name = "COMMON";
      new->filename = (char *) NULL;
      lang_list_init (&new->children);
    }
#endif

}

/*
 Add the supplied name to the symbol table as an undefined reference.
 Remove items from the chain as we open input bfds
 */
typedef struct ldlang_undef_chain_list
{
  struct ldlang_undef_chain_list *next;
  char *name;
}                       ldlang_undef_chain_list_type;

static ldlang_undef_chain_list_type *ldlang_undef_chain_list_head;

void
ldlang_add_undef (name)
     CONST char *CONST name;
{
  ldlang_undef_chain_list_type *new =
    ((ldlang_undef_chain_list_type *)
     stat_alloc (sizeof (ldlang_undef_chain_list_type)));

  new->next = ldlang_undef_chain_list_head;
  ldlang_undef_chain_list_head = new;

  new->name = buystring (name);
}

/* Run through the list of undefineds created above and place them
   into the linker hash table as undefined symbols belonging to the
   script file.
*/
static void
lang_place_undefineds ()
{
  ldlang_undef_chain_list_type *ptr;

  for (ptr = ldlang_undef_chain_list_head;
       ptr != (ldlang_undef_chain_list_type *) NULL;
       ptr = ptr->next)
    {
      struct bfd_link_hash_entry *h;

      h = bfd_link_hash_lookup (link_info.hash, ptr->name, true, false, true);
      if (h == (struct bfd_link_hash_entry *) NULL)
	einfo (_("%P%F: bfd_link_hash_lookup failed: %E\n"));
      if (h->type == bfd_link_hash_new)
	{
	  h->type = bfd_link_hash_undefined;
	  h->u.undef.abfd = NULL;
	  bfd_link_add_undef (link_info.hash, h);
	}
    }
}

/* Open input files and attatch to output sections */
static void
map_input_to_output_sections (s, target, output_section_statement)
     lang_statement_union_type * s;
     CONST char *target;
     lang_output_section_statement_type * output_section_statement;
{
  for (; s != (lang_statement_union_type *) NULL; s = s->next)
    {
      switch (s->header.type)
	{


	case lang_wild_statement_enum:
	  wild (&s->wild_statement, s->wild_statement.section_name,
		s->wild_statement.filename, target,
		output_section_statement);

	  break;
	case lang_constructors_statement_enum:
	  map_input_to_output_sections (constructor_list.head,
					target,
					output_section_statement);
	  break;
	case lang_output_section_statement_enum:
	  map_input_to_output_sections (s->output_section_statement.children.head,
					target,
					&s->output_section_statement);
	  break;
	case lang_output_statement_enum:
	  break;
	case lang_target_statement_enum:
	  target = s->target_statement.target;
	  break;
	case lang_group_statement_enum:
	  map_input_to_output_sections (s->group_statement.children.head,
					target,
					output_section_statement);
	  break;
	case lang_fill_statement_enum:
	case lang_input_section_enum:
	case lang_object_symbols_statement_enum:
	case lang_data_statement_enum:
	case lang_reloc_statement_enum:
	case lang_padding_statement_enum:
	case lang_input_statement_enum:
	  if (output_section_statement != NULL
	      && output_section_statement->bfd_section == NULL)
	    init_os (output_section_statement);
	  break;
	case lang_assignment_statement_enum:
	  if (output_section_statement != NULL
	      && output_section_statement->bfd_section == NULL)
	    init_os (output_section_statement);

	  /* Make sure that any sections mentioned in the assignment
             are initialized.  */
	  exp_init_os (s->assignment_statement.exp);
	  break;
	case lang_afile_asection_pair_statement_enum:
	  FAIL ();
	  break;
	case lang_address_statement_enum:
	  /* Mark the specified section with the supplied address */
	  {
	    lang_output_section_statement_type *os =
	      lang_output_section_statement_lookup
		(s->address_statement.section_name);

	    if (os->bfd_section == NULL)
	      init_os (os);
	    os->addr_tree = s->address_statement.address;
	  }
	  break;
	}
    }
}

static void
print_output_section_statement (output_section_statement)
     lang_output_section_statement_type * output_section_statement;
{
  asection *section = output_section_statement->bfd_section;
  int len;

  if (output_section_statement != abs_output_section)
    {
      minfo ("\n%s", output_section_statement->name);

      if (section != NULL)
	{
	  print_dot = section->vma;

	  len = strlen (output_section_statement->name);
	  if (len >= SECTION_NAME_MAP_LENGTH - 1)
	    {
	      print_nl ();
	      len = 0;
	    }
	  while (len < SECTION_NAME_MAP_LENGTH)
	    {
	      print_space ();
	      ++len;
	    }

	  minfo ("0x%V %W", section->vma, section->_raw_size);

	  if (output_section_statement->load_base != NULL)
	    {
	      bfd_vma addr;

	      addr = exp_get_abs_int (output_section_statement->load_base, 0,
				      "load base", lang_final_phase_enum);
	      minfo (_(" load address 0x%V"), addr);
	    }
	}

      print_nl ();
    }

  print_statement_list (output_section_statement->children.head,
			output_section_statement);
}

static void
print_assignment (assignment, output_section)
     lang_assignment_statement_type * assignment;
     lang_output_section_statement_type * output_section;
{
  int i;
  etree_value_type result;

  for (i = 0; i < SECTION_NAME_MAP_LENGTH; i++)
    print_space ();

  result = exp_fold_tree (assignment->exp->assign.src, output_section,
			  lang_final_phase_enum, print_dot, &print_dot);
  if (result.valid_p)
    minfo ("0x%V", result.value + result.section->bfd_section->vma);
  else
    {
      minfo ("*undef*   ");
#ifdef BFD64
      minfo ("        ");
#endif
    }

  minfo ("                ");

  exp_print_tree (assignment->exp);

  print_nl ();
}

static void
print_input_statement (statm)
     lang_input_statement_type * statm;
{
  if (statm->filename != (char *) NULL)
    {
      fprintf (config.map_file, "LOAD %s\n", statm->filename);
    }
}

/* Print all symbols defined in a particular section.  This is called
   via bfd_link_hash_traverse.  */

static boolean 
print_one_symbol (hash_entry, ptr)
     struct bfd_link_hash_entry *hash_entry;
     PTR ptr;
{
  asection *sec = (asection *) ptr;

  if ((hash_entry->type == bfd_link_hash_defined
       || hash_entry->type == bfd_link_hash_defweak)
      && sec == hash_entry->u.def.section)
    {
      int i;

      for (i = 0; i < SECTION_NAME_MAP_LENGTH; i++)
	print_space ();
      minfo ("0x%V   ",
	     (hash_entry->u.def.value
	      + hash_entry->u.def.section->output_offset
	      + hash_entry->u.def.section->output_section->vma));

      minfo ("             %T\n", hash_entry->root.string);
    }

  return true;
}

/* Print information about an input section to the map file.  */

static void
print_input_section (in)
     lang_input_section_type * in;
{
  asection *i = in->section;
  bfd_size_type size = i->_cooked_size != 0 ? i->_cooked_size : i->_raw_size;
  unsigned opb = bfd_arch_mach_octets_per_byte (ldfile_output_architecture, 
						ldfile_output_machine);
  if (size != 0)
    {
      print_space ();

      minfo ("%s", i->name);

      if (i->output_section != NULL)
	{
	  int len;

	  len = 1 + strlen (i->name);
	  if (len >= SECTION_NAME_MAP_LENGTH - 1)
	    {
	      print_nl ();
	      len = 0;
	    }
	  while (len < SECTION_NAME_MAP_LENGTH)
	    {
	      print_space ();
	      ++len;
	    }

	  minfo ("0x%V %W %B\n",
		 i->output_section->vma + i->output_offset, size / opb,
		 i->owner);

	  if (i->_cooked_size != 0 && i->_cooked_size != i->_raw_size)
	    {
	      len = SECTION_NAME_MAP_LENGTH + 3;
#ifdef BFD64
	      len += 16;
#else
	      len += 8;
#endif
	      while (len > 0)
		{
		  print_space ();
		  --len;
		}

	      minfo (_("%W (size before relaxing)\n"), i->_raw_size);
	    }

	  bfd_link_hash_traverse (link_info.hash, print_one_symbol, (PTR) i);

	  print_dot = i->output_section->vma + i->output_offset + size / opb;
	}
    }
}

static void
print_fill_statement (fill)
     lang_fill_statement_type * fill;
{
  fprintf (config.map_file, " FILL mask 0x%x\n", fill->fill);
}

static void
print_data_statement (data)
     lang_data_statement_type * data;
{
  int i;
  bfd_vma addr;
  bfd_size_type size;
  const char *name;
  unsigned opb = bfd_arch_mach_octets_per_byte (ldfile_output_architecture, 
						ldfile_output_machine);

  for (i = 0; i < SECTION_NAME_MAP_LENGTH; i++)
    print_space ();

  addr = data->output_vma;
  if (data->output_section != NULL)
    addr += data->output_section->vma;

  switch (data->type)
    {
    default:
      abort ();
    case BYTE:
      size = BYTE_SIZE;
      name = "BYTE";
      break;
    case SHORT:
      size = SHORT_SIZE;
      name = "SHORT";
      break;
    case LONG:
      size = LONG_SIZE;
      name = "LONG";
      break;
    case QUAD:
      size = QUAD_SIZE;
      name = "QUAD";
      break;
    case SQUAD:
      size = QUAD_SIZE;
      name = "SQUAD";
      break;
    }

  minfo ("0x%V %W %s 0x%v", addr, size, name, data->value);

  if (data->exp->type.node_class != etree_value)
    {
      print_space ();
      exp_print_tree (data->exp);
    }

  print_nl ();

  print_dot = addr + size / opb;

}

/* Print an address statement.  These are generated by options like
   -Ttext.  */

static void
print_address_statement (address)
     lang_address_statement_type *address;
{
  minfo (_("Address of section %s set to "), address->section_name);
  exp_print_tree (address->address);
  print_nl ();
}

/* Print a reloc statement.  */

static void
print_reloc_statement (reloc)
     lang_reloc_statement_type *reloc;
{
  int i;
  bfd_vma addr;
  bfd_size_type size;
  unsigned opb = bfd_arch_mach_octets_per_byte (ldfile_output_architecture, 
						ldfile_output_machine); 

  for (i = 0; i < SECTION_NAME_MAP_LENGTH; i++)
    print_space ();

  addr = reloc->output_vma;
  if (reloc->output_section != NULL)
    addr += reloc->output_section->vma;

  size = bfd_get_reloc_size (reloc->howto);

  minfo ("0x%V %W RELOC %s ", addr, size, reloc->howto->name);

  if (reloc->name != NULL)
    minfo ("%s+", reloc->name);
  else
    minfo ("%s+", reloc->section->name);

  exp_print_tree (reloc->addend_exp);

  print_nl ();

  print_dot = addr + size / opb;
}  

static void
print_padding_statement (s)
     lang_padding_statement_type *s;
{
  int len;
  bfd_vma addr;
  unsigned opb = bfd_arch_mach_octets_per_byte (ldfile_output_architecture, 
						ldfile_output_machine); 

  minfo (" *fill*");

  len = sizeof " *fill*" - 1;
  while (len < SECTION_NAME_MAP_LENGTH)
    {
      print_space ();
      ++len;
    }

  addr = s->output_offset;
  if (s->output_section != NULL)
    addr += s->output_section->vma;
  minfo ("0x%V %W", addr, s->size);

  if (s->fill != 0)
    minfo (" %u", s->fill);

  print_nl ();

  print_dot = addr + s->size / opb;
}

static void
print_wild_statement (w, os)
     lang_wild_statement_type * w;
     lang_output_section_statement_type * os;
{
  print_space ();

  if (w->filenames_sorted)
    minfo ("SORT(");
  if (w->exclude_filename_list != NULL)
    {
      name_list *tmp;
      minfo ("EXCLUDE_FILE ( %s", w->exclude_filename_list->name);
      for (tmp=w->exclude_filename_list->next; tmp; tmp = tmp->next)
        minfo (", %s", tmp->name);
      minfo (")");
     }
   if (w->filename != NULL)
    minfo ("%s", w->filename);
  else
    minfo ("*");
  if (w->filenames_sorted)
    minfo (")");

  minfo ("(");
  if (w->sections_sorted)
    minfo ("SORT(");
  if (w->section_name != NULL)
    minfo ("%s", w->section_name);
  else
    minfo ("*");
  if (w->sections_sorted)
    minfo (")");
  minfo (")");

  print_nl ();

  print_statement_list (w->children.head, os);
}

/* Print a group statement.  */

static void
print_group (s, os)
     lang_group_statement_type *s;
     lang_output_section_statement_type *os;
{
  fprintf (config.map_file, "START GROUP\n");
  print_statement_list (s->children.head, os);
  fprintf (config.map_file, "END GROUP\n");
}

/* Print the list of statements in S.
   This can be called for any statement type.  */

static void
print_statement_list (s, os)
     lang_statement_union_type *s;
     lang_output_section_statement_type *os;
{
  while (s != NULL)
    {
      print_statement (s, os);
      s = s->next;
    }
}

/* Print the first statement in statement list S.
   This can be called for any statement type.  */

static void
print_statement (s, os)
     lang_statement_union_type *s;
     lang_output_section_statement_type *os;
{
  switch (s->header.type)
    {
    default:
      fprintf (config.map_file, _("Fail with %d\n"), s->header.type);
      FAIL ();
      break;
    case lang_constructors_statement_enum:
      if (constructor_list.head != NULL)
	{
	  if (constructors_sorted)
	    minfo (" SORT (CONSTRUCTORS)\n");
	  else
	    minfo (" CONSTRUCTORS\n");
	  print_statement_list (constructor_list.head, os);
	}
      break;
    case lang_wild_statement_enum:
      print_wild_statement (&s->wild_statement, os);
      break;
    case lang_address_statement_enum:
      print_address_statement (&s->address_statement);
      break;
    case lang_object_symbols_statement_enum:
      minfo (" CREATE_OBJECT_SYMBOLS\n");
      break;
    case lang_fill_statement_enum:
      print_fill_statement (&s->fill_statement);
      break;
    case lang_data_statement_enum:
      print_data_statement (&s->data_statement);
      break;
    case lang_reloc_statement_enum:
      print_reloc_statement (&s->reloc_statement);
      break;
    case lang_input_section_enum:
      print_input_section (&s->input_section);
      break;
    case lang_padding_statement_enum:
      print_padding_statement (&s->padding_statement);
      break;
    case lang_output_section_statement_enum:
      print_output_section_statement (&s->output_section_statement);
      break;
    case lang_assignment_statement_enum:
      print_assignment (&s->assignment_statement, os);
      break;
    case lang_target_statement_enum:
      fprintf (config.map_file, "TARGET(%s)\n", s->target_statement.target);
      break;
    case lang_output_statement_enum:
      minfo ("OUTPUT(%s", s->output_statement.name);
      if (output_target != NULL)
	minfo (" %s", output_target);
      minfo (")\n");
      break;
    case lang_input_statement_enum:
      print_input_statement (&s->input_statement);
      break;
    case lang_group_statement_enum:
      print_group (&s->group_statement, os);
      break;
    case lang_afile_asection_pair_statement_enum:
      FAIL ();
      break;
    }
}

static void
print_statements ()
{
  print_statement_list (statement_list.head, abs_output_section);
}

/* Print the first N statements in statement list S to STDERR.
   If N == 0, nothing is printed.
   If N < 0, the entire list is printed.
   Intended to be called from GDB.  */

void
dprint_statement (s, n)
     lang_statement_union_type * s;
     int n;
{
  FILE *map_save = config.map_file;

  config.map_file = stderr;

  if (n < 0)
    print_statement_list (s, abs_output_section);
  else
    {
      while (s && --n >= 0)
	{
	  print_statement (s, abs_output_section);
	  s = s->next;
	}
    }

  config.map_file = map_save;
}

static bfd_vma
insert_pad (this_ptr, fill, power, output_section_statement, dot)
     lang_statement_union_type ** this_ptr;
     fill_type fill;
     unsigned int power;
     asection * output_section_statement;
     bfd_vma dot;
{
  /* Align this section first to the
     input sections requirement, then
     to the output section's requirement.
     If this alignment is > than any seen before,
     then record it too. Perform the alignment by
     inserting a magic 'padding' statement.
     */

  unsigned opb = bfd_arch_mach_octets_per_byte (ldfile_output_architecture, 
						ldfile_output_machine); 
  unsigned int alignment_needed = align_power (dot, power) - dot;

  if (alignment_needed != 0)
    {
      lang_statement_union_type *new =
	((lang_statement_union_type *)
	 stat_alloc (sizeof (lang_padding_statement_type)));

      /* Link into existing chain */
      new->header.next = *this_ptr;
      *this_ptr = new;
      new->header.type = lang_padding_statement_enum;
      new->padding_statement.output_section = output_section_statement;
      new->padding_statement.output_offset =
	dot - output_section_statement->vma;
      new->padding_statement.fill = fill;
      new->padding_statement.size = alignment_needed * opb;
    }


  /* Remember the most restrictive alignment */
  if (power > output_section_statement->alignment_power)
    {
      output_section_statement->alignment_power = power;
    }
  output_section_statement->_raw_size += alignment_needed * opb;

  return dot + alignment_needed;
}

/* Work out how much this section will move the dot point */
static bfd_vma
size_input_section (this_ptr, output_section_statement, fill, dot, relax)
     lang_statement_union_type ** this_ptr;
     lang_output_section_statement_type * output_section_statement;
     fill_type fill;
     bfd_vma dot;
     boolean relax ATTRIBUTE_UNUSED;
{
  lang_input_section_type *is = &((*this_ptr)->input_section);
  asection *i = is->section;
  unsigned opb = bfd_arch_mach_octets_per_byte (ldfile_output_architecture, 
						ldfile_output_machine); 

  if (is->ifile->just_syms_flag == false)
    {
      if (output_section_statement->subsection_alignment != -1)
       i->alignment_power =
	output_section_statement->subsection_alignment;

      dot = insert_pad (this_ptr, fill, i->alignment_power,
			output_section_statement->bfd_section, dot);

      /* Remember where in the output section this input section goes */

      i->output_offset = dot - output_section_statement->bfd_section->vma;

      /* Mark how big the output section must be to contain this now
	 */
      if (i->_cooked_size != 0)
	dot += i->_cooked_size / opb;
      else
	dot += i->_raw_size / opb;
      output_section_statement->bfd_section->_raw_size = 
        (dot - output_section_statement->bfd_section->vma) * opb;
    }
  else
    {
      i->output_offset = i->vma - output_section_statement->bfd_section->vma;
    }

  return dot;
}

#define IGNORE_SECTION(bfd, s) \
  (((bfd_get_section_flags (bfd, s) & (SEC_ALLOC | SEC_LOAD)) != (SEC_ALLOC | SEC_LOAD)) \
   || bfd_section_size (bfd, s) == 0)

/* Check to see if any allocated sections overlap with other allocated
   sections.  This can happen when the linker script specifically specifies
   the output section addresses of the two sections.  */
static void
lang_check_section_addresses ()
{
  asection * s;
  unsigned opb = bfd_octets_per_byte (output_bfd);

  /* Scan all sections in the output list.  */
  for (s = output_bfd->sections; s != NULL; s = s->next)
    {
      asection * os;
      
      /* Ignore sections which are not loaded or which have no contents.  */
      if (IGNORE_SECTION (output_bfd, s))
	continue;
      
      /* Once we reach section 's' stop our seach.  This prevents two
	 warning messages from being produced, one for 'section A overlaps
	 section B' and one for 'section B overlaps section A'.  */
      for (os = output_bfd->sections; os != s; os = os->next)
	{
	  bfd_vma s_start;
	  bfd_vma s_end;
	  bfd_vma os_start;
	  bfd_vma os_end;
	  
	  /* Only consider loadable sections with real contents.  */
	  if (IGNORE_SECTION (output_bfd, os))
	    continue;

	  /* We must check the sections' LMA addresses not their
	     VMA addresses because overlay sections can have
	     overlapping VMAs but they must have distinct LMAs.  */
	  s_start  = bfd_section_lma (output_bfd, s);
	  os_start = bfd_section_lma (output_bfd, os);
	  s_end    = s_start  + bfd_section_size (output_bfd, s) / opb - 1;
	  os_end   = os_start + bfd_section_size (output_bfd, os) / opb - 1;
	  
	  /* Look for an overlap.  */
	  if ((s_end < os_start) || (s_start > os_end))
	    continue;
	  
	  einfo (
_("%X%P: section %s [%V -> %V] overlaps section %s [%V -> %V]\n"),
		 s->name, s_start, s_end, os->name, os_start, os_end);
	  
	  /* Once we have found one overlap for this section,
	     stop looking for others.  */
	  break;
	}
    }
}

/* This variable indicates whether bfd_relax_section should be called
   again.  */

static boolean relax_again;

/* Make sure the new address is within the region.  We explicitly permit the
   current address to be at the exact end of the region when the address is
   non-zero, in case the region is at the end of addressable memory and the
   calculation wraps around.  */ 

static void
os_region_check (os, region, tree, base)
  lang_output_section_statement_type *os;
  struct memory_region_struct *region;
  etree_type *tree;
  bfd_vma base;
{
  if ((region->current < region->origin
       || (region->current - region->origin > region->length))
      && ((region->current != region->origin + region->length)
           || base == 0))
    {
      if (tree != (etree_type *) NULL)
        {
          einfo (_("%X%P: address 0x%v of %B section %s is not within region %s\n"),
                 region->current,
                 os->bfd_section->owner,
                 os->bfd_section->name,
                 region->name);
        }
      else
        {
          einfo (_("%X%P: region %s is full (%B section %s)\n"),
                 region->name,
                 os->bfd_section->owner,
                 os->bfd_section->name);
        }
      /* Reset the region pointer.  */
      region->current = region->origin;
    }
}

/* Set the sizes for all the output sections.  */

bfd_vma
lang_size_sections (s, output_section_statement, prev, fill, dot, relax)
     lang_statement_union_type * s;
     lang_output_section_statement_type * output_section_statement;
     lang_statement_union_type ** prev;
     fill_type fill;
     bfd_vma dot;
     boolean relax;
{
  unsigned opb = bfd_arch_mach_octets_per_byte (ldfile_output_architecture, 
						ldfile_output_machine);

  /* Size up the sections from their constituent parts.  */
  for (; s != (lang_statement_union_type *) NULL; s = s->next)
    {
      switch (s->header.type)
	{
	case lang_output_section_statement_enum:
	  {
	    bfd_vma after;
	    lang_output_section_statement_type *os = &s->output_section_statement;

	    if (os->bfd_section == NULL)
	      /* This section was never actually created.  */
	      break;

	    /* If this is a COFF shared library section, use the size and
	       address from the input section.  FIXME: This is COFF
	       specific; it would be cleaner if there were some other way
	       to do this, but nothing simple comes to mind.  */
	    if ((os->bfd_section->flags & SEC_COFF_SHARED_LIBRARY) != 0)
	      {
		asection * input;

		if (os->children.head == NULL
		    || os->children.head->next != NULL
		    || os->children.head->header.type != lang_input_section_enum)
		  einfo (_("%P%X: Internal error on COFF shared library section %s\n"),
			 os->name);

		input = os->children.head->input_section.section;
		bfd_set_section_vma (os->bfd_section->owner,
				     os->bfd_section,
				     bfd_section_vma (input->owner, input));
		os->bfd_section->_raw_size = input->_raw_size;
		break;
	      }

	    if (bfd_is_abs_section (os->bfd_section))
	      {
		/* No matter what happens, an abs section starts at zero.  */
		ASSERT (os->bfd_section->vma == 0);
	      }
	    else
	      {
		if (os->addr_tree == (etree_type *) NULL)
		  {
		    /* No address specified for this section, get one
		       from the region specification.  */
		    if (os->region == (lang_memory_region_type *) NULL
			|| (((bfd_get_section_flags (output_bfd, os->bfd_section)
			      & (SEC_ALLOC | SEC_LOAD)) != 0)
			    && os->region->name[0] == '*'
			    && strcmp (os->region->name, "*default*") == 0))
		      {
			os->region = lang_memory_default (os->bfd_section);
		      }

		    /* If a loadable section is using the default memory
		       region, and some non default memory regions were
		       defined, issue a warning.  */
		    if ((bfd_get_section_flags (output_bfd, os->bfd_section)
			 & (SEC_ALLOC | SEC_LOAD)) != 0
			&& ! link_info.relocateable
			&& strcmp (os->region->name, "*default*") == 0
			&& lang_memory_region_list != NULL
			&& (strcmp (lang_memory_region_list->name, "*default*") != 0
			    || lang_memory_region_list->next != NULL))
		      einfo (_("%P: warning: no memory region specified for section `%s'\n"),
			     bfd_get_section_name (output_bfd, os->bfd_section));

		    dot = os->region->current;
 
		    if (os->section_alignment == -1)
		      {
			bfd_vma olddot;

			olddot = dot;
			dot = align_power (dot, os->bfd_section->alignment_power);

			if (dot != olddot && config.warn_section_align)
			  einfo (_("%P: warning: changing start of section %s by %u bytes\n"),
				 os->name, (unsigned int) (dot - olddot));
		      }
		  }
		else
		  {
		    etree_value_type r;

		    r = exp_fold_tree (os->addr_tree,
				       abs_output_section,
				       lang_allocating_phase_enum,
				       dot, &dot);
		    if (r.valid_p == false)
		      {
			einfo (_("%F%S: non constant address expression for section %s\n"),
			       os->name);
		      }
		    dot = r.value + r.section->bfd_section->vma;
		  }
		
		/* The section starts here.
		   First, align to what the section needs.  */

		if (os->section_alignment != -1)
		  dot = align_power (dot, os->section_alignment);

		bfd_set_section_vma (0, os->bfd_section, dot);
		
		os->bfd_section->output_offset = 0;
	      }

	    (void) lang_size_sections (os->children.head, os, &os->children.head,
				       os->fill, dot, relax);
	    
            /* put the section within the requested block size, or align at
               the block boundary */
	    after = ALIGN_N (os->bfd_section->vma
			     + os->bfd_section->_raw_size / opb,
			     /* The coercion here is important, see ld.h.  */
			     (bfd_vma) os->block_value);

	    if (bfd_is_abs_section (os->bfd_section))
	      ASSERT (after == os->bfd_section->vma);
	    else
	      os->bfd_section->_raw_size = 
                (after - os->bfd_section->vma) * opb;
	    dot = os->bfd_section->vma + os->bfd_section->_raw_size / opb;
	    os->processed = true;

	    /* Update dot in the region ?
	       We only do this if the section is going to be allocated,
	       since unallocated sections do not contribute to the region's
	       overall size in memory.
	       
	       If the SEC_NEVER_LOAD bit is not set, it will affect the
	       addresses of sections after it. We have to update
	       dot.  */
	    if (os->region != (lang_memory_region_type *) NULL
		&& ((bfd_get_section_flags (output_bfd, os->bfd_section)
		     & SEC_NEVER_LOAD) == 0
		    || (bfd_get_section_flags (output_bfd, os->bfd_section)
			& (SEC_ALLOC | SEC_LOAD))))
	      {
		os->region->current = dot;
		
		/* Make sure the new address is within the region.  */
                os_region_check (os, os->region, os->addr_tree, 
                                 os->bfd_section->vma);

                /* if there's no load address specified, use the run region as
                   the load region */
                if (os->lma_region == NULL && os->load_base == NULL)
                    os->lma_region = os->region;

                if (os->lma_region != NULL)
                  {
                    if (os->load_base != NULL)
                      {
                        einfo (_("%X%P: use an absolute load address or a load memory region, not both\n"));
                      }
                    else
                      {
                        /* don't allocate twice */
                        if (os->lma_region != os->region)
                          {
                            /* set load_base, which will be handled later */
                            os->load_base = exp_intop (os->lma_region->current);
                            os->lma_region->current += 
                              os->bfd_section->_raw_size / opb;
                            os_region_check (os, os->lma_region, NULL,
                                             os->bfd_section->lma);
                          }
                      }
                  }
	      }
	  }
	  break;

	case lang_constructors_statement_enum:
	  dot = lang_size_sections (constructor_list.head,
				    output_section_statement,
				    &s->wild_statement.children.head,
				    fill,
				    dot, relax);
	  break;

	case lang_data_statement_enum:
	  {
	    unsigned int size = 0;

	    s->data_statement.output_vma = dot - output_section_statement->bfd_section->vma;
	    s->data_statement.output_section =
	      output_section_statement->bfd_section;

	    switch (s->data_statement.type)
	      {
              default:
                abort();
	      case QUAD:
	      case SQUAD:
		size = QUAD_SIZE;
		break;
	      case LONG:
		size = LONG_SIZE;
		break;
	      case SHORT:
		size = SHORT_SIZE;
		break;
	      case BYTE:
		size = BYTE_SIZE;
		break;
	      }
            if (size < opb)
              size = opb;
	    dot += size / opb;
	    output_section_statement->bfd_section->_raw_size += size;
	    /* The output section gets contents, and then we inspect for
	       any flags set in the input script which override any ALLOC.  */
	    output_section_statement->bfd_section->flags |= SEC_HAS_CONTENTS;
	    if (!(output_section_statement->flags & SEC_NEVER_LOAD)) {
	      output_section_statement->bfd_section->flags |= SEC_ALLOC | SEC_LOAD;
	    }
	  }
	  break;

	case lang_reloc_statement_enum:
	  {
	    int size;

	    s->reloc_statement.output_vma =
	      dot - output_section_statement->bfd_section->vma;
	    s->reloc_statement.output_section =
	      output_section_statement->bfd_section;
	    size = bfd_get_reloc_size (s->reloc_statement.howto);
	    dot += size / opb;
	    output_section_statement->bfd_section->_raw_size += size;
	  }
	  break;
     
	case lang_wild_statement_enum:

	  dot = lang_size_sections (s->wild_statement.children.head,
				    output_section_statement,
				    &s->wild_statement.children.head,

				    fill, dot, relax);

	  break;

	case lang_object_symbols_statement_enum:
	  link_info.create_object_symbols_section =
	    output_section_statement->bfd_section;
	  break;
	case lang_output_statement_enum:
	case lang_target_statement_enum:
	  break;
	case lang_input_section_enum:
	  {
	    asection *i;

	    i = (*prev)->input_section.section;
	    if (! relax)
	      {
		if (i->_cooked_size == 0)
		  i->_cooked_size = i->_raw_size;
	      }
	    else
	      {
		boolean again;

		if (! bfd_relax_section (i->owner, i, &link_info, &again))
		  einfo (_("%P%F: can't relax section: %E\n"));
		if (again)
		  relax_again = true;
	      }
	    dot = size_input_section (prev,
				      output_section_statement,
				      output_section_statement->fill,
				      dot, relax);
	  }
	  break;
	case lang_input_statement_enum:
	  break;
	case lang_fill_statement_enum:
	  s->fill_statement.output_section = output_section_statement->bfd_section;

	  fill = s->fill_statement.fill;
	  break;
	case lang_assignment_statement_enum:
	  {
	    bfd_vma newdot = dot;

	    exp_fold_tree (s->assignment_statement.exp,
			   output_section_statement,
			   lang_allocating_phase_enum,
			   dot,
			   &newdot);

	    if (newdot != dot)
	      {
		/* The assignment changed dot.  Insert a pad.  */
		if (output_section_statement == abs_output_section)
		  {
		    /* If we don't have an output section, then just adjust
		       the default memory address.  */
		    lang_memory_region_lookup ("*default*")->current = newdot;
		  }
		else if (!relax)
		  {
		    lang_statement_union_type *new =
		      ((lang_statement_union_type *)
		       stat_alloc (sizeof (lang_padding_statement_type)));

		    /* Link into existing chain.  */
		    new->header.next = *prev;
		    *prev = new;
		    new->header.type = lang_padding_statement_enum;
		    new->padding_statement.output_section =
		      output_section_statement->bfd_section;
		    new->padding_statement.output_offset =
		      dot - output_section_statement->bfd_section->vma;
		    new->padding_statement.fill = fill;
		    new->padding_statement.size = (newdot - dot) * opb;
		    output_section_statement->bfd_section->_raw_size +=
		      new->padding_statement.size;
		  }

		dot = newdot;
	      }
	  }
	  break;

	case lang_padding_statement_enum:
	  /* If we are relaxing, and this is not the first pass, some
	     padding statements may have been inserted during previous
	     passes.  We may have to move the padding statement to a new
	     location if dot has a different value at this point in this
	     pass than it did at this point in the previous pass.  */
	  s->padding_statement.output_offset =
	    dot - output_section_statement->bfd_section->vma;
	  dot += s->padding_statement.size / opb;
	  output_section_statement->bfd_section->_raw_size +=
	    s->padding_statement.size;
	  break;

	case lang_group_statement_enum:
	  dot = lang_size_sections (s->group_statement.children.head,
				    output_section_statement,
				    &s->group_statement.children.head,
				    fill, dot, relax);
	  break;

	default:
	  FAIL ();
	  break;

	  /* This can only get here when relaxing is turned on.  */

	case lang_address_statement_enum:
	  break;
	}
      prev = &s->header.next;
    }
  return dot;
}

bfd_vma
lang_do_assignments (s, output_section_statement, fill, dot)
     lang_statement_union_type * s;
     lang_output_section_statement_type * output_section_statement;
     fill_type fill;
     bfd_vma dot;
{
  unsigned opb = bfd_arch_mach_octets_per_byte (ldfile_output_architecture, 
						ldfile_output_machine); 

  for (; s != (lang_statement_union_type *) NULL; s = s->next)
    {
      switch (s->header.type)
	{
	case lang_constructors_statement_enum:
	  dot = lang_do_assignments (constructor_list.head,
				     output_section_statement,
				     fill,
				     dot);
	  break;

	case lang_output_section_statement_enum:
	  {
	    lang_output_section_statement_type *os =
	      &(s->output_section_statement);

	    if (os->bfd_section != NULL)
	      {
		dot = os->bfd_section->vma;
		(void) lang_do_assignments (os->children.head, os,
					    os->fill, dot);
		dot = os->bfd_section->vma + os->bfd_section->_raw_size / opb;

	      }
	    if (os->load_base) 
	      {
		/* If nothing has been placed into the output section then
		   it won't have a bfd_section. */
		if (os->bfd_section) 
		  {
		    os->bfd_section->lma 
		      = exp_get_abs_int(os->load_base, 0,"load base", lang_final_phase_enum);
		  }
	      }
	  }
	  break;
	case lang_wild_statement_enum:

	  dot = lang_do_assignments (s->wild_statement.children.head,
				     output_section_statement,
				     fill, dot);

	  break;

	case lang_object_symbols_statement_enum:
	case lang_output_statement_enum:
	case lang_target_statement_enum:
#if 0
	case lang_common_statement_enum:
#endif
	  break;
	case lang_data_statement_enum:
	  {
	    etree_value_type value;

	    value = exp_fold_tree (s->data_statement.exp,
				   abs_output_section,
				   lang_final_phase_enum, dot, &dot);
	    s->data_statement.value = value.value;
	    if (value.valid_p == false)
	      einfo (_("%F%P: invalid data statement\n"));
	  }
          {
            unsigned int size;
            switch (s->data_statement.type)
              {
              default:
                abort();
              case QUAD:
              case SQUAD:
                size = QUAD_SIZE;
                break;
              case LONG:
                size = LONG_SIZE;
                break;
              case SHORT:
                size = SHORT_SIZE;
                break;
              case BYTE:
                size = BYTE_SIZE;
                break;
              }
            if (size < opb)
              size = opb;
            dot += size / opb;
          }
	  break;

	case lang_reloc_statement_enum:
	  {
	    etree_value_type value;

	    value = exp_fold_tree (s->reloc_statement.addend_exp,
				   abs_output_section,
				   lang_final_phase_enum, dot, &dot);
	    s->reloc_statement.addend_value = value.value;
	    if (value.valid_p == false)
	      einfo (_("%F%P: invalid reloc statement\n"));
	  }
	  dot += bfd_get_reloc_size (s->reloc_statement.howto) / opb;
	  break;

	case lang_input_section_enum:
	  {
	    asection *in = s->input_section.section;

	    if (in->_cooked_size != 0)
	      dot += in->_cooked_size / opb;
	    else
	      dot += in->_raw_size / opb;
	  }
	  break;

	case lang_input_statement_enum:
	  break;
	case lang_fill_statement_enum:
	  fill = s->fill_statement.fill;
	  break;
	case lang_assignment_statement_enum:
	  {
	    exp_fold_tree (s->assignment_statement.exp,
			   output_section_statement,
			   lang_final_phase_enum,
			   dot,
			   &dot);
	  }

	  break;
	case lang_padding_statement_enum:
	  dot += s->padding_statement.size / opb;
	  break;

	case lang_group_statement_enum:
	  dot = lang_do_assignments (s->group_statement.children.head,
				     output_section_statement,
				     fill, dot);

	  break;

	default:
	  FAIL ();
	  break;
	case lang_address_statement_enum:
	  break;
	}

    }
  return dot;
}

/* Fix any .startof. or .sizeof. symbols.  When the assemblers see the
   operator .startof. (section_name), it produces an undefined symbol
   .startof.section_name.  Similarly, when it sees
   .sizeof. (section_name), it produces an undefined symbol
   .sizeof.section_name.  For all the output sections, we look for
   such symbols, and set them to the correct value.  */

static void
lang_set_startof ()
{
  asection *s;

  if (link_info.relocateable)
    return;

  for (s = output_bfd->sections; s != NULL; s = s->next)
    {
      const char *secname;
      char *buf;
      struct bfd_link_hash_entry *h;

      secname = bfd_get_section_name (output_bfd, s);
      buf = xmalloc (10 + strlen (secname));

      sprintf (buf, ".startof.%s", secname);
      h = bfd_link_hash_lookup (link_info.hash, buf, false, false, true);
      if (h != NULL && h->type == bfd_link_hash_undefined)
	{
	  h->type = bfd_link_hash_defined;
	  h->u.def.value = bfd_get_section_vma (output_bfd, s);
	  h->u.def.section = bfd_abs_section_ptr;
	}

      sprintf (buf, ".sizeof.%s", secname);
      h = bfd_link_hash_lookup (link_info.hash, buf, false, false, true);
      if (h != NULL && h->type == bfd_link_hash_undefined)
	{
          unsigned opb = bfd_arch_mach_octets_per_byte (ldfile_output_architecture, 
							ldfile_output_machine);
	  h->type = bfd_link_hash_defined;
	  if (s->_cooked_size != 0)
	    h->u.def.value = s->_cooked_size / opb;
	  else
	    h->u.def.value = s->_raw_size / opb;
	  h->u.def.section = bfd_abs_section_ptr;
	}

      free (buf);
    }
}

static void
lang_finish ()
{
  struct bfd_link_hash_entry *h;
  boolean warn;

  if (link_info.relocateable || link_info.shared)
    warn = false;
  else
    warn = true;

  if (entry_symbol == (char *) NULL)
    {
      /* No entry has been specified.  Look for start, but don't warn
	 if we don't find it.  */
      entry_symbol = "start";
      warn = false;
    }

  h = bfd_link_hash_lookup (link_info.hash, entry_symbol, false, false, true);
  if (h != (struct bfd_link_hash_entry *) NULL
      && (h->type == bfd_link_hash_defined
	  || h->type == bfd_link_hash_defweak)
      && h->u.def.section->output_section != NULL)
    {
      bfd_vma val;

      val = (h->u.def.value
	     + bfd_get_section_vma (output_bfd,
				    h->u.def.section->output_section)
	     + h->u.def.section->output_offset);
      if (! bfd_set_start_address (output_bfd, val))
	einfo (_("%P%F:%s: can't set start address\n"), entry_symbol);
    }
  else
    {
      bfd_vma val;
      CONST char *send;

      /* We couldn't find the entry symbol.  Try parsing it as a
         number.  */
      val = bfd_scan_vma (entry_symbol, &send, 0);
      if (*send == '\0')
	{
	  if (! bfd_set_start_address (output_bfd, val))
	    einfo (_("%P%F: can't set start address\n"));
	}
      else
	{
	  asection *ts;

	  /* Can't find the entry symbol, and it's not a number.  Use
	     the first address in the text section.  */
	  ts = bfd_get_section_by_name (output_bfd, ".text");
	  if (ts != (asection *) NULL)
	    {
	      if (warn)
		einfo (_("%P: warning: cannot find entry symbol %s; defaulting to %V\n"),
		       entry_symbol, bfd_get_section_vma (output_bfd, ts));
	      if (! bfd_set_start_address (output_bfd,
					   bfd_get_section_vma (output_bfd,
								ts)))
		einfo (_("%P%F: can't set start address\n"));
	    }
	  else
	    {
	      if (warn)
		einfo (_("%P: warning: cannot find entry symbol %s; not setting start address\n"),
		       entry_symbol);
	    }
	}
    }
}

/* This is a small function used when we want to ignore errors from
   BFD.  */

static void
#ifdef ANSI_PROTOTYPES
ignore_bfd_errors (const char *s ATTRIBUTE_UNUSED, ...)
#else
ignore_bfd_errors (s)
     const char *s ATTRIBUTE_UNUSED;
#endif
{
  /* Don't do anything.  */
}

/* Check that the architecture of all the input files is compatible
   with the output file.  Also call the backend to let it do any
   other checking that is needed.  */

static void
lang_check ()
{
  lang_statement_union_type *file;
  bfd *input_bfd;
  CONST bfd_arch_info_type *compatible;

  for (file = file_chain.head;
       file != (lang_statement_union_type *) NULL;
       file = file->input_statement.next)
    {
      input_bfd = file->input_statement.the_bfd;
      compatible = bfd_arch_get_compatible (input_bfd,
					    output_bfd);
      if (compatible == NULL)
	{
	  if (command_line.warn_mismatch)
	    einfo (_("%P: warning: %s architecture of input file `%B' is incompatible with %s output\n"),
		   bfd_printable_name (input_bfd), input_bfd,
		   bfd_printable_name (output_bfd));
	}
      else
	{
	  bfd_error_handler_type pfn = NULL;

	  /* If we aren't supposed to warn about mismatched input
             files, temporarily set the BFD error handler to a
             function which will do nothing.  We still want to call
             bfd_merge_private_bfd_data, since it may set up
             information which is needed in the output file.  */
	  if (! command_line.warn_mismatch)
	    pfn = bfd_set_error_handler (ignore_bfd_errors);
	  if (! bfd_merge_private_bfd_data (input_bfd, output_bfd))
	    {
	      if (command_line.warn_mismatch)
		einfo (_("%E%X: failed to merge target specific data of file %B\n"),
		       input_bfd);
	    }
	  if (! command_line.warn_mismatch)
	    bfd_set_error_handler (pfn);
	}
    }
}

/* Look through all the global common symbols and attach them to the
   correct section.  The -sort-common command line switch may be used
   to roughly sort the entries by size.  */

static void
lang_common ()
{
  if (link_info.relocateable
      && ! command_line.force_common_definition)
    return;

  if (! config.sort_common)
    bfd_link_hash_traverse (link_info.hash, lang_one_common, (PTR) NULL);
  else
    {
      int power;

      for (power = 4; power >= 0; power--)
	bfd_link_hash_traverse (link_info.hash, lang_one_common,
				(PTR) &power);
    }
}

/* Place one common symbol in the correct section.  */

static boolean
lang_one_common (h, info)
     struct bfd_link_hash_entry *h;
     PTR info;
{
  unsigned int power_of_two;
  bfd_vma size;
  asection *section;
  unsigned opb = bfd_arch_mach_octets_per_byte (ldfile_output_architecture, 
						ldfile_output_machine); 

  if (h->type != bfd_link_hash_common)
    return true;

  size = h->u.c.size;
  power_of_two = h->u.c.p->alignment_power;

  if (config.sort_common
      && power_of_two < (unsigned int) *(int *) info)
    return true;

  section = h->u.c.p->section;

  /* Increase the size of the section.  */
  section->_cooked_size = ALIGN_N ((section->_cooked_size + opb - 1) / opb,
				   (bfd_size_type) (1 << power_of_two)) * opb;

  /* Adjust the alignment if necessary.  */
  if (power_of_two > section->alignment_power)
    section->alignment_power = power_of_two;

  /* Change the symbol from common to defined.  */
  h->type = bfd_link_hash_defined;
  h->u.def.section = section;
  h->u.def.value = section->_cooked_size;

  /* Increase the size of the section.  */
  section->_cooked_size += size;

  /* Make sure the section is allocated in memory, and make sure that
     it is no longer a common section.  */
  section->flags |= SEC_ALLOC;
  section->flags &= ~ SEC_IS_COMMON;

  if (config.map_file != NULL)
    {
      static boolean header_printed;
      int len;
      char *name;
      char buf[50];

      if (! header_printed)
	{
	  minfo (_("\nAllocating common symbols\n"));
	  minfo (_("Common symbol       size              file\n\n"));
	  header_printed = true;
	}

      name = demangle (h->root.string);
      minfo ("%s", name);
      len = strlen (name);
      free (name);

      if (len >= 19)
	{
	  print_nl ();
	  len = 0;
	}
      while (len < 20)
	{
	  print_space ();
	  ++len;
	}

      minfo ("0x");
      if (size <= 0xffffffff)
	sprintf (buf, "%lx", (unsigned long) size);
      else
	sprintf_vma (buf, size);
      minfo ("%s", buf);
      len = strlen (buf);

      while (len < 16)
	{
	  print_space ();
	  ++len;
	}

      minfo ("%B\n", section->owner);
    }

  return true;
}

/*
run through the input files and ensure that every input
section has somewhere to go. If one is found without
a destination then create an input request and place it
into the statement tree.
*/

static void
lang_place_orphans ()
{
  LANG_FOR_EACH_INPUT_STATEMENT (file)
    {
      asection *s;

      for (s = file->the_bfd->sections;
	   s != (asection *) NULL;
	   s = s->next)
	{
	  if (s->output_section == (asection *) NULL)
	    {
	      /* This section of the file is not attatched, root
	         around for a sensible place for it to go */

	      if (file->just_syms_flag)
		{
		  /* We are only retrieving symbol values from this
                     file.  We want the symbols to act as though the
                     values in the file are absolute.  */
		  s->output_section = bfd_abs_section_ptr;
		  s->output_offset = s->vma;
		}
	      else if (strcmp (s->name, "COMMON") == 0)
		{
		  /* This is a lonely common section which must have
		     come from an archive.  We attach to the section
		     with the wildcard.  */
		  if (! link_info.relocateable
		      || command_line.force_common_definition)
		    {
		      if (default_common_section == NULL)
			{
#if 0
			  /* This message happens when using the
                             svr3.ifile linker script, so I have
                             disabled it.  */
			  info_msg (_("%P: no [COMMON] command, defaulting to .bss\n"));
#endif
			  default_common_section =
			    lang_output_section_statement_lookup (".bss");

			}
		      wild_doit (&default_common_section->children, s,
				 default_common_section, file);
		    }
		}
	      else if (ldemul_place_orphan (file, s))
		;
	      else
		{
		  lang_output_section_statement_type *os =
		  lang_output_section_statement_lookup (s->name);

		  wild_doit (&os->children, s, os, file);
		}
	    }
	}
    }
}


void
lang_set_flags (ptr, flags, invert)
     lang_memory_region_type *ptr;
     CONST char *flags;
     int invert;
{
  flagword *ptr_flags;

  ptr_flags = invert ? &ptr->not_flags : &ptr->flags;
  while (*flags)
    {
      switch (*flags)
	{
	case 'A': case 'a':
	  *ptr_flags |= SEC_ALLOC;
	  break;

	case 'R': case 'r':
	  *ptr_flags |= SEC_READONLY;
	  break;

	case 'W': case 'w':
	  *ptr_flags |= SEC_DATA;
	  break;

	case 'X': case 'x':
	  *ptr_flags |= SEC_CODE;
	  break;

	case 'L': case 'l':
	case 'I': case 'i':
	  *ptr_flags |= SEC_LOAD;
	  break;

	default:
	  einfo (_("%P%F: invalid syntax in flags\n"));
	  break;
	}
      flags++;
    }
}

/* Call a function on each input file.  This function will be called
   on an archive, but not on the elements.  */

void
lang_for_each_input_file (func)
     void (*func) PARAMS ((lang_input_statement_type *));
{
  lang_input_statement_type *f;

  for (f = (lang_input_statement_type *) input_file_chain.head;
       f != NULL;
       f = (lang_input_statement_type *) f->next_real_file)
    func (f);
}

/* Call a function on each file.  The function will be called on all
   the elements of an archive which are included in the link, but will
   not be called on the archive file itself.  */

void
lang_for_each_file (func)
     void (*func) PARAMS ((lang_input_statement_type *));
{
  LANG_FOR_EACH_INPUT_STATEMENT (f)
    {
      func (f);
    }
}

#if 0

/* Not used.  */

void
lang_for_each_input_section (func)
     void (*func) PARAMS ((bfd * ab, asection * as));
{
  LANG_FOR_EACH_INPUT_STATEMENT (f)
    {
      asection * s;

      for (s = f->the_bfd->sections;
	   s != (asection *) NULL;
	   s = s->next)
	{
	  func (f->the_bfd, s);
	}
    }
}

#endif

void
ldlang_add_file (entry)
     lang_input_statement_type * entry;
{
  bfd **pp;

  lang_statement_append (&file_chain,
			 (lang_statement_union_type *) entry,
			 &entry->next);

  /* The BFD linker needs to have a list of all input BFDs involved in
     a link.  */
  ASSERT (entry->the_bfd->link_next == (bfd *) NULL);
  ASSERT (entry->the_bfd != output_bfd);
  for (pp = &link_info.input_bfds;
       *pp != (bfd *) NULL;
       pp = &(*pp)->link_next)
    ;
  *pp = entry->the_bfd;
  entry->the_bfd->usrdata = (PTR) entry;
  bfd_set_gp_size (entry->the_bfd, g_switch_value);

  /* Look through the sections and check for any which should not be
     included in the link.  We need to do this now, so that we can
     notice when the backend linker tries to report multiple
     definition errors for symbols which are in sections we aren't
     going to link.  FIXME: It might be better to entirely ignore
     symbols which are defined in sections which are going to be
     discarded.  This would require modifying the backend linker for
     each backend which might set the SEC_LINK_ONCE flag.  If we do
     this, we should probably handle SEC_EXCLUDE in the same way.  */

  bfd_map_over_sections (entry->the_bfd, section_already_linked, (PTR) entry);
}

void
lang_add_output (name, from_script)
     CONST char *name;
     int from_script;
{
  /* Make -o on command line override OUTPUT in script.  */
  if (had_output_filename == false || !from_script)
    {
      output_filename = name;
      had_output_filename = true;
    }
}


static lang_output_section_statement_type *current_section;

static int
topower (x)
     int x;
{
  unsigned int i = 1;
  int l;

  if (x < 0)
    return -1;

  for (l = 0; l < 32; l++) 
    {
      if (i >= (unsigned int) x)
	return l;
      i <<= 1;
    }

  return 0;
}

lang_output_section_statement_type *
lang_enter_output_section_statement (output_section_statement_name,
				     address_exp, sectype, block_value,
				     align, subalign, ebase)
     const char *output_section_statement_name;
     etree_type * address_exp;
     enum section_type sectype;
     bfd_vma block_value;
     etree_type *align;
     etree_type *subalign;
     etree_type *ebase;
{
  lang_output_section_statement_type *os;

  current_section =
   os =
    lang_output_section_statement_lookup (output_section_statement_name);



  /* Add this statement to tree */
  /*  add_statement(lang_output_section_statement_enum,
      output_section_statement);*/
  /* Make next things chain into subchain of this */

  if (os->addr_tree ==
      (etree_type *) NULL)
  {
    os->addr_tree =
     address_exp;
  }
  os->sectype = sectype;
  if (sectype != noload_section)
    os->flags = SEC_NO_FLAGS;
  else
    os->flags = SEC_NEVER_LOAD;
  os->block_value = block_value ? block_value : 1;
  stat_ptr = &os->children;

  os->subsection_alignment = topower(
   exp_get_value_int(subalign, -1,
		     "subsection alignment",
		     0));
  os->section_alignment = topower(
   exp_get_value_int(align, -1,
		     "section alignment", 0));

  os->load_base = ebase;
  return os;
}


void
lang_final ()
{
  lang_output_statement_type *new =
    new_stat (lang_output_statement, stat_ptr);

  new->name = output_filename;
}

/* Reset the current counters in the regions */
static void
reset_memory_regions ()
{
  lang_memory_region_type *p = lang_memory_region_list;

  for (p = lang_memory_region_list;
       p != (lang_memory_region_type *) NULL;
       p = p->next)
    {
      p->old_length = (bfd_size_type) (p->current - p->origin);
      p->current = p->origin;
    }
}

/* Expand a wild statement for a particular FILE, marking its sections KEEP
   as needed.  SECTION may be NULL, in which case it is a wild card.  */

static void
gc_section_callback (ptr, section, file, data)
     lang_wild_statement_type *ptr;
     asection *section;
     lang_input_statement_type *file ATTRIBUTE_UNUSED;
     void *data ATTRIBUTE_UNUSED;
{
  /* If the wild pattern was marked KEEP, the member sections
     should be as well.  */
  if (ptr->keep_sections)
    section->flags |= SEC_KEEP;
}

/* Handle a wild statement, marking it against GC.  SECTION or FILE or both
   may be NULL, indicating that it is a wildcard.  */

static void
lang_gc_wild (s, section, file)
     lang_wild_statement_type *s;
     const char *section;
     const char *file;
{
  walk_wild (s, section, file, gc_section_callback, NULL);
}

/* Iterate over sections marking them against GC.  */

static void
lang_gc_sections_1 (s)
     lang_statement_union_type * s;
{
  for (; s != (lang_statement_union_type *) NULL; s = s->next)
    {
      switch (s->header.type)
	{
	case lang_wild_statement_enum:
	  lang_gc_wild (&s->wild_statement,
			s->wild_statement.section_name,
			s->wild_statement.filename);
	  break;
	case lang_constructors_statement_enum:
	  lang_gc_sections_1 (constructor_list.head);
	  break;
	case lang_output_section_statement_enum:
	  lang_gc_sections_1 (s->output_section_statement.children.head);
	  break;
	case lang_group_statement_enum:
	  lang_gc_sections_1 (s->group_statement.children.head);
	  break;
	default:
	  break;
	}
    }
}

static void
lang_gc_sections ()
{
  struct bfd_link_hash_entry *h;
  ldlang_undef_chain_list_type *ulist, fake_list_start;

  /* Keep all sections so marked in the link script.  */

  lang_gc_sections_1 (statement_list.head);

  /* Keep all sections containing symbols undefined on the command-line.
     Handle the entry symbol at the same time.  */
  
  if (entry_symbol != NULL)
    {
      fake_list_start.next = ldlang_undef_chain_list_head;
      fake_list_start.name = (char *) entry_symbol;
      ulist = &fake_list_start;
    }
  else
    ulist = ldlang_undef_chain_list_head;

  for (; ulist; ulist = ulist->next)
    {
      h = bfd_link_hash_lookup (link_info.hash, ulist->name, 
				false, false, false);

      if (h != (struct bfd_link_hash_entry *) NULL
          && (h->type == bfd_link_hash_defined
              || h->type == bfd_link_hash_defweak)
	  && ! bfd_is_abs_section (h->u.def.section))
	{
	  h->u.def.section->flags |= SEC_KEEP;
	}
    }

  bfd_gc_sections (output_bfd, &link_info);
}

void
lang_process ()
{
  lang_reasonable_defaults ();
  current_target = default_target;

  lang_for_each_statement (ldlang_open_output);	/* Open the output file */

  ldemul_create_output_section_statements ();

  /* Add to the hash table all undefineds on the command line */
  lang_place_undefineds ();

  already_linked_table_init ();

  /* Create a bfd for each input file */
  current_target = default_target;
  open_input_bfds (statement_list.head, false);

  ldemul_after_open ();

  already_linked_table_free ();

  /* Make sure that we're not mixing architectures.  We call this
     after all the input files have been opened, but before we do any
     other processing, so that any operations merge_private_bfd_data
     does on the output file will be known during the rest of the
     link.  */
  lang_check ();

  /* Handle .exports instead of a version script if we're told to do so.  */
  if (command_line.version_exports_section)
    lang_do_version_exports_section ();

  /* Build all sets based on the information gathered from the input
     files.  */
  ldctor_build_sets ();

  /* Remove unreferenced sections if asked to.  */
  if (command_line.gc_sections)
    lang_gc_sections ();

  /* Size up the common data */
  lang_common ();

  /* Run through the contours of the script and attach input sections
     to the correct output sections
     */
  map_input_to_output_sections (statement_list.head, (char *) NULL,
				(lang_output_section_statement_type *) NULL);


  /* Find any sections not attached explicitly and handle them */
  lang_place_orphans ();

  ldemul_before_allocation ();

  /* We must record the program headers before we try to fix the
     section positions, since they will affect SIZEOF_HEADERS.  */
  lang_record_phdrs ();

  /* Now run around and relax if we can */
  if (command_line.relax)
    {
      /* First time round is a trial run to get the 'worst case'
	 addresses of the objects if there was no relaxing.  */
      lang_size_sections (statement_list.head,
			  abs_output_section,
			  &(statement_list.head), 0, (bfd_vma) 0, false);

      /* Keep relaxing until bfd_relax_section gives up.  */
      do
	{
	  reset_memory_regions ();

	  relax_again = false;

	  /* Note: pe-dll.c does something like this also.  If you find
	     you need to change this code, you probably need to change
	     pe-dll.c also.  DJ */

	  /* Do all the assignments with our current guesses as to
	     section sizes.  */
	  lang_do_assignments (statement_list.head,
			       abs_output_section,
			       (fill_type) 0, (bfd_vma) 0);

	  /* Perform another relax pass - this time we know where the
	     globals are, so can make better guess.  */
	  lang_size_sections (statement_list.head,
			      abs_output_section,
			      &(statement_list.head), 0, (bfd_vma) 0, true);
	}
      while (relax_again);
    }
  else
    {
      /* Size up the sections.  */
      lang_size_sections (statement_list.head,
			  abs_output_section,
			  &(statement_list.head), 0, (bfd_vma) 0, false);
    }

  /* See if anything special should be done now we know how big
     everything is.  */
  ldemul_after_allocation ();

  /* Fix any .startof. or .sizeof. symbols.  */
  lang_set_startof ();

  /* Do all the assignments, now that we know the final restingplaces
     of all the symbols */

  lang_do_assignments (statement_list.head,
		       abs_output_section,
		       (fill_type) 0, (bfd_vma) 0);

  /* Make sure that the section addresses make sense.  */
  if (! link_info.relocateable
      && command_line.check_section_addresses)
    lang_check_section_addresses ();
  
  /* Final stuffs */

  ldemul_finish ();
  lang_finish ();
}

/* EXPORTED TO YACC */

void
lang_add_wild (section_name, sections_sorted, filename, filenames_sorted,
	       keep_sections, exclude_filename_list)
     const char *const section_name;
     boolean sections_sorted;
     const char *const filename;
     boolean filenames_sorted;
     boolean keep_sections;
     struct name_list *exclude_filename_list;
{
  lang_wild_statement_type *new = new_stat (lang_wild_statement,
					    stat_ptr);

  if (section_name != (char *) NULL && strcmp (section_name, "COMMON") == 0)
    {
      placed_commons = true;
    }
  if (filename != NULL && ! wildcardp (filename))
    {
      lang_has_input_file = true;
    }
  new->section_name = section_name;
  new->sections_sorted = sections_sorted;
  new->filename = filename;
  new->filenames_sorted = filenames_sorted;
  new->keep_sections = keep_sections;
  new->exclude_filename_list = exclude_filename_list;
  lang_list_init (&new->children);
}

void
lang_section_start (name, address)
     CONST char *name;
     etree_type * address;
{
  lang_address_statement_type *ad = new_stat (lang_address_statement, stat_ptr);

  ad->section_name = name;
  ad->address = address;
}

/* Set the start symbol to NAME.  CMDLINE is nonzero if this is called
   because of a -e argument on the command line, or zero if this is
   called by ENTRY in a linker script.  Command line arguments take
   precedence.  */

void
lang_add_entry (name, cmdline)
     CONST char *name;
     boolean cmdline;
{
  if (entry_symbol == NULL
      || cmdline
      || ! entry_from_cmdline)
    {
      entry_symbol = name;
      entry_from_cmdline = cmdline;
    }
}

void
lang_add_target (name)
     CONST char *name;
{
  lang_target_statement_type *new = new_stat (lang_target_statement,
					      stat_ptr);

  new->target = name;

}

void
lang_add_map (name)
     CONST char *name;
{
  while (*name)
    {
      switch (*name)
	{
	  case 'F':
	  map_option_f = true;
	  break;
	}
      name++;
    }
}

void
lang_add_fill (exp)
     int exp;
{
  lang_fill_statement_type *new = new_stat (lang_fill_statement,
					    stat_ptr);

  new->fill = exp;
}

void
lang_add_data (type, exp)
     int type;
     union etree_union *exp;
{

  lang_data_statement_type *new = new_stat (lang_data_statement,
					    stat_ptr);

  new->exp = exp;
  new->type = type;

}

/* Create a new reloc statement.  RELOC is the BFD relocation type to
   generate.  HOWTO is the corresponding howto structure (we could
   look this up, but the caller has already done so).  SECTION is the
   section to generate a reloc against, or NAME is the name of the
   symbol to generate a reloc against.  Exactly one of SECTION and
   NAME must be NULL.  ADDEND is an expression for the addend.  */

void
lang_add_reloc (reloc, howto, section, name, addend)
     bfd_reloc_code_real_type reloc;
     reloc_howto_type *howto;
     asection *section;
     const char *name;
     union etree_union *addend;
{
  lang_reloc_statement_type *p = new_stat (lang_reloc_statement, stat_ptr);
  
  p->reloc = reloc;
  p->howto = howto;
  p->section = section;
  p->name = name;
  p->addend_exp = addend;

  p->addend_value = 0;
  p->output_section = NULL;
  p->output_vma = 0;
}

lang_assignment_statement_type *
lang_add_assignment (exp)
     etree_type * exp;
{
  lang_assignment_statement_type *new = new_stat (lang_assignment_statement,
						  stat_ptr);

  new->exp = exp;
  return new;
}

void
lang_add_attribute (attribute)
     enum statement_enum attribute;
{
  new_statement (attribute, sizeof (lang_statement_union_type), stat_ptr);
}

void
lang_startup (name)
     CONST char *name;
{
  if (startup_file != (char *) NULL)
    {
      einfo (_("%P%Fmultiple STARTUP files\n"));
    }
  first_file->filename = name;
  first_file->local_sym_name = name;
  first_file->real = true;

  startup_file = name;
}

void
lang_float (maybe)
     boolean maybe;
{
  lang_float_flag = maybe;
}

void
lang_leave_output_section_statement (fill, memspec, phdrs, lma_memspec)
     bfd_vma fill;
     const char *memspec;
     struct lang_output_section_phdr_list *phdrs;
     const char *lma_memspec;
{
  current_section->fill = fill;
  current_section->region = lang_memory_region_lookup (memspec);
  if (strcmp (lma_memspec, "*default*") != 0)
    {
      current_section->lma_region = lang_memory_region_lookup (lma_memspec);
      /* if no runtime region has been given, but the load region has been,
         use the load region */
      if (strcmp (memspec, "*default*") == 0)
        current_section->region = lang_memory_region_lookup (lma_memspec);
    }
  current_section->phdrs = phdrs;
  stat_ptr = &statement_list;
}

/*
 Create an absolute symbol with the given name with the value of the
 address of first byte of the section named.

 If the symbol already exists, then do nothing.
*/
void
lang_abs_symbol_at_beginning_of (secname, name)
     const char *secname;
     const char *name;
{
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (link_info.hash, name, true, true, true);
  if (h == (struct bfd_link_hash_entry *) NULL)
    einfo (_("%P%F: bfd_link_hash_lookup failed: %E\n"));

  if (h->type == bfd_link_hash_new
      || h->type == bfd_link_hash_undefined)
    {
      asection *sec;

      h->type = bfd_link_hash_defined;

      sec = bfd_get_section_by_name (output_bfd, secname);
      if (sec == (asection *) NULL)
	h->u.def.value = 0;
      else
	h->u.def.value = bfd_get_section_vma (output_bfd, sec);

      h->u.def.section = bfd_abs_section_ptr;
    }
}

/*
 Create an absolute symbol with the given name with the value of the
 address of the first byte after the end of the section named.

 If the symbol already exists, then do nothing.
*/
void
lang_abs_symbol_at_end_of (secname, name)
     const char *secname;
     const char *name;
{
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (link_info.hash, name, true, true, true);
  if (h == (struct bfd_link_hash_entry *) NULL)
    einfo (_("%P%F: bfd_link_hash_lookup failed: %E\n"));

  if (h->type == bfd_link_hash_new
      || h->type == bfd_link_hash_undefined)
    {
      asection *sec;

      h->type = bfd_link_hash_defined;

      sec = bfd_get_section_by_name (output_bfd, secname);
      if (sec == (asection *) NULL)
	h->u.def.value = 0;
      else
	h->u.def.value = (bfd_get_section_vma (output_bfd, sec)
			  + bfd_section_size (output_bfd, sec) /
                          bfd_octets_per_byte (output_bfd));

      h->u.def.section = bfd_abs_section_ptr;
    }
}

void
lang_statement_append (list, element, field)
     lang_statement_list_type * list;
     lang_statement_union_type * element;
     lang_statement_union_type ** field;
{
  *(list->tail) = element;
  list->tail = field;
}

/* Set the output format type.  -oformat overrides scripts.  */

void
lang_add_output_format (format, big, little, from_script)
     const char *format;
     const char *big;
     const char *little;
     int from_script;
{
  if (output_target == NULL || !from_script)
    {
      if (command_line.endian == ENDIAN_BIG
	  && big != NULL)
	format = big;
      else if (command_line.endian == ENDIAN_LITTLE
	       && little != NULL)
	format = little;

      output_target = format;
    }
}

/* Enter a group.  This creates a new lang_group_statement, and sets
   stat_ptr to build new statements within the group.  */

void
lang_enter_group ()
{
  lang_group_statement_type *g;

  g = new_stat (lang_group_statement, stat_ptr);
  lang_list_init (&g->children);
  stat_ptr = &g->children;
}

/* Leave a group.  This just resets stat_ptr to start writing to the
   regular list of statements again.  Note that this will not work if
   groups can occur inside anything else which can adjust stat_ptr,
   but currently they can't.  */

void
lang_leave_group ()
{
  stat_ptr = &statement_list;
}

/* Add a new program header.  This is called for each entry in a PHDRS
   command in a linker script.  */

void
lang_new_phdr (name, type, filehdr, phdrs, at, flags)
     const char *name;
     etree_type *type;
     boolean filehdr;
     boolean phdrs;
     etree_type *at;
     etree_type *flags;
{
  struct lang_phdr *n, **pp;

  n = (struct lang_phdr *) stat_alloc (sizeof (struct lang_phdr));
  n->next = NULL;
  n->name = name;
  n->type = exp_get_value_int (type, 0, "program header type",
			       lang_final_phase_enum);
  n->filehdr = filehdr;
  n->phdrs = phdrs;
  n->at = at;
  n->flags = flags;

  for (pp = &lang_phdr_list; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = n;
}

/* Record the program header information in the output BFD.  FIXME: We
   should not be calling an ELF specific function here.  */

static void
lang_record_phdrs ()
{
  unsigned int alc;
  asection **secs;
  struct lang_output_section_phdr_list *last;
  struct lang_phdr *l;
  lang_statement_union_type *u;

  alc = 10;
  secs = (asection **) xmalloc (alc * sizeof (asection *));
  last = NULL;
  for (l = lang_phdr_list; l != NULL; l = l->next)
    {
      unsigned int c;
      flagword flags;
      bfd_vma at;

      c = 0;
      for (u = lang_output_section_statement.head;
	   u != NULL;
	   u = u->output_section_statement.next)
	{
	  lang_output_section_statement_type *os;
	  struct lang_output_section_phdr_list *pl;

	  os = &u->output_section_statement;

	  pl = os->phdrs;
	  if (pl != NULL)
	    last = pl;
	  else
	    {
	      if (os->sectype == noload_section
		  || os->bfd_section == NULL
		  || (os->bfd_section->flags & SEC_ALLOC) == 0)
		continue;
	      pl = last;
	    }

	  if (os->bfd_section == NULL)
	    continue;

	  for (; pl != NULL; pl = pl->next)
	    {
	      if (strcmp (pl->name, l->name) == 0)
		{
		  if (c >= alc)
		    {
		      alc *= 2;
		      secs = ((asection **)
			      xrealloc (secs, alc * sizeof (asection *)));
		    }
		  secs[c] = os->bfd_section;
		  ++c;
		  pl->used = true;
		}
	    }
	}

      if (l->flags == NULL)
	flags = 0;
      else
	flags = exp_get_vma (l->flags, 0, "phdr flags",
			     lang_final_phase_enum);

      if (l->at == NULL)
	at = 0;
      else
	at = exp_get_vma (l->at, 0, "phdr load address",
			  lang_final_phase_enum);

      if (! bfd_record_phdr (output_bfd, l->type,
			     l->flags == NULL ? false : true,
			     flags,
			     l->at == NULL ? false : true,
			     at, l->filehdr, l->phdrs, c, secs))
	einfo (_("%F%P: bfd_record_phdr failed: %E\n"));
    }

  free (secs);

  /* Make sure all the phdr assignments succeeded.  */
  for (u = lang_output_section_statement.head;
       u != NULL;
       u = u->output_section_statement.next)
    {
      struct lang_output_section_phdr_list *pl;

      if (u->output_section_statement.bfd_section == NULL)
	continue;

      for (pl = u->output_section_statement.phdrs;
	   pl != NULL;
	   pl = pl->next)
	if (! pl->used && strcmp (pl->name, "NONE") != 0)
	  einfo (_("%X%P: section `%s' assigned to non-existent phdr `%s'\n"),
		 u->output_section_statement.name, pl->name);
    }
}

/* Record a list of sections which may not be cross referenced.  */

void
lang_add_nocrossref (l)
     struct lang_nocrossref *l;
{
  struct lang_nocrossrefs *n;

  n = (struct lang_nocrossrefs *) xmalloc (sizeof *n);
  n->next = nocrossref_list;
  n->list = l;
  nocrossref_list = n;

  /* Set notice_all so that we get informed about all symbols.  */
  link_info.notice_all = true;
}

/* Overlay handling.  We handle overlays with some static variables.  */

/* The overlay virtual address.  */
static etree_type *overlay_vma;

/* The overlay load address.  */
static etree_type *overlay_lma;

/* Whether nocrossrefs is set for this overlay.  */
static int overlay_nocrossrefs;

/* An expression for the maximum section size seen so far.  */
static etree_type *overlay_max;

/* A list of all the sections in this overlay.  */

struct overlay_list
{
  struct overlay_list *next;
  lang_output_section_statement_type *os;
};

static struct overlay_list *overlay_list;

/* Start handling an overlay.  */

void
lang_enter_overlay (vma_expr, lma_expr, nocrossrefs)
     etree_type *vma_expr;
     etree_type *lma_expr;
     int nocrossrefs;
{
  /* The grammar should prevent nested overlays from occurring.  */
  ASSERT (overlay_vma == NULL
	  && overlay_lma == NULL
	  && overlay_list == NULL
	  && overlay_max == NULL);

  overlay_vma = vma_expr;
  overlay_lma = lma_expr;
  overlay_nocrossrefs = nocrossrefs;
}

/* Start a section in an overlay.  We handle this by calling
   lang_enter_output_section_statement with the correct VMA and LMA.  */

void
lang_enter_overlay_section (name)
     const char *name;
{
  struct overlay_list *n;
  etree_type *size;

  lang_enter_output_section_statement (name, overlay_vma, normal_section,
				       0, 0, 0, overlay_lma);

  /* If this is the first section, then base the VMA and LMA of future
     sections on this one.  This will work correctly even if `.' is
     used in the addresses.  */
  if (overlay_list == NULL)
    {
      overlay_vma = exp_nameop (ADDR, name);
      overlay_lma = exp_nameop (LOADADDR, name);
    }

  /* Remember the section.  */
  n = (struct overlay_list *) xmalloc (sizeof *n);
  n->os = current_section;
  n->next = overlay_list;
  overlay_list = n;

  size = exp_nameop (SIZEOF, name);

  /* Adjust the LMA for the next section.  */
  overlay_lma = exp_binop ('+', overlay_lma, size);

  /* Arrange to work out the maximum section end address.  */
  if (overlay_max == NULL)
    overlay_max = size;
  else
    overlay_max = exp_binop (MAX_K, overlay_max, size);
}

/* Finish a section in an overlay.  There isn't any special to do
   here.  */

void
lang_leave_overlay_section (fill, phdrs)
     bfd_vma fill;
     struct lang_output_section_phdr_list *phdrs;
{
  const char *name;
  char *clean, *s2;
  const char *s1;
  char *buf;

  name = current_section->name;

  lang_leave_output_section_statement (fill, "*default*", 
                                       phdrs, "*default*");

  /* Define the magic symbols.  */

  clean = xmalloc (strlen (name) + 1);
  s2 = clean;
  for (s1 = name; *s1 != '\0'; s1++)
    if (isalnum ((unsigned char) *s1) || *s1 == '_')
      *s2++ = *s1;
  *s2 = '\0';

  buf = xmalloc (strlen (clean) + sizeof "__load_start_");
  sprintf (buf, "__load_start_%s", clean);
  lang_add_assignment (exp_assop ('=', buf,
				  exp_nameop (LOADADDR, name)));

  buf = xmalloc (strlen (clean) + sizeof "__load_stop_");
  sprintf (buf, "__load_stop_%s", clean);
  lang_add_assignment (exp_assop ('=', buf,
				  exp_binop ('+',
					     exp_nameop (LOADADDR, name),
					     exp_nameop (SIZEOF, name))));

  free (clean);
}

/* Finish an overlay.  If there are any overlay wide settings, this
   looks through all the sections in the overlay and sets them.  */

void
lang_leave_overlay (fill, memspec, phdrs, lma_memspec)
     bfd_vma fill;
     const char *memspec;
     struct lang_output_section_phdr_list *phdrs;
     const char *lma_memspec;
{
  lang_memory_region_type *region;
  lang_memory_region_type *lma_region;
  struct overlay_list *l;
  struct lang_nocrossref *nocrossref;

  if (memspec == NULL)
    region = NULL;
  else
    region = lang_memory_region_lookup (memspec);

  if (lma_memspec == NULL)
    lma_region = NULL;
  else
    lma_region = lang_memory_region_lookup (lma_memspec);

  nocrossref = NULL;

  l = overlay_list;
  while (l != NULL)
    {
      struct overlay_list *next;

      if (fill != 0 && l->os->fill == 0)
	l->os->fill = fill;
      if (region != NULL && l->os->region == NULL)
	l->os->region = region;
      if (lma_region != NULL && l->os->lma_region == NULL)
        l->os->lma_region = lma_region;
      if (phdrs != NULL && l->os->phdrs == NULL)
	l->os->phdrs = phdrs;

      if (overlay_nocrossrefs)
	{
	  struct lang_nocrossref *nc;

	  nc = (struct lang_nocrossref *) xmalloc (sizeof *nc);
	  nc->name = l->os->name;
	  nc->next = nocrossref;
	  nocrossref = nc;
	}

      next = l->next;
      free (l);
      l = next;
    }

  if (nocrossref != NULL)
    lang_add_nocrossref (nocrossref);

  /* Update . for the end of the overlay.  */
  lang_add_assignment (exp_assop ('=', ".",
				  exp_binop ('+', overlay_vma, overlay_max)));

  overlay_vma = NULL;
  overlay_lma = NULL;
  overlay_nocrossrefs = 0;
  overlay_list = NULL;
  overlay_max = NULL;
}

/* Version handling.  This is only useful for ELF.  */

/* This global variable holds the version tree that we build.  */

struct bfd_elf_version_tree *lang_elf_version_info;

static int
lang_vers_match_lang_c (expr, sym)
     struct bfd_elf_version_expr *expr;
     const char *sym;
{
  if (expr->pattern[0] == '*' && expr->pattern[1] == '\0')
    return 1;
  return fnmatch (expr->pattern, sym, 0) == 0;
}

static int
lang_vers_match_lang_cplusplus (expr, sym)
     struct bfd_elf_version_expr *expr;
     const char *sym;
{
  char *alt_sym;
  int result;

  if (expr->pattern[0] == '*' && expr->pattern[1] == '\0')
    return 1;

  alt_sym = cplus_demangle(sym, /* DMGL_NO_TPARAMS */ 0);
  if (!alt_sym)
    {
      /* cplus_demangle (also) returns NULL when it is not a C++ symbol.
	 Should we early out false in this case?  */
      result = fnmatch (expr->pattern, sym, 0) == 0;
    }
  else
    {
      result = fnmatch (expr->pattern, alt_sym, 0) == 0;
      free (alt_sym);
    }

  return result;
}

static int
lang_vers_match_lang_java (expr, sym)
     struct bfd_elf_version_expr *expr;
     const char *sym;
{
  char *alt_sym;
  int result;

  if (expr->pattern[0] == '*' && expr->pattern[1] == '\0')
    return 1;

  alt_sym = cplus_demangle(sym, DMGL_JAVA);
  if (!alt_sym)
    {
      /* cplus_demangle (also) returns NULL when it is not a Java symbol.
	 Should we early out false in this case?  */
      result = fnmatch (expr->pattern, sym, 0) == 0;
    }
  else
    {
      result = fnmatch (expr->pattern, alt_sym, 0) == 0;
      free (alt_sym);
    }

  return result;
}

/* This is called for each variable name or match expression.  */

struct bfd_elf_version_expr *
lang_new_vers_regex (orig, new, lang)
     struct bfd_elf_version_expr *orig;
     const char *new;
     const char *lang;
{
  struct bfd_elf_version_expr *ret;

  ret = (struct bfd_elf_version_expr *) xmalloc (sizeof *ret);
  ret->next = orig;
  ret->pattern = new;

  if (lang == NULL || strcasecmp (lang, "C") == 0)
    ret->match = lang_vers_match_lang_c;
  else if (strcasecmp (lang, "C++") == 0)
    ret->match = lang_vers_match_lang_cplusplus;
  else if (strcasecmp (lang, "Java") == 0)
    ret->match = lang_vers_match_lang_java;
  else
    {
      einfo (_("%X%P: unknown language `%s' in version information\n"),
	     lang);
      ret->match = lang_vers_match_lang_c;
    }

  return ret;
}

/* This is called for each set of variable names and match
   expressions.  */

struct bfd_elf_version_tree *
lang_new_vers_node (globals, locals)
     struct bfd_elf_version_expr *globals;
     struct bfd_elf_version_expr *locals;
{
  struct bfd_elf_version_tree *ret;

  ret = (struct bfd_elf_version_tree *) xmalloc (sizeof *ret);
  ret->next = NULL;
  ret->name = NULL;
  ret->vernum = 0;
  ret->globals = globals;
  ret->locals = locals;
  ret->deps = NULL;
  ret->name_indx = (unsigned int) -1;
  ret->used = 0;
  return ret;
}

/* This static variable keeps track of version indices.  */

static int version_index;

/* This is called when we know the name and dependencies of the
   version.  */

void
lang_register_vers_node (name, version, deps)
     const char *name;
     struct bfd_elf_version_tree *version;
     struct bfd_elf_version_deps *deps;
{
  struct bfd_elf_version_tree *t, **pp;
  struct bfd_elf_version_expr *e1;

  /* Make sure this node has a unique name.  */
  for (t = lang_elf_version_info; t != NULL; t = t->next)
    if (strcmp (t->name, name) == 0)
      einfo (_("%X%P: duplicate version tag `%s'\n"), name);

  /* Check the global and local match names, and make sure there
     aren't any duplicates.  */

  for (e1 = version->globals; e1 != NULL; e1 = e1->next)
    {
      for (t = lang_elf_version_info; t != NULL; t = t->next)
	{
	  struct bfd_elf_version_expr *e2;

	  for (e2 = t->locals; e2 != NULL; e2 = e2->next)
	    if (strcmp (e1->pattern, e2->pattern) == 0)
	      einfo (_("%X%P: duplicate expression `%s' in version information\n"),
		     e1->pattern);
	}
    }

  for (e1 = version->locals; e1 != NULL; e1 = e1->next)
    {
      for (t = lang_elf_version_info; t != NULL; t = t->next)
	{
	  struct bfd_elf_version_expr *e2;

	  for (e2 = t->globals; e2 != NULL; e2 = e2->next)
	    if (strcmp (e1->pattern, e2->pattern) == 0)
	      einfo (_("%X%P: duplicate expression `%s' in version information\n"),
		     e1->pattern);
	}
    }

  version->deps = deps;
  version->name = name;
  ++version_index;
  version->vernum = version_index;

  for (pp = &lang_elf_version_info; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = version;
}

/* This is called when we see a version dependency.  */

struct bfd_elf_version_deps *
lang_add_vers_depend (list, name)
     struct bfd_elf_version_deps *list;
     const char *name;
{
  struct bfd_elf_version_deps *ret;
  struct bfd_elf_version_tree *t;

  ret = (struct bfd_elf_version_deps *) xmalloc (sizeof *ret);
  ret->next = list;

  for (t = lang_elf_version_info; t != NULL; t = t->next)
    {
      if (strcmp (t->name, name) == 0)
	{
	  ret->version_needed = t;
	  return ret;
	}
    }

  einfo (_("%X%P: unable to find version dependency `%s'\n"), name);

  return ret;
}

static void
lang_do_version_exports_section ()
{
  struct bfd_elf_version_expr *greg = NULL, *lreg;

  LANG_FOR_EACH_INPUT_STATEMENT (is)
    {
      asection *sec = bfd_get_section_by_name (is->the_bfd, ".exports");
      char *contents, *p;
      bfd_size_type len;

      if (sec == NULL)
        continue;

      len = bfd_section_size (is->the_bfd, sec);
      contents = xmalloc (len);
      if (!bfd_get_section_contents (is->the_bfd, sec, contents, 0, len))
	einfo (_("%X%P: unable to read .exports section contents"), sec);

      p = contents;
      while (p < contents+len)
	{
	  greg = lang_new_vers_regex (greg, p, NULL);
	  p = strchr (p, '\0') + 1;
	}

      /* Do not free the contents, as we used them creating the regex.  */

      /* Do not include this section in the link.  */
      bfd_set_section_flags (is->the_bfd, sec,
	bfd_get_section_flags (is->the_bfd, sec) | SEC_EXCLUDE);
    }

  lreg = lang_new_vers_regex (NULL, "*", NULL);
  lang_register_vers_node (command_line.version_exports_section,
			   lang_new_vers_node (greg, lreg), NULL);
}
