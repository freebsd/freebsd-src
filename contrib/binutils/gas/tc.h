/* tc.h - target cpu dependent

   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* In theory (mine, at least!) the machine dependent part of the assembler
   should only have to include one file.  This one.  -- JF */

extern const pseudo_typeS md_pseudo_table[];

/* JF moved this here from as.h under the theory that nobody except MACHINE.c
   and write.c care about it anyway.  */

struct relax_type
{
  /* Forward reach. Signed number. > 0.  */
  long rlx_forward;
  /* Backward reach. Signed number. < 0.  */
  long rlx_backward;

  /* Bytes length of this address.  */
  unsigned char rlx_length;

  /* Next longer relax-state.  0 means there is no 'next' relax-state.  */
  relax_substateT rlx_more;
};

typedef struct relax_type relax_typeS;

extern const int md_reloc_size;	/* Size of a relocation record */

char *md_atof PARAMS ((int what_statement_type, char *literalP, int *sizeP));
#ifndef md_estimate_size_before_relax
int md_estimate_size_before_relax PARAMS ((fragS * fragP, segT segment));
#endif
int md_parse_option PARAMS ((int c, char *arg));
void md_show_usage PARAMS ((FILE *));
long md_pcrel_from PARAMS ((fixS * fixP));
short tc_coff_fix2rtype PARAMS ((fixS * fixP));
void md_assemble PARAMS ((char *str));
void md_begin PARAMS ((void));
#ifndef md_create_long_jump
void md_create_long_jump PARAMS ((char *ptr, addressT from_addr,
				  addressT to_addr, fragS * frag,
				  symbolS * to_symbol));
#endif
#ifndef md_create_short_jump
void md_create_short_jump PARAMS ((char *ptr, addressT from_addr,
				   addressT to_addr, fragS * frag,
				   symbolS * to_symbol));
#endif
void md_number_to_chars PARAMS ((char *buf, valueT val, int n));

#ifndef md_operand
void md_operand PARAMS ((expressionS * expressionP));
#endif

#ifdef MD_APPLY_FIX3
int md_apply_fix3 PARAMS ((fixS * fixP, valueT *val, segT seg));
#endif
#ifdef BFD_ASSEMBLER
int md_apply_fix PARAMS ((fixS * fixP, valueT *val));
#ifndef md_convert_frag
void md_convert_frag PARAMS ((bfd * headers, segT sec, fragS * fragP));
#endif
#ifndef tc_headers_hook
void tc_headers_hook PARAMS ((segT *, fixS *));
#endif
#ifndef RELOC_EXPANSION_POSSIBLE
extern arelent *tc_gen_reloc PARAMS ((asection *, fixS *));
#else
extern arelent **tc_gen_reloc PARAMS ((asection *, fixS *));
#endif
#else /* not BFD_ASSEMBLER */
void md_apply_fix PARAMS ((fixS * fixP, long val));
#ifndef md_convert_frag
void md_convert_frag PARAMS ((object_headers * headers, segT, fragS * fragP));
#endif

#ifndef tc_crawl_symbol_chain
void tc_crawl_symbol_chain PARAMS ((object_headers * headers));
#endif /* tc_crawl_symbol_chain */

#ifndef tc_headers_hook
void tc_headers_hook PARAMS ((object_headers * headers));
#endif /* tc_headers_hook */
#endif /* BFD_ASSEMBLER */

#ifndef md_section_align
valueT md_section_align PARAMS ((segT seg, valueT size));
#endif

#ifndef md_undefined_symbol
symbolS *md_undefined_symbol PARAMS ((char *name));
#endif

/* end of tc.h */
