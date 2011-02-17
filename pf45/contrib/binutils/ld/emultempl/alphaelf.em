# This shell script emits a C file. -*- C -*-
#   Copyright 2003 Free Software Foundation, Inc.
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

# This file is sourced from elf32.em, and defines extra alpha
# specific routines.
#
cat >>e${EMULATION_NAME}.c <<EOF

#include "elf/internal.h"
#include "elf/alpha.h"
#include "elf-bfd.h"

static int elf64alpha_32bit = 0;

/* Set the start address as in the Tru64 ld.  */
#define ALPHA_TEXT_START_32BIT 0x12000000

static void
alpha_after_parse (void)
{
  if (elf64alpha_32bit && !link_info.shared && !link_info.relocatable)
    lang_section_start (".interp",
			exp_binop ('+',
				   exp_intop (ALPHA_TEXT_START_32BIT),
				   exp_nameop (SIZEOF_HEADERS, NULL)));
}

static void
alpha_finish (void)
{
  if (elf64alpha_32bit)
    elf_elfheader (output_bfd)->e_flags |= EF_ALPHA_32BIT;

  gld${EMULATION_NAME}_finish ();
}
EOF

# Define some shell vars to insert bits of code into the standard elf
# parse_args and list_options functions.
#
PARSE_AND_LIST_PROLOGUE='
#define OPTION_TASO            300
'

PARSE_AND_LIST_LONGOPTS='
  {"taso", no_argument, NULL, OPTION_TASO},
'

PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("  -taso\t\t\tLoad executable in the lower 31-bit addressable\n"));
  fprintf (file, _("\t\t\t  virtual address range\n"));
'

PARSE_AND_LIST_ARGS_CASES='
    case OPTION_TASO:
      elf64alpha_32bit = 1;
      break;
'

# Put these extra alpha routines in ld_${EMULATION_NAME}_emulation
#
LDEMUL_AFTER_PARSE=alpha_after_parse
LDEMUL_FINISH=alpha_finish
