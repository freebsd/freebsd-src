/****************************************************************
Copyright 1990, 1992, 1993, 1994 by AT&T, Lucent Technologies and Bellcore.

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

 static char Ptok[128], Pct[Table_size];
 static char *Pfname;
 static long Plineno;
 static int Pbad;
 static int *tfirst, *tlast, *tnext, tmax;

#define P_space	1
#define P_anum	2
#define P_delim	3
#define P_slash	4

#define TGULP	100

 static void
trealloc(Void)
{
	int k = tmax;
	tfirst = (int *)realloc((char *)tfirst,
		(tmax += TGULP)*sizeof(int));
	if (!tfirst) {
		fprintf(stderr,
		"Pfile: realloc failure!\n");
		exit(2);
		}
	tlast = tfirst + tmax;
	tnext = tfirst + k;
	}

 static void
#ifdef KR_headers
badchar(c)
	int c;
#else
badchar(int c)
#endif
{
	fprintf(stderr,
		"unexpected character 0x%.2x = '%c' on line %ld of %s\n",
		c, c, Plineno, Pfname);
	exit(2);
	}

 static void
bad_type(Void)
{
	fprintf(stderr,
		"unexpected type \"%s\" on line %ld of %s\n",
		Ptok, Plineno, Pfname);
	exit(2);
	}

 static void
#ifdef KR_headers
badflag(tname, option)
	char *tname;
	char *option;
#else
badflag(char *tname, char *option)
#endif
{
	fprintf(stderr, "%s type from `f2c -%s` on line %ld of %s\n",
		tname, option, Plineno, Pfname);
	Pbad++;
	}

 static void
#ifdef KR_headers
detected(msg)
	char *msg;
#else
detected(char *msg)
#endif
{
	fprintf(stderr,
	"%sdetected on line %ld of %s\n", msg, Plineno, Pfname);
	Pbad++;
	}

#if 0
 static void
#ifdef KR_headers
checklogical(k)
	int k;
#else
checklogical(int k)
#endif
{
	static int lastmsg = 0;
	static int seen[2] = {0,0};

	seen[k] = 1;
	if (seen[1-k]) {
		if (lastmsg < 3) {
			lastmsg = 3;
			detected(
	"Illegal combination of LOGICAL types -- mixing -I4 with -I2 or -i2\n\t");
			}
		return;
		}
	if (k) {
		if (tylogical == TYLONG || lastmsg >= 2)
			return;
		if (!lastmsg) {
			lastmsg = 2;
			badflag("LOGICAL", "I4");
			}
		}
	else {
		if (tylogical == TYSHORT || lastmsg & 1)
			return;
		if (!lastmsg) {
			lastmsg = 1;
			badflag("LOGICAL", "i2` or `f2c -I2");
			}
		}
	}
#else
#define checklogical(n) /* */
#endif

 static void
#ifdef KR_headers
checkreal(k)
	int k;
#else
checkreal(int k)
#endif
{
	static int warned = 0;
	static int seen[2] = {0,0};

	seen[k] = 1;
	if (seen[1-k]) {
		if (warned < 2)
			detected("Illegal mixture of -R and -!R ");
		warned = 2;
		return;
		}
	if (k == forcedouble || warned)
		return;
	warned = 1;
	badflag("REAL return", k ? "!R" : "R");
	}

 static void
#ifdef KR_headers
Pnotboth(e)
	Extsym *e;
#else
Pnotboth(Extsym *e)
#endif
{
	if (e->curno)
		return;
	Pbad++;
	e->curno = 1;
	fprintf(stderr,
	"%s cannot be both a procedure and a common block (line %ld of %s)\n",
		e->fextname, Plineno, Pfname);
	}

 static int
#ifdef KR_headers
numread(pf, n)
	register FILE *pf;
	int *n;
#else
numread(register FILE *pf, int *n)
#endif
{
	register int c, k;

	if ((c = getc(pf)) < '0' || c > '9')
		return c;
	k = c - '0';
	for(;;) {
		if ((c = getc(pf)) == ' ') {
			*n = k;
			return c;
			}
		if (c < '0' || c > '9')
			break;
		k = 10*k + c - '0';
		}
	return c;
	}

 static void argverify Argdcl((int, Extsym*));
 static void Pbadret Argdcl((int ftype, Extsym *p));

 static int
#ifdef KR_headers
readref(pf, e, ftype)
	register FILE *pf;
	Extsym *e;
	int ftype;
#else
readref(register FILE *pf, Extsym *e, int ftype)
#endif
{
	register int c, *t;
	int i, nargs, type;
	Argtypes *at;
	Atype *a, *ae;

	if (ftype > TYSUBR)
		return 0;
	if ((c = numread(pf, &nargs)) != ' ') {
		if (c != ':')
			return c == EOF;
		/* just a typed external */
		if (e->extstg == STGUNKNOWN) {
			at = 0;
			goto justsym;
			}
		if (e->extstg == STGEXT) {
			if (e->extype != ftype)
				Pbadret(ftype, e);
			}
		else
			Pnotboth(e);
		return 0;
		}

	tnext = tfirst;
	for(i = 0; i < nargs; i++) {
		if ((c = numread(pf, &type)) != ' '
		|| type >= 500
		|| type != TYFTNLEN + 100 && type % 100 > TYSUBR)
			return c == EOF;
		if (tnext >= tlast)
			trealloc();
		*tnext++ = type;
		}

	if (e->extstg == STGUNKNOWN) {
 save_at:
		at = (Argtypes *)
			gmem(sizeof(Argtypes) + (nargs-1)*sizeof(Atype), 1);
		at->dnargs = at->nargs = nargs;
		at->changes = 0;
		t = tfirst;
		a = at->atypes;
		for(ae = a + nargs; a < ae; a++) {
			a->type = *t++;
			a->cp = 0;
			}
 justsym:
		e->extstg = STGEXT;
		e->extype = ftype;
		e->arginfo = at;
		}
	else if (e->extstg != STGEXT) {
		Pnotboth(e);
		}
	else if (!e->arginfo) {
		if (e->extype != ftype)
			Pbadret(ftype, e);
		else
			goto save_at;
		}
	else
		argverify(ftype, e);
	return 0;
	}

 static int
#ifdef KR_headers
comlen(pf)
	register FILE *pf;
#else
comlen(register FILE *pf)
#endif
{
	register int c;
	register char *s, *se;
	char buf[128], cbuf[128];
	int refread;
	long L;
	Extsym *e;

	if ((c = getc(pf)) == EOF)
		return 1;
	if (c == ' ') {
		refread = 0;
		s = "comlen ";
		}
	else if (c == ':') {
		refread = 1;
		s = "ref: ";
		}
	else {
 ret0:
		if (c == '*')
			ungetc(c,pf);
		return 0;
		}
	while(*s) {
		if ((c = getc(pf)) == EOF)
			return 1;
		if (c != *s++)
			goto ret0;
		}
	s = buf;
	se = buf + sizeof(buf) - 1;
	for(;;) {
		if ((c = getc(pf)) == EOF)
			return 1;
		if (c == ' ')
			break;
		if (s >= se || Pct[c] != P_anum)
			goto ret0;
		*s++ = c;
		}
	*s-- = 0;
	if (s <= buf || *s != '_')
		return 0;
	strcpy(cbuf,buf);
	*s-- = 0;
	if (*s == '_') {
		*s-- = 0;
		if (s <= buf)
			return 0;
		}
	for(L = 0;;) {
		if ((c = getc(pf)) == EOF)
			return 1;
		if (c == ' ')
			break;
		if (c < '0' && c > '9')
			goto ret0;
		L = 10*L + c - '0';
		}
	if (!L && !refread)
		return 0;
	e = mkext1(buf, cbuf);
	if (refread)
		return readref(pf, e, (int)L);
	if (e->extstg == STGUNKNOWN) {
		e->extstg = STGCOMMON;
		e->maxleng = L;
		}
	else if (e->extstg != STGCOMMON)
		Pnotboth(e);
	else if (e->maxleng != L) {
		fprintf(stderr,
	"incompatible lengths for common block %s (line %ld of %s)\n",
				    buf, Plineno, Pfname);
		if (e->maxleng < L)
			e->maxleng = L;
		}
	return 0;
	}

 static int
#ifdef KR_headers
Ptoken(pf, canend)
	FILE *pf;
	int canend;
#else
Ptoken(FILE *pf, int canend)
#endif
{
	register int c;
	register char *s, *se;

 top:
	for(;;) {
		c = getc(pf);
		if (c == EOF) {
			if (canend)
				return 0;
			goto badeof;
			}
		if (Pct[c] != P_space)
			break;
		if (c == '\n')
			Plineno++;
		}
	switch(Pct[c]) {
		case P_anum:
			if (c == '_')
				badchar(c);
			s = Ptok;
			se = s + sizeof(Ptok) - 1;
			do {
				if (s < se)
					*s++ = c;
				if ((c = getc(pf)) == EOF) {
 badeof:
					fprintf(stderr,
					"unexpected end of file in %s\n",
						Pfname);
					exit(2);
					}
				}
				while(Pct[c] == P_anum);
			ungetc(c,pf);
			*s = 0;
			return P_anum;

		case P_delim:
			return c;

		case P_slash:
			if ((c = getc(pf)) != '*') {
				if (c == EOF)
					goto badeof;
				badchar('/');
				}
			if (canend && comlen(pf))
				goto badeof;
			for(;;) {
				while((c = getc(pf)) != '*') {
					if (c == EOF)
						goto badeof;
					if (c == '\n')
						Plineno++;
					}
 slashseek:
				switch(getc(pf)) {
					case '/':
						goto top;
					case EOF:
						goto badeof;
					case '*':
						goto slashseek;
					}
				}
		default:
			badchar(c);
		}
	/* NOT REACHED */
	return 0;
	}

 static int
Pftype(Void)
{
	switch(Ptok[0]) {
		case 'C':
			if (!strcmp(Ptok+1, "_f"))
				return TYCOMPLEX;
			break;
		case 'E':
			if (!strcmp(Ptok+1, "_f")) {
				/* TYREAL under forcedouble */
				checkreal(1);
				return TYREAL;
				}
			break;
		case 'H':
			if (!strcmp(Ptok+1, "_f"))
				return TYCHAR;
			break;
		case 'Z':
			if (!strcmp(Ptok+1, "_f"))
				return TYDCOMPLEX;
			break;
		case 'd':
			if (!strcmp(Ptok+1, "oublereal"))
				return TYDREAL;
			break;
		case 'i':
			if (!strcmp(Ptok+1, "nt"))
				return TYSUBR;
			if (!strcmp(Ptok+1, "nteger"))
				return TYLONG;
			if (!strcmp(Ptok+1, "nteger1"))
				return TYINT1;
			break;
		case 'l':
			if (!strcmp(Ptok+1, "ogical")) {
				checklogical(1);
				return TYLOGICAL;
				}
			if (!strcmp(Ptok+1, "ogical1"))
				return TYLOGICAL1;
#ifdef TYQUAD
			if (!strcmp(Ptok+1, "ongint"))
				return TYQUAD;
#endif
			break;
		case 'r':
			if (!strcmp(Ptok+1, "eal")) {
				checkreal(0);
				return TYREAL;
				}
			break;
		case 's':
			if (!strcmp(Ptok+1, "hortint"))
				return TYSHORT;
			if (!strcmp(Ptok+1, "hortlogical")) {
				checklogical(0);
				return TYLOGICAL2;
				}
			break;
		}
	bad_type();
	/* NOT REACHED */
	return 0;
	}

 static void
#ifdef KR_headers
wanted(i, what)
	int i;
	char *what;
#else
wanted(int i, char *what)
#endif
{
	if (i != P_anum) {
		Ptok[0] = i;
		Ptok[1] = 0;
		}
	fprintf(stderr,"Error: expected %s, not \"%s\" (line %ld of %s)\n",
		what, Ptok, Plineno, Pfname);
	exit(2);
	}

 static int
#ifdef KR_headers
Ptype(pf)
	FILE *pf;
#else
Ptype(FILE *pf)
#endif
{
	int i, rv;

	i = Ptoken(pf,0);
	if (i == ')')
		return 0;
	if (i != P_anum)
		badchar(i);

	rv = 0;
	switch(Ptok[0]) {
		case 'C':
			if (!strcmp(Ptok+1, "_fp"))
				rv = TYCOMPLEX+200;
			break;
		case 'D':
			if (!strcmp(Ptok+1, "_fp"))
				rv = TYDREAL+200;
			break;
		case 'E':
		case 'R':
			if (!strcmp(Ptok+1, "_fp"))
				rv = TYREAL+200;
			break;
		case 'H':
			if (!strcmp(Ptok+1, "_fp"))
				rv = TYCHAR+200;
			break;
		case 'I':
			if (!strcmp(Ptok+1, "_fp"))
				rv = TYLONG+200;
			else if (!strcmp(Ptok+1, "1_fp"))
				rv = TYINT1+200;
#ifdef TYQUAD
			else if (!strcmp(Ptok+1, "8_fp"))
				rv = TYQUAD+200;
#endif
			break;
		case 'J':
			if (!strcmp(Ptok+1, "_fp"))
				rv = TYSHORT+200;
			break;
		case 'K':
			checklogical(0);
			goto Logical;
		case 'L':
			checklogical(1);
 Logical:
			if (!strcmp(Ptok+1, "_fp"))
				rv = TYLOGICAL+200;
			else if (!strcmp(Ptok+1, "1_fp"))
				rv = TYLOGICAL1+200;
			else if (!strcmp(Ptok+1, "2_fp"))
				rv = TYLOGICAL2+200;
			break;
		case 'S':
			if (!strcmp(Ptok+1, "_fp"))
				rv = TYSUBR+200;
			break;
		case 'U':
			if (!strcmp(Ptok+1, "_fp"))
				rv = TYUNKNOWN+300;
			break;
		case 'Z':
			if (!strcmp(Ptok+1, "_fp"))
				rv = TYDCOMPLEX+200;
			break;
		case 'c':
			if (!strcmp(Ptok+1, "har"))
				rv = TYCHAR;
			else if (!strcmp(Ptok+1, "omplex"))
				rv = TYCOMPLEX;
			break;
		case 'd':
			if (!strcmp(Ptok+1, "oublereal"))
				rv = TYDREAL;
			else if (!strcmp(Ptok+1, "oublecomplex"))
				rv = TYDCOMPLEX;
			break;
		case 'f':
			if (!strcmp(Ptok+1, "tnlen"))
				rv = TYFTNLEN+100;
			break;
		case 'i':
			if (!strncmp(Ptok+1, "nteger", 6)) {
				if (!Ptok[7])
					rv = TYLONG;
				else if (Ptok[7] == '1' && !Ptok[8])
					rv = TYINT1;
				}
			break;
		case 'l':
			if (!strncmp(Ptok+1, "ogical", 6)) {
				if (!Ptok[7]) {
					checklogical(1);
					rv = TYLOGICAL;
					}
				else if (Ptok[7] == '1' && !Ptok[8])
					rv = TYLOGICAL1;
				}
#ifdef TYQUAD
			else if (!strcmp(Ptok+1,"ongint"))
				rv = TYQUAD;
#endif
			break;
		case 'r':
			if (!strcmp(Ptok+1, "eal"))
				rv = TYREAL;
			break;
		case 's':
			if (!strcmp(Ptok+1, "hortint"))
				rv = TYSHORT;
			else if (!strcmp(Ptok+1, "hortlogical")) {
				checklogical(0);
				rv = TYLOGICAL2;
				}
			break;
		case 'v':
			if (tnext == tfirst && !strcmp(Ptok+1, "oid")) {
				if ((i = Ptoken(pf,0)) != /*(*/ ')')
					wanted(i, /*(*/ "\")\"");
				return 0;
				}
		}
	if (!rv)
		bad_type();
	if (rv < 100 && (i = Ptoken(pf,0)) != '*')
			wanted(i, "\"*\"");
	if ((i = Ptoken(pf,0)) == P_anum)
		i = Ptoken(pf,0);	/* skip variable name */
	switch(i) {
		case ')':
			ungetc(i,pf);
			break;
		case ',':
			break;
		default:
			wanted(i, "\",\" or \")\"");
		}
	return rv;
	}

 static char *
trimunder(Void)
{
	register char *s;
	register int n;
	static char buf[128];

	s = Ptok + strlen(Ptok) - 1;
	if (*s != '_') {
		fprintf(stderr,
			"warning: %s does not end in _ (line %ld of %s)\n",
			Ptok, Plineno, Pfname);
		return Ptok;
		}
	if (s[-1] == '_')
		s--;
	strncpy(buf, Ptok, n = s - Ptok);
	buf[n] = 0;
	return buf;
	}

 static void
#ifdef KR_headers
Pbadmsg(msg, p)
	char *msg;
	Extsym *p;
#else
Pbadmsg(char *msg, Extsym *p)
#endif
{
	Pbad++;
	fprintf(stderr, "%s for %s (line %ld of %s):\n\t", msg,
		p->fextname, Plineno, Pfname);
	p->arginfo->nargs = -1;
	}

 static void
#ifdef KR_headers
Pbadret(ftype, p)
	int ftype;
	Extsym *p;
#else
Pbadret(int ftype, Extsym *p)
#endif
{
	char buf1[32], buf2[32];

	Pbadmsg("inconsistent types",p);
	fprintf(stderr, "here %s, previously %s\n",
		Argtype(ftype+200,buf1),
		Argtype(p->extype+200,buf2));
	}

 static void
#ifdef KR_headers
argverify(ftype, p)
	int ftype;
	Extsym *p;
#else
argverify(int ftype, Extsym *p)
#endif
{
	Argtypes *at;
	register Atype *aty;
	int i, j, k;
	register int *t, *te;
	char buf1[32], buf2[32];

	at = p->arginfo;
	if (at->nargs < 0)
		return;
	if (p->extype != ftype) {
		Pbadret(ftype, p);
		return;
		}
	t = tfirst;
	te = tnext;
	i = te - t;
	if (at->nargs != i) {
		j = at->nargs;
		Pbadmsg("differing numbers of arguments",p);
		fprintf(stderr, "here %d, previously %d\n",
			i, j);
		return;
		}
	for(aty = at->atypes; t < te; t++, aty++) {
		if (*t == aty->type)
			continue;
		j = aty->type;
		k = *t;
		if (k >= 300 || k == j)
			continue;
		if (j >= 300) {
			if (k >= 200) {
				if (k == TYUNKNOWN + 200)
					continue;
				if (j % 100 != k - 200
				 && k != TYSUBR + 200
				 && j != TYUNKNOWN + 300
				 && !type_fixup(at,aty,k))
					goto badtypes;
				}
			else if (j % 100 % TYSUBR != k % TYSUBR
					&& !type_fixup(at,aty,k))
				goto badtypes;
			}
		else if (k < 200 || j < 200)
			goto badtypes;
		else if (k == TYUNKNOWN+200)
			continue;
		else if (j != TYUNKNOWN+200)
			{
 badtypes:
			Pbadmsg("differing calling sequences",p);
			i = t - tfirst + 1;
			fprintf(stderr,
				"arg %d: here %s, prevously %s\n",
				i, Argtype(k,buf1), Argtype(j,buf2));
			return;
			}
		/* We've subsequently learned the right type,
		   as in the call on zoo below...

			subroutine foo(x, zap)
			external zap
			call goo(zap)
			x = zap(3)
			call zoo(zap)
			end
		 */
		aty->type = k;
		at->changes = 1;
		}
	}

 static void
#ifdef KR_headers
newarg(ftype, p)
	int ftype;
	Extsym *p;
#else
newarg(int ftype, Extsym *p)
#endif
{
	Argtypes *at;
	register Atype *aty;
	register int *t, *te;
	int i, k;

	if (p->extstg == STGCOMMON) {
		Pnotboth(p);
		return;
		}
	p->extstg = STGEXT;
	p->extype = ftype;
	p->exproto = 1;
	t = tfirst;
	te = tnext;
	i = te - t;
	k = sizeof(Argtypes) + (i-1)*sizeof(Atype);
	at = p->arginfo = (Argtypes *)gmem(k,1);
	at->dnargs = at->nargs = i;
	at->defined = at->changes = 0;
	for(aty = at->atypes; t < te; aty++) {
		aty->type = *t++;
		aty->cp = 0;
		}
	}

 static int
#ifdef KR_headers
Pfile(fname)
	char *fname;
#else
Pfile(char *fname)
#endif
{
	char *s;
	int ftype, i;
	FILE *pf;
	Extsym *p;

	for(s = fname; *s; s++);
	if (s - fname < 2
	|| s[-2] != '.'
	|| (s[-1] != 'P' && s[-1] != 'p'))
		return 0;

	if (!(pf = fopen(fname, textread))) {
		fprintf(stderr, "can't open %s\n", fname);
		exit(2);
		}
	Pfname = fname;
	Plineno = 1;
	if (!Pct[' ']) {
		for(s = " \t\n\r\v\f"; *s; s++)
			Pct[*s] = P_space;
		for(s = "*,();"; *s; s++)
			Pct[*s] = P_delim;
		for(i = '0'; i <= '9'; i++)
			Pct[i] = P_anum;
		for(s = "abcdefghijklmnopqrstuvwxyz"; i = *s; s++)
			Pct[i] = Pct[i+'A'-'a'] = P_anum;
		Pct['_'] = P_anum;
		Pct['/'] = P_slash;
		}

	for(;;) {
		if (!(i = Ptoken(pf,1)))
			break;
		if (i != P_anum
		|| !strcmp(Ptok, "extern") && (i = Ptoken(pf,0)) != P_anum)
			badchar(i);
		ftype = Pftype();
 getname:
		if ((i = Ptoken(pf,0)) != P_anum)
			badchar(i);
		p = mkext1(trimunder(), Ptok);

		if ((i = Ptoken(pf,0)) != '(')
			badchar(i);
		tnext = tfirst;
		while(i = Ptype(pf)) {
			if (tnext >= tlast)
				trealloc();
			*tnext++ = i;
			}
		if (p->arginfo) {
			argverify(ftype, p);
			if (p->arginfo->nargs < 0)
				newarg(ftype, p);
			}
		else
			newarg(ftype, p);
		p->arginfo->defined = 1;
		i = Ptoken(pf,0);
		switch(i) {
			case ';':
				break;
			case ',':
				goto getname;
			default:
				wanted(i, "\";\" or \",\"");
			}
		}
	fclose(pf);
	return 1;
	}

 void
#ifdef KR_headers
read_Pfiles(ffiles)
	char **ffiles;
#else
read_Pfiles(char **ffiles)
#endif
{
	char **f1files, **f1files0, *s;
	int k;
	register Extsym *e, *ee;
	register Argtypes *at;
	extern int retcode;

	f1files0 = f1files = ffiles;
	while(s = *ffiles++)
		if (!Pfile(s))
			*f1files++ = s;
	if (Pbad)
		retcode = 8;
	if (tfirst) {
		free((char *)tfirst);
		/* following should be unnecessary, as we won't be back here */
		tfirst = tnext = tlast = 0;
		tmax = 0;
		}
	*f1files = 0;
	if (f1files == f1files0)
		f1files[1] = 0;

	k = 0;
	ee = nextext;
	for (e = extsymtab; e < ee; e++)
		if (e->extstg == STGEXT
		&& (at = e->arginfo)) {
			if (at->nargs < 0 || at->changes)
				k++;
			at->changes = 2;
			}
	if (k) {
		fprintf(diagfile,
		"%d prototype%s updated while reading prototypes.\n", k,
			k > 1 ? "s" : "");
		}
	fflush(diagfile);
	}
