/* ecoff.h -- header file for ECOFF debugging support
   Copyright (C) 1993, 94, 95, 96, 1997 Free Software Foundation, Inc.
   Contributed by Cygnus Support.
   Put together by Ian Lance Taylor <ian@cygnus.com>.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifdef ECOFF_DEBUGGING

#ifndef GAS_ECOFF_H
#define GAS_ECOFF_H

#include "coff/sym.h"
#include "coff/ecoff.h"

/* Whether we have seen any ECOFF debugging information.  */
extern int ecoff_debugging_seen;

/* This function should be called at the start of assembly, by
   obj_read_begin_hook.  */
extern void ecoff_read_begin_hook PARAMS ((void));

/* This function should be called when the assembler switches to a new
   file.  */
extern void ecoff_new_file PARAMS ((const char *));

/* This function should be called when a new symbol is created, by
   obj_symbol_new_hook.  */
extern void ecoff_symbol_new_hook PARAMS ((struct symbol *));

/* This function should be called by the obj_frob_symbol hook.  */
extern void ecoff_frob_symbol PARAMS ((struct symbol *));

/* Build the ECOFF debugging information.  This should be called by
   obj_frob_file.  This fills in the counts in *HDR; the offsets are
   filled in relative to the start of the *BUFP.  It sets *BUFP to a
   block of memory holding the debugging information.  It returns the
   length of *BUFP.  */
extern unsigned long ecoff_build_debug
  PARAMS ((HDRR *hdr, char **bufp, const struct ecoff_debug_swap *));

/* Functions to handle the ECOFF debugging directives.  */
extern void ecoff_directive_begin PARAMS ((int));
extern void ecoff_directive_bend PARAMS ((int));
extern void ecoff_directive_end PARAMS ((int));
extern void ecoff_directive_ent PARAMS ((int));
extern void ecoff_directive_fmask PARAMS ((int));
extern void ecoff_directive_frame PARAMS ((int));
extern void ecoff_directive_loc PARAMS ((int));
extern void ecoff_directive_mask PARAMS ((int));

/* Other ECOFF directives.  */
extern void ecoff_directive_extern PARAMS ((int));
extern void ecoff_directive_weakext PARAMS ((int));

/* Functions to handle the COFF debugging directives.  */
extern void ecoff_directive_def PARAMS ((int));
extern void ecoff_directive_dim PARAMS ((int));
extern void ecoff_directive_endef PARAMS ((int));
extern void ecoff_directive_file PARAMS ((int));
extern void ecoff_directive_scl PARAMS ((int));
extern void ecoff_directive_size PARAMS ((int));
extern void ecoff_directive_tag PARAMS ((int));
extern void ecoff_directive_type PARAMS ((int));
extern void ecoff_directive_val PARAMS ((int));

/* Handle stabs.  */
extern void ecoff_stab PARAMS ((segT sec, int what, const char *string,
				int type, int other, int desc));

/* Set the GP prologue size.  */
extern void ecoff_set_gp_prolog_size PARAMS ((int sz));

/* This routine is called from the ECOFF code to set the external
   information for a symbol.  */
#ifndef obj_ecoff_set_ext
extern void obj_ecoff_set_ext PARAMS ((struct symbol *, EXTR *));
#endif

/* This routine is used to patch up a line number directive when
   instructions are moved around.  */
extern void ecoff_fix_loc PARAMS ((fragS *, unsigned long));

/* This function is called from read.c to peek at cur_file_ptr.  */
extern int ecoff_no_current_file PARAMS ((void));

/* This routine is called from read.c to generate line number for .s
   file.  */
extern void ecoff_generate_asm_lineno PARAMS ((const char *, int));

#endif /* ! GAS_ECOFF_H */
#endif /* ECOFF_DEBUGGING */
