/* ehopt.c--optimize gcc exception frame information.
   Copyright (C) 1998, 2000 Free Software Foundation, Inc.
   Written by Ian Lance Taylor <ian@cygnus.com>.

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

#include "as.h"
#include "subsegs.h"

/* We include this ELF file, even though we may not be assembling for
   ELF, since the exception frame information is always in a format
   derived from DWARF.  */

#include "elf/dwarf2.h"

/* Try to optimize gcc 2.8 exception frame information.

   Exception frame information is emitted for every function in the
   .eh_frame or .debug_frame sections.  Simple information for a function
   with no exceptions looks like this:

__FRAME_BEGIN__:
	.4byte	.LLCIE1	/ Length of Common Information Entry
.LSCIE1:
#if .eh_frame
	.4byte	0x0	/ CIE Identifier Tag
#elif .debug_frame
	.4byte	0xffffffff / CIE Identifier Tag
#endif
	.byte	0x1	/ CIE Version
	.byte	0x0	/ CIE Augmentation (none)
	.byte	0x1	/ ULEB128 0x1 (CIE Code Alignment Factor)
	.byte	0x7c	/ SLEB128 -4 (CIE Data Alignment Factor)
	.byte	0x8	/ CIE RA Column
	.byte	0xc	/ DW_CFA_def_cfa
	.byte	0x4	/ ULEB128 0x4
	.byte	0x4	/ ULEB128 0x4
	.byte	0x88	/ DW_CFA_offset, column 0x8
	.byte	0x1	/ ULEB128 0x1
	.align 4
.LECIE1:
	.set	.LLCIE1,.LECIE1-.LSCIE1	/ CIE Length Symbol
	.4byte	.LLFDE1	/ FDE Length
.LSFDE1:
	.4byte	.LSFDE1-__FRAME_BEGIN__	/ FDE CIE offset
	.4byte	.LFB1	/ FDE initial location
	.4byte	.LFE1-.LFB1	/ FDE address range
	.byte	0x4	/ DW_CFA_advance_loc4
	.4byte	.LCFI0-.LFB1
	.byte	0xe	/ DW_CFA_def_cfa_offset
	.byte	0x8	/ ULEB128 0x8
	.byte	0x85	/ DW_CFA_offset, column 0x5
	.byte	0x2	/ ULEB128 0x2
	.byte	0x4	/ DW_CFA_advance_loc4
	.4byte	.LCFI1-.LCFI0
	.byte	0xd	/ DW_CFA_def_cfa_register
	.byte	0x5	/ ULEB128 0x5
	.byte	0x4	/ DW_CFA_advance_loc4
	.4byte	.LCFI2-.LCFI1
	.byte	0x2e	/ DW_CFA_GNU_args_size
	.byte	0x4	/ ULEB128 0x4
	.byte	0x4	/ DW_CFA_advance_loc4
	.4byte	.LCFI3-.LCFI2
	.byte	0x2e	/ DW_CFA_GNU_args_size
	.byte	0x0	/ ULEB128 0x0
	.align 4
.LEFDE1:
	.set	.LLFDE1,.LEFDE1-.LSFDE1	/ FDE Length Symbol

   The immediate issue we can address in the assembler is the
   DW_CFA_advance_loc4 followed by a four byte value.  The value is
   the difference of two addresses in the function.  Since gcc does
   not know this value, it always uses four bytes.  We will know the
   value at the end of assembly, so we can do better.  */

static int eh_frame_code_alignment PARAMS ((int));

/* Get the code alignment factor from the CIE.  */

static int
eh_frame_code_alignment (in_seg)
     int in_seg;
{
  /* ??? Assume .eh_frame and .debug_frame have the same alignment.  */
  static int code_alignment;

  fragS *f;
  fixS *fix;
  int offset;
  char CIE_id;
  char augmentation[10];
  int iaug;

  if (code_alignment != 0)
    return code_alignment;

  /* Can't find the alignment if we've changed sections.  */
  if (! in_seg)
    return -1;

  /* We should find the CIE at the start of the section.  */

#if defined (BFD_ASSEMBLER) || defined (MANY_SEGMENTS)
  f = seg_info (now_seg)->frchainP->frch_root;
#else
  f = frchain_now->frch_root;
#endif
#ifdef BFD_ASSEMBLER
  fix = seg_info (now_seg)->frchainP->fix_root;
#else
  fix = *seg_fix_rootP;
#endif

  /* Look through the frags of the section to find the code alignment.  */

  /* First make sure that the CIE Identifier Tag is 0/-1.  */

  if (strcmp (segment_name (now_seg), ".debug_frame") == 0)
    CIE_id = (char)0xff;
  else
    CIE_id = 0;

  offset = 4;
  while (f != NULL && offset >= f->fr_fix)
    {
      offset -= f->fr_fix;
      f = f->fr_next;
    }
  if (f == NULL
      || f->fr_fix - offset < 4
      || f->fr_literal[offset] != CIE_id
      || f->fr_literal[offset + 1] != CIE_id
      || f->fr_literal[offset + 2] != CIE_id
      || f->fr_literal[offset + 3] != CIE_id)
    {
      code_alignment = -1;
      return -1;
    }

  /* Next make sure the CIE version number is 1.  */

  offset += 4;
  while (f != NULL && offset >= f->fr_fix)
    {
      offset -= f->fr_fix;
      f = f->fr_next;
    }
  if (f == NULL
      || f->fr_fix - offset < 1
      || f->fr_literal[offset] != 1)
    {
      code_alignment = -1;
      return -1;
    }

  /* Skip the augmentation (a null terminated string).  */

  iaug = 0;
  ++offset;
  while (1)
    {
      while (f != NULL && offset >= f->fr_fix)
	{
	  offset -= f->fr_fix;
	  f = f->fr_next;
	}
      if (f == NULL)
	{
	  code_alignment = -1;
	  return -1;
	}
      while (offset < f->fr_fix && f->fr_literal[offset] != '\0')
	{
	  if ((size_t) iaug < (sizeof augmentation) - 1)
	    {
	      augmentation[iaug] = f->fr_literal[offset];
	      ++iaug;
	    }
	  ++offset;
	}
      if (offset < f->fr_fix)
	break;
    }
  ++offset;
  while (f != NULL && offset >= f->fr_fix)
    {
      offset -= f->fr_fix;
      f = f->fr_next;
    }
  if (f == NULL)
    {
      code_alignment = -1;
      return -1;
    }

  augmentation[iaug] = '\0';
  if (augmentation[0] == '\0')
    {
      /* No augmentation.  */
    }
  else if (strcmp (augmentation, "eh") == 0)
    {
      /* We have to skip a pointer.  Unfortunately, we don't know how
	 large it is.  We find out by looking for a matching fixup.  */
      while (fix != NULL
	     && (fix->fx_frag != f || fix->fx_where != offset))
	fix = fix->fx_next;
      if (fix == NULL)
	offset += 4;
      else
	offset += fix->fx_size;
      while (f != NULL && offset >= f->fr_fix)
	{
	  offset -= f->fr_fix;
	  f = f->fr_next;
	}
      if (f == NULL)
	{
	  code_alignment = -1;
	  return -1;
	}
    }
  else
    {
      code_alignment = -1;
      return -1;
    }

  /* We're now at the code alignment factor, which is a ULEB128.  If
     it isn't a single byte, forget it.  */

  code_alignment = f->fr_literal[offset] & 0xff;
  if ((code_alignment & 0x80) != 0 || code_alignment == 0)
    {
      code_alignment = -1;
      return -1;
    }

  return code_alignment;
}

/* This function is called from emit_expr.  It looks for cases which
   we can optimize.

   Rather than try to parse all this information as we read it, we
   look for a single byte DW_CFA_advance_loc4 followed by a 4 byte
   difference.  We turn that into a rs_cfa_advance frag, and handle
   those frags at the end of the assembly.  If the gcc output changes
   somewhat, this optimization may stop working.

   This function returns non-zero if it handled the expression and
   emit_expr should not do anything, or zero otherwise.  It can also
   change *EXP and *PNBYTES.  */

int
check_eh_frame (exp, pnbytes)
     expressionS *exp;
     unsigned int *pnbytes;
{
  struct frame_data
  {
    symbolS *size_end_sym;
    fragS *loc4_frag;
    int saw_size;
    int saw_advance_loc4;
    int loc4_fix;
  };

  static struct frame_data eh_frame_data;
  static struct frame_data debug_frame_data;
  struct frame_data *d;

  /* Don't optimize.  */
  if (flag_traditional_format)
    return 0;

  /* Select the proper section data.  */
  if (strcmp (segment_name (now_seg), ".eh_frame") == 0)
    d = &eh_frame_data;
  else if (strcmp (segment_name (now_seg), ".debug_frame") == 0)
    d = &debug_frame_data;
  else
    return 0;

  if (d->saw_size && S_IS_DEFINED (d->size_end_sym))
    {
      /* We have come to the end of the CIE or FDE.  See below where
         we set saw_size.  We must check this first because we may now
         be looking at the next size.  */
      d->saw_size = 0;
      d->saw_advance_loc4 = 0;
    }

  if (! d->saw_size
      && *pnbytes == 4)
    {
      /* This might be the size of the CIE or FDE.  We want to know
         the size so that we don't accidentally optimize across an FDE
         boundary.  We recognize the size in one of two forms: a
         symbol which will later be defined as a difference, or a
         subtraction of two symbols.  Either way, we can tell when we
         are at the end of the FDE because the symbol becomes defined
         (in the case of a subtraction, the end symbol, from which the
         start symbol is being subtracted).  Other ways of describing
         the size will not be optimized.  */
      if ((exp->X_op == O_symbol || exp->X_op == O_subtract)
	  && ! S_IS_DEFINED (exp->X_add_symbol))
	{
	  d->saw_size = 1;
	  d->size_end_sym = exp->X_add_symbol;
	}
    }
  else if (d->saw_size
	   && *pnbytes == 1
	   && exp->X_op == O_constant
	   && exp->X_add_number == DW_CFA_advance_loc4)
    {
      /* This might be a DW_CFA_advance_loc4.  Record the frag and the
         position within the frag, so that we can change it later.  */
      d->saw_advance_loc4 = 1;
      frag_grow (1);
      d->loc4_frag = frag_now;
      d->loc4_fix = frag_now_fix ();
    }
  else if (d->saw_advance_loc4
	   && *pnbytes == 4
	   && exp->X_op == O_constant)
    {
      int ca;

      /* This is a case which we can optimize.  The two symbols being
         subtracted were in the same frag and the expression was
         reduced to a constant.  We can do the optimization entirely
         in this function.  */

      d->saw_advance_loc4 = 0;

      ca = eh_frame_code_alignment (1);
      if (ca < 0)
	{
	  /* Don't optimize.  */
	}
      else if (exp->X_add_number % ca == 0
	       && exp->X_add_number / ca < 0x40)
	{
	  d->loc4_frag->fr_literal[d->loc4_fix]
	    = DW_CFA_advance_loc | (exp->X_add_number / ca);
	  /* No more bytes needed.  */
	  return 1;
	}
      else if (exp->X_add_number < 0x100)
	{
	  d->loc4_frag->fr_literal[d->loc4_fix] = DW_CFA_advance_loc1;
	  *pnbytes = 1;
	}
      else if (exp->X_add_number < 0x10000)
	{
	  d->loc4_frag->fr_literal[d->loc4_fix] = DW_CFA_advance_loc2;
	  *pnbytes = 2;
	}
    }
  else if (d->saw_advance_loc4
	   && *pnbytes == 4
	   && exp->X_op == O_subtract)
    {
      /* This is a case we can optimize.  The expression was not
         reduced, so we can not finish the optimization until the end
         of the assembly.  We set up a variant frag which we handle
         later.  */

      d->saw_advance_loc4 = 0;

      frag_var (rs_cfa, 4, 0, 0, make_expr_symbol (exp),
		d->loc4_fix, (char *) d->loc4_frag);

      return 1;
    }
  else
    d->saw_advance_loc4 = 0;

  return 0;
}

/* The function estimates the size of a rs_cfa variant frag based on
   the current values of the symbols.  It is called before the
   relaxation loop.  We set fr_subtype to the expected length.  */

int
eh_frame_estimate_size_before_relax (frag)
     fragS *frag;
{
  int ca;
  offsetT diff;
  int ret;

  ca = eh_frame_code_alignment (0);
  diff = resolve_symbol_value (frag->fr_symbol, 0);

  if (ca < 0)
    ret = 4;
  else if (diff % ca == 0 && diff / ca < 0x40)
    ret = 0;
  else if (diff < 0x100)
    ret = 1;
  else if (diff < 0x10000)
    ret = 2;
  else
    ret = 4;

  frag->fr_subtype = ret;

  return ret;
}

/* This function relaxes a rs_cfa variant frag based on the current
   values of the symbols.  fr_subtype is the current length of the
   frag.  This returns the change in frag length.  */

int
eh_frame_relax_frag (frag)
     fragS *frag;
{
  int oldsize, newsize;

  oldsize = frag->fr_subtype;
  newsize = eh_frame_estimate_size_before_relax (frag);
  return newsize - oldsize;
}

/* This function converts a rs_cfa variant frag into a normal fill
   frag.  This is called after all relaxation has been done.
   fr_subtype will be the desired length of the frag.  */

void
eh_frame_convert_frag (frag)
     fragS *frag;
{
  offsetT diff;
  fragS *loc4_frag;
  int loc4_fix;

  loc4_frag = (fragS *) frag->fr_opcode;
  loc4_fix = (int) frag->fr_offset;

  diff = resolve_symbol_value (frag->fr_symbol, 1);

  if (frag->fr_subtype == 0)
    {
      int ca;

      ca = eh_frame_code_alignment (0);
      assert (ca > 0 && diff % ca == 0 && diff / ca < 0x40);
      loc4_frag->fr_literal[loc4_fix] = DW_CFA_advance_loc | (diff / ca);
    }
  else if (frag->fr_subtype == 1)
    {
      assert (diff < 0x100);
      loc4_frag->fr_literal[loc4_fix] = DW_CFA_advance_loc1;
      frag->fr_literal[frag->fr_fix] = diff;
    }
  else if (frag->fr_subtype == 2)
    {
      assert (diff < 0x10000);
      loc4_frag->fr_literal[loc4_fix] = DW_CFA_advance_loc2;
      md_number_to_chars (frag->fr_literal + frag->fr_fix, diff, 2);
    }
  else
    md_number_to_chars (frag->fr_literal + frag->fr_fix, diff, 4);

  frag->fr_fix += frag->fr_subtype;
  frag->fr_type = rs_fill;
  frag->fr_offset = 0;
}
