/* frags.h - Header file for the frag concept.
   Copyright 1987, 1992, 1993, 1994, 1995, 1997, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.

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

#ifndef FRAGS_H
#define FRAGS_H

#ifdef ANSI_PROTOTYPES
struct obstack;
#endif

/* A code fragment (frag) is some known number of chars, followed by some
   unknown number of chars. Typically the unknown number of chars is an
   instruction address whose size is yet unknown. We always know the greatest
   possible size the unknown number of chars may become, and reserve that
   much room at the end of the frag.
   Once created, frags do not change address during assembly.
   We chain the frags in (a) forward-linked list(s). The object-file address
   of the 1st char of a frag is generally not known until after relax().
   Many things at assembly time describe an address by {object-file-address
   of a particular frag}+offset.

   BUG: it may be smarter to have a single pointer off to various different
   notes for different frag kinds.  See how code pans.   */

struct frag {
  /* Object file address (as an octet offset).  */
  addressT fr_address;
  /* Chain forward; ascending address order.  Rooted in frch_root.  */
  struct frag *fr_next;

  /* (Fixed) number of octets we know we have.  May be 0.  */
  offsetT fr_fix;
  /* May be used for (Variable) number of octets after above.
     The generic frag handling code no longer makes any use of fr_var.  */
  offsetT fr_var;
  /* For variable-length tail.  */
  symbolS *fr_symbol;
  /* For variable-length tail.  */
  offsetT fr_offset;
  /* Points to opcode low addr byte, for relaxation.  */
  char *fr_opcode;

#ifndef NO_LISTING
  struct list_info_struct *line;
#endif

  /* Flipped each relax pass so we can easily determine whether
     fr_address has been adjusted.  */
  unsigned int relax_marker:1;

  /* What state is my tail in? */
  relax_stateT fr_type;
  relax_substateT fr_subtype;

#ifdef USING_CGEN
  /* Don't include this unless using CGEN to keep frag size down.  */
  struct {
    /* CGEN_INSN entry for this instruction.  */
    const struct cgen_insn *insn;
    /* Index into operand table.  */
    int opindex;
    /* Target specific data, usually reloc number.  */
    int opinfo;
  } fr_cgen;
#endif

#ifdef TC_FRAG_TYPE
  TC_FRAG_TYPE tc_frag_data;
#endif

  /* Where the frag was created, or where it became a variant frag.  */
  char *fr_file;
  unsigned int fr_line;

  /* Data begins here.  */
  char fr_literal[1];
};

#define SIZEOF_STRUCT_FRAG \
((char *) zero_address_frag.fr_literal - (char *) &zero_address_frag)
/* We want to say fr_literal[0] above.  */

/* Current frag we are building.  This frag is incomplete.  It is,
   however, included in frchain_now.  The fr_fix field is bogus;
   instead, use frag_now_fix ().  */
COMMON fragS *frag_now;
extern addressT frag_now_fix PARAMS ((void));
extern addressT frag_now_fix_octets PARAMS ((void));

/* For foreign-segment symbol fixups.  */
COMMON fragS zero_address_frag;
/* For local common (N_BSS segment) fixups.  */
COMMON fragS bss_address_frag;

#if 0
/* A macro to speed up appending exactly 1 char to current frag.  */
/* JF changed < 1 to <= 1 to avoid a race conditon.  */
#define FRAG_APPEND_1_CHAR(datum)			\
{							\
  if (obstack_room (&frags) <= 1)			\
    {							\
      frag_wane (frag_now);				\
      frag_new (0);					\
    }							\
  obstack_1grow (&frags, datum);			\
}
#else
extern void frag_append_1_char PARAMS ((int));
#define FRAG_APPEND_1_CHAR(X) frag_append_1_char (X)
#endif

void frag_init PARAMS ((void));
fragS *frag_alloc PARAMS ((struct obstack *));
void frag_grow PARAMS ((unsigned int nchars));
char *frag_more PARAMS ((int nchars));
void frag_align PARAMS ((int alignment, int fill_character, int max));
void frag_align_pattern PARAMS ((int alignment,
				 const char *fill_pattern,
				 int n_fill,
				 int max));
void frag_align_code PARAMS ((int alignment, int max));
void frag_new PARAMS ((int old_frags_var_max_size));
void frag_wane PARAMS ((fragS * fragP));

char *frag_variant PARAMS ((relax_stateT type,
			    int max_chars,
			    int var,
			    relax_substateT subtype,
			    symbolS * symbol,
			    offsetT offset,
			    char *opcode));

char *frag_var PARAMS ((relax_stateT type,
			int max_chars,
			int var,
			relax_substateT subtype,
			symbolS * symbol,
			offsetT offset,
			char *opcode));

#endif /* FRAGS_H */
