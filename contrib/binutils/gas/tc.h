/* tc.h - target cpu dependent

   Copyright 1987, 1990, 1991, 1992, 1993, 1994, 1995, 2000, 2001, 2003
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

char *md_atof (int what_statement_type, char *literalP, int *sizeP);
#ifndef md_estimate_size_before_relax
int md_estimate_size_before_relax (fragS * fragP, segT segment);
#endif
int md_parse_option (int c, char *arg);
void md_show_usage (FILE *);
#ifndef md_pcrel_from
long md_pcrel_from (fixS * fixP);
#endif
short tc_coff_fix2rtype (fixS * fixP);
void md_assemble (char *str);
void md_begin (void);
#ifndef md_create_long_jump
void md_create_long_jump (char *ptr, addressT from_addr,
			  addressT to_addr, fragS * frag,
			  symbolS * to_symbol);
#endif
#ifndef md_create_short_jump
void md_create_short_jump (char *ptr, addressT from_addr,
			   addressT to_addr, fragS * frag,
			   symbolS * to_symbol);
#endif
void md_number_to_chars (char *buf, valueT val, int n);

#ifndef md_operand
void md_operand (expressionS * expressionP);
#endif

void md_apply_fix3 (fixS *, valueT *, segT);

#ifdef BFD_ASSEMBLER
#ifndef md_convert_frag
void md_convert_frag (bfd * headers, segT sec, fragS * fragP);
#endif
#ifndef tc_headers_hook
void tc_headers_hook (segT *, fixS *);
#endif
#ifndef RELOC_EXPANSION_POSSIBLE
extern arelent *tc_gen_reloc (asection *, fixS *);
#else
extern arelent **tc_gen_reloc (asection *, fixS *);
#endif
#else /* not BFD_ASSEMBLER */
#ifndef md_convert_frag
void md_convert_frag (object_headers * headers, segT, fragS * fragP);
#endif

#ifndef tc_crawl_symbol_chain
void tc_crawl_symbol_chain (object_headers * headers);
#endif /* tc_crawl_symbol_chain */

#ifndef tc_headers_hook
void tc_headers_hook (object_headers * headers);
#endif /* tc_headers_hook */
#endif /* BFD_ASSEMBLER */

#ifndef md_section_align
valueT md_section_align (segT seg, valueT size);
#endif

#ifndef md_undefined_symbol
symbolS *md_undefined_symbol (char *name);
#endif

/* end of tc.h */
