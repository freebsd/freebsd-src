/* write.h -> write.c
   Copyright (C) 1987 Free Software Foundation, Inc.

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
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* The bit_fix was implemented to support machines that need variables
   to be inserted in bitfields other than 1, 2 and 4 bytes. 
   Furthermore it gives us a possibillity to mask in bits in the symbol
   when it's fixed in the objectcode and check the symbols limits.

   The or-mask is used to set the huffman bits in displacements for the
   ns32k port.
   The acbi, addqi, movqi, cmpqi instruction requires an assembler that
   can handle bitfields. Ie handle an expression, evaluate it and insert
   the result in an some bitfield. ( ex: 5 bits in a short field of a opcode) 
 */


struct bit_fix {
  int			fx_bit_size;	/* Length of bitfield		*/
  int			fx_bit_offset;	/* Bit offset to bitfield	*/
  long			fx_bit_base;	/* Where do we apply the bitfix.
					If this is zero, default is assumed. */
  long                  fx_bit_base_adj;/* Adjustment of base */
  long			fx_bit_max;	/* Signextended max for bitfield */
  long			fx_bit_min;	/* Signextended min for bitfield */
  long			fx_bit_add;	/* Or mask, used for huffman prefix */
};
typedef struct bit_fix bit_fixS;
/*
 * FixSs may be built up in any order.
 */

struct fix
{
  fragS *		fx_frag;	/* Which frag? */
  long int		fx_where;	/* Where is the 1st byte to fix up? */
  symbolS *		fx_addsy; /* NULL or Symbol whose value we add in. */
  symbolS *		fx_subsy; /* NULL or Symbol whose value we subtract. */
  long int		fx_offset;	/* Absolute number we add in. */
  struct fix *		fx_next;	/* NULL or -> next fixS. */
  short int		fx_size;	/* How many bytes are involved? */
  char			fx_pcrel;	/* TRUE: pc-relative. */
  char			fx_pcrel_adjust;/* pc-relative offset adjust */
  char			fx_im_disp;	/* TRUE: value is a displacement */
  bit_fixS *		fx_bit_fixP;	/* IF NULL no bitfix's to do */  
  char			fx_bsr;		/* sequent-hack */
#if defined(SPARC) || defined(I860)
  char			fx_r_type;	/* Sparc hacks */
  long			fx_addnumber;
#endif
};

typedef struct fix	fixS;


COMMON fixS *	text_fix_root;	/* Chains fixSs. */
COMMON fixS *	data_fix_root;	/* Chains fixSs. */
COMMON fixS **	seg_fix_rootP;	/* -> one of above. */

bit_fixS *bit_fix_new();
/* end: write.h */

