# This shell script emits a C file. -*- C -*-
#   Copyright (C) 2001 Free Software Foundation, Inc.
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

# This file is sourced from elf32.em.  It is used by targets for
# which relaxation is not just an optimization, but for correctness.

LDEMUL_BEFORE_ALLOCATION=need_relax_${EMULATION_NAME}_before_allocation

cat >>e${EMULATION_NAME}.c <<EOF

static void need_relax_${EMULATION_NAME}_before_allocation PARAMS ((void));

static void
need_relax_${EMULATION_NAME}_before_allocation ()
{
  /* Call main function; we're just extending it.  */
  gld${EMULATION_NAME}_before_allocation ();

  /* Force -relax on if not doing a relocatable link.  */
  if (! link_info.relocateable)
    command_line.relax = true;
}
EOF
