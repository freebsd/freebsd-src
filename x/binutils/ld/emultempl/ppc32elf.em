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

# This file is sourced from elf32.em, and defines extra powerpc64-elf
# specific routines.
#
cat >>e${EMULATION_NAME}.c <<EOF

#include "libbfd.h"
#include "elf32-ppc.h"

/* Whether to run tls optimization.  */
static int notlsopt = 0;

static void
ppc_before_allocation (void)
{
  extern const bfd_target bfd_elf32_powerpc_vec;
  extern const bfd_target bfd_elf32_powerpcle_vec;

  if (link_info.hash->creator == &bfd_elf32_powerpc_vec
      || link_info.hash->creator == &bfd_elf32_powerpcle_vec)
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
'

PARSE_AND_LIST_LONGOPTS='
  { "no-tls-optimize", no_argument, NULL, OPTION_NO_TLS_OPT },
'

PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("\
  --no-tls-optimize     Don'\''t try to optimize TLS accesses.\n"
		   ));
'

PARSE_AND_LIST_ARGS_CASES='
    case OPTION_NO_TLS_OPT:
      notlsopt = 1;
      break;
'

# Put these extra ppc64elf routines in ld_${EMULATION_NAME}_emulation
#
LDEMUL_BEFORE_ALLOCATION=ppc_before_allocation
