# This shell script emits a C file. -*- C -*-
#   Copyright 2003
#   Free Software Foundation, Inc.
#
# This file is part of GLD, the Gnu Linker.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

# This file is sourced from elf32.em, and defines extra xtensa-elf
# specific routines.
#
cat >>e${EMULATION_NAME}.c <<EOF

#include <xtensa-config.h>

static void xtensa_wild_group_interleave (lang_statement_union_type *);
static void xtensa_colocate_output_literals (lang_statement_union_type *);


/* Flag for the emulation-specific "--no-relax" option.  */
static bfd_boolean disable_relaxation = FALSE;

/* This number is irrelevant until we turn on use_literal_pages */
static bfd_vma xtensa_page_power = 12; /* 4K pages.  */

/* To force a page break between literals and text, change
   xtensa_use_literal_pages to "true".  */
static bfd_boolean xtensa_use_literal_pages = FALSE;

#define EXTRA_VALIDATION 0


static char *
elf_xtensa_choose_target (int argc ATTRIBUTE_UNUSED,
			  char **argv ATTRIBUTE_UNUSED)
{
  if (XCHAL_HAVE_BE)
    return "${BIG_OUTPUT_FORMAT}";
  else
    return "${LITTLE_OUTPUT_FORMAT}";
}


static bfd_boolean
elf_xtensa_place_orphan (lang_input_statement_type *file, asection *s)
{
  /* Early exit for relocatable links.  */
  if (link_info.relocatable)
    return FALSE;

  return gld${EMULATION_NAME}_place_orphan (file, s);
}


static void
elf_xtensa_before_parse (void)
{
  /* Just call the default hook.... Tensilica's version of this function
     does some other work that isn't relevant here.  */
  gld${EMULATION_NAME}_before_parse ();
}


/* This is called after the sections have been attached to output
   sections, but before any sizes or addresses have been set.  */

static void
elf_xtensa_before_allocation (void)
{
  bfd *in_bfd;
  bfd_boolean is_big_endian = XCHAL_HAVE_BE;

  /* Check that the output endianness matches the Xtensa
     configuration.  The BFD library always includes both big and
     little endian target vectors for Xtensa, but it only supports the
     detailed instruction encode/decode operations (such as are
     required to process relocations) for the selected Xtensa
     configuration.  */

  if (is_big_endian && output_bfd->xvec->byteorder == BFD_ENDIAN_LITTLE)
    {
      einfo (_("%F%P: little endian output does not match "
	       "Xtensa configuration\n"));
    }
  if (!is_big_endian && output_bfd->xvec->byteorder == BFD_ENDIAN_BIG)
    {
      einfo (_("%F%P: big endian output does not match "
	       "Xtensa configuration\n"));
    }

  /* Check that the endianness for each input file matches the output.
     The merge_private_bfd_data hook has already reported any mismatches
     as errors, but those errors are not fatal.  At this point, we
     cannot go any further if there are any mismatches.  */

  for (in_bfd = link_info.input_bfds;
       in_bfd != NULL;
       in_bfd = in_bfd->link_next)
    {
      if ((is_big_endian && in_bfd->xvec->byteorder == BFD_ENDIAN_LITTLE)
	  || (!is_big_endian && in_bfd->xvec->byteorder == BFD_ENDIAN_BIG))
	einfo (_("%F%P: cross-endian linking not supported\n"));
    }

  /* Enable relaxation by default if the "--no-relax" option was not
     specified.  This is done here instead of in the before_parse hook
     because there is a check in main() to prohibit use of --relax and
     -r together and that combination should be allowed for Xtensa.  */

  if (!disable_relaxation)
    command_line.relax = TRUE;

  gld${EMULATION_NAME}_before_allocation ();

  xtensa_wild_group_interleave (stat_ptr->head);
  if (command_line.relax)
    xtensa_colocate_output_literals (stat_ptr->head);

  /* TBD: We need to force the page alignments to here and only do
     them as needed for the entire output section.  Finally, if this
     is a relocatable link then we need to add alignment notes so
     that the literals can be separated later.  */
}


typedef struct wildcard_list section_name_list;

typedef struct reloc_deps_e_t reloc_deps_e;
typedef struct reloc_deps_section_t reloc_deps_section;
typedef struct reloc_deps_graph_t reloc_deps_graph;


struct reloc_deps_e_t
{
  asection *src; /* Contains l32rs.  */
  asection *tgt; /* Contains literals.  */
  reloc_deps_e *next;
};

/* Place these in the userdata field.  */
struct reloc_deps_section_t
{
  reloc_deps_e *preds;
  reloc_deps_e *succs;
  bfd_boolean is_only_literal;
};


struct reloc_deps_graph_t
{
  size_t count;
  size_t size;
  asection **sections;
};

static void xtensa_layout_wild
  (const reloc_deps_graph *, lang_wild_statement_type *);

typedef void (*deps_callback_t) (asection *, /* src_sec */
				 bfd_vma,    /* src_offset */
				 asection *, /* target_sec */
				 bfd_vma,    /* target_offset */
				 void *);    /* closure */

extern bfd_boolean xtensa_callback_required_dependence
  (bfd *, asection *, struct bfd_link_info *, deps_callback_t, void *);
static void xtensa_ldlang_clear_addresses
  (lang_statement_union_type *);
static bfd_boolean ld_local_file_relocations_fit
  (lang_statement_union_type *, const reloc_deps_graph *);
static bfd_vma ld_assign_relative_paged_dot
  (bfd_vma, lang_statement_union_type *, const reloc_deps_graph *,
   bfd_boolean);
static bfd_vma ld_xtensa_insert_page_offsets
  (bfd_vma, lang_statement_union_type *, reloc_deps_graph *, bfd_boolean);
#if EXTRA_VALIDATION
static size_t ld_count_children
  (lang_statement_union_type *);
#endif

extern lang_statement_list_type constructor_list;

/*  Begin verbatim code from ldlang.c:
    the following are copied from ldlang.c because they are defined
    there statically.  */

static void
lang_for_each_statement_worker (void (*func) (lang_statement_union_type *),
				lang_statement_union_type *s)
{
  for (; s != (lang_statement_union_type *) NULL; s = s->header.next)
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

/* End of verbatim code from ldlang.c.  */


static reloc_deps_section *
xtensa_get_section_deps (const reloc_deps_graph *deps ATTRIBUTE_UNUSED,
			 asection *sec)
{
  /* We have a separate function for this so that
     we could in the future keep a completely independent
     structure that maps a section to its dependence edges.
     For now, we place these in the sec->userdata field.  */
  reloc_deps_section *sec_deps = sec->userdata;
  return sec_deps;
}

static void
xtensa_set_section_deps (const reloc_deps_graph *deps ATTRIBUTE_UNUSED,
			 asection *sec,
			 reloc_deps_section *deps_section)
{
  sec->userdata = deps_section;
}


/* This is used to keep a list of all of the sections participating in
   the graph so we can clean them up quickly.  */

static void
xtensa_append_section_deps (reloc_deps_graph *deps, asection *sec)
{
  if (deps->size <= deps->count)
    {
      asection **new_sections;
      size_t i;
      size_t new_size;

      new_size = deps->size * 2;
      if (new_size == 0)
	new_size = 20;

      new_sections = xmalloc (sizeof (asection *) * new_size);
      memset (new_sections, 0, sizeof (asection *) * new_size);
      for (i = 0; i < deps->count; i++)
	{
	  new_sections[i] = deps->sections[i];
	}
      if (deps->sections != NULL)
	free (deps->sections);
      deps->sections = new_sections;
      deps->size = new_size;
    }
  deps->sections[deps->count] = sec;
  deps->count++;
}


static void
free_reloc_deps_graph (reloc_deps_graph *deps)
{
  size_t i;
  for (i = 0; i < deps->count; i++)
    {
      asection *sec = deps->sections[i];
      reloc_deps_section *sec_deps;
      sec_deps = xtensa_get_section_deps (deps, sec);
      if (sec_deps)
	{
	  reloc_deps_e *next;
	  while (sec_deps->succs != NULL)
	    {
	      next = sec_deps->succs->next;
	      free (sec_deps->succs);
	      sec_deps->succs = next;
	    }

	  while (sec_deps->preds != NULL)
	    {
	      next = sec_deps->preds->next;
	      free (sec_deps->preds);
	      sec_deps->preds = next;
	    }
	  free (sec_deps);
	}
      xtensa_set_section_deps (deps, sec, NULL);
    }
  if (deps->sections)
    free (deps->sections);

  free (deps);
}


static bfd_boolean
section_is_source (const reloc_deps_graph *deps ATTRIBUTE_UNUSED,
		   lang_statement_union_type *s)
{
  asection *sec;
  const reloc_deps_section *sec_deps;

  if (s->header.type != lang_input_section_enum)
    return FALSE;
  sec = s->input_section.section;

  sec_deps = xtensa_get_section_deps (deps, sec);
  return sec_deps && sec_deps->succs != NULL;
}


static bfd_boolean
section_is_target (const reloc_deps_graph *deps ATTRIBUTE_UNUSED,
		   lang_statement_union_type *s)
{
  asection *sec;
  const reloc_deps_section *sec_deps;

  if (s->header.type != lang_input_section_enum)
    return FALSE;
  sec = s->input_section.section;

  sec_deps = xtensa_get_section_deps (deps, sec);
  return sec_deps && sec_deps->preds != NULL;
}

static bfd_boolean
section_is_source_or_target (const reloc_deps_graph *deps ATTRIBUTE_UNUSED,
			     lang_statement_union_type *s)
{
  return (section_is_source (deps, s)
	  || section_is_target (deps, s));
}


typedef struct xtensa_ld_iter_stack_t xtensa_ld_iter_stack;
typedef struct xtensa_ld_iter_t xtensa_ld_iter;

struct xtensa_ld_iter_t
{
  lang_statement_union_type *parent;	/* Parent of the list.  */
  lang_statement_list_type *l;		/* List that holds it.  */
  lang_statement_union_type **loc;	/* Place in the list.  */
};

struct xtensa_ld_iter_stack_t
{
  xtensa_ld_iter iterloc;		/* List that hold it.  */

  xtensa_ld_iter_stack *next;		/* Next in the stack.  */
  xtensa_ld_iter_stack *prev;		/* Back pointer for stack.  */
};


static void
ld_xtensa_move_section_after (xtensa_ld_iter *to, xtensa_ld_iter *current)
{
  lang_statement_union_type *to_next;
  lang_statement_union_type *current_next;
  lang_statement_union_type **e;

#if EXTRA_VALIDATION
  size_t old_to_count, new_to_count;
  size_t old_current_count, new_current_count;
#endif

  if (to == current)
    return;

#if EXTRA_VALIDATION
  old_to_count = ld_count_children (to->parent);
  old_current_count = ld_count_children (current->parent);
#endif

  to_next = *(to->loc);
  current_next = (*current->loc)->header.next;

  *(to->loc) = *(current->loc);

  *(current->loc) = current_next;
  (*(to->loc))->header.next = to_next;

  /* reset "to" list tail */
  for (e = &to->l->head; *e != NULL; e = &(*e)->header.next)
    ;
  to->l->tail = e;

  /* reset "current" list tail */
  for (e = &current->l->head; *e != NULL; e = &(*e)->header.next)
    ;
  current->l->tail = e;

#if EXTRA_VALIDATION
  new_to_count = ld_count_children (to->parent);
  new_current_count = ld_count_children (current->parent);

  ASSERT ((old_to_count + old_current_count)
	  == (new_to_count + new_current_count));
#endif
}


/* Can only be called with lang_statements that have lists.  Returns
   false if the list is empty.  */

static bfd_boolean
iter_stack_empty (xtensa_ld_iter_stack **stack_p)
{
  return *stack_p == NULL;
}


static bfd_boolean
iter_stack_push (xtensa_ld_iter_stack **stack_p,
		 lang_statement_union_type *parent)
{
  xtensa_ld_iter_stack *stack;
  lang_statement_list_type *l = NULL;

  switch (parent->header.type)
    {
    case lang_output_section_statement_enum:
      l = &parent->output_section_statement.children;
      break;
    case lang_wild_statement_enum:
      l = &parent->wild_statement.children;
      break;
    case lang_group_statement_enum:
      l = &parent->group_statement.children;
      break;
    default:
      ASSERT (0);
      return FALSE;
    }

  /* Empty. do not push.  */
  if (l->tail == &l->head)
    return FALSE;

  stack = xmalloc (sizeof (xtensa_ld_iter_stack));
  memset (stack, 0, sizeof (xtensa_ld_iter_stack));
  stack->iterloc.parent = parent;
  stack->iterloc.l = l;
  stack->iterloc.loc = &l->head;

  stack->next = *stack_p;
  stack->prev = NULL;
  if (*stack_p != NULL)
    (*stack_p)->prev = stack;
  *stack_p = stack;
  return TRUE;
}


static void
iter_stack_pop (xtensa_ld_iter_stack **stack_p)
{
  xtensa_ld_iter_stack *stack;

  stack = *stack_p;

  if (stack == NULL)
    {
      ASSERT (stack != NULL);
      return;
    }

  if (stack->next != NULL)
    stack->next->prev = NULL;

  *stack_p = stack->next;
  free (stack);
}


/* This MUST be called if, during iteration, the user changes the
   underlying structure.  It will check for a NULL current and advance
   accordingly.  */

static void
iter_stack_update (xtensa_ld_iter_stack **stack_p)
{
  if (!iter_stack_empty (stack_p)
      && (*(*stack_p)->iterloc.loc) == NULL)
    {
      iter_stack_pop (stack_p);

      while (!iter_stack_empty (stack_p)
	     && ((*(*stack_p)->iterloc.loc)->header.next == NULL))
	{
	  iter_stack_pop (stack_p);
	}
      if (!iter_stack_empty (stack_p))
	(*stack_p)->iterloc.loc = &(*(*stack_p)->iterloc.loc)->header.next;
    }
}


static void
iter_stack_next (xtensa_ld_iter_stack **stack_p)
{
  xtensa_ld_iter_stack *stack;
  lang_statement_union_type *current;
  stack = *stack_p;

  current = *stack->iterloc.loc;
  /* If we are on the first element.  */
  if (current != NULL)
    {
      switch (current->header.type)
	{
	case lang_output_section_statement_enum:
	case lang_wild_statement_enum:
	case lang_group_statement_enum:
	  /* If the list if not empty, we are done.  */
	  if (iter_stack_push (stack_p, *stack->iterloc.loc))
	    return;
	  /* Otherwise increment the pointer as normal.  */
	  break;
	default:
	  break;
	}
    }

  while (!iter_stack_empty (stack_p)
	 && ((*(*stack_p)->iterloc.loc)->header.next == NULL))
    {
      iter_stack_pop (stack_p);
    }
  if (!iter_stack_empty (stack_p))
    (*stack_p)->iterloc.loc = &(*(*stack_p)->iterloc.loc)->header.next;
}


static lang_statement_union_type *
iter_stack_current (xtensa_ld_iter_stack **stack_p)
{
  return *((*stack_p)->iterloc.loc);
}


/* The iter stack is a preorder.  */

static void
iter_stack_create (xtensa_ld_iter_stack **stack_p,
		   lang_statement_union_type *parent)
{
  iter_stack_push (stack_p, parent);
}


static void
iter_stack_copy_current (xtensa_ld_iter_stack **stack_p,
			 xtensa_ld_iter *front)
{
  *front = (*stack_p)->iterloc;
}


static void
xtensa_colocate_literals (reloc_deps_graph *deps,
			  lang_statement_union_type *statement)
{
  /* Keep a stack of pointers to control iteration through the contours.  */
  xtensa_ld_iter_stack *stack = NULL;
  xtensa_ld_iter_stack **stack_p = &stack;

  xtensa_ld_iter front;  /* Location where new insertion should occur.  */
  xtensa_ld_iter *front_p = NULL;

  xtensa_ld_iter current; /* Location we are checking.  */
  xtensa_ld_iter *current_p = NULL;
  bfd_boolean in_literals = FALSE;

  if (deps->count == 0)
    return;

#if 0
  ld_assign_relative_paged_dot (0x100000, statement, deps,
				xtensa_use_literal_pages);

  if (!ld_local_file_relocations_fit (statement, deps))
    fprintf (stderr, "initial relocation placement does not fit\n");

  lang_for_each_statement_worker (xtensa_ldlang_clear_addresses, statement);
#endif

  iter_stack_create (stack_p, statement);

  while (!iter_stack_empty (stack_p))
    {
      bfd_boolean skip_increment = FALSE;
      lang_statement_union_type *l = iter_stack_current (stack_p);

      switch (l->header.type)
	{
	case lang_assignment_statement_enum:
	  /* Any assignment statement should block reordering across it.  */
	  front_p = NULL;
	  in_literals = FALSE;
	  break;

	case lang_input_section_enum:
	  if (front_p == NULL)
	    {
	      in_literals = (section_is_target (deps, l)
			     && !section_is_source (deps, l));
	      if (in_literals)
		{
		  front_p = &front;
		  iter_stack_copy_current (stack_p, front_p);
		}
	    }
	  else
	    {
	      bfd_boolean is_target;
	      current_p = &current;
	      iter_stack_copy_current (stack_p, current_p);
	      is_target = (section_is_target (deps, l)
			   && !section_is_source (deps, l));

	      if (in_literals)
		{
		  iter_stack_copy_current (stack_p, front_p);
		  if (!is_target)
		    in_literals = FALSE;
		}
	      else
		{
		  if (is_target)
		    {
		      /* Try to insert in place.  */
		      ld_xtensa_move_section_after (front_p, current_p);
		      ld_assign_relative_paged_dot (0x100000,
						    statement,
						    deps,
						    xtensa_use_literal_pages);

		      /* We use this code because it's already written.  */
		      if (!ld_local_file_relocations_fit (statement, deps))
			{
			  /* Move it back.  */
			  ld_xtensa_move_section_after (current_p, front_p);
			  /* Reset the literal placement.  */
			  iter_stack_copy_current (stack_p, front_p);
			}
		      else
			{
			  /* Move front pointer up by one.  */
			  front_p->loc = &(*front_p->loc)->header.next;

			  /* Do not increment the current pointer.  */
			  skip_increment = TRUE;
			}
		    }
		}
	    }
	  break;
	default:
	  break;
	}

      if (!skip_increment)
	iter_stack_next (stack_p);
      else
	/* Be careful to update the stack_p if it now is a null.  */
	iter_stack_update (stack_p);
    }

  lang_for_each_statement_worker (xtensa_ldlang_clear_addresses, statement);
}


static void
xtensa_move_dependencies_to_front (reloc_deps_graph *deps,
				   lang_wild_statement_type *w)
{
  /* Keep a front pointer and a current pointer.  */
  lang_statement_union_type **front;
  lang_statement_union_type **current;

  /* Walk to the end of the targets.  */
  for (front = &w->children.head;
       (*front != NULL) && section_is_source_or_target (deps, *front);
       front = &(*front)->header.next)
    ;

  if (*front == NULL)
    return;

  current = &(*front)->header.next;
  while (*current != NULL)
    {
      if (section_is_source_or_target (deps, *current))
	{
	  /* Insert in place.  */
	  xtensa_ld_iter front_iter;
	  xtensa_ld_iter current_iter;

	  front_iter.parent = (lang_statement_union_type *) w;
	  front_iter.l = &w->children;
	  front_iter.loc = front;

	  current_iter.parent = (lang_statement_union_type *) w;
	  current_iter.l = &w->children;
	  current_iter.loc = current;

	  ld_xtensa_move_section_after (&front_iter, &current_iter);
	  front = &(*front)->header.next;
	}
      else
	{
	  current = &(*current)->header.next;
	}
    }
}


static bfd_boolean
deps_has_sec_edge (const reloc_deps_graph *deps,
		   asection *src,
		   asection *tgt)
{
  const reloc_deps_section *sec_deps;
  const reloc_deps_e *sec_deps_e;

  sec_deps = xtensa_get_section_deps (deps, src);
  if (sec_deps == NULL)
    return FALSE;

  for (sec_deps_e = sec_deps->succs;
       sec_deps_e != NULL;
       sec_deps_e = sec_deps_e->next)
    {
      ASSERT (sec_deps_e->src == src);
      if (sec_deps_e->tgt == tgt)
	return TRUE;
    }
  return FALSE;
}


static bfd_boolean
deps_has_edge (const reloc_deps_graph *deps,
	       lang_statement_union_type *src,
	       lang_statement_union_type *tgt)
{
  if (!section_is_source (deps, src))
    return FALSE;
  if (!section_is_target (deps, tgt))
    return FALSE;

  if (src->header.type != lang_input_section_enum)
    return FALSE;
  if (tgt->header.type != lang_input_section_enum)
    return FALSE;

  return deps_has_sec_edge (deps, src->input_section.section,
			    tgt->input_section.section);
}


static void
add_deps_edge (reloc_deps_graph *deps,
	       asection *src_sec,
	       asection *tgt_sec)
{
  reloc_deps_section *src_sec_deps;
  reloc_deps_section *tgt_sec_deps;

  reloc_deps_e *src_edge;
  reloc_deps_e *tgt_edge;

  if (deps_has_sec_edge (deps, src_sec, tgt_sec))
    return;

  src_sec_deps = xtensa_get_section_deps (deps, src_sec);
  if (src_sec_deps == NULL)
    {
      /* Add a section.  */
      src_sec_deps = xmalloc (sizeof (reloc_deps_section));
      memset (src_sec_deps, 0, sizeof (reloc_deps_section));
      src_sec_deps->is_only_literal = 0;
      src_sec_deps->preds = NULL;
      src_sec_deps->succs = NULL;
      xtensa_set_section_deps (deps, src_sec, src_sec_deps);
      xtensa_append_section_deps (deps, src_sec);
    }

  tgt_sec_deps = xtensa_get_section_deps (deps, tgt_sec);
  if (tgt_sec_deps == NULL)
    {
      /* Add a section.  */
      tgt_sec_deps = xmalloc (sizeof (reloc_deps_section));
      memset (tgt_sec_deps, 0, sizeof (reloc_deps_section));
      tgt_sec_deps->is_only_literal = 0;
      tgt_sec_deps->preds = NULL;
      tgt_sec_deps->succs = NULL;
      xtensa_set_section_deps (deps, tgt_sec, tgt_sec_deps);
      xtensa_append_section_deps (deps, tgt_sec);
    }

  /* Add the edges.  */
  src_edge = xmalloc (sizeof (reloc_deps_e));
  memset (src_edge, 0, sizeof (reloc_deps_e));
  src_edge->src = src_sec;
  src_edge->tgt = tgt_sec;
  src_edge->next = src_sec_deps->succs;
  src_sec_deps->succs = src_edge;

  tgt_edge = xmalloc (sizeof (reloc_deps_e));
  memset (tgt_edge, 0, sizeof (reloc_deps_e));
  tgt_edge->src = src_sec;
  tgt_edge->tgt = tgt_sec;
  tgt_edge->next = tgt_sec_deps->preds;
  tgt_sec_deps->preds = tgt_edge;
}


static void
build_deps_graph_callback (asection *src_sec,
			   bfd_vma src_offset ATTRIBUTE_UNUSED,
			   asection *target_sec,
			   bfd_vma target_offset ATTRIBUTE_UNUSED,
			   void *closure)
{
  reloc_deps_graph *deps = closure;

  /* If the target is defined.  */
  if (target_sec != NULL)
    add_deps_edge (deps, src_sec, target_sec);
}


static reloc_deps_graph *
ld_build_required_section_dependence (lang_statement_union_type *s)
{
  reloc_deps_graph *deps;
  xtensa_ld_iter_stack *stack = NULL;

  deps = xmalloc (sizeof (reloc_deps_graph));
  deps->sections = NULL;
  deps->count = 0;
  deps->size = 0;

  for (iter_stack_create (&stack, s);
       !iter_stack_empty (&stack);
       iter_stack_next (&stack))
    {
      lang_statement_union_type *l = iter_stack_current (&stack);

      if (l->header.type == lang_input_section_enum)
	{
	  lang_input_section_type *input;
	  input = &l->input_section;
	  xtensa_callback_required_dependence (input->ifile->the_bfd,
					       input->section,
					       &link_info,
					       /* Use the same closure.  */
					       build_deps_graph_callback,
					       deps);
	}
    }
  return deps;
}


#if EXTRA_VALIDATION
static size_t
ld_count_children (lang_statement_union_type *s)
{
  size_t count = 0;
  xtensa_ld_iter_stack *stack = NULL;
  for (iter_stack_create (&stack, s);
       !iter_stack_empty (&stack);
       iter_stack_next (&stack))
    {
      lang_statement_union_type *l = iter_stack_current (&stack);
      ASSERT (l != NULL);
      count++;
    }
  return count;
}
#endif /* EXTRA_VALIDATION */


static void
xtensa_wild_group_interleave_callback (lang_statement_union_type *statement)
{
  lang_wild_statement_type *w;
  reloc_deps_graph *deps;
  if (statement->header.type == lang_wild_statement_enum)
    {
#if EXTRA_VALIDATION
      size_t old_child_count;
      size_t new_child_count;
#endif
      bfd_boolean no_reorder;

      w = &statement->wild_statement;

      no_reorder = FALSE;

      /* If it has 0 or 1 section bound, then do not reorder.  */
      if (w->children.head == NULL
	  || (w->children.head->header.type == lang_input_section_enum
	      && w->children.head->header.next == NULL))
	no_reorder = TRUE;

      if (w->filenames_sorted)
	no_reorder = TRUE;

      /* Check for sorting in a section list wildcard spec as well.  */
      if (!no_reorder)
	{
	  struct wildcard_list *l;
	  for (l = w->section_list; l != NULL; l = l->next)
	    {
	      if (l->spec.sorted == TRUE)
		{
		  no_reorder = TRUE;
		  break;
		}
	    }
	}

      /* Special case until the NOREORDER linker directive is supported:
	 *(.init) output sections and *(.fini) specs may NOT be reordered.  */

      /* Check for sorting in a section list wildcard spec as well.  */
      if (!no_reorder)
	{
	  struct wildcard_list *l;
	  for (l = w->section_list; l != NULL; l = l->next)
	    {
	      if (l->spec.name
		  && ((strcmp (".init", l->spec.name) == 0)
		      || (strcmp (".fini", l->spec.name) == 0)))
		{
		  no_reorder = TRUE;
		  break;
		}
	    }
	}

#if EXTRA_VALIDATION
      old_child_count = ld_count_children (statement);
#endif

      /* It is now officially a target.  Build the graph of source
	 section -> target section (kept as a list of edges).  */
      deps = ld_build_required_section_dependence (statement);

      /* If this wildcard does not reorder....  */
      if (!no_reorder && deps->count != 0)
	{
	  /* First check for reverse dependences.  Fix if possible.  */
	  xtensa_layout_wild (deps, w);

	  xtensa_move_dependencies_to_front (deps, w);
#if EXTRA_VALIDATION
	  new_child_count = ld_count_children (statement);
	  ASSERT (new_child_count == old_child_count);
#endif

	  xtensa_colocate_literals (deps, statement);

#if EXTRA_VALIDATION
	  new_child_count = ld_count_children (statement);
	  ASSERT (new_child_count == old_child_count);
#endif
	}

      /* Clean up.  */
      free_reloc_deps_graph (deps);
    }
}


static void
xtensa_wild_group_interleave (lang_statement_union_type *s)
{
  lang_for_each_statement_worker (xtensa_wild_group_interleave_callback, s);
}


static void
xtensa_layout_wild (const reloc_deps_graph *deps,
		    lang_wild_statement_type *w)
{
  /* If it does not fit initially, we need to do this step.  Move all
     of the wild literal sections to a new list, then move each of
     them back in just before the first section they depend on.  */
  lang_statement_union_type **s_p;
#if EXTRA_VALIDATION
  size_t old_count, new_count;
  size_t ct1, ct2;
#endif

  lang_wild_statement_type literal_wild;
  literal_wild.header.next = NULL;
  literal_wild.header.type = lang_wild_statement_enum;
  literal_wild.filename = NULL;
  literal_wild.filenames_sorted = FALSE;
  literal_wild.section_list = NULL;
  literal_wild.keep_sections = FALSE;
  literal_wild.children.head = NULL;
  literal_wild.children.tail = &literal_wild.children.head;

#if EXTRA_VALIDATION
  old_count = ld_count_children ((lang_statement_union_type*) w);
#endif

  s_p = &w->children.head;
  while (*s_p != NULL)
    {
      lang_statement_union_type *l = *s_p;
      if (l->header.type == lang_input_section_enum)
	{
	  if (section_is_target (deps, l)
	      && ! section_is_source (deps, l))
	    {
	      /* Detach.  */
	      *s_p = l->header.next;
	      if (*s_p == NULL)
		w->children.tail = s_p;
	      l->header.next = NULL;

	      /* Append.  */
	      *literal_wild.children.tail = l;
	      literal_wild.children.tail = &l->header.next;
	      continue;
	    }
	}
      s_p = &(*s_p)->header.next;
    }

#if EXTRA_VALIDATION
  ct1 = ld_count_children ((lang_statement_union_type*) w);
  ct2 = ld_count_children ((lang_statement_union_type*) &literal_wild);

  ASSERT (old_count == (ct1 + ct2));
#endif

  /* Now place them back in front of their dependent sections.  */

  while (literal_wild.children.head != NULL)
    {
      lang_statement_union_type *lit = literal_wild.children.head;
      bfd_boolean placed = FALSE;

#if EXTRA_VALIDATION
      ASSERT (ct2 > 0);
      ct2--;
#endif

      /* Detach.  */
      literal_wild.children.head = lit->header.next;
      if (literal_wild.children.head == NULL)
	literal_wild.children.tail = &literal_wild.children.head;
      lit->header.next = NULL;

      /* Find a spot to place it.  */
      for (s_p = &w->children.head; *s_p != NULL; s_p = &(*s_p)->header.next)
	{
	  lang_statement_union_type *src = *s_p;
	  if (deps_has_edge (deps, src, lit))
	    {
	      /* Place it here.  */
	      lit->header.next = *s_p;
	      *s_p = lit;
	      placed = TRUE;
	      break;
	    }
	}

      if (!placed)
	{
	  /* Put it at the end.  */
	  *w->children.tail = lit;
	  w->children.tail = &lit->header.next;
	}
    }

#if EXTRA_VALIDATION
  new_count = ld_count_children ((lang_statement_union_type*) w);
  ASSERT (new_count == old_count);
#endif
}


static void
xtensa_colocate_output_literals_callback (lang_statement_union_type *statement)
{
  lang_output_section_statement_type *os;
  reloc_deps_graph *deps;
  if (statement->header.type == lang_output_section_statement_enum)
    {
      /* Now, we walk over the contours of the output section statement.

	 First we build the literal section dependences as before.

	 At the first uniquely_literal section, we mark it as a good
	 spot to place other literals.  Continue walking (and counting
	 sizes) until we find the next literal section.  If this
	 section can be moved to the first one, then we move it.  If
	 we every find a modification of ".", start over.  If we find
	 a labeling of the current location, start over.  Finally, at
	 the end, if we require page alignment, add page alignments.  */

#if EXTRA_VALIDATION
      size_t old_child_count;
      size_t new_child_count;
#endif
      bfd_boolean no_reorder = FALSE;

      os = &statement->output_section_statement;

#if EXTRA_VALIDATION
      old_child_count = ld_count_children (statement);
#endif

      /* It is now officially a target.  Build the graph of source
	 section -> target section (kept as a list of edges).  */

      deps = ld_build_required_section_dependence (statement);

      /* If this wildcard does not reorder....  */
      if (!no_reorder)
	{
	  /* First check for reverse dependences.  Fix if possible.  */
	  xtensa_colocate_literals (deps, statement);

#if EXTRA_VALIDATION
	  new_child_count = ld_count_children (statement);
	  ASSERT (new_child_count == old_child_count);
#endif
	}

      /* Insert align/offset assignment statement.  */
      if (xtensa_use_literal_pages)
	{
	  ld_xtensa_insert_page_offsets (0, statement, deps,
					 xtensa_use_literal_pages);
	  lang_for_each_statement_worker (xtensa_ldlang_clear_addresses,
					  statement);
	}

      /* Clean up.  */
      free_reloc_deps_graph (deps);
    }
}


static void
xtensa_colocate_output_literals (lang_statement_union_type *s)
{
  lang_for_each_statement_worker (xtensa_colocate_output_literals_callback, s);
}


static void
xtensa_ldlang_clear_addresses (lang_statement_union_type *statement)
{
  switch (statement->header.type)
    {
    case lang_input_section_enum:
      {
	asection *bfd_section = statement->input_section.section;
	bfd_section->output_offset = 0;
      }
      break;
    default:
      break;
    }
}


static bfd_vma
ld_assign_relative_paged_dot (bfd_vma dot,
			      lang_statement_union_type *s,
			      const reloc_deps_graph *deps ATTRIBUTE_UNUSED,
			      bfd_boolean lit_align)
{
  /* Walk through all of the input statements in this wild statement
     assign dot to all of them.  */

  xtensa_ld_iter_stack *stack = NULL;
  xtensa_ld_iter_stack **stack_p = &stack;

  bfd_boolean first_section = FALSE;
  bfd_boolean in_literals = FALSE;

  for (iter_stack_create (stack_p, s);
       !iter_stack_empty (stack_p);
       iter_stack_next (stack_p))
    {
      lang_statement_union_type *l = iter_stack_current (stack_p);

      switch (l->header.type)
	{
	case lang_input_section_enum:
	  {
	    asection *section = l->input_section.section;
	    size_t align_pow = section->alignment_power;
	    bfd_boolean do_xtensa_alignment = FALSE;

	    if (lit_align)
	      {
		bfd_boolean sec_is_target = section_is_target (deps, l);
		bfd_boolean sec_is_source = section_is_source (deps, l);

		if (section->_raw_size != 0
		    && (first_section
			|| (in_literals && !sec_is_target)
			|| (!in_literals && sec_is_target)))
		  {
		    do_xtensa_alignment = TRUE;
		  }
		first_section = FALSE;
		if (section->_raw_size != 0)
		  in_literals = (sec_is_target && !sec_is_source);
	      }

	    if (do_xtensa_alignment && xtensa_page_power != 0)
	      dot += (1 << xtensa_page_power);

	    dot = align_power (dot, align_pow);
	    section->output_offset = dot;
	    dot += section->_raw_size;
	  }
	  break;
	case lang_fill_statement_enum:
	  dot += l->fill_statement.size;
	  break;
	case lang_padding_statement_enum:
	  dot += l->padding_statement.size;
	  break;
	default:
	  break;
	}
    }
  return dot;
}


static bfd_boolean
ld_local_file_relocations_fit (lang_statement_union_type *statement,
			       const reloc_deps_graph *deps ATTRIBUTE_UNUSED)
{
  /* Walk over all of the dependencies that we identified and make
     sure that IF the source and target are here (addr != 0):
     1) target addr < source addr
     2) (roundup(source + source_size, 4) - rounddown(target, 4))
        < (256K - (1 << bad align))
     Need a worst-case proof....  */

  xtensa_ld_iter_stack *stack = NULL;
  xtensa_ld_iter_stack **stack_p = &stack;
  size_t max_align_power = 0;
  size_t align_penalty = 256;
  reloc_deps_e *e;
  size_t i;

  /* Find the worst-case alignment requirement for this set of statements.  */
  for (iter_stack_create (stack_p, statement);
       !iter_stack_empty (stack_p);
       iter_stack_next (stack_p))
    {
      lang_statement_union_type *l = iter_stack_current (stack_p);
      if (l->header.type == lang_input_section_enum)
	{
	  lang_input_section_type *input = &l->input_section;
	  asection *section = input->section;
	  if (section->alignment_power > max_align_power)
	    max_align_power = section->alignment_power;
	}
    }

  /* Now check that everything fits.  */
  for (i = 0; i < deps->count; i++)
    {
      asection *sec = deps->sections[i];
      const reloc_deps_section *deps_section =
	xtensa_get_section_deps (deps, sec);
      if (deps_section)
	{
	  /* We choose to walk through the successors.  */
	  for (e = deps_section->succs; e != NULL; e = e->next)
	    {
	      if (e->src != e->tgt
		  && e->src->output_section == e->tgt->output_section
		  && e->src->output_offset != 0
		  && e->tgt->output_offset != 0)
		{
		  bfd_vma l32r_addr =
		    align_power (e->src->output_offset + e->src->_raw_size, 2);
		  bfd_vma target_addr = e->tgt->output_offset & ~3;
		  if (l32r_addr < target_addr)
		    {
		      fprintf (stderr, "Warning: "
			       "l32r target section before l32r\n");
		      return FALSE;
		    }

		  if (l32r_addr - target_addr > 256 * 1024 - align_penalty)
		    return FALSE;
		}
	    }
	}
    }

  return TRUE;
}


static bfd_vma
ld_xtensa_insert_page_offsets (bfd_vma dot,
			       lang_statement_union_type *s,
			       reloc_deps_graph *deps,
			       bfd_boolean lit_align)
{
  xtensa_ld_iter_stack *stack = NULL;
  xtensa_ld_iter_stack **stack_p = &stack;

  bfd_boolean first_section = FALSE;
  bfd_boolean in_literals = FALSE;

  if (!lit_align)
    return FALSE;

  for (iter_stack_create (stack_p, s);
       !iter_stack_empty (stack_p);
       iter_stack_next (stack_p))
    {
      lang_statement_union_type *l = iter_stack_current (stack_p);

      switch (l->header.type)
	{
	case lang_input_section_enum:
	  {
	    asection *section = l->input_section.section;
	    bfd_boolean do_xtensa_alignment = FALSE;

	    if (lit_align)
	      {
		if (section->_raw_size != 0
		    && (first_section
			|| (in_literals && !section_is_target (deps, l))
			|| (!in_literals && section_is_target (deps, l))))
		  {
		    do_xtensa_alignment = TRUE;
		  }
		first_section = FALSE;
		if (section->_raw_size != 0)
		  {
		    in_literals = (section_is_target (deps, l)
				   && !section_is_source (deps, l));
		  }
	      }

	    if (do_xtensa_alignment && xtensa_page_power != 0)
	      {
		/* Create an expression that increments the current address,
		   i.e., "dot", by (1 << xtensa_align_power).  */
		etree_type *name_op = exp_nameop (NAME, ".");
		etree_type *addend_op = exp_intop (1 << xtensa_page_power);
		etree_type *add_op = exp_binop ('+', name_op, addend_op);
		etree_type *assign_op = exp_assop ('=', ".", add_op);

		lang_assignment_statement_type *assign_stmt;
		lang_statement_union_type *assign_union;
		lang_statement_list_type tmplist;
		lang_statement_list_type *old_stat_ptr = stat_ptr;

		/* There is hidden state in "lang_add_assignment".  It
		   appends the new assignment statement to the stat_ptr
		   list.  Thus, we swap it before and after the call.  */

		tmplist.head = NULL;
		tmplist.tail = &tmplist.head;

		stat_ptr = &tmplist;
		/* Warning: side effect; statement appended to stat_ptr.  */
		assign_stmt = lang_add_assignment (assign_op);
		assign_union = (lang_statement_union_type *) assign_stmt;
		stat_ptr = old_stat_ptr;

		assign_union->header.next = l;
		*(*stack_p)->iterloc.loc = assign_union;
		iter_stack_next (stack_p);
	      }
	  }
	  break;
	default:
	  break;
	}
    }
  return dot;
}

EOF

# Define some shell vars to insert bits of code into the standard elf
# parse_args and list_options functions.
#
PARSE_AND_LIST_PROLOGUE='
#define OPTION_NO_RELAX			301
'

PARSE_AND_LIST_LONGOPTS='
  { "no-relax", no_argument, NULL, OPTION_NO_RELAX},
'

PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("  --no-relax\t\tDo not relax branches or coalesce literals\n"));
'

PARSE_AND_LIST_ARGS_CASES='
    case OPTION_NO_RELAX:
      disable_relaxation = TRUE;
      break;
'

# Replace some of the standard ELF functions with our own versions.
#
LDEMUL_BEFORE_PARSE=elf_xtensa_before_parse
LDEMUL_CHOOSE_TARGET=elf_xtensa_choose_target
LDEMUL_PLACE_ORPHAN=elf_xtensa_place_orphan
LDEMUL_BEFORE_ALLOCATION=elf_xtensa_before_allocation

