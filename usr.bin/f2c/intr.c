/****************************************************************
Copyright 1990, 1992 by AT&T Bell Laboratories and Bellcore.

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
#include "names.h"

void cast_args ();

union
	{
	int ijunk;
	struct Intrpacked bits;
	} packed;

struct Intrbits
	{
	char intrgroup /* :3 */;
	char intrstuff /* result type or number of generics */;
	char intrno /* :7 */;
	char dblcmplx;
	char dblintrno;	/* for -r8 */
	};

/* List of all intrinsic functions.  */

LOCAL struct Intrblock
	{
	char intrfname[8];
	struct Intrbits intrval;
	} intrtab[ ] =
{
"int", 		{ INTRCONV, TYLONG },
"real", 	{ INTRCONV, TYREAL, 1 },
		/* 1 ==> real(TYDCOMPLEX) yields TYDREAL */
"dble", 	{ INTRCONV, TYDREAL },
"cmplx", 	{ INTRCONV, TYCOMPLEX },
"dcmplx", 	{ INTRCONV, TYDCOMPLEX, 0, 1 },
"ifix", 	{ INTRCONV, TYLONG },
"idint", 	{ INTRCONV, TYLONG },
"float", 	{ INTRCONV, TYREAL },
"dfloat",	{ INTRCONV, TYDREAL },
"sngl", 	{ INTRCONV, TYREAL },
"ichar", 	{ INTRCONV, TYLONG },
"iachar", 	{ INTRCONV, TYLONG },
"char", 	{ INTRCONV, TYCHAR },
"achar", 	{ INTRCONV, TYCHAR },

/* any MAX or MIN can be used with any types; the compiler will cast them
   correctly.  So rules against bad syntax in these expressions are not
   enforced */

"max", 		{ INTRMAX, TYUNKNOWN },
"max0", 	{ INTRMAX, TYLONG },
"amax0", 	{ INTRMAX, TYREAL },
"max1", 	{ INTRMAX, TYLONG },
"amax1", 	{ INTRMAX, TYREAL },
"dmax1", 	{ INTRMAX, TYDREAL },

"and",		{ INTRBOOL, TYUNKNOWN, OPBITAND },
"or",		{ INTRBOOL, TYUNKNOWN, OPBITOR },
"xor",		{ INTRBOOL, TYUNKNOWN, OPBITXOR },
"not",		{ INTRBOOL, TYUNKNOWN, OPBITNOT },
"lshift",	{ INTRBOOL, TYUNKNOWN, OPLSHIFT },
"rshift",	{ INTRBOOL, TYUNKNOWN, OPRSHIFT },

"min", 		{ INTRMIN, TYUNKNOWN },
"min0", 	{ INTRMIN, TYLONG },
"amin0", 	{ INTRMIN, TYREAL },
"min1", 	{ INTRMIN, TYLONG },
"amin1", 	{ INTRMIN, TYREAL },
"dmin1", 	{ INTRMIN, TYDREAL },

"aint", 	{ INTRGEN, 2, 0 },
"dint", 	{ INTRSPEC, TYDREAL, 1 },

"anint", 	{ INTRGEN, 2, 2 },
"dnint", 	{ INTRSPEC, TYDREAL, 3 },

"nint", 	{ INTRGEN, 4, 4 },
"idnint", 	{ INTRGEN, 2, 6 },

"abs", 		{ INTRGEN, 6, 8 },
"iabs", 	{ INTRGEN, 2, 9 },
"dabs", 	{ INTRSPEC, TYDREAL, 11 },
"cabs", 	{ INTRSPEC, TYREAL, 12, 0, 13 },
"zabs", 	{ INTRSPEC, TYDREAL, 13, 1 },

"mod", 		{ INTRGEN, 4, 14 },
"amod", 	{ INTRSPEC, TYREAL, 16, 0, 17 },
"dmod", 	{ INTRSPEC, TYDREAL, 17 },

"sign", 	{ INTRGEN, 4, 18 },
"isign", 	{ INTRGEN, 2, 19 },
"dsign", 	{ INTRSPEC, TYDREAL, 21 },

"dim", 		{ INTRGEN, 4, 22 },
"idim", 	{ INTRGEN, 2, 23 },
"ddim", 	{ INTRSPEC, TYDREAL, 25 },

"dprod", 	{ INTRSPEC, TYDREAL, 26 },

"len", 		{ INTRSPEC, TYLONG, 27 },
"index", 	{ INTRSPEC, TYLONG, 29 },

"imag", 	{ INTRGEN, 2, 31 },
"aimag", 	{ INTRSPEC, TYREAL, 31, 0, 32 },
"dimag", 	{ INTRSPEC, TYDREAL, 32 },

"conjg", 	{ INTRGEN, 2, 33 },
"dconjg", 	{ INTRSPEC, TYDCOMPLEX, 34, 1 },

"sqrt", 	{ INTRGEN, 4, 35 },
"dsqrt", 	{ INTRSPEC, TYDREAL, 36 },
"csqrt", 	{ INTRSPEC, TYCOMPLEX, 37, 0, 38 },
"zsqrt", 	{ INTRSPEC, TYDCOMPLEX, 38, 1 },

"exp", 		{ INTRGEN, 4, 39 },
"dexp", 	{ INTRSPEC, TYDREAL, 40 },
"cexp", 	{ INTRSPEC, TYCOMPLEX, 41, 0, 42 },
"zexp", 	{ INTRSPEC, TYDCOMPLEX, 42, 1 },

"log", 		{ INTRGEN, 4, 43 },
"alog", 	{ INTRSPEC, TYREAL, 43, 0, 44 },
"dlog", 	{ INTRSPEC, TYDREAL, 44 },
"clog", 	{ INTRSPEC, TYCOMPLEX, 45, 0, 46 },
"zlog", 	{ INTRSPEC, TYDCOMPLEX, 46, 1 },

"log10", 	{ INTRGEN, 2, 47 },
"alog10", 	{ INTRSPEC, TYREAL, 47, 0, 48 },
"dlog10", 	{ INTRSPEC, TYDREAL, 48 },

"sin", 		{ INTRGEN, 4, 49 },
"dsin", 	{ INTRSPEC, TYDREAL, 50 },
"csin", 	{ INTRSPEC, TYCOMPLEX, 51, 0, 52 },
"zsin", 	{ INTRSPEC, TYDCOMPLEX, 52, 1 },

"cos", 		{ INTRGEN, 4, 53 },
"dcos", 	{ INTRSPEC, TYDREAL, 54 },
"ccos", 	{ INTRSPEC, TYCOMPLEX, 55, 0, 56 },
"zcos", 	{ INTRSPEC, TYDCOMPLEX, 56, 1 },

"tan", 		{ INTRGEN, 2, 57 },
"dtan", 	{ INTRSPEC, TYDREAL, 58 },

"asin", 	{ INTRGEN, 2, 59 },
"dasin", 	{ INTRSPEC, TYDREAL, 60 },

"acos", 	{ INTRGEN, 2, 61 },
"dacos", 	{ INTRSPEC, TYDREAL, 62 },

"atan", 	{ INTRGEN, 2, 63 },
"datan", 	{ INTRSPEC, TYDREAL, 64 },

"atan2", 	{ INTRGEN, 2, 65 },
"datan2", 	{ INTRSPEC, TYDREAL, 66 },

"sinh", 	{ INTRGEN, 2, 67 },
"dsinh", 	{ INTRSPEC, TYDREAL, 68 },

"cosh", 	{ INTRGEN, 2, 69 },
"dcosh", 	{ INTRSPEC, TYDREAL, 70 },

"tanh", 	{ INTRGEN, 2, 71 },
"dtanh", 	{ INTRSPEC, TYDREAL, 72 },

"lge",		{ INTRSPEC, TYLOGICAL, 73},
"lgt",		{ INTRSPEC, TYLOGICAL, 75},
"lle",		{ INTRSPEC, TYLOGICAL, 77},
"llt",		{ INTRSPEC, TYLOGICAL, 79},

#if 0
"epbase",	{ INTRCNST, 4, 0 },
"epprec",	{ INTRCNST, 4, 4 },
"epemin",	{ INTRCNST, 2, 8 },
"epemax",	{ INTRCNST, 2, 10 },
"eptiny",	{ INTRCNST, 2, 12 },
"ephuge",	{ INTRCNST, 4, 14 },
"epmrsp",	{ INTRCNST, 2, 18 },
#endif

"fpexpn",	{ INTRGEN, 4, 81 },
"fpabsp",	{ INTRGEN, 2, 85 },
"fprrsp",	{ INTRGEN, 2, 87 },
"fpfrac",	{ INTRGEN, 2, 89 },
"fpmake",	{ INTRGEN, 2, 91 },
"fpscal",	{ INTRGEN, 2, 93 },

"" };


LOCAL struct Specblock
	{
	char atype;		/* Argument type; every arg must have
				   this type */
	char rtype;		/* Result type */
	char nargs;		/* Number of arguments */
	char spxname[8];	/* Name of the function in Fortran */
	char othername;		/* index into callbyvalue table */
	} spectab[ ] =
{
	{ TYREAL,TYREAL,1,"r_int" },
	{ TYDREAL,TYDREAL,1,"d_int" },

	{ TYREAL,TYREAL,1,"r_nint" },
	{ TYDREAL,TYDREAL,1,"d_nint" },

	{ TYREAL,TYSHORT,1,"h_nint" },
	{ TYREAL,TYLONG,1,"i_nint" },

	{ TYDREAL,TYSHORT,1,"h_dnnt" },
	{ TYDREAL,TYLONG,1,"i_dnnt" },

	{ TYREAL,TYREAL,1,"r_abs" },
	{ TYSHORT,TYSHORT,1,"h_abs" },
	{ TYLONG,TYLONG,1,"i_abs" },
	{ TYDREAL,TYDREAL,1,"d_abs" },
	{ TYCOMPLEX,TYREAL,1,"c_abs" },
	{ TYDCOMPLEX,TYDREAL,1,"z_abs" },

	{ TYSHORT,TYSHORT,2,"h_mod" },
	{ TYLONG,TYLONG,2,"i_mod" },
	{ TYREAL,TYREAL,2,"r_mod" },
	{ TYDREAL,TYDREAL,2,"d_mod" },

	{ TYREAL,TYREAL,2,"r_sign" },
	{ TYSHORT,TYSHORT,2,"h_sign" },
	{ TYLONG,TYLONG,2,"i_sign" },
	{ TYDREAL,TYDREAL,2,"d_sign" },

	{ TYREAL,TYREAL,2,"r_dim" },
	{ TYSHORT,TYSHORT,2,"h_dim" },
	{ TYLONG,TYLONG,2,"i_dim" },
	{ TYDREAL,TYDREAL,2,"d_dim" },

	{ TYREAL,TYDREAL,2,"d_prod" },

	{ TYCHAR,TYSHORT,1,"h_len" },
	{ TYCHAR,TYLONG,1,"i_len" },

	{ TYCHAR,TYSHORT,2,"h_indx" },
	{ TYCHAR,TYLONG,2,"i_indx" },

	{ TYCOMPLEX,TYREAL,1,"r_imag" },
	{ TYDCOMPLEX,TYDREAL,1,"d_imag" },
	{ TYCOMPLEX,TYCOMPLEX,1,"r_cnjg" },
	{ TYDCOMPLEX,TYDCOMPLEX,1,"d_cnjg" },

	{ TYREAL,TYREAL,1,"r_sqrt", 1 },
	{ TYDREAL,TYDREAL,1,"d_sqrt", 1 },
	{ TYCOMPLEX,TYCOMPLEX,1,"c_sqrt" },
	{ TYDCOMPLEX,TYDCOMPLEX,1,"z_sqrt" },

	{ TYREAL,TYREAL,1,"r_exp", 2 },
	{ TYDREAL,TYDREAL,1,"d_exp", 2 },
	{ TYCOMPLEX,TYCOMPLEX,1,"c_exp" },
	{ TYDCOMPLEX,TYDCOMPLEX,1,"z_exp" },

	{ TYREAL,TYREAL,1,"r_log", 3 },
	{ TYDREAL,TYDREAL,1,"d_log", 3 },
	{ TYCOMPLEX,TYCOMPLEX,1,"c_log" },
	{ TYDCOMPLEX,TYDCOMPLEX,1,"z_log" },

	{ TYREAL,TYREAL,1,"r_lg10" },
	{ TYDREAL,TYDREAL,1,"d_lg10" },

	{ TYREAL,TYREAL,1,"r_sin", 4 },
	{ TYDREAL,TYDREAL,1,"d_sin", 4 },
	{ TYCOMPLEX,TYCOMPLEX,1,"c_sin" },
	{ TYDCOMPLEX,TYDCOMPLEX,1,"z_sin" },

	{ TYREAL,TYREAL,1,"r_cos", 5 },
	{ TYDREAL,TYDREAL,1,"d_cos", 5 },
	{ TYCOMPLEX,TYCOMPLEX,1,"c_cos" },
	{ TYDCOMPLEX,TYDCOMPLEX,1,"z_cos" },

	{ TYREAL,TYREAL,1,"r_tan", 6 },
	{ TYDREAL,TYDREAL,1,"d_tan", 6 },

	{ TYREAL,TYREAL,1,"r_asin", 7 },
	{ TYDREAL,TYDREAL,1,"d_asin", 7 },

	{ TYREAL,TYREAL,1,"r_acos", 8 },
	{ TYDREAL,TYDREAL,1,"d_acos", 8 },

	{ TYREAL,TYREAL,1,"r_atan", 9 },
	{ TYDREAL,TYDREAL,1,"d_atan", 9 },

	{ TYREAL,TYREAL,2,"r_atn2", 10 },
	{ TYDREAL,TYDREAL,2,"d_atn2", 10 },

	{ TYREAL,TYREAL,1,"r_sinh", 11 },
	{ TYDREAL,TYDREAL,1,"d_sinh", 11 },

	{ TYREAL,TYREAL,1,"r_cosh", 12 },
	{ TYDREAL,TYDREAL,1,"d_cosh", 12 },

	{ TYREAL,TYREAL,1,"r_tanh", 13 },
	{ TYDREAL,TYDREAL,1,"d_tanh", 13 },

	{ TYCHAR,TYLOGICAL,2,"hl_ge" },
	{ TYCHAR,TYLOGICAL,2,"l_ge" },

	{ TYCHAR,TYLOGICAL,2,"hl_gt" },
	{ TYCHAR,TYLOGICAL,2,"l_gt" },

	{ TYCHAR,TYLOGICAL,2,"hl_le" },
	{ TYCHAR,TYLOGICAL,2,"l_le" },

	{ TYCHAR,TYLOGICAL,2,"hl_lt" },
	{ TYCHAR,TYLOGICAL,2,"l_lt" },

	{ TYREAL,TYSHORT,1,"hr_expn" },
	{ TYREAL,TYLONG,1,"ir_expn" },
	{ TYDREAL,TYSHORT,1,"hd_expn" },
	{ TYDREAL,TYLONG,1,"id_expn" },

	{ TYREAL,TYREAL,1,"r_absp" },
	{ TYDREAL,TYDREAL,1,"d_absp" },

	{ TYREAL,TYDREAL,1,"r_rrsp" },
	{ TYDREAL,TYDREAL,1,"d_rrsp" },

	{ TYREAL,TYREAL,1,"r_frac" },
	{ TYDREAL,TYDREAL,1,"d_frac" },

	{ TYREAL,TYREAL,2,"r_make" },
	{ TYDREAL,TYDREAL,2,"d_make" },

	{ TYREAL,TYREAL,2,"r_scal" },
	{ TYDREAL,TYDREAL,2,"d_scal" },
	{ 0 }
} ;

#if 0
LOCAL struct Incstblock
	{
	char atype;
	char rtype;
	char constno;
	} consttab[ ] =
{
	{ TYSHORT, TYLONG, 0 },
	{ TYLONG, TYLONG, 1 },
	{ TYREAL, TYLONG, 2 },
	{ TYDREAL, TYLONG, 3 },

	{ TYSHORT, TYLONG, 4 },
	{ TYLONG, TYLONG, 5 },
	{ TYREAL, TYLONG, 6 },
	{ TYDREAL, TYLONG, 7 },

	{ TYREAL, TYLONG, 8 },
	{ TYDREAL, TYLONG, 9 },

	{ TYREAL, TYLONG, 10 },
	{ TYDREAL, TYLONG, 11 },

	{ TYREAL, TYREAL, 0 },
	{ TYDREAL, TYDREAL, 1 },

	{ TYSHORT, TYLONG, 12 },
	{ TYLONG, TYLONG, 13 },
	{ TYREAL, TYREAL, 2 },
	{ TYDREAL, TYDREAL, 3 },

	{ TYREAL, TYREAL, 4 },
	{ TYDREAL, TYDREAL, 5 }
};
#endif

char *callbyvalue[ ] =
	{0,
	"sqrt",
	"exp",
	"log",
	"sin",
	"cos",
	"tan",
	"asin",
	"acos",
	"atan",
	"atan2",
	"sinh",
	"cosh",
	"tanh"
	};

 void
r8fix()	/* adjust tables for -r8 */
{
	register struct Intrblock *I;
	register struct Specblock *S;

	for(I = intrtab; I->intrfname[0]; I++)
		if (I->intrval.intrgroup != INTRGEN)
		    switch(I->intrval.intrstuff) {
			case TYREAL:
				I->intrval.intrstuff = TYDREAL;
				I->intrval.intrno = I->intrval.dblintrno;
				break;
			case TYCOMPLEX:
				I->intrval.intrstuff = TYDCOMPLEX;
				I->intrval.intrno = I->intrval.dblintrno;
				I->intrval.dblcmplx = 1;
			}

	for(S = spectab; S->atype; S++)
	    switch(S->atype) {
		case TYCOMPLEX:
			S->atype = TYDCOMPLEX;
			if (S->rtype == TYREAL)
				S->rtype = TYDREAL;
			else if (S->rtype == TYCOMPLEX)
				S->rtype = TYDCOMPLEX;
			switch(S->spxname[0]) {
				case 'r':
					S->spxname[0] = 'd';
					break;
				case 'c':
					S->spxname[0] = 'z';
					break;
				default:
					Fatal("r8fix bug");
				}
			break;
		case TYREAL:
			S->atype = TYDREAL;
			switch(S->rtype) {
			    case TYREAL:
				S->rtype = TYDREAL;
				if (S->spxname[0] != 'r')
					Fatal("r8fix bug");
				S->spxname[0] = 'd';
			    case TYDREAL:	/* d_prod */
				break;

			    case TYSHORT:
				if (!strcmp(S->spxname, "hr_expn"))
					S->spxname[1] = 'd';
				else if (!strcmp(S->spxname, "h_nint"))
					strcpy(S->spxname, "h_dnnt");
				else Fatal("r8fix bug");
				break;

			    case TYLONG:
				if (!strcmp(S->spxname, "ir_expn"))
					S->spxname[1] = 'd';
				else if (!strcmp(S->spxname, "i_nint"))
					strcpy(S->spxname, "i_dnnt");
				else Fatal("r8fix bug");
				break;

			    default:
				Fatal("r8fix bug");
			    }
		}
	}

expptr intrcall(np, argsp, nargs)
Namep np;
struct Listblock *argsp;
int nargs;
{
	int i, rettype;
	Addrp ap;
	register struct Specblock *sp;
	register struct Chain *cp;
	expptr Inline(), mkcxcon(), mkrealcon();
	expptr q, ep;
	int mtype;
	int op;
	int f1field, f2field, f3field;

	packed.ijunk = np->vardesc.varno;
	f1field = packed.bits.f1;
	f2field = packed.bits.f2;
	f3field = packed.bits.f3;
	if(nargs == 0)
		goto badnargs;

	mtype = 0;
	for(cp = argsp->listp ; cp ; cp = cp->nextp)
	{
		ep = (expptr)cp->datap;
		if( ISCONST(ep) && ep->headblock.vtype==TYSHORT )
			cp->datap = (char *) mkconv(tyint, ep);
		mtype = maxtype(mtype, ep->headblock.vtype);
	}

	switch(f1field)
	{
	case INTRBOOL:
		op = f3field;
		if( ! ONEOF(mtype, MSKINT|MSKLOGICAL) )
			goto badtype;
		if(op == OPBITNOT)
		{
			if(nargs != 1)
				goto badnargs;
			q = mkexpr(OPBITNOT, (expptr)argsp->listp->datap, ENULL);
		}
		else
		{
			if(nargs != 2)
				goto badnargs;
			q = mkexpr(op, (expptr)argsp->listp->datap,
			    		(expptr)argsp->listp->nextp->datap);
		}
		frchain( &(argsp->listp) );
		free( (charptr) argsp);
		return(q);

	case INTRCONV:
		rettype = f2field;
		switch(rettype) {
		  case TYLONG:
			rettype = tyint;
			break;
		  case TYLOGICAL:
			rettype = tylog;
		  }
		if( ISCOMPLEX(rettype) && nargs==2)
		{
			expptr qr, qi;
			qr = (expptr) argsp->listp->datap;
			qi = (expptr) argsp->listp->nextp->datap;
			if(ISCONST(qr) && ISCONST(qi))
				q = mkcxcon(qr,qi);
			else	q = mkexpr(OPCONV,mkconv(rettype-2,qr),
			    mkconv(rettype-2,qi));
		}
		else if(nargs == 1) {
			if (f3field && ((Exprp)argsp->listp->datap)->vtype
					== TYDCOMPLEX)
				rettype = TYDREAL;
			q = mkconv(rettype+100, (expptr)argsp->listp->datap);
			if (q->tag == TADDR)
				q->addrblock.parenused = 1;
			}
		else goto badnargs;

		q->headblock.vtype = rettype;
		frchain(&(argsp->listp));
		free( (charptr) argsp);
		return(q);


#if 0
	case INTRCNST:

/* Machine-dependent f77 stuff that f2c omits:

intcon contains
	radix for short int
	radix for long int
	radix for single precision
	radix for double precision
	precision for short int
	precision for long int
	precision for single precision
	precision for double precision
	emin for single precision
	emin for double precision
	emax for single precision
	emax for double prcision
	largest short int
	largest long int

realcon contains
	tiny for single precision
	tiny for double precision
	huge for single precision
	huge for double precision
	mrsp (epsilon) for single precision
	mrsp (epsilon) for double precision
*/
	{	register struct Incstblock *cstp;
		extern ftnint intcon[14];
		extern double realcon[6];

		cstp = consttab + f3field;
		for(i=0 ; i<f2field ; ++i)
			if(cstp->atype == mtype)
				goto foundconst;
			else
				++cstp;
		goto badtype;

foundconst:
		switch(cstp->rtype)
		{
		case TYLONG:
			return(mkintcon(intcon[cstp->constno]));

		case TYREAL:
		case TYDREAL:
			return(mkrealcon(cstp->rtype,
			    realcon[cstp->constno]) );

		default:
			Fatal("impossible intrinsic constant");
		}
	}
#endif

	case INTRGEN:
		sp = spectab + f3field;
		if(no66flag)
			if(sp->atype == mtype)
				goto specfunct;
			else err66("generic function");

		for(i=0; i<f2field ; ++i)
			if(sp->atype == mtype)
				goto specfunct;
			else
				++sp;
		warn1 ("bad argument type to intrinsic %s", np->fvarname);

/* Made this a warning rather than an error so things like "log (5) ==>
   log (5.0)" can be accommodated.  When none of these cases matches, the
   argument is cast up to the first type in the spectab list; this first
   type is assumed to be the "smallest" type, e.g. REAL before DREAL
   before COMPLEX, before DCOMPLEX */

		sp = spectab + f3field;
		mtype = sp -> atype;
		goto specfunct;

	case INTRSPEC:
		sp = spectab + f3field;
specfunct:
		if(tyint==TYLONG && ONEOF(sp->rtype,M(TYSHORT)|M(TYLOGICAL))
		    && (sp+1)->atype==sp->atype)
			++sp;

		if(nargs != sp->nargs)
			goto badnargs;
		if(mtype != sp->atype)
			goto badtype;

/* NOTE!!  I moved fixargs (YES) into the ELSE branch so that constants in
   the inline expression wouldn't get put into the constant table */

		fixargs (NO, argsp);
		cast_args (mtype, argsp -> listp);

		if(q = Inline((int)(sp-spectab), mtype, argsp->listp))
		{
			frchain( &(argsp->listp) );
			free( (charptr) argsp);
		} else {

		    if(sp->othername) {
			/* C library routines that return double... */
			/* sp->rtype might be TYREAL */
			ap = builtin(sp->rtype,
				callbyvalue[sp->othername], 1);
			q = fixexpr((Exprp)
				mkexpr(OPCCALL, (expptr)ap, (expptr)argsp) );
		    } else {
			fixargs(YES, argsp);
			ap = builtin(sp->rtype, sp->spxname, 0);
			q = fixexpr((Exprp)
				mkexpr(OPCALL, (expptr)ap, (expptr)argsp) );
		    } /* else */
		} /* else */
		return(q);

	case INTRMIN:
	case INTRMAX:
		if(nargs < 2)
			goto badnargs;
		if( ! ONEOF(mtype, MSKINT|MSKREAL) )
			goto badtype;
		argsp->vtype = mtype;
		q = mkexpr( (f1field==INTRMIN ? OPMIN : OPMAX), (expptr)argsp, ENULL);

		q->headblock.vtype = mtype;
		rettype = f2field;
		if(rettype == TYLONG)
			rettype = tyint;
		else if(rettype == TYUNKNOWN)
			rettype = mtype;
		return( mkconv(rettype, q) );

	default:
		fatali("intrcall: bad intrgroup %d", f1field);
	}
badnargs:
	errstr("bad number of arguments to intrinsic %s", np->fvarname);
	goto bad;

badtype:
	errstr("bad argument type to intrinsic %s", np->fvarname);

bad:
	return( errnode() );
}




intrfunct(s)
char *s;
{
	register struct Intrblock *p;

	for(p = intrtab; p->intrval.intrgroup!=INTREND ; ++p)
	{
		if( !strcmp(s, p->intrfname) )
		{
			packed.bits.f1 = p->intrval.intrgroup;
			packed.bits.f2 = p->intrval.intrstuff;
			packed.bits.f3 = p->intrval.intrno;
			packed.bits.f4 = p->intrval.dblcmplx;
			return(packed.ijunk);
		}
	}

	return(0);
}





Addrp intraddr(np)
Namep np;
{
	Addrp q;
	register struct Specblock *sp;
	int f3field;

	if(np->vclass!=CLPROC || np->vprocclass!=PINTRINSIC)
		fatalstr("intraddr: %s is not intrinsic", np->fvarname);
	packed.ijunk = np->vardesc.varno;
	f3field = packed.bits.f3;

	switch(packed.bits.f1)
	{
	case INTRGEN:
		/* imag, log, and log10 arent specific functions */
		if(f3field==31 || f3field==43 || f3field==47)
			goto bad;

	case INTRSPEC:
		sp = spectab + f3field;
		if (tyint == TYLONG
		&& (sp->rtype == TYSHORT || sp->rtype == TYLOGICAL))
			++sp;
		q = builtin(sp->rtype, sp->spxname,
			sp->othername ? 1 : 0);
		return(q);

	case INTRCONV:
	case INTRMIN:
	case INTRMAX:
	case INTRBOOL:
	case INTRCNST:
bad:
		errstr("cannot pass %s as actual", np->fvarname);
		return((Addrp)errnode());
	}
	fatali("intraddr: impossible f1=%d\n", (int) packed.bits.f1);
	/* NOT REACHED */ return 0;
}



void cast_args (maxtype, args)
int maxtype;
chainp args;
{
    for (; args; args = args -> nextp) {
	expptr e = (expptr) args->datap;
	if (e -> headblock.vtype != maxtype)
	    if (e -> tag == TCONST)
		args->datap = (char *) mkconv(maxtype, e);
	    else {
		Addrp temp = mktmp(maxtype, ENULL);

		puteq(cpexpr((expptr)temp), e);
		args->datap = (char *)temp;
	    } /* else */
    } /* for */
} /* cast_args */



expptr Inline(fno, type, args)
int fno;
int type;
struct Chain *args;
{
	register expptr q, t, t1;

	switch(fno)
	{
	case 8:	/* real abs */
	case 9:	/* short int abs */
	case 10:	/* long int abs */
	case 11:	/* double precision abs */
		if( addressable(q = (expptr) args->datap) )
		{
			t = q;
			q = NULL;
		}
		else
			t = (expptr) mktmp(type,ENULL);
		t1 = mkexpr(type == TYREAL && forcedouble ? OPDABS : OPABS,
			cpexpr(t), ENULL);
		if(q)
			t1 = mkexpr(OPCOMMA, mkexpr(OPASSIGN, cpexpr(t),q), t1);
		frexpr(t);
		return(t1);

	case 26:	/* dprod */
		q = mkexpr(OPSTAR, mkconv(TYDREAL,(expptr)args->datap),
			(expptr)args->nextp->datap);
		return(q);

	case 27:	/* len of character string */
		q = (expptr) cpexpr(((tagptr)args->datap)->headblock.vleng);
		frexpr((expptr)args->datap);
		return(q);

	case 14:	/* half-integer mod */
	case 15:	/* mod */
		return mkexpr(OPMOD, (expptr) args->datap,
		    		(expptr) args->nextp->datap);
	}
	return(NULL);
}
