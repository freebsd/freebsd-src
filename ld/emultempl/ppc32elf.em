# This shell script emits a C file. -*- C -*-
#   Copyright 2003, 2005 Free Software Foundation, Inc.
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

# This file is sourced from elf32.em, and defines extra powerpc32-elf
# specific routines.
#
cat >>e${EMULATION_NAME}.c <<EOF

#include "libbfd.h"
#include "elf32-ppc.h"

extern const bfd_target bfd_elf32_powerpc_vec;
extern const bfd_target bfd_elf32_powerpcle_vec;
extern const bfd_target bfd_elf32_powerpc_vxworks_vec;

static inline int
is_ppc_elf32_vec(const bfd_target * vec)
{
  return (vec == &bfd_elf32_powerpc_vec
	  || vec == &bfd_elf32_powerpc_vxworks_vec
	  || vec == &bfd_elf32_powerpcle_vec);
}

/* Whether to run tls optimization.  */
static int notlsopt = 0;

/* Whether to emit symbols for stubs.  */
static int emit_stub_syms = 0;

/* Chooses the correct place for .plt and .got.  */
static int old_plt = 0;
static int old_got = 0;

static void
ppc_after_open (void)
{
  if (is_ppc_elf32_vec (link_info.hash->creator))
    {
      int new_plt;
      int keep_new;
      unsigned int num_plt;
      unsigned int num_got;
      lang_output_section_statement_type *os;
      lang_output_section_statement_type *plt_os[2];
      lang_output_section_statement_type *got_os[2];

      emit_stub_syms |= link_info.emitrelocations;
      new_plt = ppc_elf_select_plt_layout (output_bfd, &link_info, old_plt,
					   emit_stub_syms);
      if (new_plt < 0)
	einfo ("%X%P: select_plt_layout problem %E\n");

      num_got = 0;
      num_plt = 0;
      for (os = &lang_output_section_statement.head->output_section_statement;
	   os != NULL;
	   os = os->next)
	{
	  if (os->constraint == SPECIAL && strcmp (os->name, ".plt") == 0)
	    {
	      if (num_plt < 2)
		plt_os[num_plt] = os;
	      ++num_plt;
	    }
	  if (os->constraint == SPECIAL && strcmp (os->name, ".got") == 0)
	    {
	      if (num_got < 2)
		got_os[num_got] = os;
	      ++num_got;
	    }
	}

      keep_new = new_plt == 1 ? 0 : -1;
      if (num_plt == 2)
	{
	  plt_os[0]->constraint = keep_new;
	  plt_os[1]->constraint = ~keep_new;
	}
      if (num_got == 2)
	{
	  if (old_got)
	    keep_new = -1;
	  got_os[0]->constraint = keep_new;
	  got_os[1]->constraint = ~keep_new;
	}
    }

  gld${EMULATION_NAME}_after_open ();
}

static void
ppc_before_allocation (void)
{
  if (is_ppc_elf32_vec (link_info.hash->creator))
    {
      if (ppc_elf_tls_setup (output_bfd, &link_info) && !notlsopt)
	{
	  if (!ppc_elf_tls_optimize (output_bfd, &link_info))
	    {
	      einfo ("%X%P: TLS problem %E\n");
	      return;
	    }
	}
    }
  gld${EMULATION_NAME}_before_allocation ();
}

EOF

# Define some shell vars to insert bits of code into the standard elf
# parse_args and list_options functions.
#
PARSE_AND_LIST_PROLOGUE='
#define OPTION_NO_TLS_OPT		301
#define OPTION_OLD_PLT			302
#define OPTION_OLD_GOT			303
#define OPTION_STUBSYMS			304
'

PARSE_AND_LIST_LONGOPTS='
  { "emit-stub-syms", no_argument, NULL, OPTION_STUBSYMS },
  { "no-tls-optimize", no_argument, NULL, OPTION_NO_TLS_OPT },
  { "bss-plt", no_argument, NULL, OPTION_OLD_PLT },
  { "sdata-got", no_argument, NULL, OPTION_OLD_GOT },
'

PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("\
  --emit-stub-syms      Label linker stubs with a symbol.\n\
  --no-tls-optimize     Don'\''t try to optimize TLS accesses.\n\
  --bss-plt             Force old-style BSS PLT.\n\
  --sdata-got           Force GOT location just before .sdata.\n"
		   ));
'

PARSE_AND_LIST_ARGS_CASES='
    case OPTION_STUBSYMS:
      emit_stub_syms = 1;
      break;

    case OPTION_NO_TLS_OPT:
      notlsopt = 1;
      break;

    case OPTION_OLD_PLT:
      old_plt = 1;
      break;

    case OPTION_OLD_GOT:
      old_got = 1;
      break;
'

# Put these extra ppc32elf routines in ld_${EMULATION_NAME}_emulation
#
LDEMUL_AFTER_OPEN=ppc_after_open
LDEMUL_BEFORE_ALLOCATION=ppc_before_allocation
