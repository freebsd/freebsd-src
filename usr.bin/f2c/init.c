/****************************************************************
Copyright 1990, 1992 - 1996 by AT&T, Lucent Technologies and Bellcore.

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
#include "output.h"
#include "iob.h"

/* State required for the C output */
char *fl_fmt_string;		/* Float format string */
char *db_fmt_string;	    	/* Double format string */
char *cm_fmt_string;		/* Complex format string */
char *dcm_fmt_string;		/* Double complex format string */

chainp new_vars = CHNULL;	/* List of newly created locals in this
				   function.  These may have identifiers
				   which have underscores and more than VL
				   characters */
chainp used_builtins = CHNULL;	/* List of builtins used by this function.
				   These are all Addrps with UNAM_EXTERN
				   */
chainp assigned_fmts = CHNULL;	/* assigned formats */
chainp allargs;			/* union of args in all entry points */
chainp earlylabs;		/* labels seen before enddcl() */
char main_alias[52];		/* PROGRAM name, if any is given */
int tab_size = 4;


FILEP infile;
FILEP diagfile;

FILEP c_file;
FILEP pass1_file;
FILEP initfile;
FILEP blkdfile;


char *token;
int maxtoklen, toklen;
long err_lineno;
long lineno;			/* Current line in the input file, NOT the
				   Fortran statement label number */
char *infname;
int needkwd;
struct Labelblock *thislabel	= NULL;
int nerr;
int nwarn;

flag saveall;
flag substars;
int parstate	= OUTSIDE;
flag headerdone	= NO;
int blklevel;
int doin_setbound;
int impltype[26];
ftnint implleng[26];
int implstg[26];

int tyint	= TYLONG ;
int tylogical	= TYLONG;
int tylog	= TYLOGICAL;
int typesize[NTYPES] = {
	1, SZADDR, 1, SZSHORT, SZLONG,
#ifdef TYQUAD
		2*SZLONG,
#endif
		SZLONG, 2*SZLONG,
		2*SZLONG, 4*SZLONG, 1, SZSHORT, SZLONG, 1, 1, 0,
		4*SZLONG + SZADDR,	/* sizeof(cilist) */
		4*SZLONG + 2*SZADDR,	/* sizeof(icilist) */
		4*SZLONG + 5*SZADDR,	/* sizeof(olist) */
		2*SZLONG + SZADDR,	/* sizeof(cllist) */
		2*SZLONG,		/* sizeof(alist) */
		11*SZLONG + 15*SZADDR	/* sizeof(inlist) */
		};

int typealign[NTYPES] = {
	1, ALIADDR, 1, ALISHORT, ALILONG,
#ifdef TYQUAD
	ALIDOUBLE,
#endif
	ALILONG, ALIDOUBLE,
	ALILONG, ALIDOUBLE, 1, ALISHORT, ALILONG, 1, 1, 1,
	ALILONG, ALILONG, ALILONG, ALILONG, ALILONG, ALILONG};

int type_choice[4] = { TYDREAL, TYSHORT, TYLONG,  TYSHORT };

char *typename[] = {
	"<<unknown>>",
	"address",
	"integer1",
	"shortint",
	"integer",
#ifdef TYQUAD
	"longint",
#endif
	"real",
	"doublereal",
	"complex",
	"doublecomplex",
	"logical1",
	"shortlogical",
	"logical",
	"char"	/* character */
	};

int type_pref[NTYPES] = { 0, 0, 3, 5, 7,
#ifdef TYQUAD
			 10,
#endif
				8, 11, 9, 12, 1, 4, 6, 2 };

char *protorettypes[] = {
	"?", "??", "integer1", "shortint", "integer",
#ifdef TYQUAD
	"longint",
#endif
	"real", "doublereal",
	"C_f", "Z_f", "logical1", "shortlogical", "logical", "H_f", "int"
	};

char *casttypes[TYSUBR+1] = {
	"U_fp", "??bug??", "I1_fp",
	"J_fp", "I_fp",
#ifdef TYQUAD
	"Q_fp",
#endif
	"R_fp", "D_fp", "C_fp", "Z_fp",
	"L1_fp", "L2_fp", "L_fp", "H_fp", "S_fp"
	};
char *usedcasts[TYSUBR+1];

char *dfltarg[] = {
	0, 0, "(integer1 *)0",
	"(shortint *)0", "(integer *)0",
#ifdef TYQUAD
	"(longint *)0",
#endif
	"(real *)0",
	"(doublereal *)0", "(complex *)0", "(doublecomplex *)0",
	"(logical1 *)0","(shortlogical *)0", "(logical *)0", "(char *)0"
	};

static char *dflt0proc[] = {
	0, 0, "(integer1 (*)())0",
	"(shortint (*)())0", "(integer (*)())0",
#ifdef TYQUAD
	"(longint (*)())0",
#endif
	"(real (*)())0",
	"(doublereal (*)())0", "(complex (*)())0", "(doublecomplex (*)())0",
	"(logical1 (*)())0", "(shortlogical (*)())0",
	"(logical (*)())0", "(char (*)())0", "(int (*)())0"
	};

char *dflt1proc[] = { "(U_fp)0", "(??bug??)0", "(I1_fp)0",
	"(J_fp)0", "(I_fp)0",
#ifdef TYQUAD
	"(Q_fp)0",
#endif
	"(R_fp)0", "(D_fp)0", "(C_fp)0", "(Z_fp)0",
	"(L1_fp)0","(L2_fp)0",
	"(L_fp)0", "(H_fp)0", "(S_fp)0"
	};

char **dfltproc = dflt0proc;

static char Bug[] = "bug";

char *ftn_types[] = { "external", "??", "integer*1",
	"integer*2", "integer",
#ifdef TYQUAD
	"integer*8",
#endif
	"real",
	"double precision", "complex", "double complex",
	"logical*1", "logical*2",
	"logical", "character", "subroutine",
	Bug,Bug,Bug,Bug,Bug,Bug,Bug,Bug,Bug, "ftnlen"
	};

int init_ac[TYSUBR+1] = { 0,0,0,0,0,0,0,
#ifdef TYQUAD
			  0,
#endif
			  1, 1, 0, 0, 0, 2};

int proctype	= TYUNKNOWN;
char *procname;
int rtvlabel[NTYPES0];
Addrp retslot;			/* Holds automatic variable which was
				   allocated the function return value
				   */
Addrp xretslot[NTYPES0];	/* for multiple entry points */
int cxslot	= -1;
int chslot	= -1;
int chlgslot	= -1;
int procclass	= CLUNKNOWN;
int nentry;
int nallargs;
int nallchargs;
flag multitype;
ftnint procleng;
long lastiolabno;
long lastlabno;
int lastvarno;
int lastargslot;
int autonum[TYVOID];
char *av_pfix[TYVOID] = {"??TYUNKNOWN??", "a","i1","s","i",
#ifdef TYQUAD
			 "i8",
#endif
			"r","d","q","z","L1","L2","L","ch",
			 "??TYSUBR??", "??TYERROR??","ci", "ici",
			 "o", "cl", "al", "ioin" };

extern int maxctl;
struct Ctlframe *ctls;
struct Ctlframe *ctlstack;
struct Ctlframe *lastctl;

Namep regnamep[MAXREGVAR];
int highregvar;
int nregvar;

extern int maxext;
Extsym *extsymtab;
Extsym *nextext;
Extsym *lastext;

extern int maxequiv;
struct Equivblock *eqvclass;

extern int maxhash;
struct Hashentry *hashtab;
struct Hashentry *lasthash;

extern int maxstno;		/* Maximum number of statement labels */
struct Labelblock *labeltab;
struct Labelblock *labtabend;
struct Labelblock *highlabtab;

int maxdim	= MAXDIM;
struct Rplblock *rpllist	= NULL;
struct Chain *curdtp	= NULL;
flag toomanyinit;
ftnint curdtelt;
chainp templist[TYVOID];
chainp holdtemps;
int dorange	= 0;
struct Entrypoint *entries	= NULL;

chainp chains	= NULL;

flag inioctl;
int iostmt;
int nioctl;
int nequiv	= 0;
int eqvstart	= 0;
int nintnames	= 0;
extern int maxlablist;
struct Labelblock **labarray;

struct Literal *litpool;
int nliterals;

char dflttype[26];
char hextoi_tab[Table_size], Letters[Table_size];
char *ei_first, *ei_next, *ei_last;
char *wh_first, *wh_next, *wh_last;

#define ALLOCN(n,x)	(struct x *) ckalloc((n)*sizeof(struct x))

 void
fileinit(Void)
{
	register char *s;
	register int i, j;

	lastiolabno = 100000;
	lastlabno = 0;
	lastvarno = 0;
	nliterals = 0;
	nerr = 0;

	infile = stdin;

	maxtoklen = 502;
	token = (char *)ckalloc(maxtoklen+2);
	memset(dflttype, tyreal, 26);
	memset(dflttype + 'i' - 'a', tyint, 6);
	memset(hextoi_tab, 16, sizeof(hextoi_tab));
	for(i = 0, s = "0123456789abcdef"; *s; i++, s++)
		hextoi(*s) = i;
	for(i = 10, s = "ABCDEF"; *s; i++, s++)
		hextoi(*s) = i;
	for(j = 0, s = "abcdefghijklmnopqrstuvwxyz"; i = *s++; j++)
		Letters[i] = Letters[i+'A'-'a'] = j;

	ctls = ALLOCN(maxctl+1, Ctlframe);
	extsymtab = ALLOCN(maxext, Extsym);
	eqvclass = ALLOCN(maxequiv, Equivblock);
	hashtab = ALLOCN(maxhash, Hashentry);
	labeltab = ALLOCN(maxstno, Labelblock);
	litpool = ALLOCN(maxliterals, Literal);
	labarray = (struct Labelblock **)ckalloc(maxlablist*
					sizeof(struct Labelblock *));
	fmt_init();
	mem_init();
	np_init();

	ctlstack = ctls++;
	lastctl = ctls + maxctl;
	nextext = extsymtab;
	lastext = extsymtab + maxext;
	lasthash = hashtab + maxhash;
	labtabend = labeltab + maxstno;
	highlabtab = labeltab;
	main_alias[0] = '\0';
	if (forcedouble)
		dfltproc[TYREAL] = dfltproc[TYDREAL];

/* Initialize the routines for providing C output */

	out_init ();
}

 void
hashclear(Void)	/* clear hash table */
{
	register struct Hashentry *hp;
	register Namep p;
	register struct Dimblock *q;
	register int i;

	for(hp = hashtab ; hp < lasthash ; ++hp)
		if(p = hp->varp)
		{
			frexpr(p->vleng);
			if(q = p->vdim)
			{
				for(i = 0 ; i < q->ndim ; ++i)
				{
					frexpr(q->dims[i].dimsize);
					frexpr(q->dims[i].dimexpr);
				}
				frexpr(q->nelt);
				frexpr(q->baseoffset);
				frexpr(q->basexpr);
				free( (charptr) q);
			}
			if(p->vclass == CLNAMELIST)
				frchain( &(p->varxptr.namelist) );
			free( (charptr) p);
			hp->varp = NULL;
		}
	}

 void
procinit(Void)
{
	register struct Labelblock *lp;
	struct Chain *cp;
	int i;
	struct memblock;
	extern struct memblock *curmemblock, *firstmemblock;
	extern char *mem_first, *mem_next, *mem_last, *mem0_last;

	curmemblock = firstmemblock;
	mem_next = mem_first;
	mem_last = mem0_last;
	ei_next = ei_first = ei_last = 0;
	wh_next = wh_first = wh_last = 0;
	iob_list = 0;
	for(i = 0; i < 9; i++)
		io_structs[i] = 0;

	parstate = OUTSIDE;
	headerdone = NO;
	blklevel = 1;
	saveall = NO;
	substars = NO;
	nwarn = 0;
	thislabel = NULL;
	needkwd = 0;

	proctype = TYUNKNOWN;
	procname = "MAIN_";
	procclass = CLUNKNOWN;
	nentry = 0;
	nallargs = nallchargs = 0;
	multitype = NO;
	retslot = NULL;
	for(i = 0; i < NTYPES0; i++) {
		frexpr((expptr)xretslot[i]);
		xretslot[i] = 0;
		}
	cxslot = -1;
	chslot = -1;
	chlgslot = -1;
	procleng = 0;
	blklevel = 1;
	lastargslot = 0;

	for(lp = labeltab ; lp < labtabend ; ++lp)
		lp->stateno = 0;

	hashclear();

/* Clear the list of newly generated identifiers from the previous
   function */

	frexchain(&new_vars);
	frexchain(&used_builtins);
	frchain(&assigned_fmts);
	frchain(&allargs);
	frchain(&earlylabs);

	nintnames = 0;
	highlabtab = labeltab;

	ctlstack = ctls - 1;
	for(i = TYADDR; i < TYVOID; i++) {
		for(cp = templist[i]; cp ; cp = cp->nextp)
			free( (charptr) (cp->datap) );
		frchain(templist + i);
		autonum[i] = 0;
		}
	holdtemps = NULL;
	dorange = 0;
	nregvar = 0;
	highregvar = 0;
	entries = NULL;
	rpllist = NULL;
	inioctl = NO;
	eqvstart += nequiv;
	nequiv = 0;
	dcomplex_seen = 0;

	for(i = 0 ; i<NTYPES0 ; ++i)
		rtvlabel[i] = 0;

	if(undeftype)
		setimpl(TYUNKNOWN, (ftnint) 0, 'a', 'z');
	else
	{
		setimpl(tyreal, (ftnint) 0, 'a', 'z');
		setimpl(tyint,  (ftnint) 0, 'i', 'n');
	}
	setimpl(-STGBSS, (ftnint) 0, 'a', 'z');	/* set class */
}



 void
#ifdef KR_headers
setimpl(type, length, c1, c2)
	int type;
	ftnint length;
	int c1;
	int c2;
#else
setimpl(int type, ftnint length, int c1, int c2)
#endif
{
	int i;
	char buff[100];

	if(c1==0 || c2==0)
		return;

	if(c1 > c2) {
		sprintf(buff, "characters out of order in implicit:%c-%c", c1, c2);
		err(buff);
		}
	else {
		c1 = letter(c1);
		c2 = letter(c2);
		if(type < 0)
			for(i = c1 ; i<=c2 ; ++i)
				implstg[i] = - type;
		else {
			type = lengtype(type, length);
			if(type == TYCHAR) {
				if (length < 0) {
					err("length (*) in implicit");
					length = 1;
					}
				}
			else if (type != TYLONG)
				length = 0;
			for(i = c1 ; i<=c2 ; ++i) {
				impltype[i] = type;
				implleng[i] = length;
				}
			}
		}
	}
