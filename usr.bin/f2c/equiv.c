/****************************************************************
Copyright 1990, 1993 by AT&T Bell Laboratories and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T Bell Laboratories or
Bellcore or any of their entities not be used in advertising or
publicity pertaining to distribution of the software without
specific, written prior permission.

AT&T and Bellcore disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall AT&T or Bellcore be liable for
any special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
****************************************************************/

#include "defs.h"

LOCAL eqvcommon(), eqveqv(), nsubs();

/* ROUTINES RELATED TO EQUIVALENCE CLASS PROCESSING */

/* called at end of declarations section to process chains
   created by EQUIVALENCE statements
 */
doequiv()
{
	register int i;
	int inequiv;			/* True if one namep occurs in
					   several EQUIV declarations */
	int comno;		/* Index into Extsym table of the last
				   COMMON block seen (implicitly assuming
				   that only one will be given) */
	int ovarno;
	ftnint comoffset;	/* Index into the COMMON block */
	ftnint offset;		/* Offset from array base */
	ftnint leng;
	register struct Equivblock *equivdecl;
	register struct Eqvchain *q;
	struct Primblock *primp;
	register Namep np;
	int k, k1, ns, pref, t;
	chainp cp;
	extern int type_pref[];
	char *s;

	for(i = 0 ; i < nequiv ; ++i)
	{

/* Handle each equivalence declaration */

		equivdecl = &eqvclass[i];
		equivdecl->eqvbottom = equivdecl->eqvtop = 0;
		comno = -1;



		for(q = equivdecl->equivs ; q ; q = q->eqvnextp)
		{
			offset = 0;
			primp = q->eqvitem.eqvlhs;
			vardcl(np = primp->namep);
			if(primp->argsp || primp->fcharp)
			{
				expptr offp, suboffset();

/* Pad ones onto the end of an array declaration when needed */

				if(np->vdim!=NULL && np->vdim->ndim>1 &&
				    nsubs(primp->argsp)==1 )
				{
					if(! ftn66flag)
						warni
			("1-dim subscript in EQUIVALENCE, %d-dim declared",
						    np -> vdim -> ndim);
					cp = NULL;
					ns = np->vdim->ndim;
					while(--ns > 0)
						cp = mkchain((char *)ICON(1), cp);
					primp->argsp->listp->nextp = cp;
				}

				offp = suboffset(primp);
				if(ISICON(offp))
					offset = offp->constblock.Const.ci;
				else	{
					dclerr
			("nonconstant subscript in equivalence ",
					    np);
					np = NULL;
				}
				frexpr(offp);
			}

/* Free up the primblock, since we now have a hash table (Namep) entry */

			frexpr((expptr)primp);

			if(np && (leng = iarrlen(np))<0)
			{
				dclerr("adjustable in equivalence", np);
				np = NULL;
			}

			if(np) switch(np->vstg)
			{
			case STGUNKNOWN:
			case STGBSS:
			case STGEQUIV:
				if (in_vector(np->cvarname, st_fields,
						n_st_fields) >= 0) {
					k = strlen(np->cvarname);
					strcpy(s = mem(k+2,0), np->cvarname);
					s[k] = '_';
					s[k+1] = 0;
					np->cvarname = s;
					}
				break;

			case STGCOMMON:

/* The code assumes that all COMMON references in a given EQUIVALENCE will
   be to the same COMMON block, and will all be consistent */

				comno = np->vardesc.varno;
				comoffset = np->voffset + offset;
				break;

			default:
				dclerr("bad storage class in equivalence", np);
				np = NULL;
				break;
			}

			if(np)
			{
				q->eqvoffset = offset;

/* eqvbottom   gets the largest difference between the array base address
   and the address specified in the EQUIV declaration */

				equivdecl->eqvbottom =
				    lmin(equivdecl->eqvbottom, -offset);

/* eqvtop   gets the largest difference between the end of the array and
   the address given in the EQUIVALENCE */

				equivdecl->eqvtop =
				    lmax(equivdecl->eqvtop, leng-offset);
			}
			q->eqvitem.eqvname = np;
		}

/* Now all equivalenced variables are in the hash table with the proper
   offset, and   eqvtop and eqvbottom   are set. */

		if(comno >= 0)

/* Get rid of all STGEQUIVS, they will be mapped onto STGCOMMON variables
   */

			eqvcommon(equivdecl, comno, comoffset);
		else for(q = equivdecl->equivs ; q ; q = q->eqvnextp)
		{
			if(np = q->eqvitem.eqvname)
			{
				inequiv = NO;
				if(np->vstg==STGEQUIV)
					if( (ovarno = np->vardesc.varno) == i)
					{

/* Can't EQUIV different elements of the same array */

						if(np->voffset + q->eqvoffset != 0)
							dclerr
			("inconsistent equivalence", np);
					}
					else	{
						offset = np->voffset;
						inequiv = YES;
					}

				np->vstg = STGEQUIV;
				np->vardesc.varno = i;
				np->voffset = - q->eqvoffset;

				if(inequiv)

/* Combine 2 equivalence declarations */

					eqveqv(i, ovarno, q->eqvoffset + offset);
			}
		}
	}

/* Now each equivalence declaration is distinct (all connections have been
   merged in eqveqv()), and some may be empty. */

	for(i = 0 ; i < nequiv ; ++i)
	{
		equivdecl = & eqvclass[i];
		if(equivdecl->eqvbottom!=0 || equivdecl->eqvtop!=0) {

/* a live chain */

			k = TYCHAR;
			pref = 1;
			for(q = equivdecl->equivs ; q; q = q->eqvnextp)
			    if ((np = q->eqvitem.eqvname)
			    		&& !np->veqvadjust) {
				np->veqvadjust = 1;
				np->voffset -= equivdecl->eqvbottom;
				t = typealign[k1 = np->vtype];
				if (pref < type_pref[k1]) {
					k = k1;
					pref = type_pref[k1];
					}
				if(np->voffset % t != 0) {
					dclerr("bad alignment forced by equivalence", np);
					--nerr; /* don't give bad return code for this */
					}
				}
			equivdecl->eqvtype = k;
		}
		freqchain(equivdecl);
	}
}





/* put equivalence chain p at common block comno + comoffset */

LOCAL eqvcommon(p, comno, comoffset)
struct Equivblock *p;
int comno;
ftnint comoffset;
{
	int ovarno;
	ftnint k, offq;
	register Namep np;
	register struct Eqvchain *q;

	if(comoffset + p->eqvbottom < 0)
	{
		errstr("attempt to extend common %s backward",
		    extsymtab[comno].fextname);
		freqchain(p);
		return;
	}

	if( (k = comoffset + p->eqvtop) > extsymtab[comno].extleng)
		extsymtab[comno].extleng = k;


	for(q = p->equivs ; q ; q = q->eqvnextp)
		if(np = q->eqvitem.eqvname)
		{
			switch(np->vstg)
			{
			case STGUNKNOWN:
			case STGBSS:
				np->vstg = STGCOMMON;
				np->vcommequiv = 1;
				np->vardesc.varno = comno;

/* np -> voffset   will point to the base of the array */

				np->voffset = comoffset - q->eqvoffset;
				break;

			case STGEQUIV:
				ovarno = np->vardesc.varno;

/* offq   will point to the current element, even if it's in an array */

				offq = comoffset - q->eqvoffset - np->voffset;
				np->vstg = STGCOMMON;
				np->vcommequiv = 1;
				np->vardesc.varno = comno;

/* np -> voffset   will point to the base of the array */

				np->voffset += offq;
				if(ovarno != (p - eqvclass))
					eqvcommon(&eqvclass[ovarno], comno, offq);
				break;

			case STGCOMMON:
				if(comno != np->vardesc.varno ||
				    comoffset != np->voffset+q->eqvoffset)
					dclerr("inconsistent common usage", np);
				break;


			default:
				badstg("eqvcommon", np->vstg);
			}
		}

	freqchain(p);
	p->eqvbottom = p->eqvtop = 0;
}


/* Move all items on ovarno chain to the front of   nvarno   chain.
 * adjust offsets of ovarno elements and top and bottom of nvarno chain
 */

LOCAL eqveqv(nvarno, ovarno, delta)
int ovarno, nvarno;
ftnint delta;
{
	register struct Equivblock *neweqv, *oldeqv;
	register Namep np;
	struct Eqvchain *q, *q1;

	neweqv = eqvclass + nvarno;
	oldeqv = eqvclass + ovarno;
	neweqv->eqvbottom = lmin(neweqv->eqvbottom, oldeqv->eqvbottom - delta);
	neweqv->eqvtop = lmax(neweqv->eqvtop, oldeqv->eqvtop - delta);
	oldeqv->eqvbottom = oldeqv->eqvtop = 0;

	for(q = oldeqv->equivs ; q ; q = q1)
	{
		q1 = q->eqvnextp;
		if( (np = q->eqvitem.eqvname) && np->vardesc.varno==ovarno)
		{
			q->eqvnextp = neweqv->equivs;
			neweqv->equivs = q;
			q->eqvoffset += delta;
			np->vardesc.varno = nvarno;
			np->voffset -= delta;
		}
		else	free( (charptr) q);
	}
	oldeqv->equivs = NULL;
}




freqchain(p)
register struct Equivblock *p;
{
	register struct Eqvchain *q, *oq;

	for(q = p->equivs ; q ; q = oq)
	{
		oq = q->eqvnextp;
		free( (charptr) q);
	}
	p->equivs = NULL;
}





/* nsubs -- number of subscripts in this arglist (just the length of the
   list) */

LOCAL nsubs(p)
register struct Listblock *p;
{
	register int n;
	register chainp q;

	n = 0;
	if(p)
		for(q = p->listp ; q ; q = q->nextp)
			++n;

	return(n);
}
