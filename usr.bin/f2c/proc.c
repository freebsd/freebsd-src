/****************************************************************
Copyright 1990, 1994-6 by AT&T, Lucent Technologies and Bellcore.

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
#include "names.h"
#include "output.h"
#include "p1defs.h"

/* round a up to the nearest multiple of b:

   a = b * floor ( (a + (b - 1)) / b )*/

#undef roundup
#define roundup(a,b)    ( b * ( (a+b-1)/b) )

#define EXNULL (union Expression *)0

static void dobss Argdcl((void));
static void docomleng Argdcl((void));
static void docommon Argdcl((void));
static void doentry Argdcl((struct Entrypoint*));
static void epicode Argdcl((void));
static int nextarg Argdcl((int));
static void retval Argdcl((int));

static char Blank[] = BLANKCOMMON;

 static char *postfix[] = { "g", "h", "i",
#ifdef TYQUAD
					"j",
#endif
					"r", "d", "c", "z", "g", "h", "i" };

 chainp new_procs;
 int prev_proc, proc_argchanges, proc_protochanges;

 void
#ifdef KR_headers
changedtype(q)
	Namep q;
#else
changedtype(Namep q)
#endif
{
	char buf[200];
	int qtype, type1;
	register Extsym *e;
	Argtypes *at;

	if (q->vtypewarned)
		return;
	q->vtypewarned = 1;
	qtype = q->vtype;
	e = &extsymtab[q->vardesc.varno];
	if (!(at = e->arginfo)) {
		if (!e->exused)
			return;
		}
	else if (at->changes & 2 && qtype != TYUNKNOWN && !at->defined)
		proc_protochanges++;
	type1 = e->extype;
	if (type1 == TYUNKNOWN)
		return;
	if (qtype == TYUNKNOWN)
		/* e.g.,
			subroutine foo
			end
			external foo
			call goo(foo)
			end
		*/
		return;
	sprintf(buf, "%.90s: inconsistent declarations:\n\
	here %s%s, previously %s%s.", q->fvarname, ftn_types[qtype],
		qtype == TYSUBR ? "" : " function",
		ftn_types[type1], type1 == TYSUBR ? "" : " function");
	warn(buf);
	}

 void
#ifdef KR_headers
unamstring(q, s)
	register Addrp q;
	register char *s;
#else
unamstring(register Addrp q, register char *s)
#endif
{
	register int k;
	register char *t;

	k = strlen(s);
	if (k < IDENT_LEN) {
		q->uname_tag = UNAM_IDENT;
		t = q->user.ident;
		}
	else {
		q->uname_tag = UNAM_CHARP;
		q->user.Charp = t = mem(k+1, 0);
		}
	strcpy(t, s);
	}

 static void
fix_entry_returns(Void)	/* for multiple entry points */
{
	Addrp a;
	int i;
	struct Entrypoint *e;
	Namep np;

	e = entries = (struct Entrypoint *)revchain((chainp)entries);
	allargs = revchain(allargs);
	if (!multitype)
		return;

	/* TYLOGICAL should have been turned into TYLONG or TYSHORT by now */

	for(i = TYINT1; i <= TYLOGICAL; i++)
		if (a = xretslot[i])
			sprintf(a->user.ident, "(*ret_val).%s",
				postfix[i-TYINT1]);

	do {
		np = e->enamep;
		switch(np->vtype) {
			case TYINT1:
			case TYSHORT:
			case TYLONG:
#ifdef TYQUAD
			case TYQUAD:
#endif
			case TYREAL:
			case TYDREAL:
			case TYCOMPLEX:
			case TYDCOMPLEX:
			case TYLOGICAL1:
			case TYLOGICAL2:
			case TYLOGICAL:
				np->vstg = STGARG;
			}
		}
		while(e = e->entnextp);
	}

 static void
#ifdef KR_headers
putentries(outfile)
	FILE *outfile;
#else
putentries(FILE *outfile)
#endif
	/* put out wrappers for multiple entries */
{
	char base[MAXNAMELEN+4];
	struct Entrypoint *e;
	Namep *A, *Ae, *Ae1, **Alp, *a, **a1, np;
	chainp args, lengths;
	int i, k, mt, nL, t, type;
	extern char *dfltarg[], **dfltproc;

	e = entries;
	if (!e->enamep) /* only possible with erroneous input */
		return;
	nL = (nallargs + nallchargs) * sizeof(Namep *);
	A = (Namep *)ckalloc(nL + nallargs*sizeof(Namep **));
	Ae = A + nallargs;
	Alp = (Namep **)(Ae1 = Ae + nallchargs);
	i = k = 0;
	for(a1 = Alp, args = allargs; args; a1++, args = args->nextp) {
		np = (Namep)args->datap;
		if (np->vtype == TYCHAR && np->vclass != CLPROC)
			*a1 = &Ae[i++];
		}

	mt = multitype;
	multitype = 0;
	sprintf(base, "%s0_", e->enamep->cvarname);
	do {
		np = e->enamep;
		lengths = length_comp(e, 0);
		proctype = type = np->vtype;
		if (protofile)
			protowrite(protofile, type, np->cvarname, e, lengths);
		nice_printf(outfile, "\n%s ", c_type_decl(type, 1));
		nice_printf(outfile, "%s", np->cvarname);
		if (!Ansi) {
			listargs(outfile, e, 0, lengths);
			nice_printf(outfile, "\n");
			}
	    	list_arg_types(outfile, e, lengths, 0, "\n");
		nice_printf(outfile, "{\n");
		frchain(&lengths);
		next_tab(outfile);
		if (mt)
			nice_printf(outfile,
				"Multitype ret_val;\n%s(%d, &ret_val",
				base, k); /*)*/
		else if (ISCOMPLEX(type))
			nice_printf(outfile, "%s(%d,%s", base, k,
				xretslot[type]->user.ident); /*)*/
		else if (type == TYCHAR)
			nice_printf(outfile,
				"%s(%d, ret_val, ret_val_len", base, k); /*)*/
		else
			nice_printf(outfile, "return %s(%d", base, k); /*)*/
		k++;
		memset((char *)A, 0, nL);
		for(args = e->arglist; args; args = args->nextp) {
			np = (Namep)args->datap;
			A[np->argno] = np;
			if (np->vtype == TYCHAR && np->vclass != CLPROC)
				*Alp[np->argno] = np;
			}
		args = allargs;
		for(a = A; a < Ae; a++, args = args->nextp) {
			t = ((Namep)args->datap)->vtype;
			nice_printf(outfile, ", %s", (np = *a)
				? np->cvarname
				: ((Namep)args->datap)->vclass == CLPROC
				? dfltproc[((Namep)args->datap)->vimpltype
					? (Castargs ? TYUNKNOWN : TYSUBR)
					: t == TYREAL && forcedouble && !Castargs
					? TYDREAL : t]
				: dfltarg[((Namep)args->datap)->vtype]);
			}
		for(; a < Ae1; a++)
			if (np = *a)
				nice_printf(outfile, ", %s",
					new_arg_length(np));
			else
				nice_printf(outfile, ", (ftnint)0");
		nice_printf(outfile, /*(*/ ");\n");
		if (mt) {
			if (type == TYCOMPLEX)
				nice_printf(outfile,
		    "r_v->r = ret_val.c.r; r_v->i = ret_val.c.i;\n");
			else if (type == TYDCOMPLEX)
				nice_printf(outfile,
		    "r_v->r = ret_val.z.r; r_v->i = ret_val.z.i;\n");
			else if (type <= TYLOGICAL)
				nice_printf(outfile, "return ret_val.%s;\n",
					postfix[type-TYINT1]);
			}
		nice_printf(outfile, "}\n");
		prev_tab(outfile);
		}
		while(e = e->entnextp);
	free((char *)A);
	}

 static void
#ifdef KR_headers
entry_goto(outfile)
	FILE *outfile;
#else
entry_goto(FILE *outfile)
#endif
{
	struct Entrypoint *e = entries;
	int k = 0;

	nice_printf(outfile, "switch(n__) {\n");
	next_tab(outfile);
	while(e = e->entnextp)
		nice_printf(outfile, "case %d: goto %s;\n", ++k,
			user_label((long)(extsymtab - e->entryname - 1)));
	nice_printf(outfile, "}\n\n");
	prev_tab(outfile);
	}

/* start a new procedure */

 void
newproc(Void)
{
	if(parstate != OUTSIDE)
	{
		execerr("missing end statement", CNULL);
		endproc();
	}

	parstate = INSIDE;
	procclass = CLMAIN;	/* default */
}

 static void
zap_changes(Void)
{
	register chainp cp;
	register Argtypes *at;

	/* arrange to get correct count of prototypes that would
	   change by running f2c again */

	if (prev_proc && proc_argchanges)
		proc_protochanges++;
	prev_proc = proc_argchanges = 0;
	for(cp = new_procs; cp; cp = cp->nextp)
		if (at = ((Namep)cp->datap)->arginfo)
			at->changes &= ~1;
	frchain(&new_procs);
	}

/* end of procedure. generate variables, epilogs, and prologs */

 void
endproc(Void)
{
	struct Labelblock *lp;
	Extsym *ext;

	if(parstate < INDATA)
		enddcl();
	if(ctlstack >= ctls)
		err("DO loop or BLOCK IF not closed");
	for(lp = labeltab ; lp < labtabend ; ++lp)
		if(lp->stateno!=0 && lp->labdefined==NO)
			errstr("missing statement label %s",
				convic(lp->stateno) );

/* Save copies of the common variables in extptr -> allextp */

	for (ext = extsymtab; ext < nextext; ext++)
		if (ext -> extstg == STGCOMMON && ext -> extp) {
			extern int usedefsforcommon;

/* Write out the abbreviations for common block reference */

			copy_data (ext -> extp);
			if (usedefsforcommon) {
				wr_abbrevs (c_file, 1, ext -> extp);
				ext -> used_here = 1;
				}
			else
				ext -> extp = CHNULL;

			}

	if (nentry > 1)
		fix_entry_returns();
	epicode();
	donmlist();
	dobss();
	start_formatting ();
	if (nentry > 1)
		putentries(c_file);

	zap_changes();
	procinit();	/* clean up for next procedure */
}



/* End of declaration section of procedure.  Allocate storage. */

 void
enddcl(Void)
{
	register struct Entrypoint *ep;
	struct Entrypoint *ep0;
	chainp cp;
	extern char *err_proc;
	static char comblks[] = "common blocks";

	err_proc = comblks;
	docommon();

/* Now the hash table entries for fields of common blocks have STGCOMMON,
   vdcldone, voffset, and varno.  And the common blocks themselves have
   their full sizes in extleng. */

	err_proc = "equivalences";
	doequiv();

	err_proc = comblks;
	docomleng();

/* This implies that entry points in the declarations are buffered in
   entries   but not written out */

	err_proc = "entries";
	if (ep = ep0 = (struct Entrypoint *)revchain((chainp)entries)) {
		/* entries could be 0 in case of an error */
		do doentry(ep);
			while(ep = ep->entnextp);
		entries = (struct Entrypoint *)revchain((chainp)ep0);
		}

	err_proc = 0;
	parstate = INEXEC;
	p1put(P1_PROCODE);
	freetemps();
	if (earlylabs) {
		for(cp = earlylabs = revchain(earlylabs); cp; cp = cp->nextp)
			p1_label((long)cp->datap);
		frchain(&earlylabs);
		}
	p1_line_number(lineno); /* for files that start with a MAIN program */
				/* that starts with an executable statement */
}

/* ROUTINES CALLED WHEN ENCOUNTERING ENTRY POINTS */

/* Main program or Block data */

 void
#ifdef KR_headers
startproc(progname, class)
	Extsym *progname;
	int class;
#else
startproc(Extsym *progname, int class)
#endif
{
	register struct Entrypoint *p;

	p = ALLOC(Entrypoint);
	if(class == CLMAIN) {
		puthead(CNULL, CLMAIN);
		if (progname)
		    strcpy (main_alias, progname->cextname);
	} else {
		if (progname) {
			/* Construct an empty subroutine with this name */
			/* in case the name is needed to force loading */
			/* of this block-data subprogram: the name can */
			/* appear elsewhere in an external statement. */
			entrypt(CLPROC, TYSUBR, (ftnint)0, progname, (chainp)0);
			endproc();
			newproc();
			}
		puthead(CNULL, CLBLOCK);
		}
	if(class == CLMAIN)
		newentry( mkname(" MAIN"), 0 )->extinit = 1;
	p->entryname = progname;
	entries = p;

	procclass = class;
	fprintf(diagfile, "   %s", (class==CLMAIN ? "MAIN" : "BLOCK DATA") );
	if(progname) {
		fprintf(diagfile, " %s", progname->fextname);
		procname = progname->cextname;
		}
	fprintf(diagfile, ":\n");
	fflush(diagfile);
}

/* subroutine or function statement */

 Extsym *
#ifdef KR_headers
newentry(v, substmsg)
	register Namep v;
	int substmsg;
#else
newentry(register Namep v, int substmsg)
#endif
{
	register Extsym *p;
	char buf[128], badname[64];
	static int nbad = 0;
	static char already[] = "external name already used";

	p = mkext(v->fvarname, addunder(v->cvarname));

	if(p->extinit || ! ONEOF(p->extstg, M(STGUNKNOWN)|M(STGEXT)) )
	{
		sprintf(badname, "%s_bad%d", v->fvarname, ++nbad);
		if (substmsg) {
			sprintf(buf,"%s\n\tsubstituting \"%s\"",
				already, badname);
			dclerr(buf, v);
			}
		else
			dclerr(already, v);
		p = mkext(v->fvarname, badname);
	}
	v->vstg = STGAUTO;
	v->vprocclass = PTHISPROC;
	v->vclass = CLPROC;
	if (p->extstg == STGEXT)
		prev_proc = 1;
	else
		p->extstg = STGEXT;
	p->extinit = YES;
	v->vardesc.varno = p - extsymtab;
	return(p);
}

 void
#ifdef KR_headers
entrypt(class, type, length, entry, args)
	int class;
	int type;
	ftnint length;
	Extsym *entry;
	chainp args;
#else
entrypt(int class, int type, ftnint length, Extsym *entry, chainp args)
#endif
{
	register Namep q;
	register struct Entrypoint *p;

	if(class != CLENTRY)
		puthead( procname = entry->cextname, class);
	else
		fprintf(diagfile, "       entry ");
	fprintf(diagfile, "   %s:\n", entry->fextname);
	fflush(diagfile);
	q = mkname(entry->fextname);
	if (type == TYSUBR)
		q->vstg = STGEXT;

	type = lengtype(type, length);
	if(class == CLPROC)
	{
		procclass = CLPROC;
		proctype = type;
		procleng = type == TYCHAR ? length : 0;
	}

	p = ALLOC(Entrypoint);

	p->entnextp = entries;
	entries = p;

	p->entryname = entry;
	p->arglist = revchain(args);
	p->enamep = q;

	if(class == CLENTRY)
	{
		class = CLPROC;
		if(proctype == TYSUBR)
			type = TYSUBR;
	}

	q->vclass = class;
	q->vprocclass = 0;
	settype(q, type, length);
	q->vprocclass = PTHISPROC;
	/* hold all initial entry points till end of declarations */
	if(parstate >= INDATA)
		doentry(p);
}

/* generate epilogs */

/* epicode -- write out the proper function return mechanism at the end of
   the procedure declaration.  Handles multiple return value types, as
   well as cooercion into the proper value */

 LOCAL void
epicode(Void)
{
	extern int lastwasbranch;

	if(procclass==CLPROC)
	{
		if(proctype==TYSUBR)
		{

/* Return a zero only when the alternate return mechanism has been
   specified in the function header */

			if ((substars || Ansi) && lastwasbranch != YES)
			    p1_subr_ret (ICON(0));
		}
		else if (!multitype && lastwasbranch != YES)
			retval(proctype);
	}
	else if (procclass == CLMAIN && Ansi && lastwasbranch != YES)
		p1_subr_ret (ICON(0));
	lastwasbranch = NO;
}


/* generate code to return value of type  t */

 LOCAL void
#ifdef KR_headers
retval(t)
	register int t;
#else
retval(register int t)
#endif
{
	register Addrp p;

	switch(t)
	{
	case TYCHAR:
	case TYCOMPLEX:
	case TYDCOMPLEX:
		break;

	case TYLOGICAL:
		t = tylogical;
	case TYINT1:
	case TYADDR:
	case TYSHORT:
	case TYLONG:
#ifdef TYQUAD
	case TYQUAD:
#endif
	case TYREAL:
	case TYDREAL:
	case TYLOGICAL1:
	case TYLOGICAL2:
		p = (Addrp) cpexpr((expptr)retslot);
		p->vtype = t;
		p1_subr_ret (mkconv (t, fixtype((expptr)p)));
		break;

	default:
		badtype("retval", t);
	}
}


/* Do parameter adjustments */

 void
#ifdef KR_headers
procode(outfile)
	FILE *outfile;
#else
procode(FILE *outfile)
#endif
{
	prolog(outfile, allargs);

	if (nentry > 1)
		entry_goto(outfile);
	}

 static void
#ifdef KR_headers
bad_dimtype(q) Namep q;
#else
bad_dimtype(Namep q)
#endif
{
	errstr("bad dimension type for %.70s", q->fvarname);
	}

/* Finish bound computations now that all variables are declared.
 * This used to be in setbound(), but under -u the following incurred
 * an erroneous error message:
 *	subroutine foo(x,n)
 *	real x(n)
 *	integer n
 */

 static void
#ifdef KR_headers
dim_finish(v)
	Namep v;
#else
dim_finish(Namep v)
#endif
{
	register struct Dimblock *p;
	register expptr q;
	register int i, nd;

	p = v->vdim;
	v->vdimfinish = 0;
	nd = p->ndim;
	doin_setbound = 1;
	for(i = 0; i < nd; i++)
		if (q = p->dims[i].dimexpr) {
			q = p->dims[i].dimexpr = make_int_expr(putx(fixtype(q)));
			if (!ONEOF(q->headblock.vtype, MSKINT|MSKREAL))
				bad_dimtype(v);
			}
	if (q = p->basexpr)
		p->basexpr = make_int_expr(putx(fixtype(q)));
	doin_setbound = 0;
	}

 static void
#ifdef KR_headers
duparg(q)
	Namep q;
#else
duparg(Namep q)
#endif
{ errstr("duplicate argument %.80s", q->fvarname); }

/*
   manipulate argument lists (allocate argument slot positions)
 * keep track of return types and labels
 */

 LOCAL void
#ifdef KR_headers
doentry(ep)
	struct Entrypoint *ep;
#else
doentry(struct Entrypoint *ep)
#endif
{
	register int type;
	register Namep np;
	chainp p, p1;
	register Namep q;
	Addrp rs;
	int it, k;
	extern char dflttype[26];
	Extsym *entryname = ep->entryname;

	if (++nentry > 1)
		p1_label((long)(extsymtab - entryname - 1));

/* The main program isn't allowed to have parameters, so any given
   parameters are ignored */

	if(procclass == CLMAIN || procclass == CLBLOCK)
		return;

/* So now we're working with something other than CLMAIN or CLBLOCK.
   Determine the type of its return value. */

	impldcl( np = mkname(entryname->fextname) );
	type = np->vtype;
	proc_argchanges = prev_proc && type != entryname->extype;
	entryname->extseen = 1;
	if(proctype == TYUNKNOWN)
		if( (proctype = type) == TYCHAR)
			procleng = np->vleng ? np->vleng->constblock.Const.ci
					     : (ftnint) (-1);

	if(proctype == TYCHAR)
	{
		if(type != TYCHAR)
			err("noncharacter entry of character function");

/* Functions returning type   char   can only have multiple entries if all
   entries return the same length */

		else if( (np->vleng ? np->vleng->constblock.Const.ci :
		    (ftnint) (-1)) != procleng)
			err("mismatched character entry lengths");
	}
	else if(type == TYCHAR)
		err("character entry of noncharacter function");
	else if(type != proctype)
		multitype = YES;
	if(rtvlabel[type] == 0)
		rtvlabel[type] = (int)newlabel();
	ep->typelabel = rtvlabel[type];

	if(type == TYCHAR)
	{
		if(chslot < 0)
		{
			chslot = nextarg(TYADDR);
			chlgslot = nextarg(TYLENG);
		}
		np->vstg = STGARG;

/* Put a new argument in the function, one which will hold the result of
   a character function.  This will have to be named sometime, probably in
   mkarg(). */

		if(procleng < 0) {
			np->vleng = (expptr) mkarg(TYLENG, chlgslot);
			np->vleng->addrblock.uname_tag = UNAM_IDENT;
			strcpy (np -> vleng -> addrblock.user.ident,
				new_func_length());
			}
		if (!xretslot[TYCHAR]) {
			xretslot[TYCHAR] = rs =
				autovar(0, type, ISCONST(np->vleng)
					? np->vleng : ICON(0), "");
			strcpy(rs->user.ident, "ret_val");
			}
	}

/* Handle a   complex   return type -- declare a new parameter (pointer to
   a complex value) */

	else if( ISCOMPLEX(type) ) {
		if (!xretslot[type])
			xretslot[type] =
				autovar(0, type, EXNULL, " ret_val");
				/* the blank is for use in out_addr */
		np->vstg = STGARG;
		if(cxslot < 0)
			cxslot = nextarg(TYADDR);
		}
	else if (type != TYSUBR) {
		if (type == TYUNKNOWN) {
			dclerr("untyped function", np);
			proctype = type = np->vtype =
				dflttype[letter(np->fvarname[0])];
			}
		if (!xretslot[type])
			xretslot[type] = retslot =
				autovar(1, type, EXNULL, " ret_val");
				/* the blank is for use in out_addr */
		np->vstg = STGAUTO;
		}

	for(p = ep->arglist ; p ; p = p->nextp)
		if(! (( q = (Namep) (p->datap) )->vknownarg) ) {
			q->vknownarg = 1;
			q->vardesc.varno = nextarg(TYADDR);
			allargs = mkchain((char *)q, allargs);
			q->argno = nallargs++;
			}
		else if (nentry == 1)
			duparg(q);
		else for(p1 = ep->arglist ; p1 != p; p1 = p1->nextp)
			if ((Namep)p1->datap == q)
				duparg(q);

	k = 0;
	for(p = ep->arglist ; p ; p = p->nextp) {
		if(! (( q = (Namep) (p->datap) )->vdcldone) )
			{
			impldcl(q);
			q->vdcldone = YES;
			if(q->vtype == TYCHAR)
				{

/* If we don't know the length of a char*(*) (i.e. a string), we must add
   in this additional length argument. */

				++nallchargs;
				if (q->vclass == CLPROC)
					nallchargs--;
				else if (q->vleng == NULL) {
					/* character*(*) */
					q->vleng = (expptr)
					    mkarg(TYLENG, nextarg(TYLENG) );
					unamstring((Addrp)q->vleng,
						new_arg_length(q));
					}
				}
			}
		if (q->vdimfinish)
			dim_finish(q);
		if (q->vtype == TYCHAR && q->vclass != CLPROC)
			k++;
		}

	if (entryname->extype != type)
		changedtype(np);

	/* save information for checking consistency of arg lists */

	it = infertypes;
	if (entryname->exproto)
		infertypes = 1;
	save_argtypes(ep->arglist, &entryname->arginfo, &np->arginfo,
			0, np->fvarname, STGEXT, k, np->vtype, 2);
	infertypes = it;
}



 LOCAL int
#ifdef KR_headers
nextarg(type)
	int type;
#else
nextarg(int type)
#endif
{
	type = type;	/* shut up warning */
	return(lastargslot++);
	}

 LOCAL void
#ifdef KR_headers
dim_check(q)
	Namep q;
#else
dim_check(Namep q)
#endif
{
	register struct Dimblock *vdim = q->vdim;
	register expptr nelt;

	if(!(nelt = vdim->nelt) || !ISCONST(nelt))
		dclerr("adjustable dimension on non-argument", q);
	else if (!ONEOF(nelt->headblock.vtype, MSKINT|MSKREAL))
		bad_dimtype(q);
	else if (ISINT(nelt->headblock.vtype)
		    && nelt->constblock.Const.ci <= 0
		 || nelt->constblock.Const.cd[0] <= 0)
		dclerr("nonpositive dimension", q);
	}

 LOCAL void
dobss(Void)
{
	register struct Hashentry *p;
	register Namep q;
	int qstg, qclass, qtype;
	Extsym *e;

	for(p = hashtab ; p<lasthash ; ++p)
		if(q = p->varp)
		{
			qstg = q->vstg;
			qtype = q->vtype;
			qclass = q->vclass;

			if( (qclass==CLUNKNOWN && qstg!=STGARG) ||
			    (qclass==CLVAR && qstg==STGUNKNOWN) ) {
				if (!(q->vis_assigned | q->vimpldovar))
					warn1("local variable %s never used",
						q->fvarname);
				}
			else if(qclass==CLVAR && qstg==STGBSS)
			{ ; }

/* Give external procedures the proper storage class */

			else if(qclass==CLPROC && q->vprocclass==PEXTERNAL
					&& qstg!=STGARG) {
				e = mkext(q->fvarname,addunder(q->cvarname));
				e->extstg = STGEXT;
				q->vardesc.varno = e - extsymtab;
				if (e->extype != qtype)
					changedtype(q);
				}
			if(qclass==CLVAR) {
			    if (qstg != STGARG && q->vdim)
				dim_check(q);
			} /* if qclass == CLVAR */
		}

}


 void
donmlist(Void)
{
	register struct Hashentry *p;
	register Namep q;

	for(p=hashtab; p<lasthash; ++p)
		if( (q = p->varp) && q->vclass==CLNAMELIST)
			namelist(q);
}


/* iarrlen -- Returns the size of the array in bytes, or -1 */

 ftnint
#ifdef KR_headers
iarrlen(q)
	register Namep q;
#else
iarrlen(register Namep q)
#endif
{
	ftnint leng;

	leng = typesize[q->vtype];
	if(leng <= 0)
		return(-1);
	if(q->vdim)
		if( ISICON(q->vdim->nelt) )
			leng *= q->vdim->nelt->constblock.Const.ci;
		else	return(-1);
	if(q->vleng)
		if( ISICON(q->vleng) )
			leng *= q->vleng->constblock.Const.ci;
		else return(-1);
	return(leng);
}

 void
#ifdef KR_headers
namelist(np)
	Namep np;
#else
namelist(Namep np)
#endif
{
	register chainp q;
	register Namep v;
	int y;

	if (!np->visused)
		return;
	y = 0;

	for(q = np->varxptr.namelist ; q ; q = q->nextp)
	{
		vardcl( v = (Namep) (q->datap) );
		if( !ONEOF(v->vstg, MSKSTATIC) )
			dclerr("may not appear in namelist", v);
		else {
			v->vnamelist = 1;
			v->visused = 1;
			v->vsave = 1;
			y = 1;
			}
	np->visused = y;
	}
}

/* docommon -- called at the end of procedure declarations, before
   equivalences and the procedure body */

 LOCAL void
docommon(Void)
{
    register Extsym *extptr;
    register chainp q, q1;
    struct Dimblock *t;
    expptr neltp;
    register Namep comvar;
    ftnint size;
    int i, k, pref, type;
    extern int type_pref[];

    for(extptr = extsymtab ; extptr<nextext ; ++extptr)
	if (extptr->extstg == STGCOMMON && (q = extptr->extp)) {

/* If a common declaration also had a list of variables ... */

	    q = extptr->extp = revchain(q);
	    pref = 1;
	    for(k = TYCHAR; q ; q = q->nextp)
	    {
		comvar = (Namep) (q->datap);

		if(comvar->vdcldone == NO)
		    vardcl(comvar);
		type = comvar->vtype;
		if (pref < type_pref[type])
			pref = type_pref[k = type];
		if(extptr->extleng % typealign[type] != 0) {
		    dclerr("common alignment", comvar);
		    --nerr; /* don't give bad return code for this */
#if 0
		    extptr->extleng = roundup(extptr->extleng, typealign[type]);
#endif
		} /* if extptr -> extleng % */

/* Set the offset into the common block */

		comvar->voffset = extptr->extleng;
		comvar->vardesc.varno = extptr - extsymtab;
		if(type == TYCHAR)
			if (comvar->vleng)
				size = comvar->vleng->constblock.Const.ci;
			else  {
				dclerr("character*(*) in common", comvar);
				size = 1;
				}
		else
			size = typesize[type];
		if(t = comvar->vdim)
		    if( (neltp = t->nelt) && ISCONST(neltp) )
			size *= neltp->constblock.Const.ci;
		    else
			dclerr("adjustable array in common", comvar);

/* Adjust the length of the common block so far */

		extptr->extleng += size;
	    } /* for */

	    extptr->extype = k;

/* Determine curno and, if new, save this identifier chain */

	    q1 = extptr->extp;
	    for (q = extptr->allextp, i = 0; q; i++, q = q->nextp)
		if (struct_eq((chainp)q->datap, q1))
			break;
	    if (q)
		extptr->curno = extptr->maxno - i;
	    else {
		extptr->curno = ++extptr->maxno;
		extptr->allextp = mkchain((char *)extptr->extp,
						extptr->allextp);
		}
	} /* if extptr -> extstg == STGCOMMON */

/* Now the hash table entries have STGCOMMON, vdcldone, voffset, and
   varno.  And the common block itself has its full size in extleng. */

} /* docommon */


/* copy_data -- copy the Namep entries so they are available even after
   the hash table is empty */

 void
#ifdef KR_headers
copy_data(list)
	chainp list;
#else
copy_data(chainp list)
#endif
{
    for (; list; list = list -> nextp) {
	Namep namep = ALLOC (Nameblock);
	int size, nd, i;
	struct Dimblock *dp;

	cpn(sizeof(struct Nameblock), list->datap, (char *)namep);
	namep->fvarname = strcpy(gmem(strlen(namep->fvarname)+1,0),
		namep->fvarname);
	namep->cvarname = strcmp(namep->fvarname, namep->cvarname)
		? strcpy(gmem(strlen(namep->cvarname)+1,0), namep->cvarname)
		: namep->fvarname;
	if (namep -> vleng)
	    namep -> vleng = (expptr) cpexpr (namep -> vleng);
	if (namep -> vdim) {
	    nd = namep -> vdim -> ndim;
	    size = sizeof(int) + (3 + 2 * nd) * sizeof (expptr);
	    dp = (struct Dimblock *) ckalloc (size);
	    cpn(size, (char *)namep->vdim, (char *)dp);
	    namep -> vdim = dp;
	    dp->nelt = (expptr)cpexpr(dp->nelt);
	    for (i = 0; i < nd; i++) {
		dp -> dims[i].dimsize = (expptr) cpexpr (dp -> dims[i].dimsize);
	    } /* for */
	} /* if */
	list -> datap = (char *) namep;
    } /* for */
} /* copy_data */



 LOCAL void
docomleng(Void)
{
	register Extsym *p;

	for(p = extsymtab ; p < nextext ; ++p)
		if(p->extstg == STGCOMMON)
		{
			if(p->maxleng!=0 && p->extleng!=0 && p->maxleng!=p->extleng
			    && strcmp(Blank, p->cextname) )
				warn1("incompatible lengths for common block %.60s",
				    p->fextname);
			if(p->maxleng < p->extleng)
				p->maxleng = p->extleng;
			p->extleng = 0;
		}
}


/* ROUTINES DEALING WITH AUTOMATIC AND TEMPORARY STORAGE */

 void
#ifdef KR_headers
frtemp(p)
	Addrp p;
#else
frtemp(Addrp p)
#endif
{
	/* put block on chain of temps to be reclaimed */
	holdtemps = mkchain((char *)p, holdtemps);
}

 void
freetemps(Void)
{
	register chainp p, p1;
	register Addrp q;
	register int t;

	p1 = holdtemps;
	while(p = p1) {
		q = (Addrp)p->datap;
		t = q->vtype;
		if (t == TYCHAR && q->varleng != 0) {
			/* restore clobbered character string lengths */
			frexpr(q->vleng);
			q->vleng = ICON(q->varleng);
			}
		p1 = p->nextp;
		p->nextp = templist[t];
		templist[t] = p;
		}
	holdtemps = 0;
	}

/* allocate an automatic variable slot for each of   nelt   variables */

 Addrp
#ifdef KR_headers
autovar(nelt0, t, lengp, name)
	register int nelt0;
	register int t;
	expptr lengp;
	char *name;
#else
autovar(register int nelt0, register int t, expptr lengp, char *name)
#endif
{
	ftnint leng;
	register Addrp q;
	register int nelt = nelt0 > 0 ? nelt0 : 1;
	extern char *av_pfix[];

	if(t == TYCHAR)
		if( ISICON(lengp) )
			leng = lengp->constblock.Const.ci;
		else	{
			Fatal("automatic variable of nonconstant length");
		}
	else
		leng = typesize[t];

	q = ALLOC(Addrblock);
	q->tag = TADDR;
	q->vtype = t;
	if(t == TYCHAR)
	{
		q->vleng = ICON(leng);
		q->varleng = leng;
	}
	q->vstg = STGAUTO;
	q->ntempelt = nelt;
	q->isarray = (nelt > 1);
	q->memoffset = ICON(0);

	/* kludge for nls so we can have ret_val rather than ret_val_4 */
	if (*name == ' ')
		unamstring(q, name);
	else {
		q->uname_tag = UNAM_IDENT;
		temp_name(av_pfix[t], ++autonum[t], q->user.ident);
		}
	if (nelt0 > 0)
		declare_new_addr (q);
	return(q);
}


/* Returns a temporary of the appropriate type.  Will reuse existing
   temporaries when possible */

 Addrp
#ifdef KR_headers
mktmpn(nelt, type, lengp)
	int nelt;
	register int type;
	expptr lengp;
#else
mktmpn(int nelt, register int type, expptr lengp)
#endif
{
	ftnint leng;
	chainp p, oldp;
	register Addrp q;
	extern int krparens;

	if(type==TYUNKNOWN || type==TYERROR)
		badtype("mktmpn", type);

	if(type==TYCHAR)
		if(lengp && ISICON(lengp) )
			leng = lengp->constblock.Const.ci;
		else	{
			err("adjustable length");
			return( (Addrp) errnode() );
		}
	else if (type > TYCHAR || type < TYADDR) {
		erri("mktmpn: unexpected type %d", type);
		exit(1);
		}
/*
 * if a temporary of appropriate shape is on the templist,
 * remove it from the list and return it
 */
	if (krparens == 2 && ONEOF(type,M(TYREAL)|M(TYCOMPLEX)))
		type++;
	for(oldp=CHNULL, p=templist[type];  p  ;  oldp=p, p=p->nextp)
	{
		q = (Addrp) (p->datap);
		if(q->ntempelt==nelt &&
		    (type!=TYCHAR || q->vleng->constblock.Const.ci==leng) )
		{
			if(oldp)
				oldp->nextp = p->nextp;
			else
				templist[type] = p->nextp;
			free( (charptr) p);
			return(q);
		}
	}
	q = autovar(nelt, type, lengp, "");
	return(q);
}




/* mktmp -- create new local variable; call it something like   name
   lengp   is taken directly, not copied */

 Addrp
#ifdef KR_headers
mktmp(type, lengp)
	int type;
	expptr lengp;
#else
mktmp(int type, expptr lengp)
#endif
{
	Addrp rv;
	/* arrange for temporaries to be recycled */
	/* at the end of this statement... */
	rv = mktmpn(1,type,lengp);
	frtemp((Addrp)cpexpr((expptr)rv));
	return rv;
}

/* mktmp0 omits frtemp() */
 Addrp
#ifdef KR_headers
mktmp0(type, lengp)
	int type;
	expptr lengp;
#else
mktmp0(int type, expptr lengp)
#endif
{
	Addrp rv;
	/* arrange for temporaries to be recycled */
	/* when this Addrp is freed */
	rv = mktmpn(1,type,lengp);
	rv->istemp = YES;
	return rv;
}

/* VARIOUS ROUTINES FOR PROCESSING DECLARATIONS */

/* comblock -- Declare a new common block.  Input parameters name the block;
   s   will be NULL if the block is unnamed */

 Extsym *
#ifdef KR_headers
comblock(s)
	register char *s;
#else
comblock(register char *s)
#endif
{
	Extsym *p;
	register char *t;
	register int c, i;
	char cbuf[256], *s0;

/* Give the unnamed common block a unique name */

	if(*s == 0)
		p = mkext1(s0 = Blank, Blank);
	else {
		s0 = s;
		t = cbuf;
		for(i = 0; c = *t = *s++; t++)
			if (c == '_')
				i = 1;
		if (i)
			*t++ = '_';
		t[0] = '_';
		t[1] = 0;
		p = mkext1(s0,cbuf);
		}
	if(p->extstg == STGUNKNOWN)
		p->extstg = STGCOMMON;
	else if(p->extstg != STGCOMMON)
	{
		errstr("%.52s cannot be a common block: it is a subprogram.",
			s0);
		return(0);
	}

	return( p );
}


/* incomm -- add a new variable to a common declaration */

 void
#ifdef KR_headers
incomm(c, v)
	Extsym *c;
	Namep v;
#else
incomm(Extsym *c, Namep v)
#endif
{
	if (!c)
		return;
	if(v->vstg != STGUNKNOWN && !v->vimplstg)
		dclerr(v->vstg == STGARG
			? "dummy arguments cannot be in common"
			: "incompatible common declaration", v);
	else
	{
		v->vstg = STGCOMMON;
		c->extp = mkchain((char *)v, c->extp);
	}
}




/* settype -- set the type or storage class of a Namep object.  If
   v -> vstg == STGUNKNOWN && type < 0,   attempt to reset vstg to be
   -type.  This function will not change any earlier definitions in   v,
   in will only attempt to fill out more information give the other params */

 void
#ifdef KR_headers
settype(v, type, length)
	register Namep v;
	register int type;
	register ftnint length;
#else
settype(register Namep v, register int type, register ftnint length)
#endif
{
	int type1;

	if(type == TYUNKNOWN)
		return;

	if(type==TYSUBR && v->vtype!=TYUNKNOWN && v->vstg==STGARG)
	{
		v->vtype = TYSUBR;
		frexpr(v->vleng);
		v->vleng = 0;
		v->vimpltype = 0;
	}
	else if(type < 0)	/* storage class set */
	{
		if(v->vstg == STGUNKNOWN)
			v->vstg = - type;
		else if(v->vstg != -type)
			dclerr("incompatible storage declarations", v);
	}
	else if(v->vtype == TYUNKNOWN
		|| v->vtype != type
			&& (v->vimpltype || v->vinftype || v->vinfproc))
	{
		if( (v->vtype = lengtype(type, length))==TYCHAR )
			if (length>=0)
				v->vleng = ICON(length);
			else if (parstate >= INDATA)
				v->vleng = ICON(1);	/* avoid a memory fault */
		v->vimpltype = 0;
		v->vinftype = 0; /* 19960709 */
		v->vinfproc = 0; /* 19960709 */

		if (v->vclass == CLPROC) {
			if (v->vstg == STGEXT
			 && (type1 = extsymtab[v->vardesc.varno].extype)
			 &&  type1 != v->vtype)
				changedtype(v);
			else if (v->vprocclass == PTHISPROC
					&& (parstate >= INDATA
						|| procclass == CLMAIN)
					&& !xretslot[type]) {
				xretslot[type] = autovar(ONEOF(type,
					MSKCOMPLEX|MSKCHAR) ? 0 : 1, type,
					v->vleng, " ret_val");
				if (procclass == CLMAIN)
					errstr(
				"illegal use of %.60s (main program name)",
					v->fvarname);
				/* not completely right, but enough to */
				/* avoid memory faults; we won't */
				/* emit any C as we have illegal Fortran */
				}
			}
	}
	else if(v->vtype != type && v->vtype != lengtype(type, length)) {
 incompat:
		dclerr("incompatible type declarations", v);
		}
	else if (type==TYCHAR)
		if (v->vleng && v->vleng->constblock.Const.ci != length)
			goto incompat;
		else if (parstate >= INDATA)
			v->vleng = ICON(1);	/* avoid a memory fault */
}





/* lengtype -- returns the proper compiler type, given input of Fortran
   type and length specifier */

 int
#ifdef KR_headers
lengtype(type, len)
	register int type;
	ftnint len;
#else
lengtype(register int type, ftnint len)
#endif
{
	register int length = (int)len;
	switch(type)
	{
	case TYREAL:
		if(length == typesize[TYDREAL])
			return(TYDREAL);
		if(length == typesize[TYREAL])
			goto ret;
		break;

	case TYCOMPLEX:
		if(length == typesize[TYDCOMPLEX])
			return(TYDCOMPLEX);
		if(length == typesize[TYCOMPLEX])
			goto ret;
		break;

	case TYINT1:
	case TYSHORT:
	case TYDREAL:
	case TYDCOMPLEX:
	case TYCHAR:
	case TYLOGICAL1:
	case TYLOGICAL2:
	case TYUNKNOWN:
	case TYSUBR:
	case TYERROR:
#ifdef TYQUAD
	case TYQUAD:
#endif
		goto ret;

	case TYLOGICAL:
		switch(length) {
			case 0: return tylog;
			case 1:	return TYLOGICAL1;
			case 2: return TYLOGICAL2;
			case 4: goto ret;
			}
		break;

	case TYLONG:
		if(length == 0)
			return(tyint);
		if (length == 1)
			return TYINT1;
		if(length == typesize[TYSHORT])
			return(TYSHORT);
#ifdef TYQUAD
		if(length == typesize[TYQUAD] && use_tyquad)
			return(TYQUAD);
#endif
		if(length == typesize[TYLONG])
			goto ret;
		break;
	default:
		badtype("lengtype", type);
	}

	if(len != 0)
		err("incompatible type-length combination");

ret:
	return(type);
}





/* setintr -- Set Intrinsic function */

 void
#ifdef KR_headers
setintr(v)
	register Namep v;
#else
setintr(register Namep v)
#endif
{
	int k;

	if(k = intrfunct(v->fvarname)) {
		if ((*(struct Intrpacked *)&k).f4)
			if (noextflag)
				goto unknown;
			else
				dcomplex_seen++;
		v->vardesc.varno = k;
		}
	else {
 unknown:
		dclerr("unknown intrinsic function", v);
		return;
		}
	if(v->vstg == STGUNKNOWN)
		v->vstg = STGINTR;
	else if(v->vstg!=STGINTR)
		dclerr("incompatible use of intrinsic function", v);
	if(v->vclass==CLUNKNOWN)
		v->vclass = CLPROC;
	if(v->vprocclass == PUNKNOWN)
		v->vprocclass = PINTRINSIC;
	else if(v->vprocclass != PINTRINSIC)
		dclerr("invalid intrinsic declaration", v);
}



/* setext -- Set External declaration -- assume that unknowns will become
   procedures */

 void
#ifdef KR_headers
setext(v)
	register Namep v;
#else
setext(register Namep v)
#endif
{
	if(v->vclass == CLUNKNOWN)
		v->vclass = CLPROC;
	else if(v->vclass != CLPROC)
		dclerr("invalid external declaration", v);

	if(v->vprocclass == PUNKNOWN)
		v->vprocclass = PEXTERNAL;
	else if(v->vprocclass != PEXTERNAL)
		dclerr("invalid external declaration", v);
} /* setext */




/* create dimensions block for array variable */

 void
#ifdef KR_headers
setbound(v, nd, dims)
	register Namep v;
	int nd;
	struct Dims *dims;
#else
setbound(register Namep v, int nd, struct Dims *dims)
#endif
{
	register expptr q, t;
	register struct Dimblock *p;
	int i;
	extern chainp new_vars;
	char buf[256];

	if(v->vclass == CLUNKNOWN)
		v->vclass = CLVAR;
	else if(v->vclass != CLVAR)
	{
		dclerr("only variables may be arrays", v);
		return;
	}

	v->vdim = p = (struct Dimblock *)
	    ckalloc( sizeof(int) + (3+2*nd)*sizeof(expptr) );
	p->ndim = nd--;
	p->nelt = ICON(1);
	doin_setbound = 1;

	if (noextflag)
		for(i = 0; i <= nd; i++)
			if (((q = dims[i].lb) && !ISINT(q->headblock.vtype))
			 || ((q = dims[i].ub) && !ISINT(q->headblock.vtype))) {
				sprintf(buf, "dimension %d of %s is not an integer.",
					i+1, v->fvarname);
				errext(buf);
				break;
				}

	for(i = 0; i <= nd; i++) {
		if (((q = dims[i].lb) && !ISINT(q->headblock.vtype)))
			dims[i].lb = mkconv(TYINT, q);
		if (((q = dims[i].ub) && !ISINT(q->headblock.vtype)))
			dims[i].ub = mkconv(TYINT, q);
		}

	for(i = 0; i <= nd; ++i)
	{
		if( (q = dims[i].ub) == NULL)
		{
			if(i == nd)
			{
				frexpr(p->nelt);
				p->nelt = NULL;
			}
			else
				err("only last bound may be asterisk");
			p->dims[i].dimsize = ICON(1);
			p->dims[i].dimexpr = NULL;
		}
		else
		{

			if(dims[i].lb)
			{
				q = mkexpr(OPMINUS, q, cpexpr(dims[i].lb));
				q = mkexpr(OPPLUS, q, ICON(1) );
			}
			if( ISCONST(q) )
			{
				p->dims[i].dimsize = q;
				p->dims[i].dimexpr = (expptr) PNULL;
			}
			else {
				sprintf(buf, " %s_dim%d", v->fvarname, i+1);
				p->dims[i].dimsize = (expptr)
					autovar(1, tyint, EXNULL, buf);
				p->dims[i].dimexpr = q;
				if (i == nd)
					v->vlastdim = new_vars;
				v->vdimfinish = 1;
			}
			if(p->nelt)
				p->nelt = mkexpr(OPSTAR, p->nelt,
				    cpexpr(p->dims[i].dimsize) );
		}
	}

	q = dims[nd].lb;
	if(q == NULL)
		q = ICON(1);

	for(i = nd-1 ; i>=0 ; --i)
	{
		t = dims[i].lb;
		if(t == NULL)
			t = ICON(1);
		if(p->dims[i].dimsize)
			q = mkexpr(OPPLUS, t,
				mkexpr(OPSTAR, cpexpr(p->dims[i].dimsize), q));
	}

	if( ISCONST(q) )
	{
		p->baseoffset = q;
		p->basexpr = NULL;
	}
	else
	{
		sprintf(buf, " %s_offset", v->fvarname);
		p->baseoffset = (expptr) autovar(1, tyint, EXNULL, buf);
		p->basexpr = q;
		v->vdimfinish = 1;
	}
	doin_setbound = 0;
}


 void
#ifdef KR_headers
wr_abbrevs(outfile, function_head, vars)
	FILE *outfile;
	int function_head;
	chainp vars;
#else
wr_abbrevs(FILE *outfile, int function_head, chainp vars)
#endif
{
    for (; vars; vars = vars -> nextp) {
	Namep name = (Namep) vars -> datap;
	if (!name->visused)
		continue;

	if (function_head)
	    nice_printf (outfile, "#define ");
	else
	    nice_printf (outfile, "#undef ");
	out_name (outfile, name);

	if (function_head) {
	    Extsym *comm = &extsymtab[name -> vardesc.varno];

	    nice_printf (outfile, " (");
	    extern_out (outfile, comm);
	    nice_printf (outfile, "%d.", comm->curno);
	    nice_printf (outfile, "%s)", name->cvarname);
	} /* if function_head */
	nice_printf (outfile, "\n");
    } /* for */
} /* wr_abbrevs */
