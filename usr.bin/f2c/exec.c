/****************************************************************
Copyright 1990, 1993 - 1996 by AT&T, Lucent Technologies and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T, Bell Laboratories,
Lucent or Bellcore or any of their entities not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

AT&T, Lucent and Bellcore disclaim all warranties with regard to
this software, including all implied warranties of
merchantability and fitness.  In no event shall AT&T, Lucent or
Bellcore be liable for any special, indirect or consequential
damages or any damages whatsoever resulting from loss of use,
data or profits, whether in an action of contract, negligence or
other tortious action, arising out of or in connection with the
use or performance of this software.
****************************************************************/

#include "defs.h"
#include "p1defs.h"
#include "names.h"

static void exar2 Argdcl((int, tagptr, struct Labelblock*, struct Labelblock*));
static void popctl Argdcl((void));
static void pushctl Argdcl((int));

/*   Logical IF codes
*/

 void
#ifdef KR_headers
exif(p)
	expptr p;
#else
exif(expptr p)
#endif
{
    pushctl(CTLIF);
    putif(p, 0);	/* 0 => if, not elseif */
}


 void
#ifdef KR_headers
exelif(p)
	expptr p;
#else
exelif(expptr p)
#endif
{
    if (ctlstack->ctltype == CTLIF || ctlstack->ctltype == CTLIFX)
	putif(p, 1);	/* 1 ==> elseif */
    else
	execerr("elseif out of place", CNULL);
}




 void
exelse(Void)
{
	register struct Ctlframe *c;

	for(c = ctlstack; c->ctltype == CTLIFX; --c);
	if(c->ctltype == CTLIF) {
		p1_else ();
		c->ctltype = CTLELSE;
		}
	else
		execerr("else out of place", CNULL);
	}

 void
#ifdef KR_headers
exendif()
#else
exendif()
#endif
{
	while(ctlstack->ctltype == CTLIFX) {
		popctl();
		p1else_end();
		}
	if(ctlstack->ctltype == CTLIF) {
		popctl();
		p1_endif ();
		}
	else if(ctlstack->ctltype == CTLELSE) {
		popctl();
		p1else_end ();
		}
	else
		execerr("endif out of place", CNULL);
	}


 void
#ifdef KR_headers
new_endif()
#else
new_endif()
#endif
{
	if (ctlstack->ctltype == CTLIF || ctlstack->ctltype == CTLIFX)
		pushctl(CTLIFX);
	else
		err("new_endif bug");
	}

/* pushctl -- Start a new control construct, initialize the labels (to
   zero) */

 LOCAL void
#ifdef KR_headers
pushctl(code)
	int code;
#else
pushctl(int code)
#endif
{
	register int i;

	if(++ctlstack >= lastctl)
		many("loops or if-then-elses", 'c', maxctl);
	ctlstack->ctltype = code;
	for(i = 0 ; i < 4 ; ++i)
		ctlstack->ctlabels[i] = 0;
	ctlstack->dowhile = 0;
	ctlstack->domax = ctlstack->dostep = 0;	/* in case of errors */
	++blklevel;
}


 LOCAL void
popctl(Void)
{
	if( ctlstack-- < ctls )
		Fatal("control stack empty");
	--blklevel;
}



/* poplab -- update the flags in   labeltab   */

 LOCAL void
poplab(Void)
{
	register struct Labelblock  *lp;

	for(lp = labeltab ; lp < highlabtab ; ++lp)
		if(lp->labdefined)
		{
			/* mark all labels in inner blocks unreachable */
			if(lp->blklevel > blklevel)
				lp->labinacc = YES;
		}
		else if(lp->blklevel > blklevel)
		{
			/* move all labels referred to in inner blocks out a level */
			lp->blklevel = blklevel;
		}
}


/*  BRANCHING CODE
*/
 void
#ifdef KR_headers
exgoto(lab)
	struct Labelblock *lab;
#else
exgoto(struct Labelblock *lab)
#endif
{
	lab->labused = 1;
	p1_goto (lab -> stateno);
}






 void
#ifdef KR_headers
exequals(lp, rp)
	register struct Primblock *lp;
	register expptr rp;
#else
exequals(register struct Primblock *lp, register expptr rp)
#endif
{
	if(lp->tag != TPRIM)
	{
		err("assignment to a non-variable");
		frexpr((expptr)lp);
		frexpr(rp);
	}
	else if(lp->namep->vclass!=CLVAR && lp->argsp)
	{
		if(parstate >= INEXEC)
			errstr("statement function %.62s amid executables.",
				lp->namep->fvarname);
		mkstfunct(lp, rp);
	}
	else if (lp->vtype == TYSUBR)
		err("illegal use of subroutine name");
	else
	{
		expptr new_lp, new_rp;

		if(parstate < INDATA)
			enddcl();
		new_lp = mklhs (lp, keepsubs);
		new_rp = fixtype (rp);
		puteq(new_lp, new_rp);
	}
}



/* Make Statement Function */

long laststfcn = -1, thisstno;
int doing_stmtfcn;

 void
#ifdef KR_headers
mkstfunct(lp, rp)
	struct Primblock *lp;
	expptr rp;
#else
mkstfunct(struct Primblock *lp, expptr rp)
#endif
{
	register struct Primblock *p;
	register Namep np;
	chainp args;

	laststfcn = thisstno;
	np = lp->namep;
	if(np->vclass == CLUNKNOWN)
		np->vclass = CLPROC;
	else
	{
		dclerr("redeclaration of statement function", np);
		return;
	}
	np->vprocclass = PSTFUNCT;
	np->vstg = STGSTFUNCT;

/* Set the type of the function */

	impldcl(np);
	if (np->vtype == TYCHAR && !np->vleng)
		err("character statement function with length (*)");
	args = (lp->argsp ? lp->argsp->listp : CHNULL);
	np->varxptr.vstfdesc = mkchain((char *)args, (chainp)rp);

	for(doing_stmtfcn = 1 ; args ; args = args->nextp)

/* It is an error for the formal parameters to have arguments or
   subscripts */

		if( ((tagptr)(args->datap))->tag!=TPRIM ||
		    (p = (struct Primblock *)(args->datap) )->argsp ||
		    p->fcharp || p->lcharp ) {
			err("non-variable argument in statement function definition");
			args->datap = 0;
			}
		else
		{

/* Replace the name on the left-hand side */

			args->datap = (char *)p->namep;
			vardcl(p -> namep);
			free((char *)p);
		}
	doing_stmtfcn = 0;
}

 static void
#ifdef KR_headers
mixed_type(np)
	Namep np;
#else
mixed_type(Namep np)
#endif
{
	char buf[128];
	sprintf(buf, "%s function %.90s invoked as subroutine",
		ftn_types[np->vtype], np->fvarname);
	warn(buf);
	}

 void
#ifdef KR_headers
excall(name, args, nstars, labels)
	Namep name;
	struct Listblock *args;
	int nstars;
	struct Labelblock **labels;
#else
excall(Namep name, struct Listblock *args, int nstars, struct Labelblock **labels)
#endif
{
	register expptr p;

	if (name->vtype != TYSUBR) {
		if (name->vinfproc && !name->vcalled) {
			name->vtype = TYSUBR;
			frexpr(name->vleng);
			name->vleng = 0;
			}
		else if (!name->vimpltype && name->vtype != TYUNKNOWN)
			mixed_type(name);
		else
			settype(name, TYSUBR, (ftnint)0);
		}
	p = mkfunct( mkprim(name, args, CHNULL) );
	if (p->tag == TERROR)
		return;

/* Subroutines and their identifiers acquire the type INT */

	p->exprblock.vtype = p->exprblock.leftp->headblock.vtype = TYINT;

/* Handle the alternate return mechanism */

	if(nstars > 0)
		putcmgo(putx(fixtype(p)), nstars, labels);
	else
		putexpr(p);
}


 void
#ifdef KR_headers
exstop(stop, p)
	int stop;
	register expptr p;
#else
exstop(int stop, register expptr p)
#endif
{
	char *str;
	int n;

	if(p)
	{
		if( ! ISCONST(p) )
		{
			execerr("pause/stop argument must be constant", CNULL);
			frexpr(p);
			p = mkstrcon(0, CNULL);
		}
		else if( ISINT(p->constblock.vtype) )
		{
			str = convic(p->constblock.Const.ci);
			n = strlen(str);
			if(n > 0)
			{
				p->constblock.Const.ccp = copyn(n, str);
				p->constblock.Const.ccp1.blanks = 0;
				p->constblock.vtype = TYCHAR;
				p->constblock.vleng = (expptr) ICON(n);
			}
			else
				p = (expptr) mkstrcon(0, CNULL);
		}
		else if(p->constblock.vtype != TYCHAR)
		{
			execerr("pause/stop argument must be integer or string", CNULL);
			p = (expptr) mkstrcon(0, CNULL);
		}
	}
	else	p = (expptr) mkstrcon(0, CNULL);

    {
	expptr subr_call;

	subr_call = call1(TYSUBR, (stop ? "s_stop" : "s_paus"), p);
	putexpr( subr_call );
    }
}

/* DO LOOP CODE */

#define DOINIT	par[0]
#define DOLIMIT	par[1]
#define DOINCR	par[2]


/* Macros for   ctlstack -> dostepsign   */

#define VARSTEP	0
#define POSSTEP	1
#define NEGSTEP	2


/* exdo -- generate DO loop code.  In the case of a variable increment,
   positive increment tests are placed above the body, negative increment
   tests are placed below (see   enddo()   ) */

 void
#ifdef KR_headers
exdo(range, loopname, spec)
	int range;
	Namep loopname;
	chainp spec;
#else
exdo(int range, Namep loopname, chainp spec)
#endif
			/* range = end label */
			/* input spec must have at least 2 exprs */
{
	register expptr p;
	register Namep np;
	chainp cp;		/* loops over the fields in   spec */
	register int i;
	int dotype;		/* type of the index variable */
	int incsign;		/* sign of the increment, if it's constant
				   */
	Addrp dovarp;		/* loop index variable */
	expptr doinit;		/* constant or register for init param */
	expptr par[3];		/* local specification parameters */

	expptr init, test, inc;	/* Expressions in the resulting FOR loop */


	test = ENULL;

	pushctl(CTLDO);
	dorange = ctlstack->dolabel = range;
	ctlstack->loopname = loopname;

/* Declare the loop index */

	np = (Namep)spec->datap;
	ctlstack->donamep = NULL;
	if (!np) { /* do while */
		ctlstack->dowhile = 1;
#if 0
		if (loopname) {
			if (loopname->vtype == TYUNKNOWN) {
				loopname->vdcldone = 1;
				loopname->vclass = CLLABEL;
				loopname->vprocclass = PLABEL;
				loopname->vtype = TYLABEL;
				}
			if (loopname->vtype == TYLABEL)
				if (loopname->vdovar)
					dclerr("already in use as a loop name",
						loopname);
				else
					loopname->vdovar = 1;
			else
				dclerr("already declared; cannot be a loop name",
					loopname);
			}
#endif
		putwhile((expptr)spec->nextp);
		NOEXT("do while");
		spec->nextp = 0;
		frchain(&spec);
		return;
		}
	if(np->vdovar)
	{
		errstr("nested loops with variable %s", np->fvarname);
		ctlstack->donamep = NULL;
		return;
	}

/* Create a memory-resident version of the index variable */

	dovarp = mkplace(np);
	if( ! ONEOF(dovarp->vtype, MSKINT|MSKREAL) )
	{
		err("bad type on do variable");
		return;
	}
	ctlstack->donamep = np;

	np->vdovar = YES;

/* Now   dovarp   points to the index to be used within the loop,   dostgp
   points to the one which may need to be stored */

	dotype = dovarp->vtype;

/* Count the input specifications and type-check each one independently;
   this just eliminates non-numeric values from the specification */

	for(i=0 , cp = spec->nextp ; cp!=NULL && i<3 ; cp = cp->nextp)
	{
		p = par[i++] = fixtype((tagptr)cp->datap);
		if( ! ONEOF(p->headblock.vtype, MSKINT|MSKREAL) )
		{
			err("bad type on DO parameter");
			return;
		}
	}

	frchain(&spec);
	switch(i)
	{
	case 0:
	case 1:
		err("too few DO parameters");
		return;

	default:
		err("too many DO parameters");
		return;

	case 2:
		DOINCR = (expptr) ICON(1);

	case 3:
		break;
	}


/* Now all of the local specification fields are set, but their types are
   not yet consistent */

/* Declare the loop initialization value, casting it properly and declaring a
   register if need be */

	ctlstack->doinit = 0;
	if (ISCONST (DOINIT) || !onetripflag)
/* putx added 6-29-89 (mwm), not sure if fixtype is required, but I doubt it
   since mkconv is called just before */
		doinit = putx (mkconv (dotype, DOINIT));
	else  {
	    if (onetripflag)
	   	ctlstack->doinit = doinit = (expptr) mktmp0(dotype, ENULL);
	    else
		doinit = (expptr) mktmp(dotype, ENULL);
	    puteq (cpexpr (doinit), DOINIT);
	} /* else */

/* Declare the loop ending value, casting it to the type of the index
   variable */

	if( ISCONST(DOLIMIT) )
		ctlstack->domax = mkconv(dotype, DOLIMIT);
	else {
		ctlstack->domax = (expptr) mktmp0(dotype, ENULL);
		puteq (cpexpr (ctlstack -> domax), DOLIMIT);
	} /* else */

/* Declare the loop increment value, casting it to the type of the index
   variable */

	if( ISCONST(DOINCR) )
	{
		ctlstack->dostep = mkconv(dotype, DOINCR);
		if( (incsign = conssgn(ctlstack->dostep)) == 0)
			err("zero DO increment");
		ctlstack->dostepsign = (incsign > 0 ? POSSTEP : NEGSTEP);
	}
	else
	{
		ctlstack->dostep = (expptr) mktmp0(dotype, ENULL);
		ctlstack->dostepsign = VARSTEP;
		puteq (cpexpr (ctlstack -> dostep), DOINCR);
	}

/* All data is now properly typed and in the   ctlstack,   except for the
   initial value.  Assignments of temps have been generated already */

	switch (ctlstack -> dostepsign) {
	    case VARSTEP:
		test = mkexpr (OPQUEST, mkexpr (OPLT,
			cpexpr (ctlstack -> dostep), ICON(0)),
			mkexpr (OPCOLON,
			    mkexpr (OPGE, cpexpr((expptr)dovarp),
				    cpexpr (ctlstack -> domax)),
			    mkexpr (OPLE, cpexpr((expptr)dovarp),
				    cpexpr (ctlstack -> domax))));
		break;
	    case POSSTEP:
	        test = mkexpr (OPLE, cpexpr((expptr)dovarp),
			cpexpr (ctlstack -> domax));
	        break;
	    case NEGSTEP:
	        test = mkexpr (OPGE, cpexpr((expptr)dovarp),
			cpexpr (ctlstack -> domax));
	        break;
	    default:
	        erri ("exdo:  bad dostepsign '%d'", ctlstack -> dostepsign);
	        break;
	} /* switch (ctlstack -> dostepsign) */

	if (onetripflag)
	    test = mkexpr (OPOR, test,
		    mkexpr (OPEQ, cpexpr((expptr)dovarp), cpexpr (doinit)));
	init = mkexpr (OPASSIGN, cpexpr((expptr)dovarp),
			ctlstack->doinit ? cpexpr(doinit) : doinit);
	inc = mkexpr (OPPLUSEQ, (expptr)dovarp, cpexpr (ctlstack -> dostep));

	if (!onetripflag && ISCONST (ctlstack -> domax) && ISCONST (doinit)
		&& ctlstack -> dostepsign != VARSTEP) {
	    expptr tester;

	    tester = mkexpr (OPMINUS, cpexpr (doinit),
		    cpexpr (ctlstack -> domax));
	    if (incsign == conssgn (tester))
		warn ("DO range never executed");
	    frexpr (tester);
	} /* if !onetripflag && */

	p1_for (init, test, inc);
}

 void
#ifdef KR_headers
exenddo(np)
	Namep np;
#else
exenddo(Namep np)
#endif
{
	Namep np1;
	int here;
	struct Ctlframe *cf;

	if( ctlstack < ctls )
		goto misplaced;
	here = ctlstack->dolabel;
	if (ctlstack->ctltype != CTLDO
	|| here >= 0 && (!thislabel || thislabel->labelno != here)) {
 misplaced:
		err("misplaced ENDDO");
		return;
		}
	if (np != ctlstack->loopname) {
		if (np1 = ctlstack->loopname)
			errstr("expected \"enddo %s\"", np1->fvarname);
		else
			err("expected unnamed ENDDO");
		for(cf = ctls; cf < ctlstack; cf++)
			if (cf->ctltype == CTLDO && cf->loopname == np) {
				here = cf->dolabel;
				break;
				}
		}
	enddo(here);
	}

 void
#ifdef KR_headers
enddo(here)
	int here;
#else
enddo(int here)
#endif
{
	register struct Ctlframe *q;
	Namep np;			/* name of the current DO index */
	Addrp ap;
	register int i;
	register expptr e;

/* Many DO's can end at the same statement, so keep looping over all
   nested indicies */

	while(here == dorange)
	{
		if(np = ctlstack->donamep)
			{
			p1for_end ();

/* Now we're done with all of the tests, and the loop has terminated.
   Store the index value back in long-term memory */

			if(ap = memversion(np))
				puteq((expptr)ap, (expptr)mkplace(np));
			for(i = 0 ; i < 4 ; ++i)
				ctlstack->ctlabels[i] = 0;
			deregister(ctlstack->donamep);
			ctlstack->donamep->vdovar = NO;
			/* ctlstack->dostep and ctlstack->domax can be zero */
			/* with sufficiently bizarre (erroneous) syntax */
			if (e = ctlstack->dostep)
				if (e->tag == TADDR && e->addrblock.istemp)
					frtemp((Addrp)e);
				else
					frexpr(e);
			if (e = ctlstack->domax)
				if (e->tag == TADDR && e->addrblock.istemp)
					frtemp((Addrp)e);
				else
					frexpr(e);
			if (e = ctlstack->doinit)
				frtemp((Addrp)e);
			}
		else if (ctlstack->dowhile)
			p1for_end ();

/* Set   dorange   to the closing label of the next most enclosing DO loop
   */

		popctl();
		poplab();
		dorange = 0;
		for(q = ctlstack ; q>=ctls ; --q)
			if(q->ctltype == CTLDO)
			{
				dorange = q->dolabel;
				break;
			}
	}
}

 void
#ifdef KR_headers
exassign(vname, labelval)
	register Namep vname;
	struct Labelblock *labelval;
#else
exassign(register Namep vname, struct Labelblock *labelval)
#endif
{
	Addrp p;
	register Addrp q;
	char *fs;
	register chainp cp, cpprev;
	register ftnint k, stno;

	p = mkplace(vname);
	if( ! ONEOF(p->vtype, MSKINT|MSKADDR) ) {
		err("noninteger assign variable");
		return;
		}

	/* If the label hasn't been defined, then we do things twice:
	 * once for an executable stmt label, once for a format
	 */

	/* code for executable label... */

/* Now store the assigned value in a list associated with this variable.
   This will be used later to generate a switch() statement in the C output */

	fs = labelval->fmtstring;
	if (!labelval->labdefined || !fs) {

		if (vname -> vis_assigned == 0) {
			vname -> varxptr.assigned_values = CHNULL;
			vname -> vis_assigned = 1;
			}

		/* don't duplicate labels... */

		stno = labelval->stateno;
		cpprev = 0;
		for(k = 0, cp = vname->varxptr.assigned_values;
				cp; cpprev = cp, cp = cp->nextp, k++)
			if ((ftnint)cp->datap == stno)
				break;
		if (!cp) {
			cp = mkchain((char *)stno, CHNULL);
			if (cpprev)
				cpprev->nextp = cp;
			else
				vname->varxptr.assigned_values = cp;
			labelval->labused = 1;
			}
		putout(mkexpr(OPASSIGN, (expptr)p, mkintcon(k)));
		}

	/* Code for FORMAT label... */

	if (!labelval->labdefined || fs) {

		labelval->fmtlabused = 1;
		p = ALLOC(Addrblock);
		p->tag = TADDR;
		p->vtype = TYCHAR;
		p->vstg = STGAUTO;
		p->memoffset = ICON(0);
		fmtname(vname, p);
		q = ALLOC(Addrblock);
		q->tag = TADDR;
		q->vtype = TYCHAR;
		q->vstg = STGAUTO;
		q->ntempelt = 1;
		q->memoffset = ICON(0);
		q->uname_tag = UNAM_IDENT;
		sprintf(q->user.ident, "fmt_%ld", labelval->stateno);
		putout(mkexpr(OPASSIGN, (expptr)p, (expptr)q));
		}

} /* exassign */


 void
#ifdef KR_headers
exarif(expr, neglab, zerlab, poslab)
	expptr expr;
	struct Labelblock *neglab;
	struct Labelblock *zerlab;
	struct Labelblock *poslab;
#else
exarif(expptr expr, struct Labelblock *neglab, struct Labelblock *zerlab, struct Labelblock *poslab)
#endif
{
    register int lm, lz, lp;

    lm = neglab->stateno;
    lz = zerlab->stateno;
    lp = poslab->stateno;
    expr = fixtype(expr);

    if( ! ONEOF(expr->headblock.vtype, MSKINT|MSKREAL) )
    {
        err("invalid type of arithmetic if expression");
        frexpr(expr);
    }
    else
    {
        if (lm == lz && lz == lp)
            exgoto (neglab);
        else if(lm == lz)
            exar2(OPLE, expr, neglab, poslab);
        else if(lm == lp)
            exar2(OPNE, expr, neglab, zerlab);
        else if(lz == lp)
            exar2(OPGE, expr, zerlab, neglab);
        else {
            expptr t;

	    if (!addressable (expr)) {
		t = (expptr) mktmp(expr -> headblock.vtype, ENULL);
		expr = mkexpr (OPASSIGN, cpexpr (t), expr);
	    } else
		t = (expptr) cpexpr (expr);

	    p1_if(putx(fixtype(mkexpr (OPLT, expr, ICON (0)))));
	    exgoto(neglab);
	    p1_elif (mkexpr (OPEQ, t, ICON (0)));
	    exgoto(zerlab);
	    p1_else ();
	    exgoto(poslab);
	    p1else_end ();
        } /* else */
    }
}



/* exar2 -- Do arithmetic IF for only 2 distinct labels;   if !(e.op.0)
   goto l2 else goto l1.  If this seems backwards, that's because it is,
   in order to make the 1 pass algorithm work. */

 LOCAL void
#ifdef KR_headers
exar2(op, e, l1, l2)
	int op;
	expptr e;
	struct Labelblock *l1;
	struct Labelblock *l2;
#else
exar2(int op, expptr e, struct Labelblock *l1, struct Labelblock *l2)
#endif
{
	expptr comp;

	comp = mkexpr (op, e, ICON (0));
	p1_if(putx(fixtype(comp)));
	exgoto(l1);
	p1_else ();
	exgoto(l2);
	p1else_end ();
}


/* exreturn -- return the value in   p  from a SUBROUTINE call -- used to
   implement the alternate return mechanism */

 void
#ifdef KR_headers
exreturn(p)
	register expptr p;
#else
exreturn(register expptr p)
#endif
{
	if(procclass != CLPROC)
		warn("RETURN statement in main or block data");
	if(p && (proctype!=TYSUBR || procclass!=CLPROC) )
	{
		err("alternate return in nonsubroutine");
		p = 0;
	}

	if (p || proctype == TYSUBR) {
		if (p == ENULL) p = ICON (0);
		p = mkconv (TYLONG, fixtype (p));
		p1_subr_ret (p);
	} /* if p || proctype == TYSUBR */
	else
	    p1_subr_ret((expptr)retslot);
}


 void
#ifdef KR_headers
exasgoto(labvar)
	Namep labvar;
#else
exasgoto(Namep labvar)
#endif
{
	register Addrp p;

	p = mkplace(labvar);
	if( ! ISINT(p->vtype) )
		err("assigned goto variable must be integer");
	else {
		p1_asgoto (p);
	} /* else */
}
