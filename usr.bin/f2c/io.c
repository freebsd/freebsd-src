/****************************************************************
Copyright 1990, 1991, 1993, 1994, 1996 by AT&T, Lucent Technologies and Bellcore.

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

/* Routines to generate code for I/O statements.
   Some corrections and improvements due to David Wasley, U. C. Berkeley
*/

/* TEMPORARY */
#define TYIOINT TYLONG
#define SZIOINT SZLONG

#include "defs.h"
#include "names.h"
#include "iob.h"

extern int byterev, inqmask;

static void dofclose Argdcl((void));
static void dofinquire Argdcl((void));
static void dofmove Argdcl((char*));
static void dofopen Argdcl((void));
static void doiolist Argdcl((chainp));
static void ioset Argdcl((int, int, expptr));
static void ioseta Argdcl((int, Addrp));
static void iosetc Argdcl((int, expptr));
static void iosetip Argdcl((int, int));
static void iosetlc Argdcl((int, int, int));
static void putio Argdcl((expptr, expptr));
static void putiocall Argdcl((expptr));

iob_data *iob_list;
Addrp io_structs[9];

LOCAL char ioroutine[12];

LOCAL long ioendlab;
LOCAL long ioerrlab;
LOCAL int endbit;
LOCAL int errbit;
LOCAL long jumplab;
LOCAL long skiplab;
LOCAL int ioformatted;
LOCAL int statstruct = NO;
LOCAL struct Labelblock *skiplabel;
Addrp ioblkp;

#define UNFORMATTED 0
#define FORMATTED 1
#define LISTDIRECTED 2
#define NAMEDIRECTED 3

#define V(z)	ioc[z].iocval

#define IOALL 07777

LOCAL struct Ioclist
{
	char *iocname;
	int iotype;
	expptr iocval;
}
ioc[ ] =
{
	{ "", 0 },
	{ "unit", IOALL },
	{ "fmt", M(IOREAD) | M(IOWRITE) },
	{ "err", IOALL },
	{ "end", M(IOREAD) },
	{ "iostat", IOALL },
	{ "rec", M(IOREAD) | M(IOWRITE) },
	{ "recl", M(IOOPEN) | M(IOINQUIRE) },
	{ "file", M(IOOPEN) | M(IOINQUIRE) },
	{ "status", M(IOOPEN) | M(IOCLOSE) },
	{ "access", M(IOOPEN) | M(IOINQUIRE) },
	{ "form", M(IOOPEN) | M(IOINQUIRE) },
	{ "blank", M(IOOPEN) | M(IOINQUIRE) },
	{ "exist", M(IOINQUIRE) },
	{ "opened", M(IOINQUIRE) },
	{ "number", M(IOINQUIRE) },
	{ "named", M(IOINQUIRE) },
	{ "name", M(IOINQUIRE) },
	{ "sequential", M(IOINQUIRE) },
	{ "direct", M(IOINQUIRE) },
	{ "formatted", M(IOINQUIRE) },
	{ "unformatted", M(IOINQUIRE) },
	{ "nextrec", M(IOINQUIRE) },
	{ "nml", M(IOREAD) | M(IOWRITE) }
};

#define NIOS (sizeof(ioc)/sizeof(struct Ioclist) - 1)

/* #define IOSUNIT 1 */
/* #define IOSFMT 2 */
#define IOSERR 3
#define IOSEND 4
#define IOSIOSTAT 5
#define IOSREC 6
#define IOSRECL 7
#define IOSFILE 8
#define IOSSTATUS 9
#define IOSACCESS 10
#define IOSFORM 11
#define IOSBLANK 12
#define IOSEXISTS 13
#define IOSOPENED 14
#define IOSNUMBER 15
#define IOSNAMED 16
#define IOSNAME 17
#define IOSSEQUENTIAL 18
#define IOSDIRECT 19
#define IOSFORMATTED 20
#define IOSUNFORMATTED 21
#define IOSNEXTREC 22
#define IOSNML 23

#define IOSTP V(IOSIOSTAT)


/* offsets in generated structures */

#define SZFLAG SZIOINT

/* offsets for external READ and WRITE statements */

#define XERR 0
#define XUNIT	SZFLAG
#define XEND	SZFLAG + SZIOINT
#define XFMT	2*SZFLAG + SZIOINT
#define XREC	2*SZFLAG + SZIOINT + SZADDR

/* offsets for internal READ and WRITE statements */

#define XIUNIT	SZFLAG
#define XIEND	SZFLAG + SZADDR
#define XIFMT	2*SZFLAG + SZADDR
#define XIRLEN	2*SZFLAG + 2*SZADDR
#define XIRNUM	2*SZFLAG + 2*SZADDR + SZIOINT
#define XIREC	2*SZFLAG + 2*SZADDR + 2*SZIOINT

/* offsets for OPEN statements */

#define XFNAME	SZFLAG + SZIOINT
#define XFNAMELEN	SZFLAG + SZIOINT + SZADDR
#define XSTATUS	SZFLAG + 2*SZIOINT + SZADDR
#define XACCESS	SZFLAG + 2*SZIOINT + 2*SZADDR
#define XFORMATTED	SZFLAG + 2*SZIOINT + 3*SZADDR
#define XRECLEN	SZFLAG + 2*SZIOINT + 4*SZADDR
#define XBLANK	SZFLAG + 3*SZIOINT + 4*SZADDR

/* offset for CLOSE statement */

#define XCLSTATUS	SZFLAG + SZIOINT

/* offsets for INQUIRE statement */

#define XFILE	SZFLAG + SZIOINT
#define XFILELEN	SZFLAG + SZIOINT + SZADDR
#define XEXISTS	SZFLAG + 2*SZIOINT + SZADDR
#define XOPEN	SZFLAG + 2*SZIOINT + 2*SZADDR
#define XNUMBER	SZFLAG + 2*SZIOINT + 3*SZADDR
#define XNAMED	SZFLAG + 2*SZIOINT + 4*SZADDR
#define XNAME	SZFLAG + 2*SZIOINT + 5*SZADDR
#define XNAMELEN	SZFLAG + 2*SZIOINT + 6*SZADDR
#define XQACCESS	SZFLAG + 3*SZIOINT + 6*SZADDR
#define XQACCLEN	SZFLAG + 3*SZIOINT + 7*SZADDR
#define XSEQ	SZFLAG + 4*SZIOINT + 7*SZADDR
#define XSEQLEN	SZFLAG + 4*SZIOINT + 8*SZADDR
#define XDIRECT	SZFLAG + 5*SZIOINT + 8*SZADDR
#define XDIRLEN	SZFLAG + 5*SZIOINT + 9*SZADDR
#define XFORM	SZFLAG + 6*SZIOINT + 9*SZADDR
#define XFORMLEN	SZFLAG + 6*SZIOINT + 10*SZADDR
#define XFMTED	SZFLAG + 7*SZIOINT + 10*SZADDR
#define XFMTEDLEN	SZFLAG + 7*SZIOINT + 11*SZADDR
#define XUNFMT	SZFLAG + 8*SZIOINT + 11*SZADDR
#define XUNFMTLEN	SZFLAG + 8*SZIOINT + 12*SZADDR
#define XQRECL	SZFLAG + 9*SZIOINT + 12*SZADDR
#define XNEXTREC	SZFLAG + 9*SZIOINT + 13*SZADDR
#define XQBLANK	SZFLAG + 9*SZIOINT + 14*SZADDR
#define XQBLANKLEN	SZFLAG + 9*SZIOINT + 15*SZADDR

LOCAL char *cilist_names[] = {
	"cilist",
	"cierr",
	"ciunit",
	"ciend",
	"cifmt",
	"cirec"
	};
LOCAL char *icilist_names[] = {
	"icilist",
	"icierr",
	"iciunit",
	"iciend",
	"icifmt",
	"icirlen",
	"icirnum"
	};
LOCAL char *olist_names[] = {
	"olist",
	"oerr",
	"ounit",
	"ofnm",
	"ofnmlen",
	"osta",
	"oacc",
	"ofm",
	"orl",
	"oblnk"
	};
LOCAL char *cllist_names[] = {
	"cllist",
	"cerr",
	"cunit",
	"csta"
	};
LOCAL char *alist_names[] = {
	"alist",
	"aerr",
	"aunit"
	};
LOCAL char *inlist_names[] = {
	"inlist",
	"inerr",
	"inunit",
	"infile",
	"infilen",
	"inex",
	"inopen",
	"innum",
	"innamed",
	"inname",
	"innamlen",
	"inacc",
	"inacclen",
	"inseq",
	"inseqlen",
	"indir",
	"indirlen",
	"infmt",
	"infmtlen",
	"inform",
	"informlen",
	"inunf",
	"inunflen",
	"inrecl",
	"innrec",
	"inblank",
	"inblanklen"
	};

LOCAL char **io_fields;

#define zork(n,t) n, sizeof(n)/sizeof(char *) - 1, t

LOCAL io_setup io_stuff[] = {
	zork(cilist_names, TYCILIST),	/* external read/write */
	zork(inlist_names, TYINLIST),	/* inquire */
	zork(olist_names,  TYOLIST),	/* open */
	zork(cllist_names, TYCLLIST),	/* close */
	zork(alist_names,  TYALIST),	/* rewind */
	zork(alist_names,  TYALIST),	/* backspace */
	zork(alist_names,  TYALIST),	/* endfile */
	zork(icilist_names,TYICILIST),	/* internal read */
	zork(icilist_names,TYICILIST)	/* internal write */
	};

#undef zork

 int
#ifdef KR_headers
fmtstmt(lp)
	register struct Labelblock *lp;
#else
fmtstmt(register struct Labelblock *lp)
#endif
{
	if(lp == NULL)
	{
		execerr("unlabeled format statement" , CNULL);
		return(-1);
	}
	if(lp->labtype == LABUNKNOWN)
	{
		lp->labtype = LABFORMAT;
		lp->labelno = (int)newlabel();
	}
	else if(lp->labtype != LABFORMAT)
	{
		execerr("bad format number", CNULL);
		return(-1);
	}
	return(lp->labelno);
}


 void
#ifdef KR_headers
setfmt(lp)
	struct Labelblock *lp;
#else
setfmt(struct Labelblock *lp)
#endif
{
	int n, parity;
	char *s0;
	register char *s, *se, *t;
	register k;

	s0 = s = lexline(&n);
	se = t = s + n;

	/* warn of trivial errors, e.g. "  11 CONTINUE" (one too few spaces) */
	/* following FORMAT... */

	if (n <= 0)
		warn("No (...) after FORMAT");
	else if (*s != '(')
		warni("%c rather than ( after FORMAT", *s);
	else if (se[-1] != ')') {
		*se = 0;
		while(--t > s && *t != ')') ;
		if (t <= s)
			warn("No ) at end of FORMAT statement");
		else if (se - t > 30)
			warn1("Extraneous text at end of FORMAT: ...%s", se-12);
		else
			warn1("Extraneous text at end of FORMAT: %s", t+1);
		t = se;
		}

	/* fix MYQUOTES (\002's) and \\'s */

	parity = 1;
	while(s < se)
		switch(*s++) {
			case 2:
				if ((parity ^= 1) && *s == 2) {
					t -= 2;
					++s;
					}
				else
					t += 3;
				break;
			case '"':
			case '\\':
				t++; break;
			}
	s = s0;
	parity = 1;
	if (lp) {
		lp->fmtstring = t = mem((int)(t - s + 1), 0);
		while(s < se)
			switch(k = *s++) {
				case 2:
					if ((parity ^= 1) && *s == 2)
						s++;
					else {
						t[0] = '\\';
						t[1] = '0';
						t[2] = '0';
						t[3] = '2';
						t += 4;
						}
					break;
				case '"':
				case '\\':
					*t++ = '\\';
					/* no break */
				default:
					*t++ = k;
				}
		*t = 0;
		}
	flline();
}


 void
#ifdef KR_headers
startioctl()
#else
startioctl()
#endif
{
	register int i;

	inioctl = YES;
	nioctl = 0;
	ioformatted = UNFORMATTED;
	for(i = 1 ; i<=NIOS ; ++i)
		V(i) = NULL;
}

 static long
newiolabel(Void) {
	long rv;
	rv = ++lastiolabno;
	skiplabel = mklabel(rv);
	skiplabel->labdefined = 1;
	return rv;
	}

 void
endioctl(Void)
{
	int i;
	expptr p;
	struct io_setup *ios;

	inioctl = NO;

	/* set up for error recovery */

	ioerrlab = ioendlab = skiplab = jumplab = 0;

	if(p = V(IOSEND))
		if(ISICON(p))
			execlab(ioendlab = p->constblock.Const.ci);
		else
			err("bad end= clause");

	if(p = V(IOSERR))
		if(ISICON(p))
			execlab(ioerrlab = p->constblock.Const.ci);
		else
			err("bad err= clause");

	if(IOSTP)
		if(IOSTP->tag!=TADDR || ! ISINT(IOSTP->addrblock.vtype) )
		{
			err("iostat must be an integer variable");
			frexpr(IOSTP);
			IOSTP = NULL;
		}

	if(iostmt == IOREAD)
	{
		if(IOSTP)
		{
			if(ioerrlab && ioendlab && ioerrlab==ioendlab)
				jumplab = ioerrlab;
			else
				skiplab = jumplab = newiolabel();
		}
		else	{
			if(ioerrlab && ioendlab && ioerrlab!=ioendlab)
			{
				IOSTP = (expptr) mktmp(TYINT, ENULL);
				skiplab = jumplab = newiolabel();
			}
			else
				jumplab = (ioerrlab ? ioerrlab : ioendlab);
		}
	}
	else if(iostmt == IOWRITE)
	{
		if(IOSTP && !ioerrlab)
			skiplab = jumplab = newiolabel();
		else
			jumplab = ioerrlab;
	}
	else
		jumplab = ioerrlab;

	endbit = IOSTP!=NULL || ioendlab!=0;	/* for use in startrw() */
	errbit = IOSTP!=NULL || ioerrlab!=0;
	if (jumplab && !IOSTP)
		IOSTP = (expptr) mktmp(TYINT, ENULL);

	if(iostmt!=IOREAD && iostmt!=IOWRITE)
	{
		ios = io_stuff + iostmt;
		io_fields = ios->fields;
		ioblkp = io_structs[iostmt];
		if(ioblkp == NULL)
			io_structs[iostmt] = ioblkp =
				autovar(1, ios->type, ENULL, "");
		ioset(TYIOINT, XERR, ICON(errbit));
	}

	switch(iostmt)
	{
	case IOOPEN:
		dofopen();
		break;

	case IOCLOSE:
		dofclose();
		break;

	case IOINQUIRE:
		dofinquire();
		break;

	case IOBACKSPACE:
		dofmove("f_back");
		break;

	case IOREWIND:
		dofmove("f_rew");
		break;

	case IOENDFILE:
		dofmove("f_end");
		break;

	case IOREAD:
	case IOWRITE:
		startrw();
		break;

	default:
		fatali("impossible iostmt %d", iostmt);
	}
	for(i = 1 ; i<=NIOS ; ++i)
		if(i!=IOSIOSTAT && V(i)!=NULL)
			frexpr(V(i));
}


 int
iocname(Void)
{
	register int i;
	int found, mask;

	found = 0;
	mask = M(iostmt);
	for(i = 1 ; i <= NIOS ; ++i)
		if(!strcmp(ioc[i].iocname, token))
			if(ioc[i].iotype & mask)
				return(i);
			else {
				found = i;
				break;
				}
	if(found) {
		if (iostmt == IOOPEN && !strcmp(ioc[i].iocname, "name")) {
			NOEXT("open with \"name=\" treated as \"file=\"");
			for(i = 1; strcmp(ioc[i].iocname, "file"); i++);
			return i;
			}
		errstr("invalid control %s for statement", ioc[found].iocname);
		}
	else
		errstr("unknown iocontrol %s", token);
	return(IOSBAD);
}


 void
#ifdef KR_headers
ioclause(n, p)
	register int n;
	register expptr p;
#else
ioclause(register int n, register expptr p)
#endif
{
	struct Ioclist *iocp;

	++nioctl;
	if(n == IOSBAD)
		return;
	if(n == IOSPOSITIONAL)
		{
		n = nioctl;
		if (n == IOSFMT) {
			if (iostmt == IOOPEN) {
				n = IOSFILE;
				NOEXT("file= specifier omitted from open");
				}
			else if (iostmt < IOREAD)
				goto illegal;
			}
		else if(n > IOSFMT)
			{
 illegal:
			err("illegal positional iocontrol");
			return;
			}
		}
	else if (n == IOSNML)
		n = IOSFMT;

	if(p == NULL)
	{
		if(n == IOSUNIT)
			p = (expptr) (iostmt==IOREAD ? IOSTDIN : IOSTDOUT);
		else if(n != IOSFMT)
		{
			err("illegal * iocontrol");
			return;
		}
	}
	if(n == IOSFMT)
		ioformatted = (p==NULL ? LISTDIRECTED : FORMATTED);

	iocp = & ioc[n];
	if(iocp->iocval == NULL)
	{
		if(n!=IOSFMT && ( n!=IOSUNIT || (p && p->headblock.vtype!=TYCHAR) ) )
			p = fixtype(p);
		else if (p && p->tag == TPRIM
			   && p->primblock.namep->vclass == CLUNKNOWN) {
			/* kludge made necessary by attempt to infer types
			 * for untyped external parameters: given an error
			 * in calling sequences, an integer argument might
			 * tentatively be assumed TYCHAR; this would otherwise
			 * be corrected too late in startrw after startrw
			 * had decided this to be an internal file.
			 */
			vardcl(p->primblock.namep);
			p->primblock.vtype = p->primblock.namep->vtype;
			}
		iocp->iocval = p;
	}
	else
		errstr("iocontrol %s repeated", iocp->iocname);
}

/* io list item */

 void
#ifdef KR_headers
doio(list)
	chainp list;
#else
doio(chainp list)
#endif
{
	if(ioformatted == NAMEDIRECTED)
	{
		if(list)
			err("no I/O list allowed in NAMELIST read/write");
	}
	else
	{
		doiolist(list);
		ioroutine[0] = 'e';
		if (skiplab)
			jumplab = 0;
		putiocall( call0(TYINT, ioroutine) );
	}
}





 LOCAL void
#ifdef KR_headers
doiolist(p0)
	chainp p0;
#else
doiolist(chainp p0)
#endif
{
	chainp p;
	register tagptr q;
	register expptr qe;
	register Namep qn;
	Addrp tp;
	int range;
	extern char *ohalign;

	for (p = p0 ; p ; p = p->nextp)
	{
		q = (tagptr)p->datap;
		if(q->tag == TIMPLDO)
		{
			exdo(range = (int)newlabel(), (Namep)0,
				q->impldoblock.impdospec);
			doiolist(q->impldoblock.datalist);
			enddo(range);
			free( (charptr) q);
		}
		else	{
			if(q->tag==TPRIM && q->primblock.argsp==NULL
			    && q->primblock.namep->vdim!=NULL)
			{
				vardcl(qn = q->primblock.namep);
				if(qn->vdim->nelt) {
					putio( fixtype(cpexpr(qn->vdim->nelt)),
					    (expptr)mkscalar(qn) );
					qn->vlastdim = 0;
					}
				else
					err("attempt to i/o array of unknown size");
			}
			else if(q->tag==TPRIM && q->primblock.argsp==NULL &&
			    (qe = (expptr) memversion(q->primblock.namep)) )
				putio(ICON(1),qe);
			else if (ISCONST(q) && q->constblock.vtype == TYCHAR) {
				halign = 0;
				putio(ICON(1), qe = fixtype(cpexpr(q)));
				halign = ohalign;
				}
			else if(((qe = fixtype(cpexpr(q)))->tag==TADDR &&
			    (qe->addrblock.uname_tag != UNAM_CONST ||
			    !ISCOMPLEX(qe -> addrblock.vtype))) ||
			    (qe -> tag == TCONST && !ISCOMPLEX(qe ->
			    headblock.vtype))) {
				if (qe -> tag == TCONST)
					qe = (expptr) putconst((Constp)qe);
				putio(ICON(1), qe);
			}
			else if(qe->headblock.vtype != TYERROR)
			{
				if(iostmt == IOWRITE)
				{
					expptr qvl;
					qvl = NULL;
					if( ISCHAR(qe) )
					{
						qvl = (expptr)
						    cpexpr(qe->headblock.vleng);
						tp = mktmp(qe->headblock.vtype,
						    ICON(lencat(qe)));
					}
					else
						tp = mktmp(qe->headblock.vtype,
						    qe->headblock.vleng);
					puteq( cpexpr((expptr)tp), qe);
					if(qvl)	/* put right length on block */
					{
						frexpr(tp->vleng);
						tp->vleng = qvl;
					}
					putio(ICON(1), (expptr)tp);
				}
				else
					err("non-left side in READ list");
			}
			frexpr(q);
		}
	}
	frchain( &p0 );
}

 int iocalladdr = TYADDR;	/* for fixing TYADDR in saveargtypes */
 int typeconv[TYERROR+1] = {
#ifdef TYQUAD
		0, 1, 11, 2, 3, 14, 4, 5, 6, 7, 12, 13, 8, 9, 10, 15
#else
		0, 1, 11, 2, 3,     4, 5, 6, 7, 12, 13, 8, 9, 10, 14
#endif
		};

 LOCAL void
#ifdef KR_headers
putio(nelt, addr)
	expptr nelt;
	register expptr addr;
#else
putio(expptr nelt, register expptr addr)
#endif
{
	int type;
	register expptr q;
	register Addrp c = 0;

	type = addr->headblock.vtype;
	if(ioformatted!=LISTDIRECTED && ISCOMPLEX(type) )
	{
		nelt = mkexpr(OPSTAR, ICON(2), nelt);
		type -= (TYCOMPLEX-TYREAL);
	}

	/* pass a length with every item.  for noncharacter data, fake one */
	if(type != TYCHAR)
	{

		if( ISCONST(addr) )
			addr = (expptr) putconst((Constp)addr);
		c = ALLOC(Addrblock);
		c->tag = TADDR;
		c->vtype = TYLENG;
		c->vstg = STGAUTO;
		c->ntempelt = 1;
		c->isarray = 1;
		c->memoffset = ICON(0);
		c->uname_tag = UNAM_IDENT;
		c->charleng = 1;
		sprintf(c->user.ident, "(ftnlen)sizeof(%s)", typename[type]);
		addr = mkexpr(OPCHARCAST, addr, ENULL);
		}

	nelt = fixtype( mkconv(tyioint,nelt) );
	if(ioformatted == LISTDIRECTED) {
		expptr mc = mkconv(tyioint, ICON(typeconv[type]));
		q = c	? call4(TYINT, "do_lio", mc, nelt, addr, (expptr)c)
			: call3(TYINT, "do_lio", mc, nelt, addr);
		}
	else {
		char *s = ioformatted==FORMATTED ? "do_fio"
			: !byterev ? "do_uio"
			: ONEOF(type, M(TYCHAR)|M(TYINT1)|M(TYLOGICAL1))
			? "do_ucio" : "do_unio";
		q = c	? call3(TYINT, s, nelt, addr, (expptr)c)
			: call2(TYINT, s, nelt, addr);
		}
	iocalladdr = TYCHAR;
	putiocall(q);
	iocalladdr = TYADDR;
}



 void
endio(Void)
{
	if(skiplab)
	{
		if (ioformatted != NAMEDIRECTED)
			p1_label((long)(skiplabel - labeltab));
		if(ioendlab) {
			exif( mkexpr(OPLT, cpexpr(IOSTP), ICON(0)));
			exgoto(execlab(ioendlab));
			exendif();
			}
		if(ioerrlab) {
			exif( mkexpr(iostmt==IOREAD||iostmt==IOWRITE
					? OPGT : OPNE,
				cpexpr(IOSTP), ICON(0)));
			exgoto(execlab(ioerrlab));
			exendif();
			}
	}

	if(IOSTP)
		frexpr(IOSTP);
}



 LOCAL void
#ifdef KR_headers
putiocall(q)
	register expptr q;
#else
putiocall(register expptr q)
#endif
{
	int tyintsave;

	tyintsave = tyint;
	tyint = tyioint;	/* for -I2 and -i2 */

	if(IOSTP)
	{
		q->headblock.vtype = TYINT;
		q = fixexpr((Exprp)mkexpr(OPASSIGN, cpexpr(IOSTP), q));
	}
	putexpr(q);
	if(jumplab) {
		exif(mkexpr(OPNE, cpexpr(IOSTP), ICON(0)));
		exgoto(execlab(jumplab));
		exendif();
		}
	tyint = tyintsave;
}

 void
#ifdef KR_headers
fmtname(np, q)
	Namep np;
	register Addrp q;
#else
fmtname(Namep np, register Addrp q)
#endif
{
	register int k;
	register char *s, *t;
	extern chainp assigned_fmts;

	if (!np->vfmt_asg) {
		np->vfmt_asg = 1;
		assigned_fmts = mkchain((char *)np, assigned_fmts);
		}
	k = strlen(s = np->fvarname);
	if (k < IDENT_LEN - 4) {
		q->uname_tag = UNAM_IDENT;
		t = q->user.ident;
		}
	else {
		q->uname_tag = UNAM_CHARP;
		q->user.Charp = t = mem(k + 5,0);
		}
	sprintf(t, "%s_fmt", s);
	}

 LOCAL Addrp
#ifdef KR_headers
asg_addr(p)
	union Expression *p;
#else
asg_addr(union Expression *p)
#endif
{
	register Addrp q;

	if (p->tag != TPRIM)
		badtag("asg_addr", p->tag);
	q = ALLOC(Addrblock);
	q->tag = TADDR;
	q->vtype = TYCHAR;
	q->vstg = STGAUTO;
	q->ntempelt = 1;
	q->isarray = 0;
	q->memoffset = ICON(0);
	fmtname(p->primblock.namep, q);
	return q;
	}

 void
startrw(Void)
{
	register expptr p;
	register Namep np;
	register Addrp unitp, fmtp, recp;
	register expptr nump;
	int iostmt1;
	flag intfile, sequential, ok, varfmt;
	struct io_setup *ios;

	/* First look at all the parameters and determine what is to be done */

	ok = YES;
	statstruct = YES;

	intfile = NO;
	if(p = V(IOSUNIT))
	{
		if( ISINT(p->headblock.vtype) ) {
 int_unit:
			unitp = (Addrp) cpexpr(p);
			}
		else if(p->headblock.vtype == TYCHAR)
		{
			if (nioctl == 1 && iostmt == IOREAD) {
				/* kludge to recognize READ(format expr) */
				V(IOSFMT) = p;
				V(IOSUNIT) = p = (expptr) IOSTDIN;
				ioformatted = FORMATTED;
				goto int_unit;
				}
			intfile = YES;
			if(p->tag==TPRIM && p->primblock.argsp==NULL &&
			    (np = p->primblock.namep)->vdim!=NULL)
			{
				vardcl(np);
				if(nump = np->vdim->nelt)
				{
					nump = fixtype(cpexpr(nump));
					if( ! ISCONST(nump) ) {
						statstruct = NO;
						np->vlastdim = 0;
						}
				}
				else
				{
					err("attempt to use internal unit array of unknown size");
					ok = NO;
					nump = ICON(1);
				}
				unitp = mkscalar(np);
			}
			else	{
				nump = ICON(1);
				unitp = (Addrp /*pjw */) fixtype(cpexpr(p));
			}
			if(! isstatic((expptr)unitp) )
				statstruct = NO;
		}
		else {
			err("unit specifier not of type integer or character");
			ok = NO;
			}
	}
	else
	{
		err("bad unit specifier");
		ok = NO;
	}

	sequential = YES;
	if(p = V(IOSREC))
		if( ISINT(p->headblock.vtype) )
		{
			recp = (Addrp) cpexpr(p);
			sequential = NO;
		}
		else	{
			err("bad REC= clause");
			ok = NO;
		}
	else
		recp = NULL;


	varfmt = YES;
	fmtp = NULL;
	if(p = V(IOSFMT))
	{
		if(p->tag==TPRIM && p->primblock.argsp==NULL)
		{
			np = p->primblock.namep;
			if(np->vclass == CLNAMELIST)
			{
				ioformatted = NAMEDIRECTED;
				fmtp = (Addrp) fixtype(p);
				V(IOSFMT) = (expptr)fmtp;
				if (skiplab)
					jumplab = 0;
				goto endfmt;
			}
			vardcl(np);
			if(np->vdim)
			{
				if( ! ONEOF(np->vstg, MSKSTATIC) )
					statstruct = NO;
				fmtp = mkscalar(np);
				goto endfmt;
			}
			if( ISINT(np->vtype) )	/* ASSIGNed label */
			{
				statstruct = NO;
				varfmt = YES;
				fmtp = asg_addr(p);
				goto endfmt;
			}
		}
		p = V(IOSFMT) = fixtype(p);
		if(p->headblock.vtype == TYCHAR
			/* Since we allow write(6,n)		*/
			/* we may as well allow write(6,n(2))	*/
		|| p->tag == TADDR && ISINT(p->addrblock.vtype))
		{
			if( ! isstatic(p) )
				statstruct = NO;
			fmtp = (Addrp) cpexpr(p);
		}
		else if( ISICON(p) )
		{
			struct Labelblock *lp;
			lp = mklabel(p->constblock.Const.ci);
			if (fmtstmt(lp) > 0)
			{
				fmtp = (Addrp)mkaddcon(lp->stateno);
				/* lp->stateno for names fmt_nnn */
				lp->fmtlabused = 1;
				varfmt = NO;
			}
			else
				ioformatted = UNFORMATTED;
		}
		else	{
			err("bad format descriptor");
			ioformatted = UNFORMATTED;
			ok = NO;
		}
	}
	else
		fmtp = NULL;

endfmt:
	if(intfile) {
		if (ioformatted==UNFORMATTED) {
			err("unformatted internal I/O not allowed");
			ok = NO;
			}
		if (recp) {
			err("direct internal I/O not allowed");
			ok = NO;
			}
		}
	if(!sequential && ioformatted==LISTDIRECTED)
	{
		err("direct list-directed I/O not allowed");
		ok = NO;
	}
	if(!sequential && ioformatted==NAMEDIRECTED)
	{
		err("direct namelist I/O not allowed");
		ok = NO;
	}

	if( ! ok ) {
		statstruct = NO;
		return;
		}

	/*
   Now put out the I/O structure, statically if all the clauses
   are constants, dynamically otherwise
*/

	if (intfile) {
		ios = io_stuff + iostmt;
		iostmt1 = IOREAD;
		}
	else {
		ios = io_stuff;
		iostmt1 = 0;
		}
	io_fields = ios->fields;
	if(statstruct)
	{
		ioblkp = ALLOC(Addrblock);
		ioblkp->tag = TADDR;
		ioblkp->vtype = ios->type;
		ioblkp->vclass = CLVAR;
		ioblkp->vstg = STGINIT;
		ioblkp->memno = ++lastvarno;
		ioblkp->memoffset = ICON(0);
		ioblkp -> uname_tag = UNAM_IDENT;
		new_iob_data(ios,
			temp_name("io_", lastvarno, ioblkp->user.ident));			}
	else if(!(ioblkp = io_structs[iostmt1]))
		io_structs[iostmt1] = ioblkp =
			autovar(1, ios->type, ENULL, "");

	ioset(TYIOINT, XERR, ICON(errbit));
	if(iostmt == IOREAD)
		ioset(TYIOINT, (intfile ? XIEND : XEND), ICON(endbit) );

	if(intfile)
	{
		ioset(TYIOINT, XIRNUM, nump);
		ioset(TYIOINT, XIRLEN, cpexpr(unitp->vleng) );
		ioseta(XIUNIT, unitp);
	}
	else
		ioset(TYIOINT, XUNIT, (expptr) unitp);

	if(recp)
		ioset(TYIOINT, /* intfile ? XIREC : */ XREC, (expptr) recp);

	if(varfmt)
		ioseta( intfile ? XIFMT : XFMT , fmtp);
	else
		ioset(TYADDR, intfile ? XIFMT : XFMT, (expptr) fmtp);

	ioroutine[0] = 's';
	ioroutine[1] = '_';
	ioroutine[2] = iostmt==IOREAD ? 'r' : 'w';
	ioroutine[3] = "ds"[sequential];
	ioroutine[4] = "ufln"[ioformatted];
	ioroutine[5] = "ei"[intfile];
	ioroutine[6] = '\0';

	putiocall( call1(TYINT, ioroutine, cpexpr((expptr)ioblkp) ));

	if(statstruct)
	{
		frexpr((expptr)ioblkp);
		statstruct = NO;
		ioblkp = 0;	/* unnecessary */
	}
}



 LOCAL void
dofopen(Void)
{
	register expptr p;

	if( (p = V(IOSUNIT)) && ISINT(p->headblock.vtype) )
		ioset(TYIOINT, XUNIT, cpexpr(p) );
	else
		err("bad unit in open");
	if( (p = V(IOSFILE)) )
		if(p->headblock.vtype == TYCHAR)
			ioset(TYIOINT, XFNAMELEN, cpexpr(p->headblock.vleng) );
		else
			err("bad file in open");

	iosetc(XFNAME, p);

	if(p = V(IOSRECL))
		if( ISINT(p->headblock.vtype) )
			ioset(TYIOINT, XRECLEN, cpexpr(p) );
		else
			err("bad recl");
	else
		ioset(TYIOINT, XRECLEN, ICON(0) );

	iosetc(XSTATUS, V(IOSSTATUS));
	iosetc(XACCESS, V(IOSACCESS));
	iosetc(XFORMATTED, V(IOSFORM));
	iosetc(XBLANK, V(IOSBLANK));

	putiocall( call1(TYINT, "f_open", cpexpr((expptr)ioblkp) ));
}


 LOCAL void
dofclose(Void)
{
	register expptr p;

	if( (p = V(IOSUNIT)) && ISINT(p->headblock.vtype) )
	{
		ioset(TYIOINT, XUNIT, cpexpr(p) );
		iosetc(XCLSTATUS, V(IOSSTATUS));
		putiocall( call1(TYINT, "f_clos", cpexpr((expptr)ioblkp)) );
	}
	else
		err("bad unit in close statement");
}


 LOCAL void
dofinquire(Void)
{
	register expptr p;
	if(p = V(IOSUNIT))
	{
		if( V(IOSFILE) )
			err("inquire by unit or by file, not both");
		ioset(TYIOINT, XUNIT, cpexpr(p) );
	}
	else if( ! V(IOSFILE) )
		err("must inquire by unit or by file");
	iosetlc(IOSFILE, XFILE, XFILELEN);
	iosetip(IOSEXISTS, XEXISTS);
	iosetip(IOSOPENED, XOPEN);
	iosetip(IOSNUMBER, XNUMBER);
	iosetip(IOSNAMED, XNAMED);
	iosetlc(IOSNAME, XNAME, XNAMELEN);
	iosetlc(IOSACCESS, XQACCESS, XQACCLEN);
	iosetlc(IOSSEQUENTIAL, XSEQ, XSEQLEN);
	iosetlc(IOSDIRECT, XDIRECT, XDIRLEN);
	iosetlc(IOSFORM, XFORM, XFORMLEN);
	iosetlc(IOSFORMATTED, XFMTED, XFMTEDLEN);
	iosetlc(IOSUNFORMATTED, XUNFMT, XUNFMTLEN);
	iosetip(IOSRECL, XQRECL);
	iosetip(IOSNEXTREC, XNEXTREC);
	iosetlc(IOSBLANK, XQBLANK, XQBLANKLEN);

	putiocall( call1(TYINT,  "f_inqu", cpexpr((expptr)ioblkp) ));
}



 LOCAL void
#ifdef KR_headers
dofmove(subname)
	char *subname;
#else
dofmove(char *subname)
#endif
{
	register expptr p;

	if( (p = V(IOSUNIT)) && ISINT(p->headblock.vtype) )
	{
		ioset(TYIOINT, XUNIT, cpexpr(p) );
		putiocall( call1(TYINT, subname, cpexpr((expptr)ioblkp) ));
	}
	else
		err("bad unit in I/O motion statement");
}

static int ioset_assign = OPASSIGN;

 LOCAL void
#ifdef KR_headers
ioset(type, offset, p)
	int type;
	int offset;
	register expptr p;
#else
ioset(int type, int offset, register expptr p)
#endif
{
	offset /= SZLONG;
	if(statstruct && ISCONST(p)) {
		register char *s;
		switch(type) {
			case TYADDR:	/* stmt label */
				s = "fmt_";
				break;
			case TYIOINT:
				s = "";
				break;
			default:
				badtype("ioset", type);
			}
		iob_list->fields[offset] =
			string_num(s, p->constblock.Const.ci);
		frexpr(p);
		}
	else {
		register Addrp q;

		q = ALLOC(Addrblock);
		q->tag = TADDR;
		q->vtype = type;
		q->vstg = STGAUTO;
		q->ntempelt = 1;
		q->isarray = 0;
		q->memoffset = ICON(0);
		q->uname_tag = UNAM_IDENT;
		sprintf(q->user.ident, "%s.%s",
			statstruct ? iob_list->name : ioblkp->user.ident,
			io_fields[offset + 1]);
		if (type == TYADDR && p->tag == TCONST
				   && p->constblock.vtype == TYADDR) {
			/* kludge */
			register Addrp p1;
			p1 = ALLOC(Addrblock);
			p1->tag = TADDR;
			p1->vtype = type;
			p1->vstg = STGAUTO;	/* wrong, but who cares? */
			p1->ntempelt = 1;
			p1->isarray = 0;
			p1->memoffset = ICON(0);
			p1->uname_tag = UNAM_IDENT;
			sprintf(p1->user.ident, "fmt_%ld",
				p->constblock.Const.ci);
			frexpr(p);
			p = (expptr)p1;
			}
		if (type == TYADDR && p->headblock.vtype == TYCHAR)
			q->vtype = TYCHAR;
		putexpr(mkexpr(ioset_assign, (expptr)q, p));
		}
}




 LOCAL void
#ifdef KR_headers
iosetc(offset, p)
	int offset;
	register expptr p;
#else
iosetc(int offset, register expptr p)
#endif
{
	if(p == NULL)
		ioset(TYADDR, offset, ICON(0) );
	else if(p->headblock.vtype == TYCHAR) {
		p = putx(fixtype((expptr)putchop(cpexpr(p))));
		ioset(TYADDR, offset, addrof(p));
		}
	else
		err("non-character control clause");
}



 LOCAL void
#ifdef KR_headers
ioseta(offset, p)
	int offset;
	register Addrp p;
#else
ioseta(int offset, register Addrp p)
#endif
{
	char *s, *s1;
	static char who[] = "ioseta";
	expptr e, mo;
	Namep np;
	ftnint ci;
	int k;
	char buf[24], buf1[24];
	Extsym *comm;
	extern int usedefsforcommon;

	if(statstruct)
	{
		if (!p)
			return;
		if (p->tag != TADDR)
			badtag(who, p->tag);
		offset /= SZLONG;
		switch(p->uname_tag) {
		    case UNAM_NAME:
			mo = p->memoffset;
			if (mo->tag != TCONST)
				badtag("ioseta/memoffset", mo->tag);
			np = p->user.name;
			np->visused = 1;
			ci = mo->constblock.Const.ci - np->voffset;
			if (np->vstg == STGCOMMON
			&& !np->vcommequiv
			&& !usedefsforcommon) {
				comm = &extsymtab[np->vardesc.varno];
				sprintf(buf, "%d.", comm->curno);
				k = strlen(buf) + strlen(comm->cextname)
					+ strlen(np->cvarname);
				if (ci) {
					sprintf(buf1, "+%ld", ci);
					k += strlen(buf1);
					}
				else
					buf1[0] = 0;
				s = mem(k + 1, 0);
				sprintf(s, "%s%s%s%s", comm->cextname, buf,
					np->cvarname, buf1);
				}
			else if (ci) {
				sprintf(buf,"%ld", ci);
				s1 = p->user.name->cvarname;
				k = strlen(buf) + strlen(s1);
				sprintf(s = mem(k+2,0), "%s+%s", s1, buf);
				}
			else
				s = cpstring(np->cvarname);
			break;
		    case UNAM_CONST:
			s = tostring(p->user.Const.ccp1.ccp0,
				(int)p->vleng->constblock.Const.ci);
			break;
		    default:
			badthing("uname_tag", who, p->uname_tag);
		    }
		/* kludge for Hollerith */
		if (p->vtype != TYCHAR) {
			s1 = mem(strlen(s)+10,0);
			sprintf(s1, "(char *)%s%s", p->isarray ? "" : "&", s);
			s = s1;
			}
		iob_list->fields[offset] = s;
	}
	else {
		if (!p)
			e = ICON(0);
		else if (p->vtype != TYCHAR) {
			NOEXT("non-character variable as format or internal unit");
			e = mkexpr(OPCHARCAST, (expptr)p, ENULL);
			}
		else
			e = addrof((expptr)p);
		ioset(TYADDR, offset, e);
		}
}




 LOCAL void
#ifdef KR_headers
iosetip(i, offset)
	int i;
	int offset;
#else
iosetip(int i, int offset)
#endif
{
	register expptr p;

	if(p = V(i))
		if(p->tag==TADDR &&
		    ONEOF(p->addrblock.vtype, inqmask) ) {
			ioset_assign = OPASSIGNI;
			ioset(TYADDR, offset, addrof(cpexpr(p)) );
			ioset_assign = OPASSIGN;
			}
		else
			errstr("impossible inquire parameter %s", ioc[i].iocname);
	else
		ioset(TYADDR, offset, ICON(0) );
}



 LOCAL void
#ifdef KR_headers
iosetlc(i, offp, offl)
	int i;
	int offp;
	int offl;
#else
iosetlc(int i, int offp, int offl)
#endif
{
	register expptr p;
	if( (p = V(i)) && p->headblock.vtype==TYCHAR)
		ioset(TYIOINT, offl, cpexpr(p->headblock.vleng) );
	iosetc(offp, p);
}
