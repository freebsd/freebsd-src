/* subsegs.c - subsegments -
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
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * Segments & sub-segments.
 */

#ifndef lint
static char rcsid[] = "$Id: subsegs.c,v 1.4 1995/05/30 04:46:34 rgrimes Exp $";
#endif

#include "as.h"

#include "subsegs.h"
#include "obstack.h"

#ifdef MANY_SEGMENTS
segment_info_type segment_info[SEG_MAXIMUM_ORDINAL];

frchainS*	frchain_root,
    *	frchain_now;

#else
frchainS*	frchain_root,
    *	frchain_now,	/* Commented in "subsegs.h". */
    *	data0_frchainP;

#endif
char * const /* in: segT   out: char* */
    seg_name[] = {
	    "absolute",
#ifdef MANY_SEGMENTS
	    "e0","e1","e2","e3","e4","e5","e6","e7","e8","e9",
#else
	    "text",
	    "data",
	    "bss",
#endif
	    "unknown",
	    "absent",
	    "pass1",
	    "ASSEMBLER-INTERNAL-LOGIC-ERROR!",
	    "bignum/flonum",
	    "difference",
	    "debug",
	    "transfert vector preload",
	    "transfert vector postload",
	    "register",
	    "",
    }; /* Used by error reporters, dumpers etc. */


void
    subsegs_begin()
{
	/* Check table(s) seg_name[], seg_N_TYPE[] is in correct order */
#ifdef MANY_SEGMENTS
#else
	know(SEG_ABSOLUTE == 0);
	know(SEG_TEXT == 1);
	know(SEG_DATA == 2);
	know(SEG_BSS == 3);
	know(SEG_UNKNOWN == 4);
	know(SEG_ABSENT == 5);
	know(SEG_PASS1 == 6);
	know(SEG_GOOF == 7);
	know(SEG_BIG == 8);
	know(SEG_DIFFERENCE == 9);
	know(SEG_DEBUG == 10);
	know(SEG_NTV == 11);
	know(SEG_PTV == 12);
	know(SEG_REGISTER == 13);
	know(SEG_MAXIMUM_ORDINAL == SEG_REGISTER );
	/*  know(segment_name(SEG_MAXIMUM_ORDINAL + 1)[0] == 0);*/
#endif

	obstack_begin(&frags, 5000);
	frchain_root = NULL;
	frchain_now = NULL;		/* Warn new_subseg() that we are booting. */
	/* Fake up 1st frag. */
	/* It won't be used=> is ok if obstack... */
	/* pads the end of it for alignment. */
	frag_now=(fragS *)obstack_alloc(&frags,SIZEOF_STRUCT_FRAG);
	memset(frag_now,  SIZEOF_STRUCT_FRAG, 0);
	/* This 1st frag will not be in any frchain. */
	/* We simply give subseg_new somewhere to scribble. */
	now_subseg = 42;		/* Lie for 1st call to subseg_new. */
#ifdef MANY_SEGMENTS
	{
		int i;
		for (i = SEG_E0; i < SEG_UNKNOWN; i++) {
			subseg_new(i, 0);
			segment_info[i].frchainP = frchain_now;
		}
	}
#else
	subseg_new(SEG_DATA, 0);	/* .data 0 */
	data0_frchainP = frchain_now;
#endif

}

/*
 *			subseg_change()
 *
 * Change the subsegment we are in, BUT DO NOT MAKE A NEW FRAG for the
 * subsegment. If we are already in the correct subsegment, change nothing.
 * This is used eg as a worker for subseg_new [which does make a new frag_now]
 * and for changing segments after we have read the source. We construct eg
 * fixSs even after the source file is read, so we do have to keep the
 * segment context correct.
 */
void
    subseg_change (seg, subseg)
register segT	seg;
register int	subseg;
{
	now_seg = seg;
	now_subseg = subseg;
	know(SEG_NORMAL(seg));
#ifdef MANY_SEGMENTS
	seg_fix_rootP = &segment_info[seg].fix_root;
	seg_fix_tailP = &segment_info[seg].fix_tail;
#else
	switch (seg) {
	case SEG_DATA:
		seg_fix_rootP = &data_fix_root;
		seg_fix_tailP = &data_fix_tail;
		break;
	case SEG_TEXT:
		seg_fix_rootP = &text_fix_root;
		seg_fix_tailP = &text_fix_tail;
		break;
	case SEG_BSS:
		seg_fix_rootP = &bss_fix_root;
		seg_fix_tailP = &bss_fix_tail;
		break;
	}
#endif
}

/*
 *			subseg_new()
 *
 * If you attempt to change to the current subsegment, nothing happens.
 *
 * In:	segT, subsegT code for new subsegment.
 *	frag_now -> incomplete frag for current subsegment.
 *	If frag_now == NULL, then there is no old, incomplete frag, so
 *	the old frag is not closed off.
 *
 * Out:	now_subseg, now_seg updated.
 *	Frchain_now points to the (possibly new) struct frchain for this
 *	sub-segment.
 *	Frchain_root updated if needed.
 */

void
    subseg_new (seg, subseg)	/* begin assembly for a new sub-segment */
register segT	seg; /* SEG_DATA or SEG_TEXT */
register subsegT	subseg;
{
	long tmp;		/* JF for obstack alignment hacking */
	know(SEG_NORMAL(seg));
	if (seg != now_seg || subseg != now_subseg)
	    {				/* we just changed sub-segments */
		    register frchainS *	frcP;	/* crawl frchain chain */
		    register frchainS**	lastPP;	/* address of last pointer */
		    frchainS *newP;	/* address of new frchain */
		    register fragS *former_last_fragP;
		    register fragS *new_fragP;

		    if (frag_now)		/* If not bootstrapping. */
			{
				frag_now->fr_fix = obstack_next_free(& frags) - frag_now->fr_literal;
				frag_wane(frag_now);	/* Close off any frag in old subseg. */
			}
		    /*
		     * It would be nice to keep an obstack for each subsegment, if we swap
		     * subsegments a lot. Hence we would have much fewer frag_wanes().
		     */
		    {

			    obstack_finish( &frags);
			    /*
			     * If we don't do the above, the next object we put on obstack frags
			     * will appear to start at the fr_literal of the current frag.
			     * Also, above ensures that the next object will begin on a
			     * address that is aligned correctly for the engine that runs
			     * this program.
			     */
		    }
		    subseg_change (seg, (int)subseg);
		    /*
		     * Attempt to find or make a frchain for that sub seg.
		     * Crawl along chain of frchainSs, begins @ frchain_root.
		     * If we need to make a frchainS, link it into correct
		     * position of chain rooted in frchain_root.
		     */
		    for (frcP = * (lastPP = & frchain_root);
			 frcP
			 && (int)(frcP->frch_seg) <= (int)seg;
			 frcP = * ( lastPP = & frcP->frch_next)
			 )
			{
				if (   (int)(frcP->frch_seg) == (int)seg
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
		     *		If frcP == NULL, then we are at the end of the chain
		     *		of frchainS-s. A NULL frcP means we fell off the end
		     *		of the chain looking for a
		     *		frch_subseg >= subseg, so we
		     *		must make a new frchainS.
		     *
		     *		If we ever maintain a pointer to
		     *		the last frchainS in the chain, we change that pointer
		     *		ONLY when frcP == NULL.
		     *
		     * lastPP:	Address of the pointer with value frcP;
		     *		Never NULL.
		     *		May point to frchain_root.
		     *
		     */
		    if (   ! frcP
			|| (   (int)(frcP->frch_seg) > (int)seg
			    || frcP->frch_subseg > subseg)) /* Kinky logic only works with 2 segments. */
			{
				/*
				 * This should be the only code that creates a frchainS.
				 */
				newP=(frchainS *)obstack_alloc(&frags,sizeof(frchainS));
				memset(newP, sizeof(frchainS), 0);
				/* This begines on a good boundary */
				/* because a obstack_done() preceeded  it. */
				/* It implies an obstack_done(), so we */
				/* expect the next object allocated to */
				/* begin on a correct boundary. */
				*lastPP = newP;
				newP->frch_next = frcP; /* perhaps NULL */
				(frcP = newP)->frch_subseg		= subseg;
				newP->frch_seg		= seg;
				newP->frch_last	 	= NULL;
			}
		    /*
		     * Here with frcP->ing to the frchainS for subseg.
		     */
		    frchain_now = frcP;
		    /*
		     * Make a fresh frag for the subsegment.
		     */
		    /* We expect this to happen on a correct */
		    /* boundary since it was proceeded by a */
		    /* obstack_done(). */
		    tmp=obstack_alignment_mask(&frags);	/* JF disable alignment */
		    obstack_alignment_mask(&frags)=0;
		    frag_now=(fragS *)obstack_alloc(&frags,SIZEOF_STRUCT_FRAG);
		    obstack_alignment_mask(&frags)=tmp;
		    /* know(frags.obstack_c_next_free == frag_now->fr_literal); */
		    /* But we want any more chars to come */
		    /* immediately after the structure we just made. */
		    new_fragP = frag_now;
		    new_fragP->fr_next = NULL;
		    /*
		     * Append new frag to current frchain.
		     */
		    former_last_fragP = frcP->frch_last;
		    if (former_last_fragP)
			{
				know( former_last_fragP->fr_next == NULL );
				know( frchain_now->frch_root );
				former_last_fragP->fr_next = new_fragP;
			}
		    else
			{
				frcP->frch_root = new_fragP;
			}
		    frcP->frch_last = new_fragP;
	    }				/* if (changing subsegments) */
}				/* subseg_new() */

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of subsegs.c */
