# This shell script emits a C file. -*- C -*-
#   Copyright 2002, 2003, 2004 Free Software Foundation, Inc.
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

# This file is sourced from elf32.em, and defines extra powerpc64-elf
# specific routines.
#
cat >>e${EMULATION_NAME}.c <<EOF

#include "ldctor.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf64-ppc.h"

/* Fake input file for stubs.  */
static lang_input_statement_type *stub_file;
static int stub_added = 0;

/* Whether we need to call ppc_layout_sections_again.  */
static int need_laying_out = 0;

/* Maximum size of a group of input sections that can be handled by
   one stub section.  A value of +/-1 indicates the bfd back-end
   should use a suitable default size.  */
static bfd_signed_vma group_size = 1;

/* Whether to add ".foo" entries for each "foo" in a version script.  */
static int dotsyms = 1;

/* Whether to run tls optimization.  */
static int notlsopt = 0;

/* Whether to emit symbols for stubs.  */
static int emit_stub_syms = 0;

static asection *toc_section = 0;


/* This is called before the input files are opened.  We create a new
   fake input file to hold the stub sections.  */

static void
ppc_create_output_section_statements (void)
{
  extern const bfd_target bfd_elf64_powerpc_vec;
  extern const bfd_target bfd_elf64_powerpcle_vec;
  asection *sect;

  if (link_info.hash->creator != &bfd_elf64_powerpc_vec
      && link_info.hash->creator != &bfd_elf64_powerpcle_vec)
    return;

  link_info.wrap_char = '.';

  stub_file = lang_add_input_file ("linker stubs",
				   lang_input_file_is_fake_enum,
				   NULL);
  stub_file->the_bfd = bfd_create ("linker stubs", output_bfd);
  if (stub_file->the_bfd == NULL
      || !bfd_set_arch_mach (stub_file->the_bfd,
			     bfd_get_arch (output_bfd),
			     bfd_get_mach (output_bfd)))
    {
      einfo ("%X%P: can not create BFD %E\n");
      return;
    }

  if (bfd_get_section_by_name (stub_file->the_bfd, ".note.GNU-stack") == NULL)
    sect = bfd_make_section (stub_file->the_bfd, ".note.GNU-stack");
  ldlang_add_file (stub_file);
  ppc64_elf_init_stub_bfd (stub_file->the_bfd, &link_info);
}

static void
ppc_after_open (void)
{
  if (!ppc64_elf_mark_entry_syms (&link_info))
    {
      einfo ("%X%P: can not mark entry symbols %E\n");
      return;
    }

  gld${EMULATION_NAME}_after_open ();
}

static void
ppc_before_allocation (void)
{
  if (stub_file != NULL)
    {
      if (!ppc64_elf_edit_opd (output_bfd, &link_info))
	{
	  einfo ("%X%P: can not edit opd %E\n");
	  return;
	}

      if (ppc64_elf_tls_setup (output_bfd, &link_info) && !notlsopt)
	{
	  /* Size the sections.  This is premature, but we want to know the
	     TLS segment layout so that certain optimizations can be done.  */
	  lang_size_sections (stat_ptr->head, abs_output_section,
			      &stat_ptr->head, 0, 0, NULL, TRUE);

	  if (!ppc64_elf_tls_optimize (output_bfd, &link_info))
	    {
	      einfo ("%X%P: TLS problem %E\n");
	      return;
	    }

	  /* We must not cache anything from the preliminary sizing.  */
	  elf_tdata (output_bfd)->program_header_size = 0;
	  lang_reset_memory_regions ();
	}
    }

  gld${EMULATION_NAME}_before_allocation ();
}

struct hook_stub_info
{
  lang_statement_list_type add;
  asection *input_section;
};

/* Traverse the linker tree to find the spot where the stub goes.  */

static bfd_boolean
hook_in_stub (struct hook_stub_info *info, lang_statement_union_type **lp)
{
  lang_statement_union_type *l;
  bfd_boolean ret;

  for (; (l = *lp) != NULL; lp = &l->header.next)
    {
      switch (l->header.type)
	{
	case lang_constructors_statement_enum:
	  ret = hook_in_stub (info, &constructor_list.head);
	  if (ret)
	    return ret;
	  break;

	case lang_output_section_statement_enum:
	  ret = hook_in_stub (info,
			      &l->output_section_statement.children.head);
	  if (ret)
	    return ret;
	  break;

	case lang_wild_statement_enum:
	  ret = hook_in_stub (info, &l->wild_statement.children.head);
	  if (ret)
	    return ret;
	  break;

	case lang_group_statement_enum:
	  ret = hook_in_stub (info, &l->group_statement.children.head);
	  if (ret)
	    return ret;
	  break;

	case lang_input_section_enum:
	  if (l->input_section.section == info->input_section)
	    {
	      /* We've found our section.  Insert the stub immediately
		 before its associated input section.  */
	      *lp = info->add.head;
	      *(info->add.tail) = l;
	      return TRUE;
	    }
	  break;

	case lang_data_statement_enum:
	case lang_reloc_statement_enum:
	case lang_object_symbols_statement_enum:
	case lang_output_statement_enum:
	case lang_target_statement_enum:
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
  return FALSE;
}


/* Call-back for ppc64_elf_size_stubs.  */

/* Create a new stub section, and arrange for it to be linked
   immediately before INPUT_SECTION.  */

static asection *
ppc_add_stub_section (const char *stub_sec_name, asection *input_section)
{
  asection *stub_sec;
  flagword flags;
  asection *output_section;
  const char *secname;
  lang_output_section_statement_type *os;
  struct hook_stub_info info;

  stub_sec = bfd_make_section_anyway (stub_file->the_bfd, stub_sec_name);
  if (stub_sec == NULL)
    goto err_ret;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_READONLY | SEC_CODE
	   | SEC_HAS_CONTENTS | SEC_RELOC | SEC_IN_MEMORY | SEC_KEEP);
  if (!bfd_set_section_flags (stub_file->the_bfd, stub_sec, flags))
    goto err_ret;

  output_section = input_section->output_section;
  secname = bfd_get_section_name (output_section->owner, output_section);
  os = lang_output_section_find (secname);

  info.input_section = input_section;
  lang_list_init (&info.add);
  lang_add_section (&info.add, stub_sec, os, stub_file);

  if (info.add.head == NULL)
    goto err_ret;

  stub_added = 1;
  if (hook_in_stub (&info, &os->children.head))
    return stub_sec;

 err_ret:
  einfo ("%X%P: can not make stub section: %E\n");
  return NULL;
}


/* Another call-back for ppc64_elf_size_stubs.  */

static void
ppc_layout_sections_again (void)
{
  /* If we have changed sizes of the stub sections, then we need
     to recalculate all the section offsets.  This may mean we need to
     add even more stubs.  */
  need_laying_out = 0;

  lang_reset_memory_regions ();

  /* Resize the sections.  */
  lang_size_sections (stat_ptr->head, abs_output_section,
		      &stat_ptr->head, 0, 0, NULL, TRUE);

  /* Recalculate TOC base.  */
  ldemul_after_allocation ();

  /* Do the assignments again.  */
  lang_do_assignments (stat_ptr->head, abs_output_section, NULL, 0);
}


/* Call the back-end function to set TOC base after we have placed all
   the sections.  */
static void
gld${EMULATION_NAME}_after_allocation (void)
{
  if (!link_info.relocatable)
    _bfd_set_gp_value (output_bfd, ppc64_elf_toc (output_bfd));
}


static void
build_toc_list (lang_statement_union_type *statement)
{
  if (statement->header.type == lang_input_section_enum
      && !statement->input_section.ifile->just_syms_flag
      && statement->input_section.section->output_section == toc_section)
    ppc64_elf_next_toc_section (&link_info, statement->input_section.section);
}


static void
build_section_lists (lang_statement_union_type *statement)
{
  if (statement->header.type == lang_input_section_enum
      && !statement->input_section.ifile->just_syms_flag
      && statement->input_section.section->output_section != NULL
      && statement->input_section.section->output_section->owner == output_bfd)
    {
      if (!ppc64_elf_next_input_section (&link_info,
					 statement->input_section.section))
	einfo ("%X%P: can not size stub section: %E\n");
    }
}


/* Final emulation specific call.  */

static void
gld${EMULATION_NAME}_finish (void)
{
  /* e_entry on PowerPC64 points to the function descriptor for
     _start.  If _start is missing, default to the first function
     descriptor in the .opd section.  */
  entry_section = ".opd";

  /* bfd_elf_discard_info just plays with debugging sections,
     ie. doesn't affect any code, so we can delay resizing the
     sections.  It's likely we'll resize everything in the process of
     adding stubs.  */
  if (bfd_elf_discard_info (output_bfd, &link_info))
    need_laying_out = 1;

  /* If generating a relocatable output file, then we don't have any
     stubs.  */
  if (stub_file != NULL && !link_info.relocatable)
    {
      int ret = ppc64_elf_setup_section_lists (output_bfd, &link_info);
      if (ret != 0)
	{
	  if (ret < 0)
	    {
	      einfo ("%X%P: can not size stub section: %E\n");
	      return;
	    }

	  toc_section = bfd_get_section_by_name (output_bfd, ".got");
	  if (toc_section != NULL)
	    lang_for_each_statement (build_toc_list);

	  ppc64_elf_reinit_toc (output_bfd, &link_info);

	  lang_for_each_statement (build_section_lists);

	  /* Call into the BFD backend to do the real work.  */
	  if (!ppc64_elf_size_stubs (output_bfd,
				     &link_info,
				     group_size,
				     &ppc_add_stub_section,
				     &ppc_layout_sections_again))
	    {
	      einfo ("%X%P: can not size stub section: %E\n");
	      return;
	    }
	}
    }

  if (need_laying_out)
    ppc_layout_sections_again ();

  if (stub_added)
    {
      char *msg = NULL;
      char *line, *endline;

      if (!ppc64_elf_build_stubs (emit_stub_syms, &link_info,
				  config.stats ? &msg : NULL))
	einfo ("%X%P: can not build stubs: %E\n");

      for (line = msg; line != NULL; line = endline)
	{
	  endline = strchr (line, '\n');
	  if (endline != NULL)
	    *endline++ = '\0';
	  fprintf (stderr, "%s: %s\n", program_name, line);
	}
      if (msg != NULL)
	free (msg);
    }
}


/* Add a pattern matching ".foo" for every "foo" in a version script.

   The reason for doing this is that many shared library version
   scripts export a selected set of functions or data symbols, forcing
   others local.  eg.

   . VERS_1 {
   .       global:
   .               this; that; some; thing;
   .       local:
   .               *;
   .   };

   To make the above work for PowerPC64, we need to export ".this",
   ".that" and so on, otherwise only the function descriptor syms are
   exported.  Lack of an exported function code sym may cause a
   definition to be pulled in from a static library.  */

static struct bfd_elf_version_expr *
gld${EMULATION_NAME}_new_vers_pattern (struct bfd_elf_version_expr *entry)
{
  struct bfd_elf_version_expr *dot_entry;
  unsigned int len;
  char *dot_pat;

  if (!dotsyms || entry->pattern[0] == '*' || entry->pattern[0] == '.')
    return entry;

  dot_entry = xmalloc (sizeof *dot_entry);
  *dot_entry = *entry;
  dot_entry->next = entry;
  len = strlen (entry->pattern) + 2;
  dot_pat = xmalloc (len);
  dot_pat[0] = '.';
  memcpy (dot_pat + 1, entry->pattern, len - 1);
  dot_entry->pattern = dot_pat;
  return dot_entry;
}


/* Avoid processing the fake stub_file in vercheck, stat_needed and
   check_needed routines.  */

static void (*real_func) (lang_input_statement_type *);

static void ppc_for_each_input_file_wrapper (lang_input_statement_type *l)
{
  if (l != stub_file)
    (*real_func) (l);
}

static void
ppc_lang_for_each_input_file (void (*func) (lang_input_statement_type *))
{
  real_func = func;
  lang_for_each_input_file (&ppc_for_each_input_file_wrapper);
}

#define lang_for_each_input_file ppc_lang_for_each_input_file

EOF

# Define some shell vars to insert bits of code into the standard elf
# parse_args and list_options functions.
#
PARSE_AND_LIST_PROLOGUE='
#define OPTION_STUBGROUP_SIZE		301
#define OPTION_STUBSYMS			(OPTION_STUBGROUP_SIZE + 1)
#define OPTION_DOTSYMS			(OPTION_STUBSYMS + 1)
#define OPTION_NO_DOTSYMS		(OPTION_DOTSYMS + 1)
#define OPTION_NO_TLS_OPT		(OPTION_NO_DOTSYMS + 1)
'

PARSE_AND_LIST_LONGOPTS='
  { "stub-group-size", required_argument, NULL, OPTION_STUBGROUP_SIZE },
  { "emit-stub-syms", no_argument, NULL, OPTION_STUBSYMS },
  { "dotsyms", no_argument, NULL, OPTION_DOTSYMS },
  { "no-dotsyms", no_argument, NULL, OPTION_NO_DOTSYMS },
  { "no-tls-optimize", no_argument, NULL, OPTION_NO_TLS_OPT },
'

PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("\
  --stub-group-size=N   Maximum size of a group of input sections that can be\n\
                          handled by one stub section.  A negative value\n\
                          locates all stubs before their branches (with a\n\
                          group size of -N), while a positive value allows\n\
                          two groups of input sections, one before, and one\n\
                          after each stub section.  Values of +/-1 indicate\n\
                          the linker should choose suitable defaults.\n"
		   ));
  fprintf (file, _("\
  --emit-stub-syms      Label linker stubs with a symbol.\n"
		   ));
  fprintf (file, _("\
  --dotsyms             For every version pattern \"foo\" in a version script,\n\
                          add \".foo\" so that function code symbols are\n\
                          treated the same as function descriptor symbols.\n\
                          Defaults to on.\n"
		   ));
  fprintf (file, _("\
  --no-dotsyms          Don'\''t do anything special in version scripts.\n"
		   ));
  fprintf (file, _("\
  --no-tls-optimize     Don'\''t try to optimize TLS accesses.\n"
		   ));
'

PARSE_AND_LIST_ARGS_CASES='
    case OPTION_STUBGROUP_SIZE:
      {
	const char *end;
        group_size = bfd_scan_vma (optarg, &end, 0);
        if (*end)
	  einfo (_("%P%F: invalid number `%s'\''\n"), optarg);
      }
      break;

    case OPTION_STUBSYMS:
      emit_stub_syms = 1;
      break;

    case OPTION_DOTSYMS:
      dotsyms = 1;
      break;

    case OPTION_NO_DOTSYMS:
      dotsyms = 0;
      break;

    case OPTION_NO_TLS_OPT:
      notlsopt = 1;
      break;
'

# Put these extra ppc64elf routines in ld_${EMULATION_NAME}_emulation
#
LDEMUL_AFTER_OPEN=ppc_after_open
LDEMUL_BEFORE_ALLOCATION=ppc_before_allocation
LDEMUL_AFTER_ALLOCATION=gld${EMULATION_NAME}_after_allocation
LDEMUL_FINISH=gld${EMULATION_NAME}_finish
LDEMUL_CREATE_OUTPUT_SECTION_STATEMENTS=ppc_create_output_section_statements
LDEMUL_NEW_VERS_PATTERN=gld${EMULATION_NAME}_new_vers_pattern
