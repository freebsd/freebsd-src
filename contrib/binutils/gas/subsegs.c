/* subsegs.c - subsegments -
   Copyright 1987, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003, 2004, 2005
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
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

/* Segments & sub-segments.  */

#include "as.h"

#include "subsegs.h"
#include "obstack.h"

frchainS *frchain_root, *frchain_now;

static struct obstack frchains;

/* Gas segment information for bfd_abs_section_ptr and
   bfd_und_section_ptr.  */
static segment_info_type *abs_seg_info;
static segment_info_type *und_seg_info;

static void subseg_set_rest (segT, subsegT);

static fragS dummy_frag;

static frchainS absolute_frchain;

void
subsegs_begin (void)
{
  obstack_begin (&frchains, chunksize);
#if __GNUC__ >= 2
  obstack_alignment_mask (&frchains) = __alignof__ (frchainS) - 1;
#endif

  frchain_root = NULL;
  frchain_now = NULL;		/* Warn new_subseg() that we are booting.  */

  frag_now = &dummy_frag;

  absolute_frchain.frch_seg = absolute_section;
  absolute_frchain.frch_subseg = 0;
  absolute_frchain.fix_root = absolute_frchain.fix_tail = 0;
  absolute_frchain.frch_frag_now = &zero_address_frag;
  absolute_frchain.frch_root = absolute_frchain.frch_last = &zero_address_frag;
}

/*
 *			subseg_change()
 *
 * Change the subsegment we are in, BUT DO NOT MAKE A NEW FRAG for the
 * subsegment. If we are already in the correct subsegment, change nothing.
 * This is used eg as a worker for subseg_set [which does make a new frag_now]
 * and for changing segments after we have read the source. We construct eg
 * fixSs even after the source file is read, so we do have to keep the
 * segment context correct.
 */
void
subseg_change (register segT seg, register int subseg)
{
  segment_info_type *seginfo;
  now_seg = seg;
  now_subseg = subseg;

  if (now_seg == absolute_section)
    return;

  seginfo = (segment_info_type *) bfd_get_section_userdata (stdoutput, seg);
  if (! seginfo)
    {
      seginfo = (segment_info_type *) xmalloc (sizeof (*seginfo));
      memset ((PTR) seginfo, 0, sizeof (*seginfo));
      seginfo->fix_root = NULL;
      seginfo->fix_tail = NULL;
      seginfo->bfd_section = seg;
      seginfo->sym = 0;
      if (seg == bfd_abs_section_ptr)
	abs_seg_info = seginfo;
      else if (seg == bfd_und_section_ptr)
	und_seg_info = seginfo;
      else
	bfd_set_section_userdata (stdoutput, seg, (PTR) seginfo);
    }
}

static void
subseg_set_rest (segT seg, subsegT subseg)
{
  register frchainS *frcP;	/* crawl frchain chain */
  register frchainS **lastPP;	/* address of last pointer */
  frchainS *newP;		/* address of new frchain */

  mri_common_symbol = NULL;

  if (frag_now && frchain_now)
    frchain_now->frch_frag_now = frag_now;

  assert (frchain_now == 0
	  || now_seg == undefined_section
	  || now_seg == absolute_section
	  || frchain_now->frch_last == frag_now);

  subseg_change (seg, (int) subseg);

  if (seg == absolute_section)
    {
      frchain_now = &absolute_frchain;
      frag_now = &zero_address_frag;
      return;
    }

  assert (frchain_now == 0
	  || now_seg == undefined_section
	  || frchain_now->frch_last == frag_now);

  /*
   * Attempt to find or make a frchain for that sub seg.
   * Crawl along chain of frchainSs, begins @ frchain_root.
   * If we need to make a frchainS, link it into correct
   * position of chain rooted in frchain_root.
   */
  for (frcP = *(lastPP = &frchain_root);
       frcP && frcP->frch_seg <= seg;
       frcP = *(lastPP = &frcP->frch_next))
    {
      if (frcP->frch_seg == seg
	  && frcP->frch_subseg >= subseg)
	{
	  break;
	}
    }
  /*
   * frcP:		Address of the 1st frchainS in correct segment with
   *		frch_subseg >= subseg.
   *		We want to either use this frchainS, or we want
   *		to insert a new frchainS just before it.
   *
   *		If frcP==NULL, then we are at the end of the chain
   *		of frchainS-s. A NULL frcP means we fell off the end
   *		of the chain looking for a
   *		frch_subseg >= subseg, so we
   *		must make a new frchainS.
   *
   *		If we ever maintain a pointer to
   *		the last frchainS in the chain, we change that pointer
   *		ONLY when frcP==NULL.
   *
   * lastPP:	Address of the pointer with value frcP;
   *		Never NULL.
   *		May point to frchain_root.
   *
   */
  if (!frcP
      || (frcP->frch_seg > seg
	  || frcP->frch_subseg > subseg))	/* Kinky logic only works with 2 segments.  */
    {
      /*
       * This should be the only code that creates a frchainS.
       */
      segment_info_type *seginfo;

      newP = (frchainS *) obstack_alloc (&frchains, sizeof (frchainS));
      newP->frch_subseg = subseg;
      newP->frch_seg = seg;
      newP->fix_root = NULL;
      newP->fix_tail = NULL;
      obstack_begin (&newP->frch_obstack, chunksize);
#if __GNUC__ >= 2
      obstack_alignment_mask (&newP->frch_obstack) = __alignof__ (fragS) - 1;
#endif
      newP->frch_frag_now = frag_alloc (&newP->frch_obstack);
      newP->frch_frag_now->fr_type = rs_fill;

      newP->frch_root = newP->frch_last = newP->frch_frag_now;

      *lastPP = newP;
      newP->frch_next = frcP;	/* perhaps NULL */

      seginfo = seg_info (seg);
      if (seginfo && (!seginfo->frchainP || seginfo->frchainP == frcP))
	seginfo->frchainP = newP;

      frcP = newP;
    }
  /*
   * Here with frcP pointing to the frchainS for subseg.
   */
  frchain_now = frcP;
  frag_now = frcP->frch_frag_now;

  assert (frchain_now->frch_last == frag_now);
}

/*
 *			subseg_set(segT, subsegT)
 *
 * If you attempt to change to the current subsegment, nothing happens.
 *
 * In:	segT, subsegT code for new subsegment.
 *	frag_now -> incomplete frag for current subsegment.
 *	If frag_now==NULL, then there is no old, incomplete frag, so
 *	the old frag is not closed off.
 *
 * Out:	now_subseg, now_seg updated.
 *	Frchain_now points to the (possibly new) struct frchain for this
 *	sub-segment.
 *	Frchain_root updated if needed.
 */

segT
subseg_get (const char *segname, int force_new)
{
  segT secptr;
  segment_info_type *seginfo;
  const char *now_seg_name = (now_seg
			      ? bfd_get_section_name (stdoutput, now_seg)
			      : 0);

  if (!force_new
      && now_seg_name
      && (now_seg_name == segname
	  || !strcmp (now_seg_name, segname)))
    return now_seg;

  if (!force_new)
    secptr = bfd_make_section_old_way (stdoutput, segname);
  else
    secptr = bfd_make_section_anyway (stdoutput, segname);

#ifdef obj_sec_set_private_data
  obj_sec_set_private_data (stdoutput, secptr);
#endif

  seginfo = seg_info (secptr);
  if (! seginfo)
    {
      /* Check whether output_section is set first because secptr may
	 be bfd_abs_section_ptr.  */
      if (secptr->output_section != secptr)
	secptr->output_section = secptr;
      seginfo = (segment_info_type *) xmalloc (sizeof (*seginfo));
      memset ((PTR) seginfo, 0, sizeof (*seginfo));
      seginfo->fix_root = NULL;
      seginfo->fix_tail = NULL;
      seginfo->bfd_section = secptr;
      if (secptr == bfd_abs_section_ptr)
	abs_seg_info = seginfo;
      else if (secptr == bfd_und_section_ptr)
	und_seg_info = seginfo;
      else
	bfd_set_section_userdata (stdoutput, secptr, (PTR) seginfo);
      seginfo->frchainP = NULL;
      seginfo->lineno_list_head = seginfo->lineno_list_tail = NULL;
      seginfo->sym = NULL;
      seginfo->dot = NULL;
    }
  return secptr;
}

segT
subseg_new (const char *segname, subsegT subseg)
{
  segT secptr;
  segment_info_type *seginfo;

  secptr = subseg_get (segname, 0);
  subseg_set_rest (secptr, subseg);
  seginfo = seg_info (secptr);
  if (! seginfo->frchainP)
    seginfo->frchainP = frchain_now;
  return secptr;
}

/* Like subseg_new, except a new section is always created, even if
   a section with that name already exists.  */
segT
subseg_force_new (const char *segname, subsegT subseg)
{
  segT secptr;
  segment_info_type *seginfo;

  secptr = subseg_get (segname, 1);
  subseg_set_rest (secptr, subseg);
  seginfo = seg_info (secptr);
  if (! seginfo->frchainP)
    seginfo->frchainP = frchain_now;
  return secptr;
}

void
subseg_set (segT secptr, subsegT subseg)
{
  if (! (secptr == now_seg && subseg == now_subseg))
    subseg_set_rest (secptr, subseg);
  mri_common_symbol = NULL;
}

#ifndef obj_sec_sym_ok_for_reloc
#define obj_sec_sym_ok_for_reloc(SEC)	0
#endif

/* Get the gas information we are storing for a section.  */

segment_info_type *
seg_info (segT sec)
{
  if (sec == bfd_abs_section_ptr)
    return abs_seg_info;
  else if (sec == bfd_und_section_ptr)
    return und_seg_info;
  else
    return (segment_info_type *) bfd_get_section_userdata (stdoutput, sec);
}

symbolS *
section_symbol (segT sec)
{
  segment_info_type *seginfo = seg_info (sec);
  symbolS *s;

  if (seginfo == 0)
    abort ();
  if (seginfo->sym)
    return seginfo->sym;

#ifndef EMIT_SECTION_SYMBOLS
#define EMIT_SECTION_SYMBOLS 1
#endif

  if (! EMIT_SECTION_SYMBOLS || symbol_table_frozen)
    {
      /* Here we know it won't be going into the symbol table.  */
      s = symbol_create (sec->symbol->name, sec, 0, &zero_address_frag);
    }
  else
    {
      segT seg;
      s = symbol_find (sec->symbol->name);
      /* We have to make sure it is the right symbol when we
	 have multiple sections with the same section name.  */
      if (s == NULL
	  || ((seg = S_GET_SEGMENT (s)) != sec
	      && seg != undefined_section))
	s = symbol_new (sec->symbol->name, sec, 0, &zero_address_frag);
      else if (seg == undefined_section)
	{
	  S_SET_SEGMENT (s, sec);
	  symbol_set_frag (s, &zero_address_frag);
	}
    }

  S_CLEAR_EXTERNAL (s);

  /* Use the BFD section symbol, if possible.  */
  if (obj_sec_sym_ok_for_reloc (sec))
    symbol_set_bfdsym (s, sec->symbol);
  else
    symbol_get_bfdsym (s)->flags |= BSF_SECTION_SYM;

  seginfo->sym = s;
  return s;
}

/* Return whether the specified segment is thought to hold text.  */

int
subseg_text_p (segT sec)
{
  return (bfd_get_section_flags (stdoutput, sec) & SEC_CODE) != 0;
}

/* Return non zero if SEC has at least one byte of data.  It is
   possible that we'll return zero even on a non-empty section because
   we don't know all the fragment types, and it is possible that an
   fr_fix == 0 one still contributes data.  Think of this as
   seg_definitely_not_empty_p.  */

int
seg_not_empty_p (segT sec ATTRIBUTE_UNUSED)
{
  segment_info_type *seginfo = seg_info (sec);
  frchainS *chain;
  fragS *frag;

  if (!seginfo)
    return 0;
  
  for (chain = seginfo->frchainP; chain; chain = chain->frch_next)
    {
      for (frag = chain->frch_root; frag; frag = frag->fr_next)
	if (frag->fr_fix)
	  return 1;
      if (obstack_next_free (&chain->frch_obstack)
	  != chain->frch_last->fr_literal)
	return 1;
    }
  return 0;
}

void
subsegs_print_statistics (FILE *file)
{
  frchainS *frchp;
  fprintf (file, "frag chains:\n");
  for (frchp = frchain_root; frchp; frchp = frchp->frch_next)
    {
      int count = 0;
      fragS *fragp;

      /* If frch_subseg is non-zero, it's probably been chained onto
	 the end of a previous subsection.  Don't count it again.  */
      if (frchp->frch_subseg != 0)
	continue;

      /* Skip gas-internal sections.  */
      if (segment_name (frchp->frch_seg)[0] == '*')
	continue;

      for (fragp = frchp->frch_root; fragp; fragp = fragp->fr_next)
	{
	  count++;
	}
      fprintf (file, "\n");
      fprintf (file, "\t%p %-10s\t%10d frags\n", (void *) frchp,
	       segment_name (frchp->frch_seg), count);
    }
}

/* end of subsegs.c */
