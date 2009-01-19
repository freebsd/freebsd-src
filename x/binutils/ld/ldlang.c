/* Linker command language support.
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002, 2003, 2004
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
#include "safe-ctype.h"
#include "obstack.h"
#include "bfdlink.h"

#include "ld.h"
#include "ldmain.h"
#include "ldexp.h"
#include "ldlang.h"
#include <ldgram.h>
#include "ldlex.h"
#include "ldmisc.h"
#include "ldctor.h"
#include "ldfile.h"
#include "ldemul.h"
#include "fnmatch.h"
#include "demangle.h"
#include "hashtab.h"

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) & (((TYPE*) 0)->MEMBER))
#endif

/* Locals variables.  */
static struct obstack stat_obstack;

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
static const char *startup_file;
static lang_statement_list_type input_file_chain;
static bfd_boolean placed_commons = FALSE;
static lang_output_section_statement_type *default_common_section;
static bfd_boolean map_option_f;
static bfd_vma print_dot;
static lang_input_statement_type *first_file;
static const char *current_target;
static const char *output_target;
static lang_statement_list_type statement_list;
static struct lang_phdr *lang_phdr_list;
static struct bfd_hash_table lang_definedness_table;

/* Forward declarations.  */
static void exp_init_os (etree_type *);
static bfd_boolean wildcardp (const char *);
static lang_input_statement_type *lookup_name (const char *);
static bfd_boolean load_symbols (lang_input_statement_type *,
				 lang_statement_list_type *);
static struct bfd_hash_entry *lang_definedness_newfunc
 (struct bfd_hash_entry *, struct bfd_hash_table *, const char *);
static void insert_undefined (const char *);
static void print_statement (lang_statement_union_type *,
			     lang_output_section_statement_type *);
static void print_statement_list (lang_statement_union_type *,
				  lang_output_section_statement_type *);
static void print_statements (void);
static bfd_boolean lang_one_common (struct bfd_link_hash_entry *, void *);
static void lang_record_phdrs (void);
static void lang_do_version_exports_section (void);

typedef void (*callback_t) (lang_wild_statement_type *, struct wildcard_list *,
			    asection *, lang_input_statement_type *, void *);

/* Exported variables.  */
lang_output_section_statement_type *abs_output_section;
lang_statement_list_type lang_output_section_statement;
lang_statement_list_type *stat_ptr = &statement_list;
lang_statement_list_type file_chain = { NULL, NULL };
struct bfd_sym_chain entry_symbol = { NULL, NULL };
const char *entry_section = ".text";
bfd_boolean entry_from_cmdline;
bfd_boolean lang_has_input_file = FALSE;
bfd_boolean had_output_filename = FALSE;
bfd_boolean lang_float_flag = FALSE;
bfd_boolean delete_output_file_on_failure = FALSE;
struct lang_nocrossrefs *nocrossref_list;
struct unique_sections *unique_section_list;
static bfd_boolean ldlang_sysrooted_script = FALSE;
int lang_statement_iteration = 0;

etree_type *base; /* Relocation base - or null */

#define new_stat(x, y) \
  (x##_type *) new_statement (x##_enum, sizeof (x##_type), y)

#define outside_section_address(q) \
  ((q)->output_offset + (q)->output_section->vma)

#define outside_symbol_address(q) \
  ((q)->value + outside_section_address (q->section))

#define SECTION_NAME_MAP_LENGTH (16)

void *
stat_alloc (size_t size)
{
  return obstack_alloc (&stat_obstack, size);
}

bfd_boolean
unique_section_p (const char *secnam)
{
  struct unique_sections *unam;

  for (unam = unique_section_list; unam; unam = unam->next)
    if (wildcardp (unam->name)
	? fnmatch (unam->name, secnam, 0) == 0
	: strcmp (unam->name, secnam) == 0)
      {
	return TRUE;
      }

  return FALSE;
}

/* Generic traversal routines for finding matching sections.  */

static void
walk_wild_section (lang_wild_statement_type *ptr,
		   lang_input_statement_type *file,
		   callback_t callback,
		   void *data)
{
  asection *s;

  if (file->just_syms_flag)
    return;

  for (s = file->the_bfd->sections; s != NULL; s = s->next)
    {
      struct wildcard_list *sec;

      sec = ptr->section_list;
      if (sec == NULL)
	(*callback) (ptr, sec, s, file, data);

      while (sec != NULL)
	{
	  bfd_boolean skip = FALSE;
	  struct name_list *list_tmp;

	  /* Don't process sections from files which were
	     excluded.  */
	  for (list_tmp = sec->spec.exclude_name_list;
	       list_tmp;
	       list_tmp = list_tmp->next)
	    {
	      if (wildcardp (list_tmp->name))
		skip = fnmatch (list_tmp->name, file->filename, 0) == 0;
	      else
		skip = strcmp (list_tmp->name, file->filename) == 0;

	      /* If this file is part of an archive, and the archive is
		 excluded, exclude this file.  */
	      if (! skip && file->the_bfd != NULL
		  && file->the_bfd->my_archive != NULL
		  && file->the_bfd->my_archive->filename != NULL)
		{
		  if (wildcardp (list_tmp->name))
		    skip = fnmatch (list_tmp->name,
				    file->the_bfd->my_archive->filename,
				    0) == 0;
		  else
		    skip = strcmp (list_tmp->name,
				   file->the_bfd->my_archive->filename) == 0;
		}

	      if (skip)
		break;
	    }

	  if (!skip && sec->spec.name != NULL)
	    {
	      const char *sname = bfd_get_section_name (file->the_bfd, s);

	      if (wildcardp (sec->spec.name))
		skip = fnmatch (sec->spec.name, sname, 0) != 0;
	      else
		skip = strcmp (sec->spec.name, sname) != 0;
	    }

	  if (!skip)
	    (*callback) (ptr, sec, s, file, data);

	  sec = sec->next;
	}
    }
}

/* Handle a wild statement for a single file F.  */

static void
walk_wild_file (lang_wild_statement_type *s,
		lang_input_statement_type *f,
		callback_t callback,
		void *data)
{
  if (f->the_bfd == NULL
      || ! bfd_check_format (f->the_bfd, bfd_archive))
    walk_wild_section (s, f, callback, data);
  else
    {
      bfd *member;

      /* This is an archive file.  We must map each member of the
	 archive separately.  */
      member = bfd_openr_next_archived_file (f->the_bfd, NULL);
      while (member != NULL)
	{
	  /* When lookup_name is called, it will call the add_symbols
	     entry point for the archive.  For each element of the
	     archive which is included, BFD will call ldlang_add_file,
	     which will set the usrdata field of the member to the
	     lang_input_statement.  */
	  if (member->usrdata != NULL)
	    {
	      walk_wild_section (s, member->usrdata, callback, data);
	    }

	  member = bfd_openr_next_archived_file (f->the_bfd, member);
	}
    }
}

static void
walk_wild (lang_wild_statement_type *s, callback_t callback, void *data)
{
  const char *file_spec = s->filename;

  if (file_spec == NULL)
    {
      /* Perform the iteration over all files in the list.  */
      LANG_FOR_EACH_INPUT_STATEMENT (f)
	{
	  walk_wild_file (s, f, callback, data);
	}
    }
  else if (wildcardp (file_spec))
    {
      LANG_FOR_EACH_INPUT_STATEMENT (f)
	{
	  if (fnmatch (file_spec, f->filename, FNM_FILE_NAME) == 0)
	    walk_wild_file (s, f, callback, data);
	}
    }
  else
    {
      lang_input_statement_type *f;

      /* Perform the iteration over a single file.  */
      f = lookup_name (file_spec);
      if (f)
	walk_wild_file (s, f, callback, data);
    }
}

/* lang_for_each_statement walks the parse tree and calls the provided
   function for each node.  */

static void
lang_for_each_statement_worker (void (*func) (lang_statement_union_type *),
				lang_statement_union_type *s)
{
  for (; s != NULL; s = s->header.next)
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
lang_for_each_statement (void (*func) (lang_statement_union_type *))
{
  lang_for_each_statement_worker (func, statement_list.head);
}

/*----------------------------------------------------------------------*/

void
lang_list_init (lang_statement_list_type *list)
{
  list->head = NULL;
  list->tail = &list->head;
}

/* Build a new statement node for the parse tree.  */

static lang_statement_union_type *
new_statement (enum statement_enum type,
	       size_t size,
	       lang_statement_list_type *list)
{
  lang_statement_union_type *new;

  new = stat_alloc (size);
  new->header.type = type;
  new->header.next = NULL;
  lang_statement_append (list, new, &new->header.next);
  return new;
}

/* Build a new input file node for the language.  There are several
   ways in which we treat an input file, eg, we only look at symbols,
   or prefix it with a -l etc.

   We can be supplied with requests for input files more than once;
   they may, for example be split over several lines like foo.o(.text)
   foo.o(.data) etc, so when asked for a file we check that we haven't
   got it already so we don't duplicate the bfd.  */

static lang_input_statement_type *
new_afile (const char *name,
	   lang_input_file_enum_type file_type,
	   const char *target,
	   bfd_boolean add_to_list)
{
  lang_input_statement_type *p;

  if (add_to_list)
    p = new_stat (lang_input_statement, stat_ptr);
  else
    {
      p = stat_alloc (sizeof (lang_input_statement_type));
      p->header.next = NULL;
    }

  lang_has_input_file = TRUE;
  p->target = target;
  p->sysrooted = FALSE;
  switch (file_type)
    {
    case lang_input_file_is_symbols_only_enum:
      p->filename = name;
      p->is_archive = FALSE;
      p->real = TRUE;
      p->local_sym_name = name;
      p->just_syms_flag = TRUE;
      p->search_dirs_flag = FALSE;
      break;
    case lang_input_file_is_fake_enum:
      p->filename = name;
      p->is_archive = FALSE;
      p->real = FALSE;
      p->local_sym_name = name;
      p->just_syms_flag = FALSE;
      p->search_dirs_flag = FALSE;
      break;
    case lang_input_file_is_l_enum:
      p->is_archive = TRUE;
      p->filename = name;
      p->real = TRUE;
      p->local_sym_name = concat ("-l", name, NULL);
      p->just_syms_flag = FALSE;
      p->search_dirs_flag = TRUE;
      break;
    case lang_input_file_is_marker_enum:
      p->filename = name;
      p->is_archive = FALSE;
      p->real = FALSE;
      p->local_sym_name = name;
      p->just_syms_flag = FALSE;
      p->search_dirs_flag = TRUE;
      break;
    case lang_input_file_is_search_file_enum:
      p->sysrooted = ldlang_sysrooted_script;
      p->filename = name;
      p->is_archive = FALSE;
      p->real = TRUE;
      p->local_sym_name = name;
      p->just_syms_flag = FALSE;
      p->search_dirs_flag = TRUE;
      break;
    case lang_input_file_is_file_enum:
      p->filename = name;
      p->is_archive = FALSE;
      p->real = TRUE;
      p->local_sym_name = name;
      p->just_syms_flag = FALSE;
      p->search_dirs_flag = FALSE;
      break;
    default:
      FAIL ();
    }
  p->the_bfd = NULL;
  p->asymbols = NULL;
  p->next_real_file = NULL;
  p->next = NULL;
  p->symbol_count = 0;
  p->dynamic = config.dynamic_link;
  p->as_needed = as_needed;
  p->whole_archive = whole_archive;
  p->loaded = FALSE;
  lang_statement_append (&input_file_chain,
			 (lang_statement_union_type *) p,
			 &p->next_real_file);
  return p;
}

lang_input_statement_type *
lang_add_input_file (const char *name,
		     lang_input_file_enum_type file_type,
		     const char *target)
{
  lang_has_input_file = TRUE;
  return new_afile (name, file_type, target, TRUE);
}

/* Build enough state so that the parser can build its tree.  */

void
lang_init (void)
{
  obstack_begin (&stat_obstack, 1000);

  stat_ptr = &statement_list;

  lang_list_init (stat_ptr);

  lang_list_init (&input_file_chain);
  lang_list_init (&lang_output_section_statement);
  lang_list_init (&file_chain);
  first_file = lang_add_input_file (NULL, lang_input_file_is_marker_enum,
				    NULL);
  abs_output_section =
    lang_output_section_statement_lookup (BFD_ABS_SECTION_NAME);

  abs_output_section->bfd_section = bfd_abs_section_ptr;

  /* The value "3" is ad-hoc, somewhat related to the expected number of
     DEFINED expressions in a linker script.  For most default linker
     scripts, there are none.  Why a hash table then?  Well, it's somewhat
     simpler to re-use working machinery than using a linked list in terms
     of code-complexity here in ld, besides the initialization which just
     looks like other code here.  */
  if (bfd_hash_table_init_n (&lang_definedness_table,
			     lang_definedness_newfunc, 3) != TRUE)
    einfo (_("%P%F: out of memory during initialization"));

  /* Callers of exp_fold_tree need to increment this.  */
  lang_statement_iteration = 0;
}

/*----------------------------------------------------------------------
  A region is an area of memory declared with the
  MEMORY {  name:org=exp, len=exp ... }
  syntax.

  We maintain a list of all the regions here.

  If no regions are specified in the script, then the default is used
  which is created when looked up to be the entire data space.

  If create is true we are creating a region inside a MEMORY block.
  In this case it is probably an error to create a region that has
  already been created.  If we are not inside a MEMORY block it is
  dubious to use an undeclared region name (except DEFAULT_MEMORY_REGION)
  and so we issue a warning.  */

static lang_memory_region_type *lang_memory_region_list;
static lang_memory_region_type **lang_memory_region_list_tail = &lang_memory_region_list;

lang_memory_region_type *
lang_memory_region_lookup (const char *const name, bfd_boolean create)
{
  lang_memory_region_type *p;
  lang_memory_region_type *new;

  /* NAME is NULL for LMA memspecs if no region was specified.  */
  if (name == NULL)
    return NULL;

  for (p = lang_memory_region_list; p != NULL; p = p->next)
    if (strcmp (p->name, name) == 0)
      {
	if (create)
	  einfo (_("%P:%S: warning: redeclaration of memory region '%s'\n"), name);
	return p;
      }

#if 0
  /* This code used to always use the first region in the list as the
     default region.  I changed it to instead use a region
     encompassing all of memory as the default region.  This permits
     NOLOAD sections to work reasonably without requiring a region.
     People should specify what region they mean, if they really want
     a region.  */
  if (strcmp (name, DEFAULT_MEMORY_REGION) == 0)
    {
      if (lang_memory_region_list != NULL)
	return lang_memory_region_list;
    }
#endif

  if (!create && strcmp (name, DEFAULT_MEMORY_REGION))
    einfo (_("%P:%S: warning: memory region %s not declared\n"), name);

  new = stat_alloc (sizeof (lang_memory_region_type));

  new->name = xstrdup (name);
  new->next = NULL;

  *lang_memory_region_list_tail = new;
  lang_memory_region_list_tail = &new->next;
  new->origin = 0;
  new->flags = 0;
  new->not_flags = 0;
  new->length = ~(bfd_size_type) 0;
  new->current = 0;
  new->had_full_message = FALSE;

  return new;
}

static lang_memory_region_type *
lang_memory_default (asection *section)
{
  lang_memory_region_type *p;

  flagword sec_flags = section->flags;

  /* Override SEC_DATA to mean a writable section.  */
  if ((sec_flags & (SEC_ALLOC | SEC_READONLY | SEC_CODE)) == SEC_ALLOC)
    sec_flags |= SEC_DATA;

  for (p = lang_memory_region_list; p != NULL; p = p->next)
    {
      if ((p->flags & sec_flags) != 0
	  && (p->not_flags & sec_flags) == 0)
	{
	  return p;
	}
    }
  return lang_memory_region_lookup (DEFAULT_MEMORY_REGION, FALSE);
}

lang_output_section_statement_type *
lang_output_section_find (const char *const name)
{
  lang_statement_union_type *u;
  lang_output_section_statement_type *lookup;

  for (u = lang_output_section_statement.head; u != NULL; u = lookup->next)
    {
      lookup = &u->output_section_statement;
      if (strcmp (name, lookup->name) == 0)
	return lookup;
    }
  return NULL;
}

lang_output_section_statement_type *
lang_output_section_statement_lookup (const char *const name)
{
  lang_output_section_statement_type *lookup;

  lookup = lang_output_section_find (name);
  if (lookup == NULL)
    {
      lookup = new_stat (lang_output_section_statement, stat_ptr);
      lookup->region = NULL;
      lookup->lma_region = NULL;
      lookup->fill = 0;
      lookup->block_value = 1;
      lookup->name = name;

      lookup->next = NULL;
      lookup->bfd_section = NULL;
      lookup->processed = 0;
      lookup->sectype = normal_section;
      lookup->addr_tree = NULL;
      lang_list_init (&lookup->children);

      lookup->memspec = NULL;
      lookup->flags = 0;
      lookup->subsection_alignment = -1;
      lookup->section_alignment = -1;
      lookup->load_base = NULL;
      lookup->update_dot_tree = NULL;
      lookup->phdrs = NULL;

      lang_statement_append (&lang_output_section_statement,
			     (lang_statement_union_type *) lookup,
			     &lookup->next);
    }
  return lookup;
}

static void
lang_map_flags (flagword flag)
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
lang_map (void)
{
  lang_memory_region_type *m;

  minfo (_("\nMemory Configuration\n\n"));
  fprintf (config.map_file, "%-16s %-18s %-18s %s\n",
	   _("Name"), _("Origin"), _("Length"), _("Attributes"));

  for (m = lang_memory_region_list; m != NULL; m = m->next)
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
init_os (lang_output_section_statement_type *s)
{
  section_userdata_type *new;

  if (s->bfd_section != NULL)
    return;

  if (strcmp (s->name, DISCARD_SECTION_NAME) == 0)
    einfo (_("%P%F: Illegal use of `%s' section\n"), DISCARD_SECTION_NAME);

  new = stat_alloc (sizeof (section_userdata_type));

  s->bfd_section = bfd_get_section_by_name (output_bfd, s->name);
  if (s->bfd_section == NULL)
    s->bfd_section = bfd_make_section (output_bfd, s->name);
  if (s->bfd_section == NULL)
    {
      einfo (_("%P%F: output format %s cannot represent section called %s\n"),
	     output_bfd->xvec->name, s->name);
    }
  s->bfd_section->output_section = s->bfd_section;

  /* We initialize an output sections output offset to minus its own
     vma to allow us to output a section through itself.  */
  s->bfd_section->output_offset = 0;
  get_userdata (s->bfd_section) = new;

  /* If there is a base address, make sure that any sections it might
     mention are initialized.  */
  if (s->addr_tree != NULL)
    exp_init_os (s->addr_tree);

  if (s->load_base != NULL)
    exp_init_os (s->load_base);
}

/* Make sure that all output sections mentioned in an expression are
   initialized.  */

static void
exp_init_os (etree_type *exp)
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

    case etree_assert:
      exp_init_os (exp->assert_s.child);
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

static void
section_already_linked (bfd *abfd, asection *sec, void *data)
{
  lang_input_statement_type *entry = data;
  flagword flags;
  const char *name;
  struct already_linked *l;
  struct already_linked_hash_entry *already_linked_list;

  /* If we are only reading symbols from this object, then we want to
     discard all sections.  */
  if (entry->just_syms_flag)
    {
      bfd_link_just_syms (sec, &link_info);
      return;
    }

  flags = bfd_get_section_flags (abfd, sec);

  if ((flags & SEC_LINK_ONCE) == 0)
    return;

  /* FIXME: When doing a relocatable link, we may have trouble
     copying relocations in other sections that refer to local symbols
     in the section being discarded.  Those relocations will have to
     be converted somehow; as of this writing I'm not sure that any of
     the backends handle that correctly.

     It is tempting to instead not discard link once sections when
     doing a relocatable link (technically, they should be discarded
     whenever we are building constructors).  However, that fails,
     because the linker winds up combining all the link once sections
     into a single large link once section, which defeats the purpose
     of having link once sections in the first place.

     Also, not merging link once sections in a relocatable link
     causes trouble for MIPS ELF, which relies on link once semantics
     to handle the .reginfo section correctly.  */

  name = bfd_get_section_name (abfd, sec);

  already_linked_list =
    ((struct already_linked_hash_entry *)
     bfd_hash_lookup (&already_linked_table, name, TRUE, FALSE));

  for (l = already_linked_list->entry; l != NULL; l = l->next)
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

	  /* Set the output_section field so that lang_add_section
	     does not create a lang_input_section structure for this
	     section.  Since there might be a symbol in the section
	     being discarded, we must retain a pointer to the section
	     which we are really going to use.  */
	  sec->output_section = bfd_abs_section_ptr;
	  sec->kept_section = l->sec;

	  if (flags & SEC_GROUP)
	    bfd_discard_group (abfd, sec);

	  return;
	}
    }

  /* This is the first section with this name.  Record it.  Allocate
     the memory from the same obstack as the hash table is kept in.  */

  l = bfd_hash_allocate (&already_linked_table, sizeof *l);

  l->sec = sec;
  l->next = already_linked_list->entry;
  already_linked_list->entry = l;
}

/* Support routines for the hash table used by section_already_linked,
   initialize the table, fill in an entry and remove the table.  */

static struct bfd_hash_entry *
already_linked_newfunc (struct bfd_hash_entry *entry ATTRIBUTE_UNUSED,
			struct bfd_hash_table *table,
			const char *string ATTRIBUTE_UNUSED)
{
  struct already_linked_hash_entry *ret =
    bfd_hash_allocate (table, sizeof (struct already_linked_hash_entry));

  ret->entry = NULL;

  return &ret->root;
}

static void
already_linked_table_init (void)
{
  if (! bfd_hash_table_init_n (&already_linked_table,
			       already_linked_newfunc,
			       42))
    einfo (_("%P%F: Failed to create hash table\n"));
}

static void
already_linked_table_free (void)
{
  bfd_hash_table_free (&already_linked_table);
}

/* The wild routines.

   These expand statements like *(.text) and foo.o to a list of
   explicit actions, like foo.o(.text), bar.o(.text) and
   foo.o(.text, .data).  */

/* Return TRUE if the PATTERN argument is a wildcard pattern.
   Although backslashes are treated specially if a pattern contains
   wildcards, we do not consider the mere presence of a backslash to
   be enough to cause the pattern to be treated as a wildcard.
   That lets us handle DOS filenames more naturally.  */

static bfd_boolean
wildcardp (const char *pattern)
{
  const char *s;

  for (s = pattern; *s != '\0'; ++s)
    if (*s == '?'
	|| *s == '*'
	|| *s == '[')
      return TRUE;
  return FALSE;
}

/* Add SECTION to the output section OUTPUT.  Do this by creating a
   lang_input_section statement which is placed at PTR.  FILE is the
   input file which holds SECTION.  */

void
lang_add_section (lang_statement_list_type *ptr,
		  asection *section,
		  lang_output_section_statement_type *output,
		  lang_input_statement_type *file)
{
  flagword flags;
  bfd_boolean discard;

  flags = bfd_get_section_flags (section->owner, section);

  discard = FALSE;

  /* Discard sections marked with SEC_EXCLUDE if we are doing a final
     link.  Discard debugging sections marked with SEC_EXCLUDE on a
     relocatable link too.  */
  if ((flags & SEC_EXCLUDE) != 0
      && ((flags & SEC_DEBUGGING) != 0 || !link_info.relocatable))
    discard = TRUE;

  /* Discard input sections which are assigned to a section named
     DISCARD_SECTION_NAME.  */
  if (strcmp (output->name, DISCARD_SECTION_NAME) == 0)
    discard = TRUE;

  /* Discard debugging sections if we are stripping debugging
     information.  */
  if ((link_info.strip == strip_debugger || link_info.strip == strip_all)
      && (flags & SEC_DEBUGGING) != 0)
    discard = TRUE;

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
      bfd_boolean first;
      lang_input_section_type *new;
      flagword flags;

      if (output->bfd_section == NULL)
	init_os (output);

      first = ! output->bfd_section->linker_has_input;
      output->bfd_section->linker_has_input = 1;

      /* Add a section reference to the list.  */
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

      if (! link_info.relocatable)
	flags &= ~ (SEC_LINK_ONCE | SEC_LINK_DUPLICATES);

      /* If this is not the first input section, and the SEC_READONLY
         flag is not currently set, then don't set it just because the
         input section has it set.  */

      if (! first && (section->output_section->flags & SEC_READONLY) == 0)
	flags &= ~ SEC_READONLY;

      /* Keep SEC_MERGE and SEC_STRINGS only if they are the same.  */
      if (! first
	  && ((section->output_section->flags & (SEC_MERGE | SEC_STRINGS))
	      != (flags & (SEC_MERGE | SEC_STRINGS))
	      || ((flags & SEC_MERGE)
		  && section->output_section->entsize != section->entsize)))
	{
	  section->output_section->flags &= ~ (SEC_MERGE | SEC_STRINGS);
	  flags &= ~ (SEC_MERGE | SEC_STRINGS);
	}

      section->output_section->flags |= flags;

      if (flags & SEC_MERGE)
	section->output_section->entsize = section->entsize;

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

      /* If supplied an alignment, then force it.  */
      if (output->section_alignment != -1)
	output->bfd_section->alignment_power = output->section_alignment;

      if (section->flags & SEC_BLOCK)
	{
	  section->output_section->flags |= SEC_BLOCK;
	  /* FIXME: This value should really be obtained from the bfd...  */
	  output->block_value = 128;
	}
    }
}

/* Handle wildcard sorting.  This returns the lang_input_section which
   should follow the one we are going to create for SECTION and FILE,
   based on the sorting requirements of WILD.  It returns NULL if the
   new section should just go at the end of the current list.  */

static lang_statement_union_type *
wild_sort (lang_wild_statement_type *wild,
	   struct wildcard_list *sec,
	   lang_input_statement_type *file,
	   asection *section)
{
  const char *section_name;
  lang_statement_union_type *l;

  if (!wild->filenames_sorted && (sec == NULL || !sec->spec.sorted))
    return NULL;

  section_name = bfd_get_section_name (file->the_bfd, section);
  for (l = wild->children.head; l != NULL; l = l->header.next)
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
	  bfd_boolean fa, la;
	  int i;

	  /* The PE support for the .idata section as generated by
             dlltool assumes that files will be sorted by the name of
             the archive and then the name of the file within the
             archive.  */

	  if (file->the_bfd != NULL
	      && bfd_my_archive (file->the_bfd) != NULL)
	    {
	      fn = bfd_get_filename (bfd_my_archive (file->the_bfd));
	      fa = TRUE;
	    }
	  else
	    {
	      fn = file->filename;
	      fa = FALSE;
	    }

	  if (ls->ifile->the_bfd != NULL
	      && bfd_my_archive (ls->ifile->the_bfd) != NULL)
	    {
	      ln = bfd_get_filename (bfd_my_archive (ls->ifile->the_bfd));
	      la = TRUE;
	    }
	  else
	    {
	      ln = ls->ifile->filename;
	      la = FALSE;
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

      if (sec != NULL && sec->spec.sorted)
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
output_section_callback (lang_wild_statement_type *ptr,
			 struct wildcard_list *sec,
			 asection *section,
			 lang_input_statement_type *file,
			 void *output)
{
  lang_statement_union_type *before;

  /* Exclude sections that match UNIQUE_SECTION_LIST.  */
  if (unique_section_p (bfd_get_section_name (file->the_bfd, section)))
    return;

  /* If the wild pattern was marked KEEP, the member sections
     should be as well.  */
  if (ptr->keep_sections)
    section->flags |= SEC_KEEP;

  before = wild_sort (ptr, sec, file, section);

  /* Here BEFORE points to the lang_input_section which
     should follow the one we are about to add.  If BEFORE
     is NULL, then the section should just go at the end
     of the current list.  */

  if (before == NULL)
    lang_add_section (&ptr->children, section,
		      (lang_output_section_statement_type *) output,
		      file);
  else
    {
      lang_statement_list_type list;
      lang_statement_union_type **pp;

      lang_list_init (&list);
      lang_add_section (&list, section,
			(lang_output_section_statement_type *) output,
			file);

      /* If we are discarding the section, LIST.HEAD will
	 be NULL.  */
      if (list.head != NULL)
	{
	  ASSERT (list.head->header.next == NULL);

	  for (pp = &ptr->children.head;
	       *pp != before;
	       pp = &(*pp)->header.next)
	    ASSERT (*pp != NULL);

	  list.head->header.next = *pp;
	  *pp = list.head;
	}
    }
}

/* This is passed a file name which must have been seen already and
   added to the statement tree.  We will see if it has been opened
   already and had its symbols read.  If not then we'll read it.  */

static lang_input_statement_type *
lookup_name (const char *name)
{
  lang_input_statement_type *search;

  for (search = (lang_input_statement_type *) input_file_chain.head;
       search != NULL;
       search = (lang_input_statement_type *) search->next_real_file)
    {
      /* Use the local_sym_name as the name of the file that has
	 already been loaded as filename might have been transformed
	 via the search directory lookup mechanism.  */
      const char * filename = search->local_sym_name;

      if (filename == NULL && name == NULL)
	return search;
      if (filename != NULL
	  && name != NULL
	  && strcmp (filename, name) == 0)
	break;
    }

  if (search == NULL)
    search = new_afile (name, lang_input_file_is_search_file_enum, default_target,
			FALSE);

  /* If we have already added this file, or this file is not real
     (FIXME: can that ever actually happen?) or the name is NULL
     (FIXME: can that ever actually happen?) don't add this file.  */
  if (search->loaded
      || ! search->real
      || search->filename == NULL)
    return search;

  if (! load_symbols (search, NULL))
    return NULL;

  return search;
}

/* Get the symbols for an input file.  */

static bfd_boolean
load_symbols (lang_input_statement_type *entry,
	      lang_statement_list_type *place)
{
  char **matching;

  if (entry->loaded)
    return TRUE;

  ldfile_open_file (entry);

  if (! bfd_check_format (entry->the_bfd, bfd_archive)
      && ! bfd_check_format_matches (entry->the_bfd, bfd_object, &matching))
    {
      bfd_error_type err;
      lang_statement_list_type *hold;
      bfd_boolean bad_load = TRUE;
      bfd_boolean save_ldlang_sysrooted_script;

      err = bfd_get_error ();

      /* See if the emulation has some special knowledge.  */
      if (ldemul_unrecognized_file (entry))
	return TRUE;

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
      else
	bad_load = FALSE;

      bfd_close (entry->the_bfd);
      entry->the_bfd = NULL;

      /* Try to interpret the file as a linker script.  */
      ldfile_open_command_file (entry->filename);

      hold = stat_ptr;
      stat_ptr = place;
      save_ldlang_sysrooted_script = ldlang_sysrooted_script;
      ldlang_sysrooted_script = entry->sysrooted;

      ldfile_assumed_script = TRUE;
      parser_input = input_script;
      yyparse ();
      ldfile_assumed_script = FALSE;

      ldlang_sysrooted_script = save_ldlang_sysrooted_script;
      stat_ptr = hold;

      return ! bad_load;
    }

  if (ldemul_recognized_file (entry))
    return TRUE;

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
	  bfd *member = NULL;
	  bfd_boolean loaded = TRUE;

	  for (;;)
	    {
	      member = bfd_openr_next_archived_file (entry->the_bfd, member);

	      if (member == NULL)
		break;

	      if (! bfd_check_format (member, bfd_object))
		{
		  einfo (_("%F%B: member %B in archive is not an object\n"),
			 entry->the_bfd, member);
		  loaded = FALSE;
		}

	      if (! ((*link_info.callbacks->add_archive_element)
		     (&link_info, member, "--whole-archive")))
		abort ();

	      if (! bfd_link_add_symbols (member, &link_info))
		{
		  einfo (_("%F%B: could not read symbols: %E\n"), member);
		  loaded = FALSE;
		}
	    }

	  entry->loaded = loaded;
	  return loaded;
	}
      break;
    }

  if (bfd_link_add_symbols (entry->the_bfd, &link_info))
    entry->loaded = TRUE;
  else
    einfo (_("%F%B: could not read symbols: %E\n"), entry->the_bfd);

  return entry->loaded;
}

/* Handle a wild statement.  S->FILENAME or S->SECTION_LIST or both
   may be NULL, indicating that it is a wildcard.  Separate
   lang_input_section statements are created for each part of the
   expansion; they are added after the wild statement S.  OUTPUT is
   the output section.  */

static void
wild (lang_wild_statement_type *s,
      const char *target ATTRIBUTE_UNUSED,
      lang_output_section_statement_type *output)
{
  struct wildcard_list *sec;

  walk_wild (s, output_section_callback, output);

  for (sec = s->section_list; sec != NULL; sec = sec->next)
    {
      if (default_common_section != NULL)
	break;
      if (sec->spec.name != NULL && strcmp (sec->spec.name, "COMMON") == 0)
	{
	  /* Remember the section that common is going to in case we
	     later get something which doesn't know where to put it.  */
	  default_common_section = output;
	}
    }
}

/* Return TRUE iff target is the sought target.  */

static int
get_target (const bfd_target *target, void *data)
{
  const char *sought = data;

  return strcmp (target->name, sought) == 0;
}

/* Like strcpy() but convert to lower case as well.  */

static void
stricpy (char *dest, char *src)
{
  char c;

  while ((c = *src++) != 0)
    *dest++ = TOLOWER (c);

  *dest = 0;
}

/* Remove the first occurrence of needle (if any) in haystack
   from haystack.  */

static void
strcut (char *haystack, char *needle)
{
  haystack = strstr (haystack, needle);

  if (haystack)
    {
      char *src;

      for (src = haystack + strlen (needle); *src;)
	*haystack++ = *src++;

      *haystack = 0;
    }
}

/* Compare two target format name strings.
   Return a value indicating how "similar" they are.  */

static int
name_compare (char *first, char *second)
{
  char *copy1;
  char *copy2;
  int result;

  copy1 = xmalloc (strlen (first) + 1);
  copy2 = xmalloc (strlen (second) + 1);

  /* Convert the names to lower case.  */
  stricpy (copy1, first);
  stricpy (copy2, second);

  /* Remove size and endian strings from the name.  */
  strcut (copy1, "big");
  strcut (copy1, "little");
  strcut (copy2, "big");
  strcut (copy2, "little");

  /* Return a value based on how many characters match,
     starting from the beginning.   If both strings are
     the same then return 10 * their length.  */
  for (result = 0; copy1[result] == copy2[result]; result++)
    if (copy1[result] == 0)
      {
	result *= 10;
	break;
      }

  free (copy1);
  free (copy2);

  return result;
}

/* Set by closest_target_match() below.  */
static const bfd_target *winner;

/* Scan all the valid bfd targets looking for one that has the endianness
   requirement that was specified on the command line, and is the nearest
   match to the original output target.  */

static int
closest_target_match (const bfd_target *target, void *data)
{
  const bfd_target *original = data;

  if (command_line.endian == ENDIAN_BIG
      && target->byteorder != BFD_ENDIAN_BIG)
    return 0;

  if (command_line.endian == ENDIAN_LITTLE
      && target->byteorder != BFD_ENDIAN_LITTLE)
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
     Compare their names and choose the better one.  */
  if (name_compare (target->name, original->name)
      > name_compare (winner->name, original->name))
    winner = target;

  /* Keep on searching until wqe have checked them all.  */
  return 0;
}

/* Return the BFD target format of the first input file.  */

static char *
get_first_input_target (void)
{
  char *target = NULL;

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

const char *
lang_get_output_target (void)
{
  const char *target;

  /* Has the user told us which output format to use?  */
  if (output_target != NULL)
    return output_target;

  /* No - has the current target been set to something other than
     the default?  */
  if (current_target != default_target)
    return current_target;

  /* No - can we determine the format of the first input file?  */
  target = get_first_input_target ();
  if (target != NULL)
    return target;

  /* Failed - use the default output target.  */
  return default_target;
}

/* Open the output file.  */

static bfd *
open_output (const char *name)
{
  bfd *output;

  output_target = lang_get_output_target ();

  /* Has the user requested a particular endianness on the command
     line?  */
  if (command_line.endian != ENDIAN_UNSET)
    {
      const bfd_target *target;
      enum bfd_endian desired_endian;

      /* Get the chosen target.  */
      target = bfd_search_for_target (get_target, (void *) output_target);

      /* If the target is not supported, we cannot do anything.  */
      if (target != NULL)
	{
	  if (command_line.endian == ENDIAN_BIG)
	    desired_endian = BFD_ENDIAN_BIG;
	  else
	    desired_endian = BFD_ENDIAN_LITTLE;

	  /* See if the target has the wrong endianness.  This should
	     not happen if the linker script has provided big and
	     little endian alternatives, but some scrips don't do
	     this.  */
	  if (target->byteorder != desired_endian)
	    {
	      /* If it does, then see if the target provides
		 an alternative with the correct endianness.  */
	      if (target->alternative_target != NULL
		  && (target->alternative_target->byteorder == desired_endian))
		output_target = target->alternative_target->name;
	      else
		{
		  /* Try to find a target as similar as possible to
		     the default target, but which has the desired
		     endian characteristic.  */
		  bfd_search_for_target (closest_target_match,
					 (void *) target);

		  /* Oh dear - we could not find any targets that
		     satisfy our requirements.  */
		  if (winner == NULL)
		    einfo (_("%P: warning: could not find any targets that match endianness requirement\n"));
		  else
		    output_target = winner->name;
		}
	    }
	}
    }

  output = bfd_openw (name, output_target);

  if (output == NULL)
    {
      if (bfd_get_error () == bfd_error_invalid_target)
	einfo (_("%P%F: target %s not found\n"), output_target);

      einfo (_("%P%F: cannot open output file %s: %E\n"), name);
    }

  delete_output_file_on_failure = TRUE;

#if 0
  output->flags |= D_PAGED;
#endif

  if (! bfd_set_format (output, bfd_object))
    einfo (_("%P%F:%s: can not make object file: %E\n"), name);
  if (! bfd_set_arch_mach (output,
			   ldfile_output_architecture,
			   ldfile_output_machine))
    einfo (_("%P%F:%s: can not set architecture: %E\n"), name);

  link_info.hash = bfd_link_hash_table_create (output);
  if (link_info.hash == NULL)
    einfo (_("%P%F: can not create link hash table: %E\n"));

  bfd_set_gp_size (output, g_switch_value);
  return output;
}

static void
ldlang_open_output (lang_statement_union_type *statement)
{
  switch (statement->header.type)
    {
    case lang_output_statement_enum:
      ASSERT (output_bfd == NULL);
      output_bfd = open_output (statement->output_statement.name);
      ldemul_set_output_arch ();
      if (config.magic_demand_paged && !link_info.relocatable)
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

/* Convert between addresses in bytes and sizes in octets.
   For currently supported targets, octets_per_byte is always a power
   of two, so we can use shifts.  */
#define TO_ADDR(X) ((X) >> opb_shift)
#define TO_SIZE(X) ((X) << opb_shift)

/* Support the above.  */
static unsigned int opb_shift = 0;

static void
init_opb (void)
{
  unsigned x = bfd_arch_mach_octets_per_byte (ldfile_output_architecture,
					      ldfile_output_machine);
  opb_shift = 0;
  if (x > 1)
    while ((x & 1) == 0)
      {
	x >>= 1;
	++opb_shift;
      }
  ASSERT (x == 1);
}

/* Open all the input files.  */

static void
open_input_bfds (lang_statement_union_type *s, bfd_boolean force)
{
  for (; s != NULL; s = s->header.next)
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
	  /* Maybe we should load the file's symbols.  */
	  if (s->wild_statement.filename
	      && ! wildcardp (s->wild_statement.filename))
	    lookup_name (s->wild_statement.filename);
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
		open_input_bfds (s->group_statement.children.head, TRUE);
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
                 force it to be researched unless the whole archive
		 has been loaded already.  */
	      if (force
		  && !s->input_statement.whole_archive
		  && s->input_statement.loaded
		  && bfd_check_format (s->input_statement.the_bfd,
				       bfd_archive))
		s->input_statement.loaded = FALSE;

	      lang_list_init (&add);

	      if (! load_symbols (&s->input_statement, &add))
		config.make_executable = FALSE;

	      if (add.head != NULL)
		{
		  *add.tail = s->header.next;
		  s->header.next = add.head;
		}
	    }
	  break;
	default:
	  break;
	}
    }
}

/* If there are [COMMONS] statements, put a wild one into the bss
   section.  */

static void
lang_reasonable_defaults (void)
{
#if 0
  lang_output_section_statement_lookup (".text");
  lang_output_section_statement_lookup (".data");

  default_common_section = lang_output_section_statement_lookup (".bss");

  if (!placed_commons)
    {
      lang_wild_statement_type *new =
      new_stat (lang_wild_statement,
		&default_common_section->children);

      new->section_name = "COMMON";
      new->filename = NULL;
      lang_list_init (&new->children);
    }
#endif
}

/* Add a symbol to a hash of symbols used in DEFINED (NAME) expressions.  */

void
lang_track_definedness (const char *name)
{
  if (bfd_hash_lookup (&lang_definedness_table, name, TRUE, FALSE) == NULL)
    einfo (_("%P%F: bfd_hash_lookup failed creating symbol %s\n"), name);
}

/* New-function for the definedness hash table.  */

static struct bfd_hash_entry *
lang_definedness_newfunc (struct bfd_hash_entry *entry,
			  struct bfd_hash_table *table ATTRIBUTE_UNUSED,
			  const char *name ATTRIBUTE_UNUSED)
{
  struct lang_definedness_hash_entry *ret
    = (struct lang_definedness_hash_entry *) entry;

  if (ret == NULL)
    ret = (struct lang_definedness_hash_entry *)
      bfd_hash_allocate (table, sizeof (struct lang_definedness_hash_entry));

  if (ret == NULL)
    einfo (_("%P%F: bfd_hash_allocate failed creating symbol %s\n"), name);

  ret->iteration = -1;
  return &ret->root;
}

/* Return the iteration when the definition of NAME was last updated.  A
   value of -1 means that the symbol is not defined in the linker script
   or the command line, but may be defined in the linker symbol table.  */

int
lang_symbol_definition_iteration (const char *name)
{
  struct lang_definedness_hash_entry *defentry
    = (struct lang_definedness_hash_entry *)
    bfd_hash_lookup (&lang_definedness_table, name, FALSE, FALSE);

  /* We've already created this one on the presence of DEFINED in the
     script, so it can't be NULL unless something is borked elsewhere in
     the code.  */
  if (defentry == NULL)
    FAIL ();

  return defentry->iteration;
}

/* Update the definedness state of NAME.  */

void
lang_update_definedness (const char *name, struct bfd_link_hash_entry *h)
{
  struct lang_definedness_hash_entry *defentry
    = (struct lang_definedness_hash_entry *)
    bfd_hash_lookup (&lang_definedness_table, name, FALSE, FALSE);

  /* We don't keep track of symbols not tested with DEFINED.  */
  if (defentry == NULL)
    return;

  /* If the symbol was already defined, and not from an earlier statement
     iteration, don't update the definedness iteration, because that'd
     make the symbol seem defined in the linker script at this point, and
     it wasn't; it was defined in some object.  If we do anyway, DEFINED
     would start to yield false before this point and the construct "sym =
     DEFINED (sym) ? sym : X;" would change sym to X despite being defined
     in an object.  */
  if (h->type != bfd_link_hash_undefined
      && h->type != bfd_link_hash_common
      && h->type != bfd_link_hash_new
      && defentry->iteration == -1)
    return;

  defentry->iteration = lang_statement_iteration;
}

/* Add the supplied name to the symbol table as an undefined reference.
   This is a two step process as the symbol table doesn't even exist at
   the time the ld command line is processed.  First we put the name
   on a list, then, once the output file has been opened, transfer the
   name to the symbol table.  */

typedef struct bfd_sym_chain ldlang_undef_chain_list_type;

#define ldlang_undef_chain_list_head entry_symbol.next

void
ldlang_add_undef (const char *const name)
{
  ldlang_undef_chain_list_type *new =
    stat_alloc (sizeof (ldlang_undef_chain_list_type));

  new->next = ldlang_undef_chain_list_head;
  ldlang_undef_chain_list_head = new;

  new->name = xstrdup (name);

  if (output_bfd != NULL)
    insert_undefined (new->name);
}

/* Insert NAME as undefined in the symbol table.  */

static void
insert_undefined (const char *name)
{
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (link_info.hash, name, TRUE, FALSE, TRUE);
  if (h == NULL)
    einfo (_("%P%F: bfd_link_hash_lookup failed: %E\n"));
  if (h->type == bfd_link_hash_new)
    {
      h->type = bfd_link_hash_undefined;
      h->u.undef.abfd = NULL;
      bfd_link_add_undef (link_info.hash, h);
    }
}

/* Run through the list of undefineds created above and place them
   into the linker hash table as undefined symbols belonging to the
   script file.  */

static void
lang_place_undefineds (void)
{
  ldlang_undef_chain_list_type *ptr;

  for (ptr = ldlang_undef_chain_list_head; ptr != NULL; ptr = ptr->next)
    insert_undefined (ptr->name);
}

/* Open input files and attach to output sections.  */

static void
map_input_to_output_sections
  (lang_statement_union_type *s, const char *target,
   lang_output_section_statement_type *output_section_statement)
{
  for (; s != NULL; s = s->header.next)
    {
      switch (s->header.type)
	{
	case lang_wild_statement_enum:
	  wild (&s->wild_statement, target, output_section_statement);
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
	case lang_data_statement_enum:
	  /* Make sure that any sections mentioned in the expression
	     are initialized.  */
	  exp_init_os (s->data_statement.exp);
	  /* FALLTHROUGH */
	case lang_fill_statement_enum:
	case lang_input_section_enum:
	case lang_object_symbols_statement_enum:
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
	  /* Mark the specified section with the supplied address.  */
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

/* An output section might have been removed after its statement was
   added.  For example, ldemul_before_allocation can remove dynamic
   sections if they turn out to be not needed.  Clean them up here.  */

static void
strip_excluded_output_sections (void)
{
  lang_statement_union_type *u;

  for (u = lang_output_section_statement.head;
       u != NULL;
       u = u->output_section_statement.next)
    {
      lang_output_section_statement_type *os;
      asection *s;

      os = &u->output_section_statement;
      s = os->bfd_section;
      if (s != NULL && (s->flags & SEC_EXCLUDE) != 0)
	{
	  asection **p;

	  os->bfd_section = NULL;

	  for (p = &output_bfd->sections; *p; p = &(*p)->next)
	    if (*p == s)
	      {
		bfd_section_list_remove (output_bfd, p);
		output_bfd->section_count--;
		break;
	      }
	}
    }
}

static void
print_output_section_statement
  (lang_output_section_statement_type *output_section_statement)
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
print_assignment (lang_assignment_statement_type *assignment,
		  lang_output_section_statement_type *output_section)
{
  int i;
  etree_value_type result;

  for (i = 0; i < SECTION_NAME_MAP_LENGTH; i++)
    print_space ();

  result = exp_fold_tree (assignment->exp->assign.src, output_section,
			  lang_final_phase_enum, print_dot, &print_dot);
  if (result.valid_p)
    {
      const char *dst;
      bfd_vma value;

      value = result.value + result.section->bfd_section->vma;
      dst = assignment->exp->assign.dst;

      minfo ("0x%V", value);
      if (dst[0] == '.' && dst[1] == 0)
	print_dot = value;
    }
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
print_input_statement (lang_input_statement_type *statm)
{
  if (statm->filename != NULL)
    {
      fprintf (config.map_file, "LOAD %s\n", statm->filename);
    }
}

/* Print all symbols defined in a particular section.  This is called
   via bfd_link_hash_traverse.  */

static bfd_boolean
print_one_symbol (struct bfd_link_hash_entry *hash_entry, void *ptr)
{
  asection *sec = ptr;

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

  return TRUE;
}

/* Print information about an input section to the map file.  */

static void
print_input_section (lang_input_section_type *in)
{
  asection *i = in->section;
  bfd_size_type size = i->_cooked_size != 0 ? i->_cooked_size : i->_raw_size;

  init_opb ();
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
		 i->output_section->vma + i->output_offset, TO_ADDR (size),
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

	  bfd_link_hash_traverse (link_info.hash, print_one_symbol, i);

	  print_dot = (i->output_section->vma + i->output_offset
		       + TO_ADDR (size));
	}
    }
}

static void
print_fill_statement (lang_fill_statement_type *fill)
{
  size_t size;
  unsigned char *p;
  fputs (" FILL mask 0x", config.map_file);
  for (p = fill->fill->data, size = fill->fill->size; size != 0; p++, size--)
    fprintf (config.map_file, "%02x", *p);
  fputs ("\n", config.map_file);
}

static void
print_data_statement (lang_data_statement_type *data)
{
  int i;
  bfd_vma addr;
  bfd_size_type size;
  const char *name;

  init_opb ();
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

  print_dot = addr + TO_ADDR (size);
}

/* Print an address statement.  These are generated by options like
   -Ttext.  */

static void
print_address_statement (lang_address_statement_type *address)
{
  minfo (_("Address of section %s set to "), address->section_name);
  exp_print_tree (address->address);
  print_nl ();
}

/* Print a reloc statement.  */

static void
print_reloc_statement (lang_reloc_statement_type *reloc)
{
  int i;
  bfd_vma addr;
  bfd_size_type size;

  init_opb ();
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

  print_dot = addr + TO_ADDR (size);
}

static void
print_padding_statement (lang_padding_statement_type *s)
{
  int len;
  bfd_vma addr;

  init_opb ();
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
  minfo ("0x%V %W ", addr, s->size);

  if (s->fill->size != 0)
    {
      size_t size;
      unsigned char *p;
      for (p = s->fill->data, size = s->fill->size; size != 0; p++, size--)
	fprintf (config.map_file, "%02x", *p);
    }

  print_nl ();

  print_dot = addr + TO_ADDR (s->size);
}

static void
print_wild_statement (lang_wild_statement_type *w,
		      lang_output_section_statement_type *os)
{
  struct wildcard_list *sec;

  print_space ();

  if (w->filenames_sorted)
    minfo ("SORT(");
  if (w->filename != NULL)
    minfo ("%s", w->filename);
  else
    minfo ("*");
  if (w->filenames_sorted)
    minfo (")");

  minfo ("(");
  for (sec = w->section_list; sec; sec = sec->next)
    {
      if (sec->spec.sorted)
	minfo ("SORT(");
      if (sec->spec.exclude_name_list != NULL)
	{
	  name_list *tmp;
	  minfo ("EXCLUDE_FILE(%s", sec->spec.exclude_name_list->name);
	  for (tmp = sec->spec.exclude_name_list->next; tmp; tmp = tmp->next)
	    minfo (" %s", tmp->name);
	  minfo (") ");
	}
      if (sec->spec.name != NULL)
	minfo ("%s", sec->spec.name);
      else
	minfo ("*");
      if (sec->spec.sorted)
	minfo (")");
      if (sec->next)
	minfo (" ");
    }
  minfo (")");

  print_nl ();

  print_statement_list (w->children.head, os);
}

/* Print a group statement.  */

static void
print_group (lang_group_statement_type *s,
	     lang_output_section_statement_type *os)
{
  fprintf (config.map_file, "START GROUP\n");
  print_statement_list (s->children.head, os);
  fprintf (config.map_file, "END GROUP\n");
}

/* Print the list of statements in S.
   This can be called for any statement type.  */

static void
print_statement_list (lang_statement_union_type *s,
		      lang_output_section_statement_type *os)
{
  while (s != NULL)
    {
      print_statement (s, os);
      s = s->header.next;
    }
}

/* Print the first statement in statement list S.
   This can be called for any statement type.  */

static void
print_statement (lang_statement_union_type *s,
		 lang_output_section_statement_type *os)
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
print_statements (void)
{
  print_statement_list (statement_list.head, abs_output_section);
}

/* Print the first N statements in statement list S to STDERR.
   If N == 0, nothing is printed.
   If N < 0, the entire list is printed.
   Intended to be called from GDB.  */

void
dprint_statement (lang_statement_union_type *s, int n)
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
	  s = s->header.next;
	}
    }

  config.map_file = map_save;
}

static void
insert_pad (lang_statement_union_type **ptr,
	    fill_type *fill,
	    unsigned int alignment_needed,
	    asection *output_section,
	    bfd_vma dot)
{
  static fill_type zero_fill = { 1, { 0 } };
  lang_statement_union_type *pad;

  pad = ((lang_statement_union_type *)
	 ((char *) ptr - offsetof (lang_statement_union_type, header.next)));
  if (ptr != &statement_list.head
      && pad->header.type == lang_padding_statement_enum
      && pad->padding_statement.output_section == output_section)
    {
      /* Use the existing pad statement.  The above test on output
	 section is probably redundant, but it doesn't hurt to check.  */
    }
  else
    {
      /* Make a new padding statement, linked into existing chain.  */
      pad = stat_alloc (sizeof (lang_padding_statement_type));
      pad->header.next = *ptr;
      *ptr = pad;
      pad->header.type = lang_padding_statement_enum;
      pad->padding_statement.output_section = output_section;
      if (fill == NULL)
	fill = &zero_fill;
      pad->padding_statement.fill = fill;
    }
  pad->padding_statement.output_offset = dot - output_section->vma;
  pad->padding_statement.size = alignment_needed;
  output_section->_raw_size += alignment_needed;
}

/* Work out how much this section will move the dot point.  */

static bfd_vma
size_input_section (lang_statement_union_type **this_ptr,
		    lang_output_section_statement_type *output_section_statement,
		    fill_type *fill,
		    bfd_vma dot)
{
  lang_input_section_type *is = &((*this_ptr)->input_section);
  asection *i = is->section;

  if (!is->ifile->just_syms_flag)
    {
      unsigned int alignment_needed;
      asection *o;

      /* Align this section first to the input sections requirement,
	 then to the output section's requirement.  If this alignment
	 is greater than any seen before, then record it too.  Perform
	 the alignment by inserting a magic 'padding' statement.  */

      if (output_section_statement->subsection_alignment != -1)
	i->alignment_power = output_section_statement->subsection_alignment;

      o = output_section_statement->bfd_section;
      if (o->alignment_power < i->alignment_power)
	o->alignment_power = i->alignment_power;

      alignment_needed = align_power (dot, i->alignment_power) - dot;

      if (alignment_needed != 0)
	{
	  insert_pad (this_ptr, fill, TO_SIZE (alignment_needed), o, dot);
	  dot += alignment_needed;
	}

      /* Remember where in the output section this input section goes.  */

      i->output_offset = dot - o->vma;

      /* Mark how big the output section must be to contain this now.  */
      if (i->_cooked_size != 0)
	dot += TO_ADDR (i->_cooked_size);
      else
	dot += TO_ADDR (i->_raw_size);
      o->_raw_size = TO_SIZE (dot - o->vma);
    }
  else
    {
      i->output_offset = i->vma - output_section_statement->bfd_section->vma;
    }

  return dot;
}

#define IGNORE_SECTION(bfd, s) \
  (((bfd_get_section_flags (bfd, s) & SEC_THREAD_LOCAL)			\
    ? ((bfd_get_section_flags (bfd, s) & (SEC_LOAD | SEC_NEVER_LOAD))	\
       != SEC_LOAD)							\
    :  ((bfd_get_section_flags (bfd, s) & (SEC_ALLOC | SEC_NEVER_LOAD)) \
	!= SEC_ALLOC))							\
   || bfd_section_size (bfd, s) == 0)

/* Check to see if any allocated sections overlap with other allocated
   sections.  This can happen when the linker script specifically specifies
   the output section addresses of the two sections.  */

static void
lang_check_section_addresses (void)
{
  asection *s;

  /* Scan all sections in the output list.  */
  for (s = output_bfd->sections; s != NULL; s = s->next)
    {
      asection *os;

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
	  s_start = bfd_section_lma (output_bfd, s);
	  os_start = bfd_section_lma (output_bfd, os);
	  s_end = s_start + TO_ADDR (bfd_section_size (output_bfd, s)) - 1;
	  os_end = os_start + TO_ADDR (bfd_section_size (output_bfd, os)) - 1;

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

/* Make sure the new address is within the region.  We explicitly permit the
   current address to be at the exact end of the region when the address is
   non-zero, in case the region is at the end of addressable memory and the
   calculation wraps around.  */

static void
os_region_check (lang_output_section_statement_type *os,
		 lang_memory_region_type *region,
		 etree_type *tree,
		 bfd_vma base)
{
  if ((region->current < region->origin
       || (region->current - region->origin > region->length))
      && ((region->current != region->origin + region->length)
	  || base == 0))
    {
      if (tree != NULL)
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

static bfd_vma
lang_size_sections_1
  (lang_statement_union_type *s,
   lang_output_section_statement_type *output_section_statement,
   lang_statement_union_type **prev,
   fill_type *fill,
   bfd_vma dot,
   bfd_boolean *relax,
   bfd_boolean check_regions)
{
  /* Size up the sections from their constituent parts.  */
  for (; s != NULL; s = s->header.next)
    {
      switch (s->header.type)
	{
	case lang_output_section_statement_enum:
	  {
	    bfd_vma after;
	    lang_output_section_statement_type *os;

	    os = &s->output_section_statement;
	    if (os->bfd_section == NULL)
	      /* This section was never actually created.  */
	      break;

	    /* If this is a COFF shared library section, use the size and
	       address from the input section.  FIXME: This is COFF
	       specific; it would be cleaner if there were some other way
	       to do this, but nothing simple comes to mind.  */
	    if ((os->bfd_section->flags & SEC_COFF_SHARED_LIBRARY) != 0)
	      {
		asection *input;

		if (os->children.head == NULL
		    || os->children.head->header.next != NULL
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
		if (os->addr_tree == NULL)
		  {
		    /* No address specified for this section, get one
		       from the region specification.  */
		    if (os->region == NULL
			|| (((bfd_get_section_flags (output_bfd, os->bfd_section)
			      & (SEC_ALLOC | SEC_LOAD)) != 0)
			    && os->region->name[0] == '*'
			    && strcmp (os->region->name, DEFAULT_MEMORY_REGION) == 0))
		      {
			os->region = lang_memory_default (os->bfd_section);
		      }

		    /* If a loadable section is using the default memory
		       region, and some non default memory regions were
		       defined, issue an error message.  */
		    if (!IGNORE_SECTION (output_bfd, os->bfd_section)
			&& ! link_info.relocatable
			&& check_regions
			&& strcmp (os->region->name, DEFAULT_MEMORY_REGION) == 0
			&& lang_memory_region_list != NULL
			&& (strcmp (lang_memory_region_list->name,
				    DEFAULT_MEMORY_REGION) != 0
			    || lang_memory_region_list->next != NULL))
		      {
			/* By default this is an error rather than just a
			   warning because if we allocate the section to the
			   default memory region we can end up creating an
			   excessively large binary, or even seg faulting when
			   attempting to perform a negative seek.  See
			     http://sources.redhat.com/ml/binutils/2003-04/msg00423.html
			   for an example of this.  This behaviour can be
			   overridden by the using the --no-check-sections
			   switch.  */
			if (command_line.check_section_addresses)
			  einfo (_("%P%F: error: no memory region specified for loadable section `%s'\n"),
				 bfd_get_section_name (output_bfd,
						       os->bfd_section));
			else
			  einfo (_("%P: warning: no memory region specified for loadable section `%s'\n"),
				 bfd_get_section_name (output_bfd,
						       os->bfd_section));
		      }

		    dot = os->region->current;

		    if (os->section_alignment == -1)
		      {
			bfd_vma olddot;

			olddot = dot;
			dot = align_power (dot,
					   os->bfd_section->alignment_power);

			if (dot != olddot && config.warn_section_align)
			  einfo (_("%P: warning: changing start of section %s by %u bytes\n"),
				 os->name, (unsigned int) (dot - olddot));
		      }
		  }
		else
		  {
		    etree_value_type r;

		    os->processed = -1;
		    r = exp_fold_tree (os->addr_tree,
				       abs_output_section,
				       lang_allocating_phase_enum,
				       dot, &dot);
		    os->processed = 0;
		    
		    if (!r.valid_p)
		      einfo (_("%F%S: non constant or forward reference address expression for section %s\n"),
			     os->name);

		    dot = r.value + r.section->bfd_section->vma;
		  }

		/* The section starts here.
		   First, align to what the section needs.  */

		if (os->section_alignment != -1)
		  dot = align_power (dot, os->section_alignment);

		bfd_set_section_vma (0, os->bfd_section, dot);

		os->bfd_section->output_offset = 0;
	      }

	    lang_size_sections_1 (os->children.head, os, &os->children.head,
				  os->fill, dot, relax, check_regions);

	    /* Put the section within the requested block size, or
	       align at the block boundary.  */
	    after = ((os->bfd_section->vma
		      + TO_ADDR (os->bfd_section->_raw_size)
		      + os->block_value - 1)
		     & - (bfd_vma) os->block_value);

	    if (bfd_is_abs_section (os->bfd_section))
	      ASSERT (after == os->bfd_section->vma);
	    else
	      os->bfd_section->_raw_size
		= TO_SIZE (after - os->bfd_section->vma);

	    dot = os->bfd_section->vma;
	    /* .tbss sections effectively have zero size.  */
	    if ((os->bfd_section->flags & SEC_HAS_CONTENTS) != 0
		|| (os->bfd_section->flags & SEC_THREAD_LOCAL) == 0
		|| link_info.relocatable)
	      dot += TO_ADDR (os->bfd_section->_raw_size);

	    os->processed = 1;

	    if (os->update_dot_tree != 0)
	      exp_fold_tree (os->update_dot_tree, abs_output_section,
			     lang_allocating_phase_enum, dot, &dot);

	    /* Update dot in the region ?
	       We only do this if the section is going to be allocated,
	       since unallocated sections do not contribute to the region's
	       overall size in memory.

	       If the SEC_NEVER_LOAD bit is not set, it will affect the
	       addresses of sections after it. We have to update
	       dot.  */
	    if (os->region != NULL
		&& ((bfd_get_section_flags (output_bfd, os->bfd_section)
		     & SEC_NEVER_LOAD) == 0
		    || (bfd_get_section_flags (output_bfd, os->bfd_section)
			& (SEC_ALLOC | SEC_LOAD))))
	      {
		os->region->current = dot;

		if (check_regions)
		  /* Make sure the new address is within the region.  */
		  os_region_check (os, os->region, os->addr_tree,
				   os->bfd_section->vma);

		/* If there's no load address specified, use the run
		   region as the load region.  */
		if (os->lma_region == NULL && os->load_base == NULL)
		  os->lma_region = os->region;

		if (os->lma_region != NULL && os->lma_region != os->region)
		  {
		    /* Set load_base, which will be handled later.  */
		    os->load_base = exp_intop (os->lma_region->current);
		    os->lma_region->current +=
		      TO_ADDR (os->bfd_section->_raw_size);
		    if (check_regions)
		      os_region_check (os, os->lma_region, NULL,
				       os->bfd_section->lma);
		  }
	      }
	  }
	  break;

	case lang_constructors_statement_enum:
	  dot = lang_size_sections_1 (constructor_list.head,
				      output_section_statement,
				      &s->wild_statement.children.head,
				      fill, dot, relax, check_regions);
	  break;

	case lang_data_statement_enum:
	  {
	    unsigned int size = 0;

	    s->data_statement.output_vma =
	      dot - output_section_statement->bfd_section->vma;
	    s->data_statement.output_section =
	      output_section_statement->bfd_section;

	    /* We might refer to provided symbols in the expression, and
	       need to mark them as needed.  */
	    exp_fold_tree (s->data_statement.exp, abs_output_section,
			   lang_allocating_phase_enum, dot, &dot);

	    switch (s->data_statement.type)
	      {
	      default:
		abort ();
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
	    if (size < TO_SIZE ((unsigned) 1))
	      size = TO_SIZE ((unsigned) 1);
	    dot += TO_ADDR (size);
	    output_section_statement->bfd_section->_raw_size += size;
	    /* The output section gets contents, and then we inspect for
	       any flags set in the input script which override any ALLOC.  */
	    output_section_statement->bfd_section->flags |= SEC_HAS_CONTENTS;
	    if (!(output_section_statement->flags & SEC_NEVER_LOAD))
	      {
		output_section_statement->bfd_section->flags |=
		  SEC_ALLOC | SEC_LOAD;
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
	    dot += TO_ADDR (size);
	    output_section_statement->bfd_section->_raw_size += size;
	  }
	  break;

	case lang_wild_statement_enum:

	  dot = lang_size_sections_1 (s->wild_statement.children.head,
				      output_section_statement,
				      &s->wild_statement.children.head,
				      fill, dot, relax, check_regions);

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
		bfd_boolean again;

		if (! bfd_relax_section (i->owner, i, &link_info, &again))
		  einfo (_("%P%F: can't relax section: %E\n"));
		if (again)
		  *relax = TRUE;
	      }
	    dot = size_input_section (prev, output_section_statement,
				      output_section_statement->fill, dot);
	  }
	  break;
	case lang_input_statement_enum:
	  break;
	case lang_fill_statement_enum:
	  s->fill_statement.output_section =
	    output_section_statement->bfd_section;

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
		if (output_section_statement == abs_output_section)
		  {
		    /* If we don't have an output section, then just adjust
		       the default memory address.  */
		    lang_memory_region_lookup (DEFAULT_MEMORY_REGION, FALSE)->current = newdot;
		  }
		else
		  {
		    /* Insert a pad after this statement.  We can't
		       put the pad before when relaxing, in case the
		       assignment references dot.  */
		    insert_pad (&s->header.next, fill, TO_SIZE (newdot - dot),
				output_section_statement->bfd_section, dot);

		    /* Don't neuter the pad below when relaxing.  */
		    s = s->header.next;
		  }

		/* If dot is advanced, this implies that the section should
		   have space allocated to it, unless the user has explicitly
		   stated that the section should never be loaded.  */
		if (!(output_section_statement->flags & (SEC_NEVER_LOAD | SEC_ALLOC)))
		  output_section_statement->bfd_section->flags |= SEC_ALLOC;

		dot = newdot;
	      }
	  }
	  break;

	case lang_padding_statement_enum:
	  /* If this is the first time lang_size_sections is called,
	     we won't have any padding statements.  If this is the
	     second or later passes when relaxing, we should allow
	     padding to shrink.  If padding is needed on this pass, it
	     will be added back in.  */
	  s->padding_statement.size = 0;

	  /* Make sure output_offset is valid.  If relaxation shrinks
	     the section and this pad isn't needed, it's possible to
	     have output_offset larger than the final size of the
	     section.  bfd_set_section_contents will complain even for
	     a pad size of zero.  */
	  s->padding_statement.output_offset
	    = dot - output_section_statement->bfd_section->vma;
	  break;

	case lang_group_statement_enum:
	  dot = lang_size_sections_1 (s->group_statement.children.head,
				      output_section_statement,
				      &s->group_statement.children.head,
				      fill, dot, relax, check_regions);
	  break;

	default:
	  FAIL ();
	  break;

	  /* We can only get here when relaxing is turned on.  */
	case lang_address_statement_enum:
	  break;
	}
      prev = &s->header.next;
    }
  return dot;
}

bfd_vma
lang_size_sections
  (lang_statement_union_type *s,
   lang_output_section_statement_type *output_section_statement,
   lang_statement_union_type **prev,
   fill_type *fill,
   bfd_vma dot,
   bfd_boolean *relax,
   bfd_boolean check_regions)
{
  bfd_vma result;
  asection *o;

  /* Callers of exp_fold_tree need to increment this.  */
  lang_statement_iteration++;

  exp_data_seg.phase = exp_dataseg_none;
  result = lang_size_sections_1 (s, output_section_statement, prev, fill,
				 dot, relax, check_regions);
  if (exp_data_seg.phase == exp_dataseg_end_seen)
    {
      /* If DATA_SEGMENT_ALIGN DATA_SEGMENT_END pair was seen, check whether
	 a page could be saved in the data segment.  */
      bfd_vma first, last;

      first = -exp_data_seg.base & (exp_data_seg.pagesize - 1);
      last = exp_data_seg.end & (exp_data_seg.pagesize - 1);
      if (first && last
	  && ((exp_data_seg.base & ~(exp_data_seg.pagesize - 1))
	      != (exp_data_seg.end & ~(exp_data_seg.pagesize - 1)))
	  && first + last <= exp_data_seg.pagesize)
	{
	  exp_data_seg.phase = exp_dataseg_adjust;
	  lang_statement_iteration++;
	  result = lang_size_sections_1 (s, output_section_statement, prev,
					 fill, dot, relax, check_regions);
	}
    }

  /* Some backend relaxers want to refer to the output section size.  Give
     them a section size that does not change on the next call while they
     relax.  We can't set this at top because lang_reset_memory_regions
     which is called before we get here, sets _raw_size to 0 on relaxing
     rounds.  */
  for (o = output_bfd->sections; o != NULL; o = o->next)
    o->_cooked_size = o->_raw_size;

  return result;
}

/* Worker function for lang_do_assignments.  Recursiveness goes here.  */

static bfd_vma
lang_do_assignments_1
  (lang_statement_union_type *s,
   lang_output_section_statement_type *output_section_statement,
   fill_type *fill,
   bfd_vma dot)
{
  for (; s != NULL; s = s->header.next)
    {
      switch (s->header.type)
	{
	case lang_constructors_statement_enum:
	  dot = lang_do_assignments_1 (constructor_list.head,
				       output_section_statement,
				       fill,
				       dot);
	  break;

	case lang_output_section_statement_enum:
	  {
	    lang_output_section_statement_type *os;

	    os = &(s->output_section_statement);
	    if (os->bfd_section != NULL)
	      {
		dot = os->bfd_section->vma;
		lang_do_assignments_1 (os->children.head, os, os->fill, dot);
		dot = (os->bfd_section->vma
		       + TO_ADDR (os->bfd_section->_raw_size));

	      }
	    if (os->load_base)
	      {
		/* If nothing has been placed into the output section then
		   it won't have a bfd_section.  */
		if (os->bfd_section)
		  {
		    os->bfd_section->lma
		      = exp_get_abs_int (os->load_base, 0, "load base",
					 lang_final_phase_enum);
		  }
	      }
	  }
	  break;
	case lang_wild_statement_enum:

	  dot = lang_do_assignments_1 (s->wild_statement.children.head,
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
	    if (!value.valid_p)
	      einfo (_("%F%P: invalid data statement\n"));
	    s->data_statement.value
	      = value.value + value.section->bfd_section->vma;
	  }
	  {
	    unsigned int size;
	    switch (s->data_statement.type)
	      {
	      default:
		abort ();
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
	    if (size < TO_SIZE ((unsigned) 1))
	      size = TO_SIZE ((unsigned) 1);
	    dot += TO_ADDR (size);
	  }
	  break;

	case lang_reloc_statement_enum:
	  {
	    etree_value_type value;

	    value = exp_fold_tree (s->reloc_statement.addend_exp,
				   abs_output_section,
				   lang_final_phase_enum, dot, &dot);
	    s->reloc_statement.addend_value = value.value;
	    if (!value.valid_p)
	      einfo (_("%F%P: invalid reloc statement\n"));
	  }
	  dot += TO_ADDR (bfd_get_reloc_size (s->reloc_statement.howto));
	  break;

	case lang_input_section_enum:
	  {
	    asection *in = s->input_section.section;

	    if (in->_cooked_size != 0)
	      dot += TO_ADDR (in->_cooked_size);
	    else
	      dot += TO_ADDR (in->_raw_size);
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
	  dot += TO_ADDR (s->padding_statement.size);
	  break;

	case lang_group_statement_enum:
	  dot = lang_do_assignments_1 (s->group_statement.children.head,
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

void
lang_do_assignments (lang_statement_union_type *s,
		     lang_output_section_statement_type *output_section_statement,
		     fill_type *fill,
		     bfd_vma dot)
{
  /* Callers of exp_fold_tree need to increment this.  */
  lang_statement_iteration++;
  lang_do_assignments_1 (s, output_section_statement, fill, dot);
}

/* Fix any .startof. or .sizeof. symbols.  When the assemblers see the
   operator .startof. (section_name), it produces an undefined symbol
   .startof.section_name.  Similarly, when it sees
   .sizeof. (section_name), it produces an undefined symbol
   .sizeof.section_name.  For all the output sections, we look for
   such symbols, and set them to the correct value.  */

static void
lang_set_startof (void)
{
  asection *s;

  if (link_info.relocatable)
    return;

  for (s = output_bfd->sections; s != NULL; s = s->next)
    {
      const char *secname;
      char *buf;
      struct bfd_link_hash_entry *h;

      secname = bfd_get_section_name (output_bfd, s);
      buf = xmalloc (10 + strlen (secname));

      sprintf (buf, ".startof.%s", secname);
      h = bfd_link_hash_lookup (link_info.hash, buf, FALSE, FALSE, TRUE);
      if (h != NULL && h->type == bfd_link_hash_undefined)
	{
	  h->type = bfd_link_hash_defined;
	  h->u.def.value = bfd_get_section_vma (output_bfd, s);
	  h->u.def.section = bfd_abs_section_ptr;
	}

      sprintf (buf, ".sizeof.%s", secname);
      h = bfd_link_hash_lookup (link_info.hash, buf, FALSE, FALSE, TRUE);
      if (h != NULL && h->type == bfd_link_hash_undefined)
	{
	  h->type = bfd_link_hash_defined;
	  if (s->_cooked_size != 0)
	    h->u.def.value = TO_ADDR (s->_cooked_size);
	  else
	    h->u.def.value = TO_ADDR (s->_raw_size);
	  h->u.def.section = bfd_abs_section_ptr;
	}

      free (buf);
    }
}

static void
lang_finish (void)
{
  struct bfd_link_hash_entry *h;
  bfd_boolean warn;

  if (link_info.relocatable || link_info.shared)
    warn = FALSE;
  else
    warn = TRUE;

  if (entry_symbol.name == NULL)
    {
      /* No entry has been specified.  Look for start, but don't warn
	 if we don't find it.  */
      entry_symbol.name = "start";
      warn = FALSE;
    }

  h = bfd_link_hash_lookup (link_info.hash, entry_symbol.name,
			    FALSE, FALSE, TRUE);
  if (h != NULL
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
	einfo (_("%P%F:%s: can't set start address\n"), entry_symbol.name);
    }
  else
    {
      bfd_vma val;
      const char *send;

      /* We couldn't find the entry symbol.  Try parsing it as a
         number.  */
      val = bfd_scan_vma (entry_symbol.name, &send, 0);
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
	  ts = bfd_get_section_by_name (output_bfd, entry_section);
	  if (ts != NULL)
	    {
	      if (warn)
		einfo (_("%P: warning: cannot find entry symbol %s; defaulting to %V\n"),
		       entry_symbol.name,
		       bfd_get_section_vma (output_bfd, ts));
	      if (! bfd_set_start_address (output_bfd,
					   bfd_get_section_vma (output_bfd,
								ts)))
		einfo (_("%P%F: can't set start address\n"));
	    }
	  else
	    {
	      if (warn)
		einfo (_("%P: warning: cannot find entry symbol %s; not setting start address\n"),
		       entry_symbol.name);
	    }
	}
    }

  bfd_hash_table_free (&lang_definedness_table);
}

/* This is a small function used when we want to ignore errors from
   BFD.  */

static void
ignore_bfd_errors (const char *s ATTRIBUTE_UNUSED, ...)
{
  /* Don't do anything.  */
}

/* Check that the architecture of all the input files is compatible
   with the output file.  Also call the backend to let it do any
   other checking that is needed.  */

static void
lang_check (void)
{
  lang_statement_union_type *file;
  bfd *input_bfd;
  const bfd_arch_info_type *compatible;

  for (file = file_chain.head; file != NULL; file = file->input_statement.next)
    {
      input_bfd = file->input_statement.the_bfd;
      compatible = bfd_arch_get_compatible (input_bfd, output_bfd,
					    command_line.accept_unknown_input_arch);

      /* In general it is not possible to perform a relocatable
	 link between differing object formats when the input
	 file has relocations, because the relocations in the
	 input format may not have equivalent representations in
	 the output format (and besides BFD does not translate
	 relocs for other link purposes than a final link).  */
      if ((link_info.relocatable || link_info.emitrelocations)
	  && (compatible == NULL
	      || bfd_get_flavour (input_bfd) != bfd_get_flavour (output_bfd))
	  && (bfd_get_file_flags (input_bfd) & HAS_RELOC) != 0)
	{
	  einfo (_("%P%F: Relocatable linking with relocations from format %s (%B) to format %s (%B) is not supported\n"),
		 bfd_get_target (input_bfd), input_bfd,
		 bfd_get_target (output_bfd), output_bfd);
	  /* einfo with %F exits.  */
	}

      if (compatible == NULL)
	{
	  if (command_line.warn_mismatch)
	    einfo (_("%P: warning: %s architecture of input file `%B' is incompatible with %s output\n"),
		   bfd_printable_name (input_bfd), input_bfd,
		   bfd_printable_name (output_bfd));
	}
      else if (bfd_count_sections (input_bfd))
	{
	  /* If the input bfd has no contents, it shouldn't set the
	     private data of the output bfd.  */

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
		einfo (_("%P%X: failed to merge target specific data of file %B\n"),
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
lang_common (void)
{
  if (command_line.inhibit_common_definition)
    return;
  if (link_info.relocatable
      && ! command_line.force_common_definition)
    return;

  if (! config.sort_common)
    bfd_link_hash_traverse (link_info.hash, lang_one_common, NULL);
  else
    {
      int power;

      for (power = 4; power >= 0; power--)
	bfd_link_hash_traverse (link_info.hash, lang_one_common, &power);
    }
}

/* Place one common symbol in the correct section.  */

static bfd_boolean
lang_one_common (struct bfd_link_hash_entry *h, void *info)
{
  unsigned int power_of_two;
  bfd_vma size;
  asection *section;

  if (h->type != bfd_link_hash_common)
    return TRUE;

  size = h->u.c.size;
  power_of_two = h->u.c.p->alignment_power;

  if (config.sort_common
      && power_of_two < (unsigned int) *(int *) info)
    return TRUE;

  section = h->u.c.p->section;

  /* Increase the size of the section to align the common sym.  */
  section->_cooked_size += ((bfd_vma) 1 << (power_of_two + opb_shift)) - 1;
  section->_cooked_size &= (- (bfd_vma) 1 << (power_of_two + opb_shift));

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
  section->flags &= ~SEC_IS_COMMON;

  if (config.map_file != NULL)
    {
      static bfd_boolean header_printed;
      int len;
      char *name;
      char buf[50];

      if (! header_printed)
	{
	  minfo (_("\nAllocating common symbols\n"));
	  minfo (_("Common symbol       size              file\n\n"));
	  header_printed = TRUE;
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

  return TRUE;
}

/* Run through the input files and ensure that every input section has
   somewhere to go.  If one is found without a destination then create
   an input request and place it into the statement tree.  */

static void
lang_place_orphans (void)
{
  LANG_FOR_EACH_INPUT_STATEMENT (file)
    {
      asection *s;

      for (s = file->the_bfd->sections; s != NULL; s = s->next)
	{
	  if (s->output_section == NULL)
	    {
	      /* This section of the file is not attached, root
	         around for a sensible place for it to go.  */

	      if (file->just_syms_flag)
		{
		  abort ();
		}
	      else if (strcmp (s->name, "COMMON") == 0)
		{
		  /* This is a lonely common section which must have
		     come from an archive.  We attach to the section
		     with the wildcard.  */
		  if (! link_info.relocatable
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
		      lang_add_section (&default_common_section->children, s,
					default_common_section, file);
		    }
		}
	      else if (ldemul_place_orphan (file, s))
		;
	      else
		{
		  lang_output_section_statement_type *os;

		  os = lang_output_section_statement_lookup (s->name);
		  lang_add_section (&os->children, s, os, file);
		}
	    }
	}
    }
}

void
lang_set_flags (lang_memory_region_type *ptr, const char *flags, int invert)
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
lang_for_each_input_file (void (*func) (lang_input_statement_type *))
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
lang_for_each_file (void (*func) (lang_input_statement_type *))
{
  LANG_FOR_EACH_INPUT_STATEMENT (f)
    {
      func (f);
    }
}

void
ldlang_add_file (lang_input_statement_type *entry)
{
  bfd **pp;

  lang_statement_append (&file_chain,
			 (lang_statement_union_type *) entry,
			 &entry->next);

  /* The BFD linker needs to have a list of all input BFDs involved in
     a link.  */
  ASSERT (entry->the_bfd->link_next == NULL);
  ASSERT (entry->the_bfd != output_bfd);
  for (pp = &link_info.input_bfds; *pp != NULL; pp = &(*pp)->link_next)
    ;
  *pp = entry->the_bfd;
  entry->the_bfd->usrdata = entry;
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

  bfd_map_over_sections (entry->the_bfd, section_already_linked, entry);
}

void
lang_add_output (const char *name, int from_script)
{
  /* Make -o on command line override OUTPUT in script.  */
  if (!had_output_filename || !from_script)
    {
      output_filename = name;
      had_output_filename = TRUE;
    }
}

static lang_output_section_statement_type *current_section;

static int
topower (int x)
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
lang_enter_output_section_statement (const char *output_section_statement_name,
				     etree_type *address_exp,
				     enum section_type sectype,
				     etree_type *align,
				     etree_type *subalign,
				     etree_type *ebase)
{
  lang_output_section_statement_type *os;

  current_section =
   os =
    lang_output_section_statement_lookup (output_section_statement_name);

  /* Add this statement to tree.  */
#if 0
  add_statement (lang_output_section_statement_enum,
		 output_section_statement);
#endif
  /* Make next things chain into subchain of this.  */

  if (os->addr_tree == NULL)
    {
      os->addr_tree = address_exp;
    }
  os->sectype = sectype;
  if (sectype != noload_section)
    os->flags = SEC_NO_FLAGS;
  else
    os->flags = SEC_NEVER_LOAD;
  os->block_value = 1;
  stat_ptr = &os->children;

  os->subsection_alignment =
    topower (exp_get_value_int (subalign, -1, "subsection alignment", 0));
  os->section_alignment =
    topower (exp_get_value_int (align, -1, "section alignment", 0));

  os->load_base = ebase;
  return os;
}

void
lang_final (void)
{
  lang_output_statement_type *new =
    new_stat (lang_output_statement, stat_ptr);

  new->name = output_filename;
}

/* Reset the current counters in the regions.  */

void
lang_reset_memory_regions (void)
{
  lang_memory_region_type *p = lang_memory_region_list;
  asection *o;

  for (p = lang_memory_region_list; p != NULL; p = p->next)
    {
      p->old_length = (bfd_size_type) (p->current - p->origin);
      p->current = p->origin;
    }

  for (o = output_bfd->sections; o != NULL; o = o->next)
    o->_raw_size = 0;
}

/* If the wild pattern was marked KEEP, the member sections
   should be as well.  */

static void
gc_section_callback (lang_wild_statement_type *ptr,
		     struct wildcard_list *sec ATTRIBUTE_UNUSED,
		     asection *section,
		     lang_input_statement_type *file ATTRIBUTE_UNUSED,
		     void *data ATTRIBUTE_UNUSED)
{
  if (ptr->keep_sections)
    section->flags |= SEC_KEEP;
}

/* Handle a wild statement, marking it against GC.  */

static void
lang_gc_wild (lang_wild_statement_type *s)
{
  walk_wild (s, gc_section_callback, NULL);
}

/* Iterate over sections marking them against GC.  */

static void
lang_gc_sections_1 (lang_statement_union_type *s)
{
  for (; s != NULL; s = s->header.next)
    {
      switch (s->header.type)
	{
	case lang_wild_statement_enum:
	  lang_gc_wild (&s->wild_statement);
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
lang_gc_sections (void)
{
  struct bfd_link_hash_entry *h;
  ldlang_undef_chain_list_type *ulist;

  /* Keep all sections so marked in the link script.  */

  lang_gc_sections_1 (statement_list.head);

  /* Keep all sections containing symbols undefined on the command-line,
     and the section containing the entry symbol.  */

  for (ulist = link_info.gc_sym_list; ulist; ulist = ulist->next)
    {
      h = bfd_link_hash_lookup (link_info.hash, ulist->name,
				FALSE, FALSE, FALSE);

      if (h != NULL
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
lang_process (void)
{
  lang_reasonable_defaults ();
  current_target = default_target;

  /* Open the output file.  */
  lang_for_each_statement (ldlang_open_output);
  init_opb ();

  ldemul_create_output_section_statements ();

  /* Add to the hash table all undefineds on the command line.  */
  lang_place_undefineds ();

  already_linked_table_init ();

  /* Create a bfd for each input file.  */
  current_target = default_target;
  open_input_bfds (statement_list.head, FALSE);

  link_info.gc_sym_list = &entry_symbol;
  if (entry_symbol.name == NULL)
    link_info.gc_sym_list = ldlang_undef_chain_list_head;

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

  /* If there were any SEC_MERGE sections, finish their merging, so that
     section sizes can be computed.  This has to be done after GC of sections,
     so that GCed sections are not merged, but before assigning output
     sections, since removing whole input sections is hard then.  */
  bfd_merge_sections (output_bfd, &link_info);

  /* Size up the common data.  */
  lang_common ();

  /* Run through the contours of the script and attach input sections
     to the correct output sections.  */
  map_input_to_output_sections (statement_list.head, NULL, NULL);

  /* Find any sections not attached explicitly and handle them.  */
  lang_place_orphans ();

  if (! link_info.relocatable)
    {
      /* Look for a text section and set the readonly attribute in it.  */
      asection *found = bfd_get_section_by_name (output_bfd, ".text");

      if (found != NULL)
	{
	  if (config.text_read_only)
	    found->flags |= SEC_READONLY;
	  else
	    found->flags &= ~SEC_READONLY;
	}
    }

  /* Do anything special before sizing sections.  This is where ELF
     and other back-ends size dynamic sections.  */
  ldemul_before_allocation ();

  if (!link_info.relocatable)
    strip_excluded_output_sections ();

  /* We must record the program headers before we try to fix the
     section positions, since they will affect SIZEOF_HEADERS.  */
  lang_record_phdrs ();

  /* Size up the sections.  */
  lang_size_sections (statement_list.head, abs_output_section,
		      &statement_list.head, 0, 0, NULL,
		      command_line.relax ? FALSE : TRUE);

  /* Now run around and relax if we can.  */
  if (command_line.relax)
    {
      /* Keep relaxing until bfd_relax_section gives up.  */
      bfd_boolean relax_again;

      do
	{
	  relax_again = FALSE;

	  /* Note: pe-dll.c does something like this also.  If you find
	     you need to change this code, you probably need to change
	     pe-dll.c also.  DJ  */

	  /* Do all the assignments with our current guesses as to
	     section sizes.  */
	  lang_do_assignments (statement_list.head, abs_output_section,
			       NULL, 0);

	  /* We must do this after lang_do_assignments, because it uses
	     _raw_size.  */
	  lang_reset_memory_regions ();

	  /* Perform another relax pass - this time we know where the
	     globals are, so can make a better guess.  */
	  lang_size_sections (statement_list.head, abs_output_section,
			      &statement_list.head, 0, 0, &relax_again, FALSE);

	  /* If the normal relax is done and the relax finalize pass
	     is not performed yet, we perform another relax pass.  */
	  if (!relax_again && link_info.need_relax_finalize)
	    {
	      link_info.need_relax_finalize = FALSE;
	      relax_again = TRUE;
	    }
	}
      while (relax_again);

      /* Final extra sizing to report errors.  */
      lang_do_assignments (statement_list.head, abs_output_section, NULL, 0);
      lang_reset_memory_regions ();
      lang_size_sections (statement_list.head, abs_output_section,
			  &statement_list.head, 0, 0, NULL, TRUE);
    }

  /* See if anything special should be done now we know how big
     everything is.  */
  ldemul_after_allocation ();

  /* Fix any .startof. or .sizeof. symbols.  */
  lang_set_startof ();

  /* Do all the assignments, now that we know the final resting places
     of all the symbols.  */

  lang_do_assignments (statement_list.head, abs_output_section, NULL, 0);

  /* Make sure that the section addresses make sense.  */
  if (! link_info.relocatable
      && command_line.check_section_addresses)
    lang_check_section_addresses ();

  /* Final stuffs.  */

  ldemul_finish ();
  lang_finish ();
}

/* EXPORTED TO YACC */

void
lang_add_wild (struct wildcard_spec *filespec,
	       struct wildcard_list *section_list,
	       bfd_boolean keep_sections)
{
  struct wildcard_list *curr, *next;
  lang_wild_statement_type *new;

  /* Reverse the list as the parser puts it back to front.  */
  for (curr = section_list, section_list = NULL;
       curr != NULL;
       section_list = curr, curr = next)
    {
      if (curr->spec.name != NULL && strcmp (curr->spec.name, "COMMON") == 0)
	placed_commons = TRUE;

      next = curr->next;
      curr->next = section_list;
    }

  if (filespec != NULL && filespec->name != NULL)
    {
      if (strcmp (filespec->name, "*") == 0)
	filespec->name = NULL;
      else if (! wildcardp (filespec->name))
	lang_has_input_file = TRUE;
    }

  new = new_stat (lang_wild_statement, stat_ptr);
  new->filename = NULL;
  new->filenames_sorted = FALSE;
  if (filespec != NULL)
    {
      new->filename = filespec->name;
      new->filenames_sorted = filespec->sorted;
    }
  new->section_list = section_list;
  new->keep_sections = keep_sections;
  lang_list_init (&new->children);
}

void
lang_section_start (const char *name, etree_type *address)
{
  lang_address_statement_type *ad;

  ad = new_stat (lang_address_statement, stat_ptr);
  ad->section_name = name;
  ad->address = address;
}

/* Set the start symbol to NAME.  CMDLINE is nonzero if this is called
   because of a -e argument on the command line, or zero if this is
   called by ENTRY in a linker script.  Command line arguments take
   precedence.  */

void
lang_add_entry (const char *name, bfd_boolean cmdline)
{
  if (entry_symbol.name == NULL
      || cmdline
      || ! entry_from_cmdline)
    {
      entry_symbol.name = name;
      entry_from_cmdline = cmdline;
    }
}

void
lang_add_target (const char *name)
{
  lang_target_statement_type *new = new_stat (lang_target_statement,
					      stat_ptr);

  new->target = name;

}

void
lang_add_map (const char *name)
{
  while (*name)
    {
      switch (*name)
	{
	case 'F':
	  map_option_f = TRUE;
	  break;
	}
      name++;
    }
}

void
lang_add_fill (fill_type *fill)
{
  lang_fill_statement_type *new = new_stat (lang_fill_statement,
					    stat_ptr);

  new->fill = fill;
}

void
lang_add_data (int type, union etree_union *exp)
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
lang_add_reloc (bfd_reloc_code_real_type reloc,
		reloc_howto_type *howto,
		asection *section,
		const char *name,
		union etree_union *addend)
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
lang_add_assignment (etree_type *exp)
{
  lang_assignment_statement_type *new = new_stat (lang_assignment_statement,
						  stat_ptr);

  new->exp = exp;
  return new;
}

void
lang_add_attribute (enum statement_enum attribute)
{
  new_statement (attribute, sizeof (lang_statement_union_type), stat_ptr);
}

void
lang_startup (const char *name)
{
  if (startup_file != NULL)
    {
      einfo (_("%P%Fmultiple STARTUP files\n"));
    }
  first_file->filename = name;
  first_file->local_sym_name = name;
  first_file->real = TRUE;

  startup_file = name;
}

void
lang_float (bfd_boolean maybe)
{
  lang_float_flag = maybe;
}


/* Work out the load- and run-time regions from a script statement, and
   store them in *LMA_REGION and *REGION respectively.

   MEMSPEC is the name of the run-time region, or the value of
   DEFAULT_MEMORY_REGION if the statement didn't specify one.
   LMA_MEMSPEC is the name of the load-time region, or null if the
   statement didn't specify one.HAVE_LMA_P is TRUE if the statement
   had an explicit load address.

   It is an error to specify both a load region and a load address.  */

static void
lang_get_regions (lang_memory_region_type **region,
		  lang_memory_region_type **lma_region,
		  const char *memspec,
		  const char *lma_memspec,
		  bfd_boolean have_lma,
		  bfd_boolean have_vma)
{
  *lma_region = lang_memory_region_lookup (lma_memspec, FALSE);

  /* If no runtime region or VMA has been specified, but the load region has
     been specified, then use the load region for the runtime region as well.  */
  if (lma_memspec != NULL
      && ! have_vma
      && strcmp (memspec, DEFAULT_MEMORY_REGION) == 0)
    *region = *lma_region;
  else
    *region = lang_memory_region_lookup (memspec, FALSE);

  if (have_lma && lma_memspec != 0)
    einfo (_("%X%P:%S: section has both a load address and a load region\n"));
}

void
lang_leave_output_section_statement (fill_type *fill, const char *memspec,
				     lang_output_section_phdr_list *phdrs,
				     const char *lma_memspec)
{
  lang_get_regions (&current_section->region,
		    &current_section->lma_region,
		    memspec, lma_memspec,
		    current_section->load_base != NULL,
		    current_section->addr_tree != NULL);
  current_section->fill = fill;
  current_section->phdrs = phdrs;
  stat_ptr = &statement_list;
}

/* Create an absolute symbol with the given name with the value of the
   address of first byte of the section named.

   If the symbol already exists, then do nothing.  */

void
lang_abs_symbol_at_beginning_of (const char *secname, const char *name)
{
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (link_info.hash, name, TRUE, TRUE, TRUE);
  if (h == NULL)
    einfo (_("%P%F: bfd_link_hash_lookup failed: %E\n"));

  if (h->type == bfd_link_hash_new
      || h->type == bfd_link_hash_undefined)
    {
      asection *sec;

      h->type = bfd_link_hash_defined;

      sec = bfd_get_section_by_name (output_bfd, secname);
      if (sec == NULL)
	h->u.def.value = 0;
      else
	h->u.def.value = bfd_get_section_vma (output_bfd, sec);

      h->u.def.section = bfd_abs_section_ptr;
    }
}

/* Create an absolute symbol with the given name with the value of the
   address of the first byte after the end of the section named.

   If the symbol already exists, then do nothing.  */

void
lang_abs_symbol_at_end_of (const char *secname, const char *name)
{
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (link_info.hash, name, TRUE, TRUE, TRUE);
  if (h == NULL)
    einfo (_("%P%F: bfd_link_hash_lookup failed: %E\n"));

  if (h->type == bfd_link_hash_new
      || h->type == bfd_link_hash_undefined)
    {
      asection *sec;

      h->type = bfd_link_hash_defined;

      sec = bfd_get_section_by_name (output_bfd, secname);
      if (sec == NULL)
	h->u.def.value = 0;
      else
	h->u.def.value = (bfd_get_section_vma (output_bfd, sec)
			  + TO_ADDR (bfd_section_size (output_bfd, sec)));

      h->u.def.section = bfd_abs_section_ptr;
    }
}

void
lang_statement_append (lang_statement_list_type *list,
		       lang_statement_union_type *element,
		       lang_statement_union_type **field)
{
  *(list->tail) = element;
  list->tail = field;
}

/* Set the output format type.  -oformat overrides scripts.  */

void
lang_add_output_format (const char *format,
			const char *big,
			const char *little,
			int from_script)
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
lang_enter_group (void)
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
lang_leave_group (void)
{
  stat_ptr = &statement_list;
}

/* Add a new program header.  This is called for each entry in a PHDRS
   command in a linker script.  */

void
lang_new_phdr (const char *name,
	       etree_type *type,
	       bfd_boolean filehdr,
	       bfd_boolean phdrs,
	       etree_type *at,
	       etree_type *flags)
{
  struct lang_phdr *n, **pp;

  n = stat_alloc (sizeof (struct lang_phdr));
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
lang_record_phdrs (void)
{
  unsigned int alc;
  asection **secs;
  lang_output_section_phdr_list *last;
  struct lang_phdr *l;
  lang_statement_union_type *u;

  alc = 10;
  secs = xmalloc (alc * sizeof (asection *));
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
	  lang_output_section_phdr_list *pl;

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
		      secs = xrealloc (secs, alc * sizeof (asection *));
		    }
		  secs[c] = os->bfd_section;
		  ++c;
		  pl->used = TRUE;
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
			     l->flags != NULL, flags, l->at != NULL,
			     at, l->filehdr, l->phdrs, c, secs))
	einfo (_("%F%P: bfd_record_phdr failed: %E\n"));
    }

  free (secs);

  /* Make sure all the phdr assignments succeeded.  */
  for (u = lang_output_section_statement.head;
       u != NULL;
       u = u->output_section_statement.next)
    {
      lang_output_section_phdr_list *pl;

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
lang_add_nocrossref (lang_nocrossref_type *l)
{
  struct lang_nocrossrefs *n;

  n = xmalloc (sizeof *n);
  n->next = nocrossref_list;
  n->list = l;
  nocrossref_list = n;

  /* Set notice_all so that we get informed about all symbols.  */
  link_info.notice_all = TRUE;
}

/* Overlay handling.  We handle overlays with some static variables.  */

/* The overlay virtual address.  */
static etree_type *overlay_vma;
/* And subsection alignment.  */
static etree_type *overlay_subalign;

/* An expression for the maximum section size seen so far.  */
static etree_type *overlay_max;

/* A list of all the sections in this overlay.  */

struct overlay_list {
  struct overlay_list *next;
  lang_output_section_statement_type *os;
};

static struct overlay_list *overlay_list;

/* Start handling an overlay.  */

void
lang_enter_overlay (etree_type *vma_expr, etree_type *subalign)
{
  /* The grammar should prevent nested overlays from occurring.  */
  ASSERT (overlay_vma == NULL
	  && overlay_subalign == NULL
	  && overlay_max == NULL);

  overlay_vma = vma_expr;
  overlay_subalign = subalign;
}

/* Start a section in an overlay.  We handle this by calling
   lang_enter_output_section_statement with the correct VMA.
   lang_leave_overlay sets up the LMA and memory regions.  */

void
lang_enter_overlay_section (const char *name)
{
  struct overlay_list *n;
  etree_type *size;

  lang_enter_output_section_statement (name, overlay_vma, normal_section,
				       0, overlay_subalign, 0);

  /* If this is the first section, then base the VMA of future
     sections on this one.  This will work correctly even if `.' is
     used in the addresses.  */
  if (overlay_list == NULL)
    overlay_vma = exp_nameop (ADDR, name);

  /* Remember the section.  */
  n = xmalloc (sizeof *n);
  n->os = current_section;
  n->next = overlay_list;
  overlay_list = n;

  size = exp_nameop (SIZEOF, name);

  /* Arrange to work out the maximum section end address.  */
  if (overlay_max == NULL)
    overlay_max = size;
  else
    overlay_max = exp_binop (MAX_K, overlay_max, size);
}

/* Finish a section in an overlay.  There isn't any special to do
   here.  */

void
lang_leave_overlay_section (fill_type *fill,
			    lang_output_section_phdr_list *phdrs)
{
  const char *name;
  char *clean, *s2;
  const char *s1;
  char *buf;

  name = current_section->name;

  /* For now, assume that DEFAULT_MEMORY_REGION is the run-time memory
     region and that no load-time region has been specified.  It doesn't
     really matter what we say here, since lang_leave_overlay will
     override it.  */
  lang_leave_output_section_statement (fill, DEFAULT_MEMORY_REGION, phdrs, 0);

  /* Define the magic symbols.  */

  clean = xmalloc (strlen (name) + 1);
  s2 = clean;
  for (s1 = name; *s1 != '\0'; s1++)
    if (ISALNUM (*s1) || *s1 == '_')
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
lang_leave_overlay (etree_type *lma_expr,
		    int nocrossrefs,
		    fill_type *fill,
		    const char *memspec,
		    lang_output_section_phdr_list *phdrs,
		    const char *lma_memspec)
{
  lang_memory_region_type *region;
  lang_memory_region_type *lma_region;
  struct overlay_list *l;
  lang_nocrossref_type *nocrossref;

  lang_get_regions (&region, &lma_region,
		    memspec, lma_memspec,
		    lma_expr != NULL, FALSE);

  nocrossref = NULL;

  /* After setting the size of the last section, set '.' to end of the
     overlay region.  */
  if (overlay_list != NULL)
    overlay_list->os->update_dot_tree
      = exp_assop ('=', ".", exp_binop ('+', overlay_vma, overlay_max));

  l = overlay_list;
  while (l != NULL)
    {
      struct overlay_list *next;

      if (fill != NULL && l->os->fill == NULL)
	l->os->fill = fill;

      l->os->region = region;
      l->os->lma_region = lma_region;

      /* The first section has the load address specified in the
	 OVERLAY statement.  The rest are worked out from that.
	 The base address is not needed (and should be null) if
	 an LMA region was specified.  */
      if (l->next == 0)
	l->os->load_base = lma_expr;
      else if (lma_region == 0)
	l->os->load_base = exp_binop ('+',
				      exp_nameop (LOADADDR, l->next->os->name),
				      exp_nameop (SIZEOF, l->next->os->name));

      if (phdrs != NULL && l->os->phdrs == NULL)
	l->os->phdrs = phdrs;

      if (nocrossrefs)
	{
	  lang_nocrossref_type *nc;

	  nc = xmalloc (sizeof *nc);
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

  overlay_vma = NULL;
  overlay_list = NULL;
  overlay_max = NULL;
}

/* Version handling.  This is only useful for ELF.  */

/* This global variable holds the version tree that we build.  */

struct bfd_elf_version_tree *lang_elf_version_info;

/* If PREV is NULL, return first version pattern matching particular symbol.
   If PREV is non-NULL, return first version pattern matching particular
   symbol after PREV (previously returned by lang_vers_match).  */

static struct bfd_elf_version_expr *
lang_vers_match (struct bfd_elf_version_expr_head *head,
		 struct bfd_elf_version_expr *prev,
		 const char *sym)
{
  const char *cxx_sym = sym;
  const char *java_sym = sym;
  struct bfd_elf_version_expr *expr = NULL;

  if (head->mask & BFD_ELF_VERSION_CXX_TYPE)
    {
      cxx_sym = cplus_demangle (sym, DMGL_PARAMS | DMGL_ANSI);
      if (!cxx_sym)
	cxx_sym = sym;
    }
  if (head->mask & BFD_ELF_VERSION_JAVA_TYPE)
    {
      java_sym = cplus_demangle (sym, DMGL_JAVA);
      if (!java_sym)
	java_sym = sym;
    }

  if (head->htab && (prev == NULL || prev->symbol))
    {
      struct bfd_elf_version_expr e;

      switch (prev ? prev->mask : 0)
	{
	  case 0:
	    if (head->mask & BFD_ELF_VERSION_C_TYPE)
	      {
		e.symbol = sym;
		expr = htab_find (head->htab, &e);
		while (expr && strcmp (expr->symbol, sym) == 0)
		  if (expr->mask == BFD_ELF_VERSION_C_TYPE)
		    goto out_ret;
		else
		  expr = expr->next;
	      }
	    /* Fallthrough */
	  case BFD_ELF_VERSION_C_TYPE:
	    if (head->mask & BFD_ELF_VERSION_CXX_TYPE)
	      {
		e.symbol = cxx_sym;
		expr = htab_find (head->htab, &e);
		while (expr && strcmp (expr->symbol, cxx_sym) == 0)
		  if (expr->mask == BFD_ELF_VERSION_CXX_TYPE)
		    goto out_ret;
		else
		  expr = expr->next;
	      }
	    /* Fallthrough */
	  case BFD_ELF_VERSION_CXX_TYPE:
	    if (head->mask & BFD_ELF_VERSION_JAVA_TYPE)
	      {
		e.symbol = java_sym;
		expr = htab_find (head->htab, &e);
		while (expr && strcmp (expr->symbol, java_sym) == 0)
		  if (expr->mask == BFD_ELF_VERSION_JAVA_TYPE)
		    goto out_ret;
		else
		  expr = expr->next;
	      }
	    /* Fallthrough */
	  default:
	    break;
	}
    }

  /* Finally, try the wildcards.  */
  if (prev == NULL || prev->symbol)
    expr = head->remaining;
  else
    expr = prev->next;
  while (expr)
    {
      const char *s;

      if (expr->pattern[0] == '*' && expr->pattern[1] == '\0')
	break;

      if (expr->mask == BFD_ELF_VERSION_JAVA_TYPE)
	s = java_sym;
      else if (expr->mask == BFD_ELF_VERSION_CXX_TYPE)
	s = cxx_sym;
      else
	s = sym;
      if (fnmatch (expr->pattern, s, 0) == 0)
	break;
      expr = expr->next;
    }

out_ret:
  if (cxx_sym != sym)
    free ((char *) cxx_sym);
  if (java_sym != sym)
    free ((char *) java_sym);
  return expr;
}

/* Return NULL if the PATTERN argument is a glob pattern, otherwise,
   return a string pointing to the symbol name.  */

static const char *
realsymbol (const char *pattern)
{
  const char *p;
  bfd_boolean changed = FALSE, backslash = FALSE;
  char *s, *symbol = xmalloc (strlen (pattern) + 1);

  for (p = pattern, s = symbol; *p != '\0'; ++p)
    {
      /* It is a glob pattern only if there is no preceding
	 backslash.  */
      if (! backslash && (*p == '?' || *p == '*' || *p == '['))
	{
	  free (symbol);
	  return NULL;
	}

      if (backslash)
	{
	  /* Remove the preceding backslash.  */
	  *(s - 1) = *p;
	  changed = TRUE;
	}
      else
	*s++ = *p;

      backslash = *p == '\\';
    }

  if (changed)
    {
      *s = '\0';
      return symbol;
    }
  else
    {
      free (symbol);
      return pattern;
    }
}

/* This is called for each variable name or match expression.  */

struct bfd_elf_version_expr *
lang_new_vers_pattern (struct bfd_elf_version_expr *orig,
		       const char *new,
		       const char *lang)
{
  struct bfd_elf_version_expr *ret;

  ret = xmalloc (sizeof *ret);
  ret->next = orig;
  ret->pattern = new;
  ret->symver = 0;
  ret->script = 0;
  ret->symbol = realsymbol (new);

  if (lang == NULL || strcasecmp (lang, "C") == 0)
    ret->mask = BFD_ELF_VERSION_C_TYPE;
  else if (strcasecmp (lang, "C++") == 0)
    ret->mask = BFD_ELF_VERSION_CXX_TYPE;
  else if (strcasecmp (lang, "Java") == 0)
    ret->mask = BFD_ELF_VERSION_JAVA_TYPE;
  else
    {
      einfo (_("%X%P: unknown language `%s' in version information\n"),
	     lang);
      ret->mask = BFD_ELF_VERSION_C_TYPE;
    }

  return ldemul_new_vers_pattern (ret);
}

/* This is called for each set of variable names and match
   expressions.  */

struct bfd_elf_version_tree *
lang_new_vers_node (struct bfd_elf_version_expr *globals,
		    struct bfd_elf_version_expr *locals)
{
  struct bfd_elf_version_tree *ret;

  ret = xcalloc (1, sizeof *ret);
  ret->globals.list = globals;
  ret->locals.list = locals;
  ret->match = lang_vers_match;
  ret->name_indx = (unsigned int) -1;
  return ret;
}

/* This static variable keeps track of version indices.  */

static int version_index;

static hashval_t
version_expr_head_hash (const void *p)
{
  const struct bfd_elf_version_expr *e = p;

  return htab_hash_string (e->symbol);
}

static int
version_expr_head_eq (const void *p1, const void *p2)
{
  const struct bfd_elf_version_expr *e1 = p1;
  const struct bfd_elf_version_expr *e2 = p2;

  return strcmp (e1->symbol, e2->symbol) == 0;
}

static void
lang_finalize_version_expr_head (struct bfd_elf_version_expr_head *head)
{
  size_t count = 0;
  struct bfd_elf_version_expr *e, *next;
  struct bfd_elf_version_expr **list_loc, **remaining_loc;

  for (e = head->list; e; e = e->next)
    {
      if (e->symbol)
	count++;
      head->mask |= e->mask;
    }

  if (count)
    {
      head->htab = htab_create (count * 2, version_expr_head_hash,
				version_expr_head_eq, NULL);
      list_loc = &head->list;
      remaining_loc = &head->remaining;
      for (e = head->list; e; e = next)
	{
	  next = e->next;
	  if (!e->symbol)
	    {
	      *remaining_loc = e;
	      remaining_loc = &e->next;
	    }
	  else
	    {
	      void **loc = htab_find_slot (head->htab, e, INSERT);

	      if (*loc)
		{
		  struct bfd_elf_version_expr *e1, *last;

		  e1 = *loc;
		  last = NULL;
		  do
		    {
		      if (e1->mask == e->mask)
			{
			  last = NULL;
			  break;
			}
		      last = e1;
		      e1 = e1->next;
		    }
		  while (e1 && strcmp (e1->symbol, e->symbol) == 0);

		  if (last == NULL)
		    {
		      /* This is a duplicate.  */
		      /* FIXME: Memory leak.  Sometimes pattern is not
			 xmalloced alone, but in larger chunk of memory.  */
		      /* free (e->symbol); */
		      free (e);
		    }
		  else
		    {
		      e->next = last->next;
		      last->next = e;
		    }
		}
	      else
		{
		  *loc = e;
		  *list_loc = e;
		  list_loc = &e->next;
		}
	    }
	}
      *remaining_loc = NULL;
      *list_loc = head->remaining;
    }
  else
    head->remaining = head->list;
}

/* This is called when we know the name and dependencies of the
   version.  */

void
lang_register_vers_node (const char *name,
			 struct bfd_elf_version_tree *version,
			 struct bfd_elf_version_deps *deps)
{
  struct bfd_elf_version_tree *t, **pp;
  struct bfd_elf_version_expr *e1;

  if (name == NULL)
    name = "";

  if ((name[0] == '\0' && lang_elf_version_info != NULL)
      || (lang_elf_version_info && lang_elf_version_info->name[0] == '\0'))
    {
      einfo (_("%X%P: anonymous version tag cannot be combined with other version tags\n"));
      free (version);
      return;
    }

  /* Make sure this node has a unique name.  */
  for (t = lang_elf_version_info; t != NULL; t = t->next)
    if (strcmp (t->name, name) == 0)
      einfo (_("%X%P: duplicate version tag `%s'\n"), name);

  lang_finalize_version_expr_head (&version->globals);
  lang_finalize_version_expr_head (&version->locals);

  /* Check the global and local match names, and make sure there
     aren't any duplicates.  */

  for (e1 = version->globals.list; e1 != NULL; e1 = e1->next)
    {
      for (t = lang_elf_version_info; t != NULL; t = t->next)
	{
	  struct bfd_elf_version_expr *e2;

	  if (t->locals.htab && e1->symbol)
	    {
	      e2 = htab_find (t->locals.htab, e1);
	      while (e2 && strcmp (e1->symbol, e2->symbol) == 0)
		{
		  if (e1->mask == e2->mask)
		    einfo (_("%X%P: duplicate expression `%s' in version information\n"),
			   e1->symbol);
		  e2 = e2->next;
		}
	    }
	  else if (!e1->symbol)
	    for (e2 = t->locals.remaining; e2 != NULL; e2 = e2->next)
	      if (strcmp (e1->pattern, e2->pattern) == 0 && e1->mask == e2->mask)
		einfo (_("%X%P: duplicate expression `%s' in version information\n"),
		       e1->pattern);
	}
    }

  for (e1 = version->locals.list; e1 != NULL; e1 = e1->next)
    {
      for (t = lang_elf_version_info; t != NULL; t = t->next)
	{
	  struct bfd_elf_version_expr *e2;

	  if (t->globals.htab && e1->symbol)
	    {
	      e2 = htab_find (t->globals.htab, e1);
	      while (e2 && strcmp (e1->symbol, e2->symbol) == 0)
		{
		  if (e1->mask == e2->mask)
		    einfo (_("%X%P: duplicate expression `%s' in version information\n"),
			   e1->symbol);
		  e2 = e2->next;
		}
	    }
	  else if (!e1->symbol)
	    for (e2 = t->globals.remaining; e2 != NULL; e2 = e2->next)
	      if (strcmp (e1->pattern, e2->pattern) == 0 && e1->mask == e2->mask)
		einfo (_("%X%P: duplicate expression `%s' in version information\n"),
		       e1->pattern);
	}
    }

  version->deps = deps;
  version->name = name;
  if (name[0] != '\0')
    {
      ++version_index;
      version->vernum = version_index;
    }
  else
    version->vernum = 0;

  for (pp = &lang_elf_version_info; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = version;
}

/* This is called when we see a version dependency.  */

struct bfd_elf_version_deps *
lang_add_vers_depend (struct bfd_elf_version_deps *list, const char *name)
{
  struct bfd_elf_version_deps *ret;
  struct bfd_elf_version_tree *t;

  ret = xmalloc (sizeof *ret);
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
lang_do_version_exports_section (void)
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
	einfo (_("%X%P: unable to read .exports section contents\n"), sec);

      p = contents;
      while (p < contents + len)
	{
	  greg = lang_new_vers_pattern (greg, p, NULL);
	  p = strchr (p, '\0') + 1;
	}

      /* Do not free the contents, as we used them creating the regex.  */

      /* Do not include this section in the link.  */
      bfd_set_section_flags (is->the_bfd, sec,
	bfd_get_section_flags (is->the_bfd, sec) | SEC_EXCLUDE);
    }

  lreg = lang_new_vers_pattern (NULL, "*", NULL);
  lang_register_vers_node (command_line.version_exports_section,
			   lang_new_vers_node (greg, lreg), NULL);
}

void
lang_add_unique (const char *name)
{
  struct unique_sections *ent;

  for (ent = unique_section_list; ent; ent = ent->next)
    if (strcmp (ent->name, name) == 0)
      return;

  ent = xmalloc (sizeof *ent);
  ent->name = xstrdup (name);
  ent->next = unique_section_list;
  unique_section_list = ent;
}
