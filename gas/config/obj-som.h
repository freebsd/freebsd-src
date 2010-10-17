/* SOM object file format.
   Copyright 1993, 1994, 1995, 1998, 2000 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.

   Written by the Center for Software Science at the University of Utah
   and by Cygnus Support.  */

#ifndef _OBJ_SOM_H
#define _OBJ_SOM_H

#define OBJ_SOM 1

#include "bfd.h"
#include "bfd/som.h"
#include "targ-cpu.h"

#ifndef FALSE
#define FALSE 0
#define TRUE !FALSE
#endif

/* should be conditional on address size!  */
#define som_symbol(asymbol) ((som_symbol_type *) (&(asymbol)->the_bfd))

extern void som_file_symbol PARAMS ((char *));
extern void obj_som_version PARAMS ((int));
extern void obj_som_init_stab_section PARAMS ((segT));
extern void obj_som_copyright PARAMS ((int));
extern void obj_som_compiler PARAMS ((int));

#define obj_symbol_new_hook(s) {;}

/* SOM has several attributes for spaces/subspaces which can not
   be easily expressed in BFD.  We use these macros to trigger calls
   into the SOM BFD backend to set these attributes.  */
#define obj_set_section_attributes  bfd_som_set_section_attributes
#define obj_set_subsection_attributes bfd_som_set_subsection_attributes

/* Likewise for symbol types.  */
#define obj_set_symbol_type bfd_som_set_symbol_type

/* Stabs go in a separate sections.  GDB expects to find them in sections
   with the names $GDB_SYMBOLS$ and $GDB_STRINGS$ rather than .stab and
   .stabstr.  */
#define SEPARATE_STAB_SECTIONS 1
#define STAB_SECTION_NAME "$GDB_SYMBOLS$"
#define STAB_STRING_SECTION_NAME "$GDB_STRINGS$"

/* We use INIT_STAB_SECTION to record the space/subspace relationships
   for the various debugging sections.  */
#define INIT_STAB_SECTION(seg) obj_som_init_stab_section (seg)

/* We'll be updating the magic 1st stab entry once the entire assembly
   fail has been processed.  */
#define obj_frob_file() som_frob_file()

#endif /* _OBJ_SOM_H */
