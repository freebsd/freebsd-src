# This shell script emits a C file. -*- C -*-
#   Copyright 1991, 1993, 1994, 1997, 1999, 2000, 2001, 2002, 2003, 2004,
#   2005 Free Software Foundation, Inc.
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
# Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.
#

# This file is sourced from elf32.em, and defines extra hppa-elf
# specific routines.
#
cat >>e${EMULATION_NAME}.c <<EOF

#include "ldctor.h"
#include "elf32-hppa.h"


/* Fake input file for stubs.  */
static lang_input_statement_type *stub_file;

/* Type of import/export stubs to build.  For a single sub-space model,
   we can build smaller import stubs and there is no need for export
   stubs.  */
static int multi_subspace = 0;

/* Whether we need to call hppa_layout_sections_again.  */
static int need_laying_out = 0;

/* Maximum size of a group of input sections that can be handled by
   one stub section.  A value of +/-1 indicates the bfd back-end
   should use a suitable default size.  */
static bfd_signed_vma group_size = 1;

/* Stops the linker merging .text sections on a relocatable link,
   and adds millicode library to the list of input files.  */

static void
hppaelf_after_parse (void)
{
  if (link_info.relocatable)
    lang_add_unique (".text");

  /* Enable this once we split millicode stuff from libgcc:
     lang_add_input_file ("milli",
     			  lang_input_file_is_l_enum,
			  NULL);
  */
}

/* This is called before the input files are opened.  We create a new
   fake input file to hold the stub sections.  */

static void
hppaelf_create_output_section_statements (void)
{
  extern const bfd_target bfd_elf32_hppa_linux_vec;
  extern const bfd_target bfd_elf32_hppa_nbsd_vec;
  extern const bfd_target bfd_elf32_hppa_vec;

  if (link_info.hash->creator != &bfd_elf32_hppa_linux_vec
      && link_info.hash->creator != &bfd_elf32_hppa_nbsd_vec
      && link_info.hash->creator != &bfd_elf32_hppa_vec)
    return;

  stub_file = lang_add_input_file ("linker stubs",
				   lang_input_file_is_fake_enum,
				   NULL);
  stub_file->the_bfd = bfd_create ("linker stubs", output_bfd);
  if (stub_file->the_bfd == NULL
      || ! bfd_set_arch_mach (stub_file->the_bfd,
			      bfd_get_arch (output_bfd),
			      bfd_get_mach (output_bfd)))
    {
      einfo ("%X%P: can not create BFD %E\n");
      return;
    }

  stub_file->the_bfd->flags |= BFD_LINKER_CREATED;
  ldlang_add_file (stub_file);
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


/* Call-back for elf32_hppa_size_stubs.  */

/* Create a new stub section, and arrange for it to be linked
   immediately before INPUT_SECTION.  */

static asection *
hppaelf_add_stub_section (const char *stub_sec_name, asection *input_section)
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
  lang_add_section (&info.add, stub_sec, os);

  if (info.add.head == NULL)
    goto err_ret;

  if (hook_in_stub (&info, &os->children.head))
    return stub_sec;

 err_ret:
  einfo ("%X%P: can not make stub section: %E\n");
  return NULL;
}


/* Another call-back for elf32_hppa_size_stubs.  */

static void
hppaelf_layout_sections_again (void)
{
  /* If we have changed sizes of the stub sections, then we need
     to recalculate all the section offsets.  This may mean we need to
     add even more stubs.  */
  need_laying_out = 0;

  gld${EMULATION_NAME}_layout_sections_again ();
}


static void
build_section_lists (lang_statement_union_type *statement)
{
  if (statement->header.type == lang_input_section_enum)
    {
      asection *i = statement->input_section.section;

      if (!((lang_input_statement_type *) i->owner->usrdata)->just_syms_flag
	  && (i->flags & SEC_EXCLUDE) == 0
	  && i->output_section != NULL
	  && i->output_section->owner == output_bfd)
	{
	  elf32_hppa_next_input_section (&link_info, i);
	}
    }
}


/* Final emulation specific call.  For the PA we use this opportunity
   to build linker stubs.  */

static void
hppaelf_finish (void)
{
  /* bfd_elf_discard_info just plays with debugging sections,
     ie. doesn't affect any code, so we can delay resizing the
     sections.  It's likely we'll resize everything in the process of
     adding stubs.  */
  if (bfd_elf_discard_info (output_bfd, &link_info))
    need_laying_out = 1;

  /* If generating a relocatable output file, then we don't
     have to examine the relocs.  */
  if (stub_file != NULL && !link_info.relocatable)
    {
      int ret = elf32_hppa_setup_section_lists (output_bfd, &link_info);

      if (ret != 0)
	{
	  if (ret < 0)
	    {
	      einfo ("%X%P: can not size stub section: %E\n");
	      return;
	    }

	  lang_for_each_statement (build_section_lists);

	  /* Call into the BFD backend to do the real work.  */
	  if (! elf32_hppa_size_stubs (output_bfd,
				       stub_file->the_bfd,
				       &link_info,
				       multi_subspace,
				       group_size,
				       &hppaelf_add_stub_section,
				       &hppaelf_layout_sections_again))
	    {
	      einfo ("%X%P: can not size stub section: %E\n");
	      return;
	    }
	}
    }

  if (need_laying_out)
    hppaelf_layout_sections_again ();

  if (! link_info.relocatable)
    {
      /* Set the global data pointer.  */
      if (! elf32_hppa_set_gp (output_bfd, &link_info))
	{
	  einfo ("%X%P: can not set gp\n");
	  return;
	}

      /* Now build the linker stubs.  */
      if (stub_file != NULL && stub_file->the_bfd->sections != NULL)
	{
	  if (! elf32_hppa_build_stubs (&link_info))
	    einfo ("%X%P: can not build stubs: %E\n");
	}
    }

  finish_default ();
}


/* Avoid processing the fake stub_file in vercheck, stat_needed and
   check_needed routines.  */

static void (*real_func) (lang_input_statement_type *);

static void hppa_for_each_input_file_wrapper (lang_input_statement_type *l)
{
  if (l != stub_file)
    (*real_func) (l);
}

static void
hppa_lang_for_each_input_file (void (*func) (lang_input_statement_type *))
{
  real_func = func;
  lang_for_each_input_file (&hppa_for_each_input_file_wrapper);
}

#define lang_for_each_input_file hppa_lang_for_each_input_file

EOF

# Define some shell vars to insert bits of code into the standard elf
# parse_args and list_options functions.
#
PARSE_AND_LIST_PROLOGUE='
#define OPTION_MULTI_SUBSPACE		301
#define OPTION_STUBGROUP_SIZE		(OPTION_MULTI_SUBSPACE + 1)
'

PARSE_AND_LIST_LONGOPTS='
  { "multi-subspace", no_argument, NULL, OPTION_MULTI_SUBSPACE },
  { "stub-group-size", required_argument, NULL, OPTION_STUBGROUP_SIZE },
'

PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("\
  --multi-subspace      Generate import and export stubs to support\n\
                          multiple sub-space shared libraries\n"
		   ));
  fprintf (file, _("\
  --stub-group-size=N   Maximum size of a group of input sections that can be\n\
                          handled by one stub section.  A negative value\n\
                          locates all stubs before their branches (with a\n\
                          group size of -N), while a positive value allows\n\
                          two groups of input sections, one before, and one\n\
                          after each stub section.  Values of +/-1 indicate\n\
                          the linker should choose suitable defaults.\n"
		   ));
'

PARSE_AND_LIST_ARGS_CASES='
    case OPTION_MULTI_SUBSPACE:
      multi_subspace = 1;
      break;

    case OPTION_STUBGROUP_SIZE:
      {
	const char *end;
        group_size = bfd_scan_vma (optarg, &end, 0);
        if (*end)
	  einfo (_("%P%F: invalid number `%s'\''\n"), optarg);
      }
      break;
'

# Put these extra hppaelf routines in ld_${EMULATION_NAME}_emulation
#
LDEMUL_AFTER_PARSE=hppaelf_after_parse
LDEMUL_FINISH=hppaelf_finish
LDEMUL_CREATE_OUTPUT_SECTION_STATEMENTS=hppaelf_create_output_section_statements
