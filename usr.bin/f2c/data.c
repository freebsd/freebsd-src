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

/* ROUTINES CALLED DURING DATA AND PARAMETER STATEMENT PROCESSING */

static char datafmt[] = "%s\t%09ld\t%d";
static char *cur_varname;

/* another initializer, called from parser */
 void
#ifdef KR_headers
dataval(repp, valp)
	register expptr repp;
	register expptr valp;
#else
dataval(register expptr repp, register expptr valp)
#endif
{
	int i, nrep;
	ftnint elen;
	register Addrp p;

	if (parstate < INDATA) {
		frexpr(repp);
		goto ret;
		}
	if(repp == NULL)
		nrep = 1;
	else if (ISICON(repp) && repp->constblock.Const.ci >= 0)
		nrep = repp->constblock.Const.ci;
	else
	{
		err("invalid repetition count in DATA statement");
		frexpr(repp);
		goto ret;
	}
	frexpr(repp);

	if( ! ISCONST(valp) ) {
		if (valp->tag == TADDR
		 && valp->addrblock.uname_tag == UNAM_CONST) {
			/* kludge */
			frexpr(valp->addrblock.memoffset);
			valp->tag = TCONST;
			}
		else {
			err("non-constant initializer");
			goto ret;
			}
		}

	if(toomanyinit) goto ret;
	for(i = 0 ; i < nrep ; ++i)
	{
		p = nextdata(&elen);
		if(p == NULL)
		{
			if (lineno != err_lineno)
				err("too many initializers");
			toomanyinit = YES;
			goto ret;
		}
		setdata((Addrp)p, (Constp)valp, elen);
		frexpr((expptr)p);
	}

ret:
	frexpr(valp);
}


 Addrp
#ifdef KR_headers
nextdata(elenp)
	ftnint *elenp;
#else
nextdata(ftnint *elenp)
#endif
{
	register struct Impldoblock *ip;
	struct Primblock *pp;
	register Namep np;
	register struct Rplblock *rp;
	tagptr p;
	expptr neltp;
	register expptr q;
	int skip;
	ftnint off, vlen;

	while(curdtp)
	{
		p = (tagptr)curdtp->datap;
		if(p->tag == TIMPLDO)
		{
			ip = &(p->impldoblock);
			if(ip->implb==NULL || ip->impub==NULL || ip->varnp==NULL)
				fatali("bad impldoblock 0%o", (int) ip);
			if(ip->isactive)
				ip->varvp->Const.ci += ip->impdiff;
			else
			{
				q = fixtype(cpexpr(ip->implb));
				if( ! ISICON(q) )
					goto doerr;
				ip->varvp = (Constp) q;

				if(ip->impstep)
				{
					q = fixtype(cpexpr(ip->impstep));
					if( ! ISICON(q) )
						goto doerr;
					ip->impdiff = q->constblock.Const.ci;
					frexpr(q);
				}
				else
					ip->impdiff = 1;

				q = fixtype(cpexpr(ip->impub));
				if(! ISICON(q))
					goto doerr;
				ip->implim = q->constblock.Const.ci;
				frexpr(q);

				ip->isactive = YES;
				rp = ALLOC(Rplblock);
				rp->rplnextp = rpllist;
				rpllist = rp;
				rp->rplnp = ip->varnp;
				rp->rplvp = (expptr) (ip->varvp);
				rp->rpltag = TCONST;
			}

			if( (ip->impdiff>0 && (ip->varvp->Const.ci <= ip->implim))
			    || (ip->impdiff<0 && (ip->varvp->Const.ci >= ip->implim)) )
			{ /* start new loop */
				curdtp = ip->datalist;
				goto next;
			}

			/* clean up loop */

			if(rpllist)
			{
				rp = rpllist;
				rpllist = rpllist->rplnextp;
				free( (charptr) rp);
			}
			else
				Fatal("rpllist empty");

			frexpr((expptr)ip->varvp);
			ip->isactive = NO;
			curdtp = curdtp->nextp;
			goto next;
		}

		pp = (struct Primblock *) p;
		np = pp->namep;
		cur_varname = np->fvarname;
		skip = YES;

		if(p->primblock.argsp==NULL && np->vdim!=NULL)
		{   /* array initialization */
			q = (expptr) mkaddr(np);
			off = typesize[np->vtype] * curdtelt;
			if(np->vtype == TYCHAR)
				off *= np->vleng->constblock.Const.ci;
			q->addrblock.memoffset =
			    mkexpr(OPPLUS, q->addrblock.memoffset, mkintcon(off) );
			if( (neltp = np->vdim->nelt) && ISCONST(neltp))
			{
				if(++curdtelt < neltp->constblock.Const.ci)
					skip = NO;
			}
			else
				err("attempt to initialize adjustable array");
		}
		else
			q = mklhs((struct Primblock *)cpexpr((expptr)pp), 0);
		if(skip)
		{
			curdtp = curdtp->nextp;
			curdtelt = 0;
		}
		if(q->headblock.vtype == TYCHAR)
			if(ISICON(q->headblock.vleng))
				*elenp = q->headblock.vleng->constblock.Const.ci;
			else	{
				err("initialization of string of nonconstant length");
				continue;
			}
		else	*elenp = typesize[q->headblock.vtype];

		if (np->vstg == STGBSS) {
			vlen = np->vtype==TYCHAR
				? np->vleng->constblock.Const.ci
				: typesize[np->vtype];
			if(vlen > 0)
				np->vstg = STGINIT;
			}
		return( (Addrp) q );

doerr:
		err("nonconstant implied DO parameter");
		frexpr(q);
		curdtp = curdtp->nextp;

next:
		curdtelt = 0;
	}

	return(NULL);
}



LOCAL FILEP dfile;

 void
#ifdef KR_headers
setdata(varp, valp, elen)
	register Addrp varp;
	register Constp valp;
	ftnint elen;
#else
setdata(register Addrp varp, register Constp valp, ftnint elen)
#endif
{
	struct Constblock con;
	register int type;
	int i, k, valtype;
	ftnint offset;
	char *varname;
	static Addrp badvar;
	register unsigned char *s;
	static int last_lineno;
	static char *last_varname;

	if (varp->vstg == STGCOMMON) {
		if (!(dfile = blkdfile))
			dfile = blkdfile = opf(blkdfname, textwrite);
		}
	else {
		if (procclass == CLBLOCK) {
			if (varp != badvar) {
				badvar = varp;
				warn1("%s is not in a COMMON block",
					varp->uname_tag == UNAM_NAME
					? varp->user.name->fvarname
					: "???");
				}
			return;
			}
		if (!(dfile = initfile))
			dfile = initfile = opf(initfname, textwrite);
		}
	varname = dataname(varp->vstg, varp->memno);
	offset = varp->memoffset->constblock.Const.ci;
	type = varp->vtype;
	valtype = valp->vtype;
	if(type!=TYCHAR && valtype==TYCHAR)
	{
		if(! ftn66flag
		&& (last_varname != cur_varname || last_lineno != lineno)) {
			/* prevent multiple warnings */
			last_lineno = lineno;
			warn1(
	"non-character datum %.42s initialized with character string",
				last_varname = cur_varname);
			}
		varp->vleng = ICON(typesize[type]);
		varp->vtype = type = TYCHAR;
	}
	else if( (type==TYCHAR && valtype!=TYCHAR) ||
	    (cktype(OPASSIGN,type,valtype) == TYERROR) )
	{
		err("incompatible types in initialization");
		return;
	}
	if(type == TYADDR)
		con.Const.ci = valp->Const.ci;
	else if(type != TYCHAR)
	{
		if(valtype == TYUNKNOWN)
			con.Const.ci = valp->Const.ci;
		else	consconv(type, &con, valp);
	}

	k = 1;

	switch(type)
	{
	case TYLOGICAL:
	case TYINT1:
	case TYLOGICAL1:
	case TYLOGICAL2:
	case TYSHORT:
	case TYLONG:
#ifdef TYQUAD
	case TYQUAD:
#endif
		dataline(varname, offset, type);
		prconi(dfile, con.Const.ci);
		break;

	case TYADDR:
		dataline(varname, offset, type);
		prcona(dfile, con.Const.ci);
		break;

	case TYCOMPLEX:
	case TYDCOMPLEX:
		k = 2;
	case TYREAL:
	case TYDREAL:
		dataline(varname, offset, type);
		prconr(dfile, &con, k);
		break;

	case TYCHAR:
		k = valp -> vleng -> constblock.Const.ci;
		if (elen < k)
			k = elen;
		s = (unsigned char *)valp->Const.ccp;
		for(i = 0 ; i < k ; ++i) {
			dataline(varname, offset++, TYCHAR);
			fprintf(dfile, "\t%d\n", *s++);
			}
		k = elen - valp->vleng->constblock.Const.ci;
		if(k > 0) {
			dataline(varname, offset, TYBLANK);
			fprintf(dfile, "\t%d\n", k);
			}
		break;

	default:
		badtype("setdata", type);
	}

}



/*
   output form of name is padded with blanks and preceded
   with a storage class digit
*/
 char*
#ifdef KR_headers
dataname(stg, memno)
	int stg;
	long memno;
#else
dataname(int stg, long memno)
#endif
{
	static char varname[64];
	register char *s, *t;
	char buf[16];

	if (stg == STGCOMMON) {
		varname[0] = '2';
		sprintf(s = buf, "Q.%ld", memno);
		}
	else {
		varname[0] = stg==STGEQUIV ? '1' : '0';
		s = memname(stg, memno);
		}
	t = varname + 1;
	while(*t++ = *s++);
	*t = 0;
	return(varname);
}




 void
#ifdef KR_headers
frdata(p0)
	chainp p0;
#else
frdata(chainp p0)
#endif
{
	register struct Chain *p;
	register tagptr q;

	for(p = p0 ; p ; p = p->nextp)
	{
		q = (tagptr)p->datap;
		if(q->tag == TIMPLDO)
		{
			if(q->impldoblock.isbusy)
				return;	/* circular chain completed */
			q->impldoblock.isbusy = YES;
			frdata(q->impldoblock.datalist);
			free( (charptr) q);
		}
		else
			frexpr(q);
	}

	frchain( &p0);
}


 void
#ifdef KR_headers
dataline(varname, offset, type)
	char *varname;
	ftnint offset;
	int type;
#else
dataline(char *varname, ftnint offset, int type)
#endif
{
	fprintf(dfile, datafmt, varname, offset, type);
}

 void
#ifdef KR_headers
make_param(p, e)
	register struct Paramblock *p;
	expptr e;
#else
make_param(register struct Paramblock *p, expptr e)
#endif
{
	register expptr q;
	Constp qc;

	if (p->vstg == STGARG)
		errstr("Dummy argument %.50s appears in a parameter statement.",
			p->fvarname);
	p->vclass = CLPARAM;
	impldcl((Namep)p);
	if (e->headblock.vtype != TYCHAR)
		e = putx(fixtype(e));
	p->paramval = q = mkconv(p->vtype, e);
	if (p->vtype == TYCHAR) {
		if (q->tag == TEXPR)
			p->paramval = q = fixexpr((Exprp)q);
		if (q->tag == TADDR && q->addrblock.uname_tag == UNAM_CONST) {
			qc = mkconst(TYCHAR);
			qc->Const = q->addrblock.user.Const;
			qc->vleng = q->addrblock.vleng;
			q->addrblock.vleng = 0;
			frexpr(q);
			p->paramval = q = (expptr)qc;
			}
		if (!ISCONST(q) || q->constblock.vtype != TYCHAR) {
			errstr("invalid value for character parameter %s",
				p->fvarname);
			return;
			}
		if (!(e = p->vleng))
			p->vleng = ICON(q->constblock.vleng->constblock.Const.ci
					+ q->constblock.Const.ccp1.blanks);
		else if (q->constblock.vleng->constblock.Const.ci
				> e->constblock.Const.ci) {
			q->constblock.vleng->constblock.Const.ci
				= e->constblock.Const.ci;
			q->constblock.Const.ccp1.blanks = 0;
			}
		else
			q->constblock.Const.ccp1.blanks
				= e->constblock.Const.ci
				- q->constblock.vleng->constblock.Const.ci;
		}
	}
