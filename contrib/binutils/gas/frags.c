/* frags.c - manage frags -
   Copyright (C) 1987, 90, 91, 92, 93, 94, 95, 96, 97, 1998
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

#include "as.h"
#include "subsegs.h"
#include "obstack.h"

extern fragS zero_address_frag;
extern fragS bss_address_frag;

/* Initialization for frag routines.  */
void
frag_init ()
{
  zero_address_frag.fr_type = rs_fill;
  bss_address_frag.fr_type = rs_fill;
}

/* Allocate a frag on the specified obstack.
   Call this routine from everywhere else, so that all the weird alignment
   hackery can be done in just one place.  */
fragS *
frag_alloc (ob)
     struct obstack *ob;
{
  fragS *ptr;
  int oalign;

  (void) obstack_alloc (ob, 0);
  oalign = obstack_alignment_mask (ob);
  obstack_alignment_mask (ob) = 0;
  ptr = (fragS *) obstack_alloc (ob, SIZEOF_STRUCT_FRAG);
  obstack_alignment_mask (ob) = oalign;
  memset (ptr, 0, SIZEOF_STRUCT_FRAG);
  return ptr;
}

/*
 *			frag_grow()
 *
 * Try to augment current frag by nchars chars.
 * If there is no room, close of the current frag with a ".fill 0"
 * and begin a new frag. Unless the new frag has nchars chars available
 * do not return. Do not set up any fields of *now_frag.
 */
void 
frag_grow (nchars)
     unsigned int nchars;
{
  if (obstack_room (&frchain_now->frch_obstack) < nchars)
    {
      unsigned int n, oldn;
      long oldc;

      frag_wane (frag_now);
      frag_new (0);
      oldn = (unsigned) -1;
      oldc = frchain_now->frch_obstack.chunk_size;
      frchain_now->frch_obstack.chunk_size = 2 * nchars;
      while ((n = obstack_room (&frchain_now->frch_obstack)) < nchars
	     && n < oldn)
	{
	  frag_wane (frag_now);
	  frag_new (0);
	  oldn = n;
	}
      frchain_now->frch_obstack.chunk_size = oldc;
    }
  if (obstack_room (&frchain_now->frch_obstack) < nchars)
    as_fatal ("Can't extend frag %d. chars", nchars);
}

/*
 *			frag_new()
 *
 * Call this to close off a completed frag, and start up a new (empty)
 * frag, in the same subsegment as the old frag.
 * [frchain_now remains the same but frag_now is updated.]
 * Because this calculates the correct value of fr_fix by
 * looking at the obstack 'frags', it needs to know how many
 * characters at the end of the old frag belong to (the maximal)
 * fr_var: the rest must belong to fr_fix.
 * It doesn't actually set up the old frag's fr_var: you may have
 * set fr_var == 1, but allocated 10 chars to the end of the frag:
 * in this case you pass old_frags_var_max_size == 10.
 *
 * Make a new frag, initialising some components. Link new frag at end
 * of frchain_now.
 */
void 
frag_new (old_frags_var_max_size)
     /* Number of chars (already allocated on obstack frags) in
	variable_length part of frag. */
     int old_frags_var_max_size;
{
  fragS *former_last_fragP;
  frchainS *frchP;

  assert (frchain_now->frch_last == frag_now);

  /* Fix up old frag's fr_fix.  */
  frag_now->fr_fix = frag_now_fix () - old_frags_var_max_size;
  /* Make sure its type is valid.  */
  assert (frag_now->fr_type != 0);

  /* This will align the obstack so the next struct we allocate on it
     will begin at a correct boundary. */
  obstack_finish (&frchain_now->frch_obstack);
  frchP = frchain_now;
  know (frchP);
  former_last_fragP = frchP->frch_last;
  assert (former_last_fragP != 0);
  assert (former_last_fragP == frag_now);
  frag_now = frag_alloc (&frchP->frch_obstack);

  as_where (&frag_now->fr_file, &frag_now->fr_line);

  /* Generally, frag_now->points to an address rounded up to next
     alignment.  However, characters will add to obstack frags
     IMMEDIATELY after the struct frag, even if they are not starting
     at an alignment address. */
  former_last_fragP->fr_next = frag_now;
  frchP->frch_last = frag_now;

#ifndef NO_LISTING
  {
    extern struct list_info_struct *listing_tail;
    frag_now->line = listing_tail;
  }
#endif

  assert (frchain_now->frch_last == frag_now);

  frag_now->fr_next = NULL;
}				/* frag_new() */

/*
 *			frag_more()
 *
 * Start a new frag unless we have n more chars of room in the current frag.
 * Close off the old frag with a .fill 0.
 *
 * Return the address of the 1st char to write into. Advance
 * frag_now_growth past the new chars.
 */

char *
frag_more (nchars)
     int nchars;
{
  register char *retval;

  if (now_seg == absolute_section)
    {
      as_bad ("attempt to allocate data in absolute section");
      subseg_set (text_section, 0);
    }

  if (mri_common_symbol != NULL)
    {
      as_bad ("attempt to allocate data in common section");
      mri_common_symbol = NULL;
    }

  frag_grow (nchars);
  retval = obstack_next_free (&frchain_now->frch_obstack);
  obstack_blank_fast (&frchain_now->frch_obstack, nchars);
  return (retval);
}				/* frag_more() */

/*
 *			frag_var()
 *
 * Start a new frag unless we have max_chars more chars of room in the current frag.
 * Close off the old frag with a .fill 0.
 *
 * Set up a machine_dependent relaxable frag, then start a new frag.
 * Return the address of the 1st char of the var part of the old frag
 * to write into.
 */

char *
frag_var (type, max_chars, var, subtype, symbol, offset, opcode)
     relax_stateT type;
     int max_chars;
     int var;
     relax_substateT subtype;
     symbolS *symbol;
     offsetT offset;
     char *opcode;
{
  register char *retval;

  frag_grow (max_chars);
  retval = obstack_next_free (&frchain_now->frch_obstack);
  obstack_blank_fast (&frchain_now->frch_obstack, max_chars);
  frag_now->fr_var = var;
  frag_now->fr_type = type;
  frag_now->fr_subtype = subtype;
  frag_now->fr_symbol = symbol;
  frag_now->fr_offset = offset;
  frag_now->fr_opcode = opcode;
#ifdef TC_FRAG_INIT
  TC_FRAG_INIT (frag_now);
#endif
  as_where (&frag_now->fr_file, &frag_now->fr_line);
  frag_new (max_chars);
  return (retval);
}

/*
 *			frag_variant()
 *
 * OVE: This variant of frag_var assumes that space for the tail has been
 *      allocated by caller.
 *      No call to frag_grow is done.
 */

char *
frag_variant (type, max_chars, var, subtype, symbol, offset, opcode)
     relax_stateT type;
     int max_chars;
     int var;
     relax_substateT subtype;
     symbolS *symbol;
     offsetT offset;
     char *opcode;
{
  register char *retval;

  retval = obstack_next_free (&frchain_now->frch_obstack);
  frag_now->fr_var = var;
  frag_now->fr_type = type;
  frag_now->fr_subtype = subtype;
  frag_now->fr_symbol = symbol;
  frag_now->fr_offset = offset;
  frag_now->fr_opcode = opcode;
#ifdef TC_FRAG_INIT
  TC_FRAG_INIT (frag_now);
#endif
  as_where (&frag_now->fr_file, &frag_now->fr_line);
  frag_new (max_chars);
  return (retval);
}				/* frag_variant() */

/*
 *			frag_wane()
 *
 * Reduce the variable end of a frag to a harmless state.
 */
void 
frag_wane (fragP)
     register fragS *fragP;
{
  fragP->fr_type = rs_fill;
  fragP->fr_offset = 0;
  fragP->fr_var = 0;
}

/* Make an alignment frag.  The size of this frag will be adjusted to
   force the next frag to have the appropriate alignment.  ALIGNMENT
   is the power of two to which to align.  FILL_CHARACTER is the
   character to use to fill in any bytes which are skipped.  MAX is
   the maximum number of characters to skip when doing the alignment,
   or 0 if there is no maximum.  */

void 
frag_align (alignment, fill_character, max)
     int alignment;
     int fill_character;
     int max;
{
  if (now_seg == absolute_section)
    {
      addressT new_off;

      new_off = ((abs_section_offset + alignment - 1)
		 &~ ((1 << alignment) - 1));
      if (max == 0 || new_off - abs_section_offset <= (addressT) max)
	abs_section_offset = new_off;
    }
  else
    {
      char *p;

      p = frag_var (rs_align, 1, 1, (relax_substateT) max,
		    (symbolS *) 0, (offsetT) alignment, (char *) 0);
      *p = fill_character;
    }
}

/* Make an alignment frag like frag_align, but fill with a repeating
   pattern rather than a single byte.  ALIGNMENT is the power of two
   to which to align.  FILL_PATTERN is the fill pattern to repeat in
   the bytes which are skipped.  N_FILL is the number of bytes in
   FILL_PATTERN.  MAX is the maximum number of characters to skip when
   doing the alignment, or 0 if there is no maximum.  */

void 
frag_align_pattern (alignment, fill_pattern, n_fill, max)
     int alignment;
     const char *fill_pattern;
     int n_fill;
     int max;
{
  char *p;

  p = frag_var (rs_align, n_fill, n_fill, (relax_substateT) max,
		(symbolS *) 0, (offsetT) alignment, (char *) 0);
  memcpy (p, fill_pattern, n_fill);
}

addressT
frag_now_fix ()
{
  if (now_seg == absolute_section)
    return abs_section_offset;
  return (addressT) ((char*) obstack_next_free (&frchain_now->frch_obstack)
		     - frag_now->fr_literal);
}

void
frag_append_1_char (datum)
     int datum;
{
  if (obstack_room (&frchain_now->frch_obstack) <= 1)
    {
      frag_wane (frag_now);
      frag_new (0);
    }
  obstack_1grow (&frchain_now->frch_obstack, datum);
}

/* end of frags.c */
