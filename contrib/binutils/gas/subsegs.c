/* subsegs.c - subsegments -
   Copyright 1987, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000
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

/* Segments & sub-segments.  */

#include "as.h"

#include "subsegs.h"
#include "obstack.h"

frchainS *frchain_root, *frchain_now;

static struct obstack frchains;

#ifndef BFD_ASSEMBLER
#ifdef MANY_SEGMENTS
segment_info_type segment_info[SEG_MAXIMUM_ORDINAL];

#else
/* Commented in "subsegs.h".  */
frchainS *data0_frchainP, *bss0_frchainP;

#endif /* MANY_SEGMENTS */
char const *const seg_name[] = {
  "absolute",
#ifdef MANY_SEGMENTS
  "e0", "e1", "e2", "e3", "e4", "e5", "e6", "e7", "e8", "e9",
  "e10", "e11", "e12", "e13", "e14", "e15", "e16", "e17", "e18", "e19",
  "e20", "e21", "e22", "e23", "e24", "e25", "e26", "e27", "e28", "e29",
  "e30", "e31", "e32", "e33", "e34", "e35", "e36", "e37", "e38", "e39",
#else
  "text",
  "data",
  "bss",
#endif /* MANY_SEGMENTS */
  "unknown",
  "ASSEMBLER-INTERNAL-LOGIC-ERROR!",
  "expr",
  "debug",
  "transfert vector preload",
  "transfert vector postload",
  "register",
  "",
};				/* Used by error reporters, dumpers etc.  */
#else /* BFD_ASSEMBLER */

/* Gas segment information for bfd_abs_section_ptr and
   bfd_und_section_ptr.  */
static segment_info_type *abs_seg_info;
static segment_info_type *und_seg_info;

#endif /* BFD_ASSEMBLER */

static void subseg_set_rest PARAMS ((segT, subsegT));

static fragS dummy_frag;

static frchainS absolute_frchain;

void
subsegs_begin ()
{
  /* Check table(s) seg_name[], seg_N_TYPE[] is in correct order */
#if !defined (MANY_SEGMENTS) && !defined (BFD_ASSEMBLER)
  know (SEG_ABSOLUTE == 0);
  know (SEG_TEXT == 1);
  know (SEG_DATA == 2);
  know (SEG_BSS == 3);
  know (SEG_UNKNOWN == 4);
  know (SEG_GOOF == 5);
  know (SEG_EXPR == 6);
  know (SEG_DEBUG == 7);
  know (SEG_NTV == 8);
  know (SEG_PTV == 9);
  know (SEG_REGISTER == 10);
  know (SEG_MAXIMUM_ORDINAL == SEG_REGISTER);
#endif

  obstack_begin (&frchains, chunksize);
#if __GNUC__ >= 2
  obstack_alignment_mask (&frchains) = __alignof__ (frchainS) - 1;
#endif

  frchain_root = NULL;
  frchain_now = NULL;		/* Warn new_subseg() that we are booting.  */

  frag_now = &dummy_frag;

#ifndef BFD_ASSEMBLER
  now_subseg = 42;		/* Lie for 1st call to subseg_new.  */
#ifdef MANY_SEGMENTS
  {
    int i;
    for (i = SEG_E0; i < SEG_UNKNOWN; i++)
      {
	subseg_set (i, 0);
	segment_info[i].frchainP = frchain_now;
      }
  }
#else
  subseg_set (SEG_DATA, 0);	/* .data 0 */
  data0_frchainP = frchain_now;

  subseg_set (SEG_BSS, 0);
  bss0_frchainP = frchain_now;

#endif /* ! MANY_SEGMENTS */
#endif /* ! BFD_ASSEMBLER */

  absolute_frchain.frch_seg = absolute_section;
  absolute_frchain.frch_subseg = 0;
#ifdef BFD_ASSEMBLER
  absolute_frchain.fix_root = absolute_frchain.fix_tail = 0;
#endif
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
subseg_change (seg, subseg)
     register segT seg;
     register int subseg;
{
  now_seg = seg;
  now_subseg = subseg;

  if (now_seg == absolute_section)
    return;

#ifdef BFD_ASSEMBLER
  {
    segment_info_type *seginfo;
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
#else
#ifdef MANY_SEGMENTS
  seg_fix_rootP = &segment_info[seg].fix_root;
  seg_fix_tailP = &segment_info[seg].fix_tail;
#else
  if (seg == SEG_DATA)
    {
      seg_fix_rootP = &data_fix_root;
      seg_fix_tailP = &data_fix_tail;
    }
  else if (seg == SEG_TEXT)
    {
      seg_fix_rootP = &text_fix_root;
      seg_fix_tailP = &text_fix_tail;
    }
  else
    {
      know (seg == SEG_BSS);
      seg_fix_rootP = &bss_fix_root;
      seg_fix_tailP = &bss_fix_tail;
    }

#endif
#endif
}

static void
subseg_set_rest (seg, subseg)
     segT seg;
     subsegT subseg;
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
      newP = (frchainS *) obstack_alloc (&frchains, sizeof (frchainS));
      newP->frch_subseg = subseg;
      newP->frch_seg = seg;
#ifdef BFD_ASSEMBLER
      newP->fix_root = NULL;
      newP->fix_tail = NULL;
#endif
      obstack_begin (&newP->frch_obstack, chunksize);
#if __GNUC__ >= 2
      obstack_alignment_mask (&newP->frch_obstack) = __alignof__ (fragS) - 1;
#endif
      newP->frch_frag_now = frag_alloc (&newP->frch_obstack);
      newP->frch_frag_now->fr_type = rs_fill;

      newP->frch_root = newP->frch_last = newP->frch_frag_now;

      *lastPP = newP;
      newP->frch_next = frcP;	/* perhaps NULL */

#ifdef BFD_ASSEMBLER
      {
	segment_info_type *seginfo;
	seginfo = seg_info (seg);
	if (seginfo && seginfo->frchainP == frcP)
	  seginfo->frchainP = newP;
      }
#endif

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

#ifndef BFD_ASSEMBLER

segT
subseg_new (segname, subseg)
     const char *segname;
     subsegT subseg;
{
  int i;

  for (i = 0; i < (int) SEG_MAXIMUM_ORDINAL; i++)
    {
      const char *s;

      s = segment_name ((segT) i);
      if (strcmp (segname, s) == 0
	  || (segname[0] == '.'
	      && strcmp (segname + 1, s) == 0))
	{
	  subseg_set ((segT) i, subseg);
	  return (segT) i;
	}
#ifdef obj_segment_name
      s = obj_segment_name ((segT) i);
      if (strcmp (segname, s) == 0
	  || (segname[0] == '.'
	      && strcmp (segname + 1, s) == 0))
	{
	  subseg_set ((segT) i, subseg);
	  return (segT) i;
	}
#endif
    }

#ifdef obj_add_segment
  {
    segT new_seg;
    new_seg = obj_add_segment (segname);
    subseg_set (new_seg, subseg);
    return new_seg;
  }
#else
  as_bad (_("attempt to switch to nonexistent segment \"%s\""), segname);
  return now_seg;
#endif
}

void
subseg_set (seg, subseg)	/* begin assembly for a new sub-segment */
     register segT seg;		/* SEG_DATA or SEG_TEXT */
     register subsegT subseg;
{
#ifndef MANY_SEGMENTS
  know (seg == SEG_DATA
	|| seg == SEG_TEXT
	|| seg == SEG_BSS
	|| seg == SEG_ABSOLUTE);
#endif

  if (seg != now_seg || subseg != now_subseg)
    {				/* we just changed sub-segments */
      subseg_set_rest (seg, subseg);
    }
  mri_common_symbol = NULL;
}

#else /* BFD_ASSEMBLER */

segT
subseg_get (segname, force_new)
     const char *segname;
     int force_new;
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
subseg_new (segname, subseg)
     const char *segname;
     subsegT subseg;
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
subseg_force_new (segname, subseg)
     const char *segname;
     subsegT subseg;
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
subseg_set (secptr, subseg)
     segT secptr;
     subsegT subseg;
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
seg_info (sec)
     segT sec;
{
  if (sec == bfd_abs_section_ptr)
    return abs_seg_info;
  else if (sec == bfd_und_section_ptr)
    return und_seg_info;
  else
    return (segment_info_type *) bfd_get_section_userdata (stdoutput, sec);
}

symbolS *
section_symbol (sec)
     segT sec;
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

  if (! EMIT_SECTION_SYMBOLS
#ifdef BFD_ASSEMBLER
      || symbol_table_frozen
#endif
      )
    {
      /* Here we know it won't be going into the symbol table.  */
      s = symbol_create (sec->name, sec, 0, &zero_address_frag);
    }
  else
    {
      s = symbol_find_base (sec->name, 0);
      if (s == NULL)
	s = symbol_new (sec->name, sec, 0, &zero_address_frag);
      else
	{
	  if (S_GET_SEGMENT (s) == undefined_section)
	    {
	      S_SET_SEGMENT (s, sec);
	      symbol_set_frag (s, &zero_address_frag);
	    }
	}
    }

  S_CLEAR_EXTERNAL (s);

  /* Use the BFD section symbol, if possible.  */
  if (obj_sec_sym_ok_for_reloc (sec))
    symbol_set_bfdsym (s, sec->symbol);

  seginfo->sym = s;
  return s;
}

#endif /* BFD_ASSEMBLER */

/* Return whether the specified segment is thought to hold text.  */

#ifndef BFD_ASSEMBLER
const char * const nontext_section_names[] = {
  ".eh_frame",
  ".gcc_except_table",
#ifdef OBJ_COFF
#ifndef COFF_LONG_SECTION_NAMES
  ".eh_fram",
  ".gcc_exc",
#endif
#endif
  NULL
};
#endif /* ! BFD_ASSEMBLER */

int
subseg_text_p (sec)
     segT sec;
{
#ifdef BFD_ASSEMBLER
  return (bfd_get_section_flags (stdoutput, sec) & SEC_CODE) != 0;
#else /* ! BFD_ASSEMBLER */
  const char * const *p;

  if (sec == data_section || sec == bss_section || sec == absolute_section)
    return 0;

  for (p = nontext_section_names; *p != NULL; ++p)
    {
      if (strcmp (segment_name (sec), *p) == 0)
	return 0;

#ifdef obj_segment_name
      if (strcmp (obj_segment_name (sec), *p) == 0)
	return 0;
#endif
    }

  return 1;

#endif /* ! BFD_ASSEMBLER */
}

void
subsegs_print_statistics (file)
     FILE *file;
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
#if 0
	  switch (fragp->fr_type)
	    {
	    case rs_fill:
	      fprintf (file, "f"); break;
	    case rs_align:
	      fprintf (file, "a"); break;
	    case rs_align_code:
	      fprintf (file, "c"); break;
	    case rs_org:
	      fprintf (file, "o"); break;
	    case rs_machine_dependent:
	      fprintf (file, "m"); break;
	    case rs_space:
	      fprintf (file, "s"); break;
	    case 0:
	      fprintf (file, "0"); break;
	    default:
	      fprintf (file, "?"); break;
	    }
#endif
	  count++;
	}
      fprintf (file, "\n");
      fprintf (file, "\t%p %-10s\t%10d frags\n", (void *) frchp,
	       segment_name (frchp->frch_seg), count);
    }
}

/* end of subsegs.c */
