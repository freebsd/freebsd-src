/*    av.h
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

struct xpvav {
    char*	xav_array;      /* pointer to first array element */
    SSize_t	xav_fill;       /* Index of last element present */
    SSize_t	xav_max;        /* max index for which array has space */
    IV		xof_off;	/* ptr is incremented by offset */
    NV		xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* magic for scalar array */
    HV*		xmg_stash;	/* class package */

    SV**	xav_alloc;	/* pointer to malloced string */
    SV*		xav_arylen;
    U8		xav_flags;
};


/* AVf_REAL is set for all AVs whose xav_array contents are refcounted.
 * Some things like "@_" and the scratchpad list do not set this, to
 * indicate that they are cheating (for efficiency) by not refcounting
 * the AV's contents.
 * 
 * AVf_REIFY is only meaningful on such "fake" AVs (i.e. where AVf_REAL
 * is not set).  It indicates that the fake AV is capable of becoming
 * real if the array needs to be modified in some way.  Functions that
 * modify fake AVs check both flags to call av_reify() as appropriate.
 *
 * Note that the Perl stack and @DB::args have neither flag set. (Thus,
 * items that go on the stack are never refcounted.)
 *
 * These internal details are subject to change any time.  AV
 * manipulations external to perl should not care about any of this.
 * GSAR 1999-09-10
 */
#define AVf_REAL 1	/* free old entries */
#define AVf_REIFY 2	/* can become real */

/* XXX this is not used anywhere */
#define AVf_REUSED 4	/* got undeffed--don't turn old memory into SVs now */

/*
=for apidoc AmU||Nullav
Null AV pointer.

=for apidoc Am|int|AvFILL|AV* av
Same as C<av_len()>.  Deprecated, use C<av_len()> instead.

=cut
*/

#define Nullav Null(AV*)

#define AvARRAY(av)	((SV**)((XPVAV*)  SvANY(av))->xav_array)
#define AvALLOC(av)	((XPVAV*)  SvANY(av))->xav_alloc
#define AvMAX(av)	((XPVAV*)  SvANY(av))->xav_max
#define AvFILLp(av)	((XPVAV*)  SvANY(av))->xav_fill
#define AvARYLEN(av)	((XPVAV*)  SvANY(av))->xav_arylen
#define AvFLAGS(av)	((XPVAV*)  SvANY(av))->xav_flags

#define AvREAL(av)	(AvFLAGS(av) & AVf_REAL)
#define AvREAL_on(av)	(AvFLAGS(av) |= AVf_REAL)
#define AvREAL_off(av)	(AvFLAGS(av) &= ~AVf_REAL)
#define AvREIFY(av)	(AvFLAGS(av) & AVf_REIFY)
#define AvREIFY_on(av)	(AvFLAGS(av) |= AVf_REIFY)
#define AvREIFY_off(av)	(AvFLAGS(av) &= ~AVf_REIFY)
#define AvREUSED(av)	(AvFLAGS(av) & AVf_REUSED)
#define AvREUSED_on(av)	(AvFLAGS(av) |= AVf_REUSED)
#define AvREUSED_off(av) (AvFLAGS(av) &= ~AVf_REUSED)

#define AvREALISH(av)	(AvFLAGS(av) & (AVf_REAL|AVf_REIFY))
                                          
#define AvFILL(av)	((SvRMAGICAL((SV *) (av))) \
			  ? mg_size((SV *) av) : AvFILLp(av))

