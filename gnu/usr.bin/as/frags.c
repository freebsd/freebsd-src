/* frags.c - manage frags -

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

#ifndef lint
static char rcsid[] = "$FreeBSD: src/gnu/usr.bin/as/frags.c,v 1.6 1999/08/27 23:34:16 peter Exp $";
#endif

#include "as.h"
#include "subsegs.h"
#include "obstack.h"

struct obstack  frags;	/* All, and only, frags live here. */

fragS zero_address_frag = {
	0,			/* fr_address */
	NULL,			/* fr_next */
	0,			/* fr_fix */
	0,			/* fr_var */
	0,			/* fr_symbol */
	0,			/* fr_offset */
	NULL,			/* fr_opcode */
	rs_fill,		/* fr_type */
	0,			/* fr_subtype */
	0,			/* fr_pcrel_adjust */
	0,			/* fr_bsr */
	0			/* fr_literal[0] */
    };

fragS bss_address_frag = {
	0,			/* fr_address. Gets filled in to make up
				   sy_value-s. */
	NULL,			/* fr_next */
	0,			/* fr_fix */
	0,			/* fr_var */
	0,			/* fr_symbol */
	0,			/* fr_offset */
	NULL,			/* fr_opcode */
	rs_fill,		/* fr_type */
	0,			/* fr_subtype */
	0,			/* fr_pcrel_adjust */
	0,			/* fr_bsr */
	0			/* fr_literal[0] */
    };

/*
 *			frag_grow()
 *
 * Internal.
 * Try to augment current frag by nchars chars.
 * If there is no room, close of the current frag with a ".fill 0"
 * and begin a new frag. Unless the new frag has nchars chars available
 * do not return. Do not set up any fields of *now_frag.
 */
static void frag_grow(nchars)
unsigned int nchars;
{
	if (obstack_room (&frags) < nchars) {
		unsigned int n,oldn;
		long oldc;

		frag_wane(frag_now);
		frag_new(0);
		oldn=(unsigned)-1;
		oldc=frags.chunk_size;
		frags.chunk_size=2*nchars;
		while ((n=obstack_room(&frags))<nchars && n<oldn) {
			frag_wane(frag_now);
			frag_new(0);
			oldn=n;
		}
		frags.chunk_size=oldc;
	}
	if (obstack_room (&frags) < nchars)
	    as_fatal("Can't extend frag %d. chars", nchars);
} /* frag_grow() */

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
void frag_new(old_frags_var_max_size)
int old_frags_var_max_size;	/* Number of chars (already allocated on
				   obstack frags) */
/* in variable_length part of frag. */
{
	register    fragS * former_last_fragP;
	/*    char   *throw_away_pointer; JF unused */
	register    frchainS * frchP;
	long	tmp;		/* JF */

	frag_now->fr_fix = (char *) (obstack_next_free (&frags)) -
	    (frag_now->fr_literal) - old_frags_var_max_size;
	/* Fix up old frag's fr_fix. */

	obstack_finish (&frags);
	/* This will align the obstack so the */
	/* next struct we allocate on it will */
	/* begin at a correct boundary. */
	frchP = frchain_now;
	know (frchP);
	former_last_fragP = frchP->frch_last;
	know (former_last_fragP);
	know (former_last_fragP == frag_now);
	obstack_blank (&frags, SIZEOF_STRUCT_FRAG);
	/* We expect this will begin at a correct */
	/* boundary for a struct. */
	tmp=obstack_alignment_mask(&frags);
	obstack_alignment_mask(&frags)=0;		/* Turn off alignment */
	/* If we ever hit a machine
	   where strings must be
	   aligned, we Lose Big */
	frag_now=(fragS *)obstack_finish(&frags);
	obstack_alignment_mask(&frags)=tmp;		/* Restore alignment */

	/* Just in case we don't get zero'd bytes */
	memset(frag_now, '\0', SIZEOF_STRUCT_FRAG);

	/*    obstack_unaligned_done (&frags, &frag_now); */
	/*    know (frags.obstack_c_next_free == frag_now->fr_literal); */
	/* Generally, frag_now->points to an */
	/* address rounded up to next alignment. */
	/* However, characters will add to obstack */
	/* frags IMMEDIATELY after the struct frag, */
	/* even if they are not starting at an */
	/* alignment address. */
	former_last_fragP->fr_next = frag_now;
	frchP->frch_last = frag_now;

#ifndef NO_LISTING
	{
		extern struct list_info_struct *listing_tail;
		frag_now->line = listing_tail;
	}
#endif

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

char *frag_more (nchars)
int nchars;
{
	register char  *retval;

	frag_grow (nchars);
	retval = obstack_next_free (&frags);
	obstack_blank_fast (&frags, nchars);
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

char *frag_var(type, max_chars, var, subtype, symbol, offset, opcode)
relax_stateT type;
int max_chars;
int var;
relax_substateT subtype;
symbolS *symbol;
long offset;
char *opcode;
{
	register char  *retval;

	frag_grow (max_chars);
	retval = obstack_next_free (&frags);
	obstack_blank_fast (&frags, max_chars);
	frag_now->fr_var = var;
	frag_now->fr_type = type;
	frag_now->fr_subtype = subtype;
	frag_now->fr_symbol = symbol;
	frag_now->fr_offset = offset;
	frag_now->fr_opcode = opcode;
	/* default these to zero. */
	frag_now->fr_pcrel_adjust = 0;
	frag_now->fr_bsr = 0;
	frag_new (max_chars);
	return (retval);
}				/* frag_var() */

/*
 *			frag_variant()
 *
 * OVE: This variant of frag_var assumes that space for the tail has been
 *      allocated by caller.
 *      No call to frag_grow is done.
 *      Two new arguments have been added.
 */

char *frag_variant(type, max_chars, var, subtype, symbol, offset, opcode, pcrel_adjust,bsr)
relax_stateT type;
int max_chars;
int var;
relax_substateT subtype;
symbolS *symbol;
long offset;
char *opcode;
int pcrel_adjust;
char bsr;
{
	register char  *retval;

	/* frag_grow (max_chars); */
	retval = obstack_next_free (&frags);
	/*  obstack_blank_fast (&frags, max_chars); */ /* OVE: so far the only diff */
	frag_now->fr_var = var;
	frag_now->fr_type = type;
	frag_now->fr_subtype = subtype;
	frag_now->fr_symbol = symbol;
	frag_now->fr_offset = offset;
	frag_now->fr_opcode = opcode;
	frag_now->fr_pcrel_adjust = pcrel_adjust;
	frag_now->fr_bsr = bsr;
	frag_new(max_chars);
	return(retval);
}				/* frag_variant() */

/*
 *			frag_wane()
 *
 * Reduce the variable end of a frag to a harmless state.
 */
void frag_wane(fragP)
register    fragS * fragP;
{
	fragP->fr_type = rs_fill;
	fragP->fr_offset = 0;
	fragP->fr_var = 0;
}

/*
 *			frag_align()
 *
 * Make a frag for ".align foo,bar". Call is "frag_align (foo,bar);".
 * Foo & bar are absolute integers.
 *
 * Call to close off the current frag with a ".align", then start a new
 * (so far empty) frag, in the same subsegment as the last frag.
 */

void frag_align(alignment, fill_character)
int alignment;
int fill_character;
{
	*(frag_var (rs_align, 1, 1, (relax_substateT)0, (symbolS *)0,
		    (long)alignment, (char *)0)) = fill_character;
} /* frag_align() */

/* end of frags.c */
