/****************************************************************
Copyright 1990, 1991, 1993 by AT&T Bell Laboratories and Bellcore.

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

/*
 * INTERMEDIATE CODE GENERATION PROCEDURES COMMON TO BOTH
 * JOHNSON (PORTABLE) AND RITCHIE FAMILIES OF SECOND PASSES
*/

#include "defs.h"
#include "names.h"		/* For LOCAL_CONST_NAME */
#include "pccdefs.h"
#include "p1defs.h"

/* Definitions for   putconst()   */

#define LIT_CHAR 1
#define LIT_FLOAT 2
#define LIT_INT 3


/*
char *ops [ ] =
	{
	"??", "+", "-", "*", "/", "**", "-",
	"OR", "AND", "EQV", "NEQV", "NOT",
	"CONCAT",
	"<", "==", ">", "<=", "!=", ">=",
	" of ", " ofC ", " = ", " += ", " *= ", " CONV ", " << ", " % ",
	" , ", " ? ", " : "
	" abs ", " min ", " max ", " addr ", " indirect ",
	" bitor ", " bitand ", " bitxor ", " bitnot ", " >> ",
	};
*/

/* Each of these values is defined in   pccdefs   */

int ops2 [ ] =
{
	P2BAD, P2PLUS, P2MINUS, P2STAR, P2SLASH, P2BAD, P2NEG,
	P2OROR, P2ANDAND, P2EQ, P2NE, P2NOT,
	P2BAD,
	P2LT, P2EQ, P2GT, P2LE, P2NE, P2GE,
	P2CALL, P2CALL, P2ASSIGN, P2PLUSEQ, P2STAREQ, P2CONV, P2LSHIFT, P2MOD,
	P2COMOP, P2QUEST, P2COLON,
	1, P2BAD, P2BAD, P2BAD, P2BAD,
	P2BITOR, P2BITAND, P2BITXOR, P2BITNOT, P2RSHIFT,
	P2BAD, P2BAD, P2BAD, P2BAD, P2BAD, P2BAD, P2BAD, P2BAD, P2BAD,
	P2BAD, P2BAD, P2BAD, P2BAD,
	1,1,1,1,1, /* OPNEG1, OPDMIN, OPDMAX, OPASSIGNI, OPIDENTITY */
	1,1,1,1	/* OPCHARCAST, OPDABS, OPMIN2, OPMAX2 */
};


setlog()
{
	typesize[TYLOGICAL] = typesize[tylogical];
	typealign[TYLOGICAL] = typealign[tylogical];
}


putexpr(p)
expptr p;
{
/* Write the expression to the p1 file */

	p = (expptr) putx (fixtype (p));
	p1_expr (p);
}





expptr putassign(lp, rp)
expptr lp, rp;
{
	return putx(fixexpr((Exprp)mkexpr(OPASSIGN, lp, rp)));
}




void puteq(lp, rp)
expptr lp, rp;
{
	putexpr(mkexpr(OPASSIGN, lp, rp) );
}




/* put code for  a *= b */

expptr putsteq(a, b)
Addrp a, b;
{
	return putx( fixexpr((Exprp)
		mkexpr(OPSTAREQ, cpexpr((expptr)a), cpexpr((expptr)b))));
}




Addrp mkfield(res, f, ty)
register Addrp res;
char *f;
int ty;
{
    res -> vtype = ty;
    res -> Field = f;
    return res;
} /* mkfield */


Addrp realpart(p)
register Addrp p;
{
	register Addrp q;
	expptr mkrealcon();

	if (p->tag == TADDR
	 && p->uname_tag == UNAM_CONST
	 && ISCOMPLEX (p->vtype))
		return (Addrp)mkrealcon (p -> vtype + TYREAL - TYCOMPLEX,
			p->user.kludge.vstg1 ? p->user.Const.cds[0]
				: cds(dtos(p->user.Const.cd[0]),CNULL));

	q = (Addrp) cpexpr((expptr) p);
	if( ISCOMPLEX(p->vtype) )
		q = mkfield (q, "r", p -> vtype + TYREAL - TYCOMPLEX);

	return(q);
}




expptr imagpart(p)
register Addrp p;
{
	register Addrp q;
	expptr mkrealcon();

	if( ISCOMPLEX(p->vtype) )
	{
		if (p->tag == TADDR && p->uname_tag == UNAM_CONST)
			return mkrealcon (p -> vtype + TYREAL - TYCOMPLEX,
				p->user.kludge.vstg1 ? p->user.Const.cds[1]
				: cds(dtos(p->user.Const.cd[1]),CNULL));
		q = (Addrp) cpexpr((expptr) p);
		q = mkfield (q, "i", p -> vtype + TYREAL - TYCOMPLEX);
		return( (expptr) q );
	}
	else

/* Cast an integer type onto a Double Real type */

		return( mkrealcon( ISINT(p->vtype) ? TYDREAL : p->vtype , "0"));
}





/* ncat -- computes the number of adjacent concatenation operations */

ncat(p)
register expptr p;
{
	if(p->tag==TEXPR && p->exprblock.opcode==OPCONCAT)
		return( ncat(p->exprblock.leftp) + ncat(p->exprblock.rightp) );
	else	return(1);
}




/* lencat -- returns the length of the concatenated string.  Each
   substring must have a static (i.e. compile-time) fixed length */

ftnint lencat(p)
register expptr p;
{
	if(p->tag==TEXPR && p->exprblock.opcode==OPCONCAT)
		return( lencat(p->exprblock.leftp) + lencat(p->exprblock.rightp) );
	else if( p->headblock.vleng!=NULL && ISICON(p->headblock.vleng) )
		return(p->headblock.vleng->constblock.Const.ci);
	else if(p->tag==TADDR && p->addrblock.varleng!=0)
		return(p->addrblock.varleng);
	else
	{
		err("impossible element in concatenation");
		return(0);
	}
}

/* putconst -- Creates a new Addrp value which maps onto the input
   constant value.  The Addrp doesn't retain the value of the constant,
   instead that value is copied into a table of constants (called
   litpool,   for pool of literal values).  The only way to retrieve the
   actual value of the constant is to look at the   memno   field of the
   Addrp result.  You know that the associated literal is the one referred
   to by   q   when   (q -> memno == litp -> litnum).
*/

Addrp putconst(p)
register Constp p;
{
	register Addrp q;
	struct Literal *litp, *lastlit;
	int k, len, type;
	int litflavor;
	double cd[2];
	ftnint nblanks;
	char *strp;
	char cdsbuf0[64], cdsbuf1[64], *ds[2];

	if (p->tag != TCONST)
		badtag("putconst", p->tag);

	q = ALLOC(Addrblock);
	q->tag = TADDR;
	type = p->vtype;
	q->vtype = ( type==TYADDR ? tyint : type );
	q->vleng = (expptr) cpexpr(p->vleng);
	q->vstg = STGCONST;

/* Create the new label for the constant.  This is wasteful of labels
   because when the constant value already exists in the literal pool,
   this label gets thrown away and is never reclaimed.  It might be
   cleaner to move this down past the first   switch()   statement below */

	q->memno = newlabel();
	q->memoffset = ICON(0);
	q -> uname_tag = UNAM_CONST;

/* Copy the constant info into the Addrblock; do this by copying the
   largest storage elts */

	q -> user.Const = p -> Const;
	q->user.kludge.vstg1 = p->vstg;	/* distinguish string from binary fp */

	/* check for value in literal pool, and update pool if necessary */

	k = 1;
	switch(type)
	{
	case TYCHAR:
		if (halign) {
			strp = p->Const.ccp;
			nblanks = p->Const.ccp1.blanks;
			len = p->vleng->constblock.Const.ci;
			litflavor = LIT_CHAR;
			goto loop;
			}
		else
			q->memno = BAD_MEMNO;
		break;
	case TYCOMPLEX:
	case TYDCOMPLEX:
		k = 2;
		if (p->vstg)
			cd[1] = atof(ds[1] = p->Const.cds[1]);
		else
			ds[1] = cds(dtos(cd[1] = p->Const.cd[1]), cdsbuf1);
	case TYREAL:
	case TYDREAL:
		litflavor = LIT_FLOAT;
		if (p->vstg)
			cd[0] = atof(ds[0] = p->Const.cds[0]);
		else
			ds[0] = cds(dtos(cd[0] = p->Const.cd[0]), cdsbuf0);
		goto loop;

	case TYLOGICAL1:
	case TYLOGICAL2:
	case TYLOGICAL:
		type = tylogical;
		goto lit_int_flavor;
	case TYLONG:
		type = tyint;
	case TYSHORT:
	case TYINT1:
#ifdef TYQUAD
	case TYQUAD:
#endif
 lit_int_flavor:
		litflavor = LIT_INT;

/* Scan the literal pool for this constant value.  If this same constant
   has been assigned before, use the same label.  Note that this routine
   does NOT consider two differently-typed constants with the same bit
   pattern to be the same constant */

 loop:
		lastlit = litpool + nliterals;
		for(litp = litpool ; litp<lastlit ; ++litp)

/* Remove this type checking to ensure that all bit patterns are reused */

			if(type == litp->littype) switch(litflavor)
			{
			case LIT_CHAR:
				if (len == (int)litp->litval.litival2[0]
				&& nblanks == litp->litval.litival2[1]
				&& !memcmp(strp, litp->cds[0], len)) {
					q->memno = litp->litnum;
					frexpr((expptr)p);
					q->user.Const.ccp1.ccp0 = litp->cds[0];
					return(q);
					}
				break;
			case LIT_FLOAT:
				if(cd[0] == litp->litval.litdval[0]
				&& !strcmp(ds[0], litp->cds[0])
				&& (k == 1 ||
				    cd[1] == litp->litval.litdval[1]
				    && !strcmp(ds[1], litp->cds[1]))) {
ret:
					q->memno = litp->litnum;
					frexpr((expptr)p);
					return(q);
					}
				break;

			case LIT_INT:
				if(p->Const.ci == litp->litval.litival)
					goto ret;
				break;
			}

/* If there's room in the literal pool, add this new value to the pool */

		if(nliterals < maxliterals)
		{
			++nliterals;

			/* litp   now points to the next free elt */

			litp->littype = type;
			litp->litnum = q->memno;
			switch(litflavor)
			{
			case LIT_CHAR:
				litp->litval.litival2[0] = len;
				litp->litval.litival2[1] = nblanks;
				q->user.Const.ccp = litp->cds[0] =
					memcpy(gmem(len,0), strp, len);
				break;

			case LIT_FLOAT:
				litp->litval.litdval[0] = cd[0];
				litp->cds[0] = copys(ds[0]);
				if (k == 2) {
					litp->litval.litdval[1] = cd[1];
					litp->cds[1] = copys(ds[1]);
					}
				break;

			case LIT_INT:
				litp->litval.litival = p->Const.ci;
				break;
			} /* switch (litflavor) */
		}
		else
			many("literal constants", 'L', maxliterals);

		break;
	case TYADDR:
	    break;
	default:
		badtype ("putconst", p -> vtype);
		break;
	} /* switch */

	if (type != TYCHAR || halign)
	    frexpr((expptr)p);
	return( q );
}
