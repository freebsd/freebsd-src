/****************************************************************
Copyright 1990, 1992 - 1995 by AT&T, Lucent Technologies and Bellcore.

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
#include "limits.h"

 int
#ifdef KR_headers
oneof_stg(name, stg, mask)
	Namep name;
	int stg;
	int mask;
#else
oneof_stg(Namep name, int stg, int mask)
#endif
{
	if (stg == STGCOMMON && name) {
		if ((mask & M(STGEQUIV)))
			return name->vcommequiv;
		if ((mask & M(STGCOMMON)))
			return !name->vcommequiv;
		}
	return ONEOF(stg, mask);
	}


/* op_assign -- given a binary opcode, return the associated assignment
   operator */

 int
#ifdef KR_headers
op_assign(opcode)
	int opcode;
#else
op_assign(int opcode)
#endif
{
    int retval = -1;

    switch (opcode) {
        case OPPLUS: retval = OPPLUSEQ; break;
	case OPMINUS: retval = OPMINUSEQ; break;
	case OPSTAR: retval = OPSTAREQ; break;
	case OPSLASH: retval = OPSLASHEQ; break;
	case OPMOD: retval = OPMODEQ; break;
	case OPLSHIFT: retval = OPLSHIFTEQ; break;
	case OPRSHIFT: retval = OPRSHIFTEQ; break;
	case OPBITAND: retval = OPBITANDEQ; break;
	case OPBITXOR: retval = OPBITXOREQ; break;
	case OPBITOR: retval = OPBITOREQ; break;
	default:
	    erri ("op_assign:  bad opcode '%d'", opcode);
	    break;
    } /* switch */

    return retval;
} /* op_assign */


 char *
#ifdef KR_headers
Alloc(n)
	int n;
#else
Alloc(int n)
#endif
		/* error-checking version of malloc */
		/* ckalloc initializes memory to 0; Alloc does not */
{
	char errbuf[32];
	register char *rv;

	rv = malloc(n);
	if (!rv) {
		sprintf(errbuf, "malloc(%d) failure!", n);
		Fatal(errbuf);
		}
	return rv;
	}

 void
#ifdef KR_headers
cpn(n, a, b)
	register int n;
	register char *a;
	register char *b;
#else
cpn(register int n, register char *a, register char *b)
#endif
{
	while(--n >= 0)
		*b++ = *a++;
}


 int
#ifdef KR_headers
eqn(n, a, b)
	register int n;
	register char *a;
	register char *b;
#else
eqn(register int n, register char *a, register char *b)
#endif
{
	while(--n >= 0)
		if(*a++ != *b++)
			return(NO);
	return(YES);
}






 int
#ifdef KR_headers
cmpstr(a, b, la, lb)
	register char *a;
	register char *b;
	ftnint la;
	ftnint lb;
#else
cmpstr(register char *a, register char *b, ftnint la, ftnint lb)
#endif
	/* compare two strings */
{
	register char *aend, *bend;
	aend = a + la;
	bend = b + lb;


	if(la <= lb)
	{
		while(a < aend)
			if(*a != *b)
				return( *a - *b );
			else
			{
				++a;
				++b;
			}

		while(b < bend)
			if(*b != ' ')
				return(' ' - *b);
			else
				++b;
	}

	else
	{
		while(b < bend)
			if(*a != *b)
				return( *a - *b );
			else
			{
				++a;
				++b;
			}
		while(a < aend)
			if(*a != ' ')
				return(*a - ' ');
			else
				++a;
	}
	return(0);
}


/* hookup -- Same as LISP NCONC, that is a destructive append of two lists */

 chainp
#ifdef KR_headers
hookup(x, y)
	register chainp x;
	register chainp y;
#else
hookup(register chainp x, register chainp y)
#endif
{
	register chainp p;

	if(x == NULL)
		return(y);

	for(p = x ; p->nextp ; p = p->nextp)
		;
	p->nextp = y;
	return(x);
}



 struct Listblock *
#ifdef KR_headers
mklist(p)
	chainp p;
#else
mklist(chainp p)
#endif
{
	register struct Listblock *q;

	q = ALLOC(Listblock);
	q->tag = TLIST;
	q->listp = p;
	return(q);
}


 chainp
#ifdef KR_headers
mkchain(p, q)
	register char * p;
	register chainp q;
#else
mkchain(register char * p, register chainp q)
#endif
{
	register chainp r;

	if(chains)
	{
		r = chains;
		chains = chains->nextp;
	}
	else
		r = ALLOC(Chain);

	r->datap = p;
	r->nextp = q;
	return(r);
}

 chainp
#ifdef KR_headers
revchain(next)
	register chainp next;
#else
revchain(register chainp next)
#endif
{
	register chainp p, prev = 0;

	while(p = next) {
		next = p->nextp;
		p->nextp = prev;
		prev = p;
		}
	return prev;
	}


/* addunder -- turn a cvarname into an external name */
/* The cvarname may already end in _ (to avoid C keywords); */
/* if not, it has room for appending an _. */

 char *
#ifdef KR_headers
addunder(s)
	register char *s;
#else
addunder(register char *s)
#endif
{
	register int c, i, j;
	char *s0 = s;

	i = j = 0;
	while(c = *s++)
		if (c == '_')
			i++, j++;
		else
			i = 0;
	if (!i) {
		*s-- = 0;
		*s = '_';
		}
	else if (j == 2)
		s[-2] = 0;
	return( s0 );
	}


/* copyn -- return a new copy of the input Fortran-string */

 char *
#ifdef KR_headers
copyn(n, s)
	register int n;
	register char *s;
#else
copyn(register int n, register char *s)
#endif
{
	register char *p, *q;

	p = q = (char *) Alloc(n);
	while(--n >= 0)
		*q++ = *s++;
	return(p);
}



/* copys -- return a new copy of the input C-string */

 char *
#ifdef KR_headers
copys(s)
	char *s;
#else
copys(char *s)
#endif
{
	return( copyn( strlen(s)+1 , s) );
}



/* convci -- Convert Fortran-string to integer; assumes that input is a
   legal number, with no trailing blanks */

 ftnint
#ifdef KR_headers
convci(n, s)
	register int n;
	register char *s;
#else
convci(register int n, register char *s)
#endif
{
	ftnint sum, t;
	char buff[100], *s0;
	int n0;

	s0 = s;
	n0 = n;
	sum = 0;
	while(n-- > 0) {
		/* sum = 10*sum + (*s++ - '0'); */
		t = *s++ - '0';
		if (sum > LONG_MAX/10) {
 ovfl:
			if (n0 > 60)
				n0 = 60;
			sprintf(buff, "integer constant %.*s truncated.",
				n0, s0);
			err(buff);
			return LONG_MAX;
			}
		sum *= 10;
		if (sum > LONG_MAX - t)
			goto ovfl;
		sum += t;
		}
	return(sum);
	}

/* convic - Convert Integer constant to string */

 char *
#ifdef KR_headers
convic(n)
	ftnint n;
#else
convic(ftnint n)
#endif
{
	static char s[20];
	register char *t;

	s[19] = '\0';
	t = s+19;

	do	{
		*--t = '0' + n%10;
		n /= 10;
	} while(n > 0);

	return(t);
}



/* mkname -- add a new identifier to the environment, including the closed
   hash table. */

 Namep
#ifdef KR_headers
mkname(s)
	register char *s;
#else
mkname(register char *s)
#endif
{
	struct Hashentry *hp;
	register Namep q;
	register int c, hash, i;
	register char *t;
	char *s0;
	char errbuf[64];

	hash = i = 0;
	s0 = s;
	while(c = *s++) {
		hash += c;
		if (c == '_')
			i = 2;
		}
	if (!i && in_vector(s0,c_keywords,n_keywords) >= 0)
		i = 2;
	hash %= maxhash;

/* Add the name to the closed hash table */

	hp = hashtab + hash;

	while(q = hp->varp)
		if( hash == hp->hashval && !strcmp(s0,q->fvarname) )
			return(q);
		else if(++hp >= lasthash)
			hp = hashtab;

	if(++nintnames >= maxhash-1)
		many("names", 'n', maxhash);	/* Fatal error */
	hp->varp = q = ALLOC(Nameblock);
	hp->hashval = hash;
	q->tag = TNAME;	/* TNAME means the tag type is NAME */
	c = s - s0;
	if (c > 7 && noextflag) {
		sprintf(errbuf, "\"%.35s%s\" over 6 characters long", s0,
			c > 36 ? "..." : "");
		errext(errbuf);
		}
	q->fvarname = strcpy(mem(c,0), s0);
	t = q->cvarname = mem(c + i + 1, 0);
	s = s0;
	/* add __ to the end of any name containing _ and to any C keyword */
	while(*t = *s++)
		t++;
	if (i) {
		do *t++ = '_';
			while(--i > 0);
		*t = 0;
		}
	return(q);
}


 struct Labelblock *
#ifdef KR_headers
mklabel(l)
	ftnint l;
#else
mklabel(ftnint l)
#endif
{
	register struct Labelblock *lp;

	if(l <= 0)
		return(NULL);

	for(lp = labeltab ; lp < highlabtab ; ++lp)
		if(lp->stateno == l)
			return(lp);

	if(++highlabtab > labtabend)
		many("statement labels", 's', maxstno);

	lp->stateno = l;
	lp->labelno = (int)newlabel();
	lp->blklevel = 0;
	lp->labused = NO;
	lp->fmtlabused = NO;
	lp->labdefined = NO;
	lp->labinacc = NO;
	lp->labtype = LABUNKNOWN;
	lp->fmtstring = 0;
	return(lp);
}

 long
newlabel(Void)
{
	return ++lastlabno;
}


/* this label appears in a branch context */

 struct Labelblock *
#ifdef KR_headers
execlab(stateno)
	ftnint stateno;
#else
execlab(ftnint stateno)
#endif
{
	register struct Labelblock *lp;

	if(lp = mklabel(stateno))
	{
		if(lp->labinacc)
			warn1("illegal branch to inner block, statement label %s",
			    convic(stateno) );
		else if(lp->labdefined == NO)
			lp->blklevel = blklevel;
		if(lp->labtype == LABFORMAT)
			err("may not branch to a format");
		else
			lp->labtype = LABEXEC;
	}
	else
		execerr("illegal label %s", convic(stateno));

	return(lp);
}


/* find or put a name in the external symbol table */

 Extsym *
#ifdef KR_headers
mkext1(f, s)
	char *f;
	char *s;
#else
mkext1(char *f, char *s)
#endif
{
	Extsym *p;

	for(p = extsymtab ; p<nextext ; ++p)
		if(!strcmp(s,p->cextname))
			return( p );

	if(nextext >= lastext)
		many("external symbols", 'x', maxext);

	nextext->fextname = strcpy(gmem(strlen(f)+1,0), f);
	nextext->cextname = f == s
				? nextext->fextname
				: strcpy(gmem(strlen(s)+1,0), s);
	nextext->extstg = STGUNKNOWN;
	nextext->extp = 0;
	nextext->allextp = 0;
	nextext->extleng = 0;
	nextext->maxleng = 0;
	nextext->extinit = 0;
	nextext->curno = nextext->maxno = 0;
	return( nextext++ );
}


 Extsym *
#ifdef KR_headers
mkext(f, s)
	char *f;
	char *s;
#else
mkext(char *f, char *s)
#endif
{
	Extsym *e = mkext1(f, s);
	if (e->extstg == STGCOMMON)
		errstr("%.52s cannot be a subprogram: it is a common block.",f);
	return e;
	}

 Addrp
#ifdef KR_headers
builtin(t, s, dbi)
	int t;
	char *s;
	int dbi;
#else
builtin(int t, char *s, int dbi)
#endif
{
	register Extsym *p;
	register Addrp q;
	extern chainp used_builtins;

	p = mkext(s,s);
	if(p->extstg == STGUNKNOWN)
		p->extstg = STGEXT;
	else if(p->extstg != STGEXT)
	{
		errstr("improper use of builtin %s", s);
		return(0);
	}

	q = ALLOC(Addrblock);
	q->tag = TADDR;
	q->vtype = t;
	q->vclass = CLPROC;
	q->vstg = STGEXT;
	q->memno = p - extsymtab;
	q->dbl_builtin = dbi;

/* A NULL pointer here tells you to use   memno   to check the external
   symbol table */

	q -> uname_tag = UNAM_EXTERN;

/* Add to the list of used builtins */

	if (dbi >= 0)
		add_extern_to_list (q, &used_builtins);
	return(q);
}


 void
#ifdef KR_headers
add_extern_to_list(addr, list_store)
	Addrp addr;
	chainp *list_store;
#else
add_extern_to_list(Addrp addr, chainp *list_store)
#endif
{
    chainp last = CHNULL;
    chainp list;
    int memno;

    if (list_store == (chainp *) NULL || addr == (Addrp) NULL)
	return;

    list = *list_store;
    memno = addr -> memno;

    for (;list; last = list, list = list -> nextp) {
	Addrp this = (Addrp) (list -> datap);

	if (this -> tag == TADDR && this -> uname_tag == UNAM_EXTERN &&
		this -> memno == memno)
	    return;
    } /* for */

    if (*list_store == CHNULL)
	*list_store = mkchain((char *)cpexpr((expptr)addr), CHNULL);
    else
	last->nextp = mkchain((char *)cpexpr((expptr)addr), CHNULL);

} /* add_extern_to_list */


 void
#ifdef KR_headers
frchain(p)
	register chainp *p;
#else
frchain(register chainp *p)
#endif
{
	register chainp q;

	if(p==0 || *p==0)
		return;

	for(q = *p; q->nextp ; q = q->nextp)
		;
	q->nextp = chains;
	chains = *p;
	*p = 0;
}

 void
#ifdef KR_headers
frexchain(p)
	register chainp *p;
#else
frexchain(register chainp *p)
#endif
{
	register chainp q, r;

	if (q = *p) {
		for(;;q = r) {
			frexpr((expptr)q->datap);
			if (!(r = q->nextp))
				break;
			}
		q->nextp = chains;
		chains = *p;
		*p = 0;
		}
	}


 tagptr
#ifdef KR_headers
cpblock(n, p)
	register int n;
	register char *p;
#else
cpblock(register int n, register char *p)
#endif
{
	register ptr q;

	memcpy((char *)(q = ckalloc(n)), (char *)p, n);
	return( (tagptr) q);
}



 ftnint
#ifdef KR_headers
lmax(a, b)
	ftnint a;
	ftnint b;
#else
lmax(ftnint a, ftnint b)
#endif
{
	return( a>b ? a : b);
}

 ftnint
#ifdef KR_headers
lmin(a, b)
	ftnint a;
	ftnint b;
#else
lmin(ftnint a, ftnint b)
#endif
{
	return(a < b ? a : b);
}




#ifdef KR_headers
maxtype(t1, t2)
	int t1;
	int t2;
#else
maxtype(int t1, int t2)
#endif
{
	int t;

	t = t1 >= t2 ? t1 : t2;
	if(t==TYCOMPLEX && (t1==TYDREAL || t2==TYDREAL) )
		t = TYDCOMPLEX;
	return(t);
}



/* return log base 2 of n if n a power of 2; otherwise -1 */
 int
#ifdef KR_headers
log_2(n)
	ftnint n;
#else
log_2(ftnint n)
#endif
{
	int k;

	/* trick based on binary representation */

	if(n<=0 || (n & (n-1))!=0)
		return(-1);

	for(k = 0 ;  n >>= 1  ; ++k)
		;
	return(k);
}


 void
frrpl(Void)
{
	struct Rplblock *rp;

	while(rpllist)
	{
		rp = rpllist->rplnextp;
		free( (charptr) rpllist);
		rpllist = rp;
	}
}



/* Call a Fortran function with an arbitrary list of arguments */

int callk_kludge;

 expptr
#ifdef KR_headers
callk(type, name, args)
	int type;
	char *name;
	chainp args;
#else
callk(int type, char *name, chainp args)
#endif
{
	register expptr p;

	p = mkexpr(OPCALL,
		(expptr)builtin(callk_kludge ? callk_kludge : type, name, 0),
		(expptr)args);
	p->exprblock.vtype = type;
	return(p);
}



 expptr
#ifdef KR_headers
call4(type, name, arg1, arg2, arg3, arg4)
	int type;
	char *name;
	expptr arg1;
	expptr arg2;
	expptr arg3;
	expptr arg4;
#else
call4(int type, char *name, expptr arg1, expptr arg2, expptr arg3, expptr arg4)
#endif
{
	struct Listblock *args;
	args = mklist( mkchain((char *)arg1,
			mkchain((char *)arg2,
				mkchain((char *)arg3,
	    				mkchain((char *)arg4, CHNULL)) ) ) );
	return( callk(type, name, (chainp)args) );
}




 expptr
#ifdef KR_headers
call3(type, name, arg1, arg2, arg3)
	int type;
	char *name;
	expptr arg1;
	expptr arg2;
	expptr arg3;
#else
call3(int type, char *name, expptr arg1, expptr arg2, expptr arg3)
#endif
{
	struct Listblock *args;
	args = mklist( mkchain((char *)arg1,
			mkchain((char *)arg2,
				mkchain((char *)arg3, CHNULL) ) ) );
	return( callk(type, name, (chainp)args) );
}





 expptr
#ifdef KR_headers
call2(type, name, arg1, arg2)
	int type;
	char *name;
	expptr arg1;
	expptr arg2;
#else
call2(int type, char *name, expptr arg1, expptr arg2)
#endif
{
	struct Listblock *args;

	args = mklist( mkchain((char *)arg1, mkchain((char *)arg2, CHNULL) ) );
	return( callk(type,name, (chainp)args) );
}




 expptr
#ifdef KR_headers
call1(type, name, arg)
	int type;
	char *name;
	expptr arg;
#else
call1(int type, char *name, expptr arg)
#endif
{
	return( callk(type,name, (chainp)mklist(mkchain((char *)arg,CHNULL)) ));
}


 expptr
#ifdef KR_headers
call0(type, name)
	int type;
	char *name;
#else
call0(int type, char *name)
#endif
{
	return( callk(type, name, CHNULL) );
}



 struct Impldoblock *
#ifdef KR_headers
mkiodo(dospec, list)
	chainp dospec;
	chainp list;
#else
mkiodo(chainp dospec, chainp list)
#endif
{
	register struct Impldoblock *q;

	q = ALLOC(Impldoblock);
	q->tag = TIMPLDO;
	q->impdospec = dospec;
	q->datalist = list;
	return(q);
}




/* ckalloc -- Allocate 1 memory unit of size   n,   checking for out of
   memory error */

 ptr
#ifdef KR_headers
ckalloc(n)
	register int n;
#else
ckalloc(register int n)
#endif
{
	register ptr p;
	p = (ptr)calloc(1, (unsigned) n);
	if (p || !n)
		return(p);
	fprintf(stderr, "failing to get %d bytes\n",n);
	Fatal("out of memory");
	/* NOT REACHED */ return 0;
}


 int
#ifdef KR_headers
isaddr(p)
	register expptr p;
#else
isaddr(register expptr p)
#endif
{
	if(p->tag == TADDR)
		return(YES);
	if(p->tag == TEXPR)
		switch(p->exprblock.opcode)
		{
		case OPCOMMA:
			return( isaddr(p->exprblock.rightp) );

		case OPASSIGN:
		case OPASSIGNI:
		case OPPLUSEQ:
		case OPMINUSEQ:
		case OPSLASHEQ:
		case OPMODEQ:
		case OPLSHIFTEQ:
		case OPRSHIFTEQ:
		case OPBITANDEQ:
		case OPBITXOREQ:
		case OPBITOREQ:
			return( isaddr(p->exprblock.leftp) );
		}
	return(NO);
}



 int
#ifdef KR_headers
isstatic(p)
	register expptr p;
#else
isstatic(register expptr p)
#endif
{
	extern int useauto;
	if(p->headblock.vleng && !ISCONST(p->headblock.vleng))
		return(NO);

	switch(p->tag)
	{
	case TCONST:
		return(YES);

	case TADDR:
		if(ONEOF(p->addrblock.vstg,MSKSTATIC) &&
		    ISCONST(p->addrblock.memoffset) && !useauto)
			return(YES);

	default:
		return(NO);
	}
}



/* addressable -- return True iff it is a constant value, or can be
   referenced by constant values */

 int
#ifdef KR_headers
addressable(p)
	register expptr p;
#else
addressable(register expptr p)
#endif
{
	switch(p->tag)
	{
	case TCONST:
		return(YES);

	case TADDR:
		return( addressable(p->addrblock.memoffset) );

	default:
		return(NO);
	}
}


/* isnegative_const -- returns true if the constant is negative.  Returns
   false for imaginary and nonnumeric constants */

 int
#ifdef KR_headers
isnegative_const(cp)
	struct Constblock *cp;
#else
isnegative_const(struct Constblock *cp)
#endif
{
    int retval;

    if (cp == NULL)
	return 0;

    switch (cp -> vtype) {
	case TYINT1:
        case TYSHORT:
	case TYLONG:
#ifdef TYQUAD
	case TYQUAD:
#endif
	    retval = cp -> Const.ci < 0;
	    break;
	case TYREAL:
	case TYDREAL:
		retval = cp->vstg ? *cp->Const.cds[0] == '-'
				  :  cp->Const.cd[0] < 0.0;
	    break;
	default:

	    retval = 0;
	    break;
    } /* switch */

    return retval;
} /* isnegative_const */

 void
#ifdef KR_headers
negate_const(cp)
	Constp cp;
#else
negate_const(Constp cp)
#endif
{
    if (cp == (struct Constblock *) NULL)
	return;

    switch (cp -> vtype) {
	case TYINT1:
	case TYSHORT:
	case TYLONG:
#ifdef TYQUAD
	case TYQUAD:
#endif
	    cp -> Const.ci = - cp -> Const.ci;
	    break;
	case TYCOMPLEX:
	case TYDCOMPLEX:
		if (cp->vstg)
		    switch(*cp->Const.cds[1]) {
			case '-':
				++cp->Const.cds[1];
				break;
			case '0':
				break;
			default:
				--cp->Const.cds[1];
			}
		else
	    		cp->Const.cd[1] = -cp->Const.cd[1];
		/* no break */
	case TYREAL:
	case TYDREAL:
		if (cp->vstg)
		    switch(*cp->Const.cds[0]) {
			case '-':
				++cp->Const.cds[0];
				break;
			case '0':
				break;
			default:
				--cp->Const.cds[0];
			}
		else
	    		cp->Const.cd[0] = -cp->Const.cd[0];
	    break;
	case TYCHAR:
	case TYLOGICAL1:
	case TYLOGICAL2:
	case TYLOGICAL:
	    erri ("negate_const:  can't negate type '%d'", cp -> vtype);
	    break;
	default:
	    erri ("negate_const:  bad type '%d'",
		    cp -> vtype);
	    break;
    } /* switch */
} /* negate_const */

 void
#ifdef KR_headers
ffilecopy(infp, outfp)
	FILE *infp;
	FILE *outfp;
#else
ffilecopy(FILE *infp, FILE *outfp)
#endif
{
    while (!feof (infp)) {
	register c = getc (infp);
	if (!feof (infp))
	putc (c, outfp);
    } /* while */
} /* ffilecopy */


/* in_vector -- verifies whether   str   is in c_keywords.
   If so, the index is returned else  -1  is returned.
   c_keywords must be in alphabetical order (as defined by strcmp).
*/

 int
#ifdef KR_headers
in_vector(str, keywds, n)
	char *str;
	char **keywds;
	register int n;
#else
in_vector(char *str, char **keywds, register int n)
#endif
{
	register char **K = keywds;
	register int n1, t;

	do {
		n1 = n >> 1;
		if (!(t = strcmp(str, K[n1])))
			return K - keywds + n1;
		if (t < 0)
			n = n1;
		else {
			n -= ++n1;
			K += n1;
			}
		}
		while(n > 0);

	return -1;
	} /* in_vector */


 int
#ifdef KR_headers
is_negatable(Const)
	Constp Const;
#else
is_negatable(Constp Const)
#endif
{
    int retval = 0;
    if (Const != (Constp) NULL)
	switch (Const -> vtype) {
	    case TYINT1:
		retval = Const -> Const.ci >= -BIGGEST_CHAR;
		break;
	    case TYSHORT:
	        retval = Const -> Const.ci >= -BIGGEST_SHORT;
	        break;
	    case TYLONG:
#ifdef TYQUAD
	    case TYQUAD:
#endif
	        retval = Const -> Const.ci >= -BIGGEST_LONG;
	        break;
	    case TYREAL:
	    case TYDREAL:
	    case TYCOMPLEX:
	    case TYDCOMPLEX:
	        retval = 1;
	        break;
	    case TYLOGICAL1:
	    case TYLOGICAL2:
	    case TYLOGICAL:
	    case TYCHAR:
	    case TYSUBR:
	    default:
	        retval = 0;
	        break;
	} /* switch */

    return retval;
} /* is_negatable */

 void
#ifdef KR_headers
backup(fname, bname)
	char *fname;
	char *bname;
#else
backup(char *fname, char *bname)
#endif
{
	FILE *b, *f;
	static char couldnt[] = "Couldn't open %.80s";

	if (!(f = fopen(fname, binread))) {
		warn1(couldnt, fname);
		return;
		}
	if (!(b = fopen(bname, binwrite))) {
		warn1(couldnt, bname);
		return;
		}
	ffilecopy(f, b);
	fclose(f);
	fclose(b);
	}


/* struct_eq -- returns YES if structures have the same field names and
   types, NO otherwise */

 int
#ifdef KR_headers
struct_eq(s1, s2)
	chainp s1;
	chainp s2;
#else
struct_eq(chainp s1, chainp s2)
#endif
{
    struct Dimblock *d1, *d2;
    Constp cp1, cp2;

    if (s1 == CHNULL && s2 == CHNULL)
	return YES;
    for(; s1 && s2; s1 = s1->nextp, s2 = s2->nextp) {
	register Namep v1 = (Namep) s1 -> datap;
	register Namep v2 = (Namep) s2 -> datap;

	if (v1 == (Namep) NULL || v1 -> tag != TNAME ||
		v2 == (Namep) NULL || v2 -> tag != TNAME)
	    return NO;

	if (v1->vtype != v2->vtype || v1->vclass != v2->vclass
		|| strcmp(v1->fvarname, v2->fvarname))
	    return NO;

	/* compare dimensions (needed for comparing COMMON blocks) */

	if (d1 = v1->vdim) {
		if (!(cp1 = (Constp)d1->nelt) || cp1->tag != TCONST
		||  !(d2 = v2->vdim)
		||  !(cp2 = (Constp)d2->nelt) || cp2->tag != TCONST
		||  cp1->Const.ci != cp2->Const.ci)
			return NO;
		}
	else if (v2->vdim)
		return NO;
    } /* while s1 != CHNULL && s2 != CHNULL */

    return s1 == CHNULL && s2 == CHNULL;
} /* struct_eq */
