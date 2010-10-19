# This shell script emits a C file. -*- C -*-
#   Copyright 1991, 1993, 1994, 1997, 1999, 2000, 2001, 2002, 2003
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
# Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.
#

# This file is sourced from elf32.em, and defines extra m68hc12-elf
# and m68hc11-elf specific routines.  It is used to generate the
# HC11/HC12 trampolines to call a far function by using a normal 'jsr/bsr'.
#
# - The HC11/HC12 relocations are checked to see if there is a
#   R_M68HC11_16 relocation to a symbol marked with STO_M68HC12_FAR.
#   This relocation cannot be made on the symbol but must be made on
#   its trampoline
#   The trampolines to generate are collected during this pass
#   (See elf32_m68hc11_size_stubs)
#
# - The trampolines are generated in a ".tramp" section.  The generation
#   takes care of HC11 and HC12 specificities.
#   (See elf32_m68hc11_build_stubs)
#
# - During relocation the R_M68HC11_16 relocation to the far symbols
#   are redirected to the trampoline that was generated.
#
# Copied from hppaelf and adapted for M68HC11/M68HC12 specific needs.
#
cat >>e${EMULATION_NAME}.c <<EOF

#include "ldctor.h"
#include "elf32-m68hc1x.h"

static asection *m68hc11elf_add_stub_section (const char *, asection *);

/* Fake input file for stubs.  */
static lang_input_statement_type *stub_file;

/* By default the HC11/HC12 trampolines to call a far function using
   a normal 'bsr' and 'jsr' convention are generated during the link.
   The --no-trampoline option prevents that.  */
static int no_trampoline = 0;

/* Name of memory bank window in the MEMORY description.
   This is set by --bank-window option.  */
static const char* bank_window_name = 0;

static void
m68hc11_elf_${EMULATION_NAME}_before_allocation (void)
{
  lang_memory_region_type* region;
  int ret;

  gld${EMULATION_NAME}_before_allocation ();

  /* If generating a relocatable output file, then we don't
     have to generate the trampolines.  */
  if (link_info.relocatable)
    return;

  ret = elf32_m68hc11_setup_section_lists (output_bfd, &link_info);
  if (ret != 0 && no_trampoline == 0)
    {
      if (ret < 0)
	{
	  einfo ("%X%P: can not size stub section: %E\n");
	  return;
	}

      /* Call into the BFD backend to do the real work.  */
      if (!elf32_m68hc11_size_stubs (output_bfd,
				     stub_file->the_bfd,
				     &link_info,
				     &m68hc11elf_add_stub_section))
	{
	  einfo ("%X%P: can not size stub section: %E\n");
	  return;
	}
    }

  if (bank_window_name == 0)
    return;

  /* The 'bank_window_name' memory region is a special region that describes
     the memory bank window to access to paged memory.  For 68HC12
     this is fixed and should be:

     window (rx) : ORIGIN = 0x8000, LENGTH = 16K

     But for 68HC11 this is board specific.  The definition of such
     memory region allows to control how this paged memory is accessed.  */
  region = lang_memory_region_lookup (bank_window_name, FALSE);

  /* Check the length to see if it was defined in the script.  */
  if (region->length != 0)
    {
      struct m68hc11_page_info *pinfo;
      unsigned i;

      /* Get default values  */
      m68hc11_elf_get_bank_parameters (&link_info);
      pinfo = &m68hc11_elf_hash_table (&link_info)->pinfo;

      /* And override them with the region definition.  */
      pinfo->bank_size = region->length;
      pinfo->bank_shift = 0;
      for (i = pinfo->bank_size; i != 0; i >>= 1)
	pinfo->bank_shift++;
      pinfo->bank_shift--;
      pinfo->bank_size = 1L << pinfo->bank_shift;
      pinfo->bank_mask = (1 << pinfo->bank_shift) - 1;
      pinfo->bank_physical = region->origin;
      pinfo->bank_physical_end = region->origin + pinfo->bank_size;

      if (pinfo->bank_size != region->length)
	{
	  einfo (_("warning: the size of the 'window' memory region "
		   "is not a power of 2\n"));
	  einfo (_("warning: its size %d is truncated to %d\n"),
		 region->length, pinfo->bank_size);
	}
    }
}

/* This is called before the input files are opened.  We create a new
   fake input file to hold the stub sections.  */

static void
m68hc11elf_create_output_section_statements (void)
{
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
	  if (l->input_section.section == info->input_section
	      || strcmp (bfd_get_section_name (output_section,
					       l->input_section.section),
			 bfd_get_section_name (output_section,
					       info->input_section)) == 0)
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


/* Call-back for elf32_m68hc11_size_stubs.  */

/* Create a new stub section, and arrange for it to be linked
   immediately before INPUT_SECTION.  */

static asection *
m68hc11elf_add_stub_section (const char *stub_sec_name,
			     asection *tramp_section)
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

  output_section = tramp_section->output_section;
  secname = bfd_get_section_name (output_section->owner, output_section);
  os = lang_output_section_find (secname);

  /* Try to put the new section at the same place as an existing
     .tramp section.  Such .tramp section exists in most cases and
     contains the trampoline code.  This way we put the generated trampoline
     at the correct place.  */
  info.input_section = tramp_section;
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

/* Final emulation specific call.  For the 68HC12 we use this opportunity
   to build linker stubs.  */

static void
m68hc11elf_finish (void)
{
  /* Now build the linker stubs.  */
  if (stub_file->the_bfd->sections != NULL)
    {
      /* Call again the trampoline analyzer to initialize the trampoline
	 stubs with the correct symbol addresses.  Since there could have
	 been relaxation, the symbol addresses that were found during
	 first call may no longer be correct.  */
      if (!elf32_m68hc11_size_stubs (output_bfd,
				     stub_file->the_bfd,
				     &link_info, 0))
	{
	  einfo ("%X%P: can not size stub section: %E\n");
	  return;
	}
      if (!elf32_m68hc11_build_stubs (output_bfd, &link_info))
	einfo ("%X%P: can not build stubs: %E\n");
    }

  gld${EMULATION_NAME}_finish ();
}


/* Avoid processing the fake stub_file in vercheck, stat_needed and
   check_needed routines.  */

static void (*real_func) (lang_input_statement_type *);

static void m68hc11_for_each_input_file_wrapper (lang_input_statement_type *l)
{
  if (l != stub_file)
    (*real_func) (l);
}

static void
m68hc11_lang_for_each_input_file (void (*func) (lang_input_statement_type *))
{
  real_func = func;
  lang_for_each_input_file (&m68hc11_for_each_input_file_wrapper);
}

#define lang_for_each_input_file m68hc11_lang_for_each_input_file

EOF

# Define some shell vars to insert bits of code into the standard elf
# parse_args and list_options functions.
#
PARSE_AND_LIST_PROLOGUE='
#define OPTION_NO_TRAMPOLINE		300
#define OPTION_BANK_WINDOW		301
'

# The options are repeated below so that no abbreviations are allowed.
# Otherwise -s matches stub-group-size
PARSE_AND_LIST_LONGOPTS='
  { "no-trampoline", no_argument, NULL, OPTION_NO_TRAMPOLINE },
  { "bank-window",   required_argument, NULL, OPTION_BANK_WINDOW },
'

PARSE_AND_LIST_OPTIONS='
  fprintf (file, _(""
"  --no-trampoline         Do not generate the far trampolines used to call\n"
"                          a far function using 'jsr' or 'bsr'.\n"
"  --bank-window NAME      Specify the name of the memory region describing\n"
"                          the layout of the memory bank window.\n"
		   ));
'

PARSE_AND_LIST_ARGS_CASES='
    case OPTION_NO_TRAMPOLINE:
      no_trampoline = 1;
      break;
    case OPTION_BANK_WINDOW:
      bank_window_name = optarg;
      break;
'

# Put these extra m68hc11elf routines in ld_${EMULATION_NAME}_emulation
#
LDEMUL_BEFORE_ALLOCATION=m68hc11_elf_${EMULATION_NAME}_before_allocation
LDEMUL_FINISH=m68hc11elf_finish
LDEMUL_CREATE_OUTPUT_SECTION_STATEMENTS=m68hc11elf_create_output_section_statements
