/*	B.xs
 *
 *	Copyright (c) 1996 Malcolm Beattie
 *
 *	You may distribute under the terms of either the GNU General Public
 *	License or the Artistic License, as specified in the README file.
 *
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "INTERN.h"

#ifdef PERL_OBJECT
#undef op_name
#undef opargs 
#undef op_desc
#define op_name (pPerl->Perl_get_op_names())
#define opargs (pPerl->Perl_get_opargs())
#define op_desc (pPerl->Perl_get_op_descs())
#endif

#ifdef PerlIO
typedef PerlIO * InputStream;
#else
typedef FILE * InputStream;
#endif


static char *svclassnames[] = {
    "B::NULL",
    "B::IV",
    "B::NV",
    "B::RV",
    "B::PV",
    "B::PVIV",
    "B::PVNV",
    "B::PVMG",
    "B::BM",
    "B::PVLV",
    "B::AV",
    "B::HV",
    "B::CV",
    "B::GV",
    "B::FM",
    "B::IO",
};

typedef enum {
    OPc_NULL,	/* 0 */
    OPc_BASEOP,	/* 1 */
    OPc_UNOP,	/* 2 */
    OPc_BINOP,	/* 3 */
    OPc_LOGOP,	/* 4 */
    OPc_CONDOP,	/* 5 */
    OPc_LISTOP,	/* 6 */
    OPc_PMOP,	/* 7 */
    OPc_SVOP,	/* 8 */
    OPc_GVOP,	/* 9 */
    OPc_PVOP,	/* 10 */
    OPc_CVOP,	/* 11 */
    OPc_LOOP,	/* 12 */
    OPc_COP	/* 13 */
} opclass;

static char *opclassnames[] = {
    "B::NULL",
    "B::OP",
    "B::UNOP",
    "B::BINOP",
    "B::LOGOP",
    "B::CONDOP",
    "B::LISTOP",
    "B::PMOP",
    "B::SVOP",
    "B::GVOP",
    "B::PVOP",
    "B::CVOP",
    "B::LOOP",
    "B::COP"	
};

static int walkoptree_debug = 0;	/* Flag for walkoptree debug hook */

static opclass
cc_opclass(OP *o)
{
    if (!o)
	return OPc_NULL;

    if (o->op_type == 0)
	return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;

    if (o->op_type == OP_SASSIGN)
	return ((o->op_private & OPpASSIGN_BACKWARDS) ? OPc_UNOP : OPc_BINOP);

    switch (opargs[o->op_type] & OA_CLASS_MASK) {
    case OA_BASEOP:
	return OPc_BASEOP;

    case OA_UNOP:
	return OPc_UNOP;

    case OA_BINOP:
	return OPc_BINOP;

    case OA_LOGOP:
	return OPc_LOGOP;

    case OA_CONDOP:
	return OPc_CONDOP;

    case OA_LISTOP:
	return OPc_LISTOP;

    case OA_PMOP:
	return OPc_PMOP;

    case OA_SVOP:
	return OPc_SVOP;

    case OA_GVOP:
	return OPc_GVOP;

    case OA_PVOP:
	return OPc_PVOP;

    case OA_LOOP:
	return OPc_LOOP;

    case OA_COP:
	return OPc_COP;

    case OA_BASEOP_OR_UNOP:
	/*
	 * UNI(OP_foo) in toke.c returns token UNI or FUNC1 depending on
	 * whether parens were seen. perly.y uses OPf_SPECIAL to
	 * signal whether a BASEOP had empty parens or none.
	 * Some other UNOPs are created later, though, so the best
	 * test is OPf_KIDS, which is set in newUNOP.
	 */
	return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;

    case OA_FILESTATOP:
	/*
	 * The file stat OPs are created via UNI(OP_foo) in toke.c but use
	 * the OPf_REF flag to distinguish between OP types instead of the
	 * usual OPf_SPECIAL flag. As usual, if OPf_KIDS is set, then we
	 * return OPc_UNOP so that walkoptree can find our children. If
	 * OPf_KIDS is not set then we check OPf_REF. Without OPf_REF set
	 * (no argument to the operator) it's an OP; with OPf_REF set it's
	 * a GVOP (and op_gv is the GV for the filehandle argument).
	 */
	return ((o->op_flags & OPf_KIDS) ? OPc_UNOP :
		(o->op_flags & OPf_REF) ? OPc_GVOP : OPc_BASEOP);

    case OA_LOOPEXOP:
	/*
	 * next, last, redo, dump and goto use OPf_SPECIAL to indicate that a
	 * label was omitted (in which case it's a BASEOP) or else a term was
	 * seen. In this last case, all except goto are definitely PVOP but
	 * goto is either a PVOP (with an ordinary constant label), an UNOP
	 * with OPf_STACKED (with a non-constant non-sub) or an UNOP for
	 * OP_REFGEN (with goto &sub) in which case OPf_STACKED also seems to
	 * get set.
	 */
	if (o->op_flags & OPf_STACKED)
	    return OPc_UNOP;
	else if (o->op_flags & OPf_SPECIAL)
	    return OPc_BASEOP;
	else
	    return OPc_PVOP;
    }
    warn("can't determine class of operator %s, assuming BASEOP\n",
	 op_name[o->op_type]);
    return OPc_BASEOP;
}

static char *
cc_opclassname(OP *o)
{
    return opclassnames[cc_opclass(o)];
}

static SV *
make_sv_object(SV *arg, SV *sv)
{
    char *type = 0;
    IV iv;
    
    for (iv = 0; iv < sizeof(PL_specialsv_list)/sizeof(SV*); iv++) {
	if (sv == PL_specialsv_list[iv]) {
	    type = "B::SPECIAL";
	    break;
	}
    }
    if (!type) {
	type = svclassnames[SvTYPE(sv)];
	iv = (IV)sv;
    }
    sv_setiv(newSVrv(arg, type), iv);
    return arg;
}

static SV *
make_mg_object(SV *arg, MAGIC *mg)
{
    sv_setiv(newSVrv(arg, "B::MAGIC"), (IV)mg);
    return arg;
}

static SV *
cstring(SV *sv)
{
    SV *sstr = newSVpv("", 0);
    STRLEN len;
    char *s;

    if (!SvOK(sv))
	sv_setpvn(sstr, "0", 1);
    else
    {
	/* XXX Optimise? */
	s = SvPV(sv, len);
	sv_catpv(sstr, "\"");
	for (; len; len--, s++)
	{
	    /* At least try a little for readability */
	    if (*s == '"')
		sv_catpv(sstr, "\\\"");
	    else if (*s == '\\')
		sv_catpv(sstr, "\\\\");
	    else if (*s >= ' ' && *s < 127) /* XXX not portable */
		sv_catpvn(sstr, s, 1);
	    else if (*s == '\n')
		sv_catpv(sstr, "\\n");
	    else if (*s == '\r')
		sv_catpv(sstr, "\\r");
	    else if (*s == '\t')
		sv_catpv(sstr, "\\t");
	    else if (*s == '\a')
		sv_catpv(sstr, "\\a");
	    else if (*s == '\b')
		sv_catpv(sstr, "\\b");
	    else if (*s == '\f')
		sv_catpv(sstr, "\\f");
	    else if (*s == '\v')
		sv_catpv(sstr, "\\v");
	    else
	    {
		/* no trigraph support */
		char escbuff[5]; /* to fit backslash, 3 octals + trailing \0 */
		/* Don't want promotion of a signed -1 char in sprintf args */
		unsigned char c = (unsigned char) *s;
		sprintf(escbuff, "\\%03o", c);
		sv_catpv(sstr, escbuff);
	    }
	    /* XXX Add line breaks if string is long */
	}
	sv_catpv(sstr, "\"");
    }
    return sstr;
}

static SV *
cchar(SV *sv)
{
    SV *sstr = newSVpv("'", 0);
    STRLEN n_a;
    char *s = SvPV(sv, n_a);

    if (*s == '\'')
	sv_catpv(sstr, "\\'");
    else if (*s == '\\')
	sv_catpv(sstr, "\\\\");
    else if (*s >= ' ' && *s < 127) /* XXX not portable */
	sv_catpvn(sstr, s, 1);
    else if (*s == '\n')
	sv_catpv(sstr, "\\n");
    else if (*s == '\r')
	sv_catpv(sstr, "\\r");
    else if (*s == '\t')
	sv_catpv(sstr, "\\t");
    else if (*s == '\a')
	sv_catpv(sstr, "\\a");
    else if (*s == '\b')
	sv_catpv(sstr, "\\b");
    else if (*s == '\f')
	sv_catpv(sstr, "\\f");
    else if (*s == '\v')
	sv_catpv(sstr, "\\v");
    else
    {
	/* no trigraph support */
	char escbuff[5]; /* to fit backslash, 3 octals + trailing \0 */
	/* Don't want promotion of a signed -1 char in sprintf args */
	unsigned char c = (unsigned char) *s;
	sprintf(escbuff, "\\%03o", c);
	sv_catpv(sstr, escbuff);
    }
    sv_catpv(sstr, "'");
    return sstr;
}

#ifdef INDIRECT_BGET_MACROS
void freadpv(U32 len, void *data)
{
    New(666, pv.xpv_pv, len, char);
    fread(pv.xpv_pv, 1, len, (FILE*)data);
    pv.xpv_len = len;
    pv.xpv_cur = len - 1;
}

void byteload_fh(InputStream fp)
{
    struct bytestream bs;
    bs.data = fp;
    bs.fgetc = (int(*) _((void*)))fgetc;
    bs.fread = (int(*) _((char*,size_t,size_t,void*)))fread;
    bs.freadpv = freadpv;
    byterun(bs);
}

static int fgetc_fromstring(void *data)
{
    char **strp = (char **)data;
    return *(*strp)++;
}

static int fread_fromstring(char *argp, size_t elemsize, size_t nelem,
			    void *data)
{
    char **strp = (char **)data;
    size_t len = elemsize * nelem;
    
    memcpy(argp, *strp, len);
    *strp += len;
    return (int)len;
}

static void freadpv_fromstring(U32 len, void *data)
{
    char **strp = (char **)data;
    
    New(666, pv.xpv_pv, len, char);
    memcpy(pv.xpv_pv, *strp, len);
    pv.xpv_len = len;
    pv.xpv_cur = len - 1;
    *strp += len;
}    

void byteload_string(char *str)
{
    struct bytestream bs;
    bs.data = &str;
    bs.fgetc = fgetc_fromstring;
    bs.fread = fread_fromstring;
    bs.freadpv = freadpv_fromstring;
    byterun(bs);
}
#else
void byteload_fh(InputStream fp)
{
    byterun(fp);
}

void byteload_string(char *str)
{
    croak("Must compile with -DINDIRECT_BGET_MACROS for byteload_string");
}    
#endif /* INDIRECT_BGET_MACROS */

void
walkoptree(SV *opsv, char *method)
{
    dSP;
    OP *o;
    
    if (!SvROK(opsv))
	croak("opsv is not a reference");
    opsv = sv_mortalcopy(opsv);
    o = (OP*)SvIV((SV*)SvRV(opsv));
    if (walkoptree_debug) {
	PUSHMARK(sp);
	XPUSHs(opsv);
	PUTBACK;
	perl_call_method("walkoptree_debug", G_DISCARD);
    }
    PUSHMARK(sp);
    XPUSHs(opsv);
    PUTBACK;
    perl_call_method(method, G_DISCARD);
    if (o && (o->op_flags & OPf_KIDS)) {
	OP *kid;
	for (kid = ((UNOP*)o)->op_first; kid; kid = kid->op_sibling) {
	    /* Use the same opsv. Rely on methods not to mess it up. */
	    sv_setiv(newSVrv(opsv, cc_opclassname(kid)), (IV)kid);
	    walkoptree(opsv, method);
	}
    }
}

typedef OP	*B__OP;
typedef UNOP	*B__UNOP;
typedef BINOP	*B__BINOP;
typedef LOGOP	*B__LOGOP;
typedef CONDOP	*B__CONDOP;
typedef LISTOP	*B__LISTOP;
typedef PMOP	*B__PMOP;
typedef SVOP	*B__SVOP;
typedef GVOP	*B__GVOP;
typedef PVOP	*B__PVOP;
typedef LOOP	*B__LOOP;
typedef COP	*B__COP;

typedef SV	*B__SV;
typedef SV	*B__IV;
typedef SV	*B__PV;
typedef SV	*B__NV;
typedef SV	*B__PVMG;
typedef SV	*B__PVLV;
typedef SV	*B__BM;
typedef SV	*B__RV;
typedef AV	*B__AV;
typedef HV	*B__HV;
typedef CV	*B__CV;
typedef GV	*B__GV;
typedef IO	*B__IO;

typedef MAGIC	*B__MAGIC;

MODULE = B	PACKAGE = B	PREFIX = B_

PROTOTYPES: DISABLE

BOOT:
    INIT_SPECIALSV_LIST;

#define B_main_cv()	PL_main_cv
#define B_init_av()	PL_initav
#define B_main_root()	PL_main_root
#define B_main_start()	PL_main_start
#define B_comppadlist()	(PL_main_cv ? CvPADLIST(PL_main_cv) : CvPADLIST(PL_compcv))
#define B_sv_undef()	&PL_sv_undef
#define B_sv_yes()	&PL_sv_yes
#define B_sv_no()	&PL_sv_no

B::AV
B_init_av()

B::CV
B_main_cv()

B::OP
B_main_root()

B::OP
B_main_start()

B::AV
B_comppadlist()

B::SV
B_sv_undef()

B::SV
B_sv_yes()

B::SV
B_sv_no()

MODULE = B	PACKAGE = B


void
walkoptree(opsv, method)
	SV *	opsv
	char *	method

int
walkoptree_debug(...)
    CODE:
	RETVAL = walkoptree_debug;
	if (items > 0 && SvTRUE(ST(1)))
	    walkoptree_debug = 1;
    OUTPUT:
	RETVAL

int
byteload_fh(fp)
	InputStream    fp
    CODE:
	byteload_fh(fp);
	RETVAL = 1;
    OUTPUT:
	RETVAL

void
byteload_string(str)
	char *	str

#define address(sv) (IV)sv

IV
address(sv)
	SV *	sv

B::SV
svref_2object(sv)
	SV *	sv
    CODE:
	if (!SvROK(sv))
	    croak("argument is not a reference");
	RETVAL = (SV*)SvRV(sv);
    OUTPUT:
	RETVAL

void
ppname(opnum)
	int	opnum
    CODE:
	ST(0) = sv_newmortal();
	if (opnum >= 0 && opnum < PL_maxo) {
	    sv_setpvn(ST(0), "pp_", 3);
	    sv_catpv(ST(0), op_name[opnum]);
	}

void
hash(sv)
	SV *	sv
    CODE:
	char *s;
	STRLEN len;
	U32 hash = 0;
	char hexhash[11]; /* must fit "0xffffffff" plus trailing \0 */
	s = SvPV(sv, len);
	while (len--)
	    hash = hash * 33 + *s++;
	sprintf(hexhash, "0x%x", hash);
	ST(0) = sv_2mortal(newSVpv(hexhash, 0));

#define cast_I32(foo) (I32)foo
IV
cast_I32(i)
	IV	i

void
minus_c()
    CODE:
	PL_minus_c = TRUE;

SV *
cstring(sv)
	SV *	sv

SV *
cchar(sv)
	SV *	sv

void
threadsv_names()
    PPCODE:
#ifdef USE_THREADS
	int i;
	STRLEN len = strlen(PL_threadsv_names);

	EXTEND(sp, len);
	for (i = 0; i < len; i++)
	    PUSHs(sv_2mortal(newSVpv(&PL_threadsv_names[i], 1)));
#endif


#define OP_next(o)	o->op_next
#define OP_sibling(o)	o->op_sibling
#define OP_desc(o)	op_desc[o->op_type]
#define OP_targ(o)	o->op_targ
#define OP_type(o)	o->op_type
#define OP_seq(o)	o->op_seq
#define OP_flags(o)	o->op_flags
#define OP_private(o)	o->op_private

MODULE = B	PACKAGE = B::OP		PREFIX = OP_

B::OP
OP_next(o)
	B::OP		o

B::OP
OP_sibling(o)
	B::OP		o

char *
OP_ppaddr(o)
	B::OP		o
    CODE:
	ST(0) = sv_newmortal();
	sv_setpvn(ST(0), "pp_", 3);
	sv_catpv(ST(0), op_name[o->op_type]);

char *
OP_desc(o)
	B::OP		o

U16
OP_targ(o)
	B::OP		o

U16
OP_type(o)
	B::OP		o

U16
OP_seq(o)
	B::OP		o

U8
OP_flags(o)
	B::OP		o

U8
OP_private(o)
	B::OP		o

#define UNOP_first(o)	o->op_first

MODULE = B	PACKAGE = B::UNOP		PREFIX = UNOP_

B::OP 
UNOP_first(o)
	B::UNOP	o

#define BINOP_last(o)	o->op_last

MODULE = B	PACKAGE = B::BINOP		PREFIX = BINOP_

B::OP
BINOP_last(o)
	B::BINOP	o

#define LOGOP_other(o)	o->op_other

MODULE = B	PACKAGE = B::LOGOP		PREFIX = LOGOP_

B::OP
LOGOP_other(o)
	B::LOGOP	o

#define CONDOP_true(o)	o->op_true
#define CONDOP_false(o)	o->op_false

MODULE = B	PACKAGE = B::CONDOP		PREFIX = CONDOP_

B::OP
CONDOP_true(o)
	B::CONDOP	o

B::OP
CONDOP_false(o)
	B::CONDOP	o

#define LISTOP_children(o)	o->op_children

MODULE = B	PACKAGE = B::LISTOP		PREFIX = LISTOP_

U32
LISTOP_children(o)
	B::LISTOP	o

#define PMOP_pmreplroot(o)	o->op_pmreplroot
#define PMOP_pmreplstart(o)	o->op_pmreplstart
#define PMOP_pmnext(o)		o->op_pmnext
#define PMOP_pmregexp(o)	o->op_pmregexp
#define PMOP_pmflags(o)		o->op_pmflags
#define PMOP_pmpermflags(o)	o->op_pmpermflags

MODULE = B	PACKAGE = B::PMOP		PREFIX = PMOP_

void
PMOP_pmreplroot(o)
	B::PMOP		o
	OP *		root = NO_INIT
    CODE:
	ST(0) = sv_newmortal();
	root = o->op_pmreplroot;
	/* OP_PUSHRE stores an SV* instead of an OP* in op_pmreplroot */
	if (o->op_type == OP_PUSHRE) {
	    sv_setiv(newSVrv(ST(0), root ?
			     svclassnames[SvTYPE((SV*)root)] : "B::SV"),
		     (IV)root);
	}
	else {
	    sv_setiv(newSVrv(ST(0), cc_opclassname(root)), (IV)root);
	}

B::OP
PMOP_pmreplstart(o)
	B::PMOP		o

B::PMOP
PMOP_pmnext(o)
	B::PMOP		o

U16
PMOP_pmflags(o)
	B::PMOP		o

U16
PMOP_pmpermflags(o)
	B::PMOP		o

void
PMOP_precomp(o)
	B::PMOP		o
	REGEXP *	rx = NO_INIT
    CODE:
	ST(0) = sv_newmortal();
	rx = o->op_pmregexp;
	if (rx)
	    sv_setpvn(ST(0), rx->precomp, rx->prelen);

#define SVOP_sv(o)	o->op_sv

MODULE = B	PACKAGE = B::SVOP		PREFIX = SVOP_


B::SV
SVOP_sv(o)
	B::SVOP	o

#define GVOP_gv(o)	o->op_gv

MODULE = B	PACKAGE = B::GVOP		PREFIX = GVOP_


B::GV
GVOP_gv(o)
	B::GVOP	o

MODULE = B	PACKAGE = B::PVOP		PREFIX = PVOP_

void
PVOP_pv(o)
	B::PVOP	o
    CODE:
	/*
	 * OP_TRANS uses op_pv to point to a table of 256 shorts
	 * whereas other PVOPs point to a null terminated string.
	 */
	ST(0) = sv_2mortal(newSVpv(o->op_pv, (o->op_type == OP_TRANS) ?
				   256 * sizeof(short) : 0));

#define LOOP_redoop(o)	o->op_redoop
#define LOOP_nextop(o)	o->op_nextop
#define LOOP_lastop(o)	o->op_lastop

MODULE = B	PACKAGE = B::LOOP		PREFIX = LOOP_


B::OP
LOOP_redoop(o)
	B::LOOP	o

B::OP
LOOP_nextop(o)
	B::LOOP	o

B::OP
LOOP_lastop(o)
	B::LOOP	o

#define COP_label(o)	o->cop_label
#define COP_stash(o)	o->cop_stash
#define COP_filegv(o)	o->cop_filegv
#define COP_cop_seq(o)	o->cop_seq
#define COP_arybase(o)	o->cop_arybase
#define COP_line(o)	o->cop_line

MODULE = B	PACKAGE = B::COP		PREFIX = COP_

char *
COP_label(o)
	B::COP	o

B::HV
COP_stash(o)
	B::COP	o

B::GV
COP_filegv(o)
	B::COP	o

U32
COP_cop_seq(o)
	B::COP	o

I32
COP_arybase(o)
	B::COP	o

U16
COP_line(o)
	B::COP	o

MODULE = B	PACKAGE = B::SV		PREFIX = Sv

U32
SvREFCNT(sv)
	B::SV	sv

U32
SvFLAGS(sv)
	B::SV	sv

MODULE = B	PACKAGE = B::IV		PREFIX = Sv

IV
SvIV(sv)
	B::IV	sv

IV
SvIVX(sv)
	B::IV	sv

MODULE = B	PACKAGE = B::IV

#define needs64bits(sv) ((I32)SvIVX(sv) != SvIVX(sv))

int
needs64bits(sv)
	B::IV	sv

void
packiv(sv)
	B::IV	sv
    CODE:
	if (sizeof(IV) == 8) {
	    U32 wp[2];
	    IV iv = SvIVX(sv);
	    /*
	     * The following way of spelling 32 is to stop compilers on
	     * 32-bit architectures from moaning about the shift count
	     * being >= the width of the type. Such architectures don't
	     * reach this code anyway (unless sizeof(IV) > 8 but then
	     * everything else breaks too so I'm not fussed at the moment).
	     */
	    wp[0] = htonl(((U32)iv) >> (sizeof(IV)*4));
	    wp[1] = htonl(iv & 0xffffffff);
	    ST(0) = sv_2mortal(newSVpv((char *)wp, 8));
	} else {
	    U32 w = htonl((U32)SvIVX(sv));
	    ST(0) = sv_2mortal(newSVpv((char *)&w, 4));
	}

MODULE = B	PACKAGE = B::NV		PREFIX = Sv

double
SvNV(sv)
	B::NV	sv

double
SvNVX(sv)
	B::NV	sv

MODULE = B	PACKAGE = B::RV		PREFIX = Sv

B::SV
SvRV(sv)
	B::RV	sv

MODULE = B	PACKAGE = B::PV		PREFIX = Sv

void
SvPV(sv)
	B::PV	sv
    CODE:
	ST(0) = sv_newmortal();
	sv_setpvn(ST(0), SvPVX(sv), SvCUR(sv));

MODULE = B	PACKAGE = B::PVMG	PREFIX = Sv

void
SvMAGIC(sv)
	B::PVMG	sv
	MAGIC *	mg = NO_INIT
    PPCODE:
	for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic)
	    XPUSHs(make_mg_object(sv_newmortal(), mg));

MODULE = B	PACKAGE = B::PVMG

B::HV
SvSTASH(sv)
	B::PVMG	sv

#define MgMOREMAGIC(mg) mg->mg_moremagic
#define MgPRIVATE(mg) mg->mg_private
#define MgTYPE(mg) mg->mg_type
#define MgFLAGS(mg) mg->mg_flags
#define MgOBJ(mg) mg->mg_obj

MODULE = B	PACKAGE = B::MAGIC	PREFIX = Mg	

B::MAGIC
MgMOREMAGIC(mg)
	B::MAGIC	mg

U16
MgPRIVATE(mg)
	B::MAGIC	mg

char
MgTYPE(mg)
	B::MAGIC	mg

U8
MgFLAGS(mg)
	B::MAGIC	mg

B::SV
MgOBJ(mg)
	B::MAGIC	mg

void
MgPTR(mg)
	B::MAGIC	mg
    CODE:
	ST(0) = sv_newmortal();
	if (mg->mg_ptr)
	    sv_setpvn(ST(0), mg->mg_ptr, mg->mg_len);

MODULE = B	PACKAGE = B::PVLV	PREFIX = Lv

U32
LvTARGOFF(sv)
	B::PVLV	sv

U32
LvTARGLEN(sv)
	B::PVLV	sv

char
LvTYPE(sv)
	B::PVLV	sv

B::SV
LvTARG(sv)
	B::PVLV sv

MODULE = B	PACKAGE = B::BM		PREFIX = Bm

I32
BmUSEFUL(sv)
	B::BM	sv

U16
BmPREVIOUS(sv)
	B::BM	sv

U8
BmRARE(sv)
	B::BM	sv

void
BmTABLE(sv)
	B::BM	sv
	STRLEN	len = NO_INIT
	char *	str = NO_INIT
    CODE:
	str = SvPV(sv, len);
	/* Boyer-Moore table is just after string and its safety-margin \0 */
	ST(0) = sv_2mortal(newSVpv(str + len + 1, 256));

MODULE = B	PACKAGE = B::GV		PREFIX = Gv

void
GvNAME(gv)
	B::GV	gv
    CODE:
	ST(0) = sv_2mortal(newSVpv(GvNAME(gv), GvNAMELEN(gv)));

B::HV
GvSTASH(gv)
	B::GV	gv

B::SV
GvSV(gv)
	B::GV	gv

B::IO
GvIO(gv)
	B::GV	gv

B::CV
GvFORM(gv)
	B::GV	gv

B::AV
GvAV(gv)
	B::GV	gv

B::HV
GvHV(gv)
	B::GV	gv

B::GV
GvEGV(gv)
	B::GV	gv

B::CV
GvCV(gv)
	B::GV	gv

U32
GvCVGEN(gv)
	B::GV	gv

U16
GvLINE(gv)
	B::GV	gv

B::GV
GvFILEGV(gv)
	B::GV	gv

MODULE = B	PACKAGE = B::GV

U32
GvREFCNT(gv)
	B::GV	gv

U8
GvFLAGS(gv)
	B::GV	gv

MODULE = B	PACKAGE = B::IO		PREFIX = Io

long
IoLINES(io)
	B::IO	io

long
IoPAGE(io)
	B::IO	io

long
IoPAGE_LEN(io)
	B::IO	io

long
IoLINES_LEFT(io)
	B::IO	io

char *
IoTOP_NAME(io)
	B::IO	io

B::GV
IoTOP_GV(io)
	B::IO	io

char *
IoFMT_NAME(io)
	B::IO	io

B::GV
IoFMT_GV(io)
	B::IO	io

char *
IoBOTTOM_NAME(io)
	B::IO	io

B::GV
IoBOTTOM_GV(io)
	B::IO	io

short
IoSUBPROCESS(io)
	B::IO	io

MODULE = B	PACKAGE = B::IO

char
IoTYPE(io)
	B::IO	io

U8
IoFLAGS(io)
	B::IO	io

MODULE = B	PACKAGE = B::AV		PREFIX = Av

SSize_t
AvFILL(av)
	B::AV	av

SSize_t
AvMAX(av)
	B::AV	av

#define AvOFF(av) ((XPVAV*)SvANY(av))->xof_off

IV
AvOFF(av)
	B::AV	av

void
AvARRAY(av)
	B::AV	av
    PPCODE:
	if (AvFILL(av) >= 0) {
	    SV **svp = AvARRAY(av);
	    I32 i;
	    for (i = 0; i <= AvFILL(av); i++)
		XPUSHs(make_sv_object(sv_newmortal(), svp[i]));
	}

MODULE = B	PACKAGE = B::AV

U8
AvFLAGS(av)
	B::AV	av

MODULE = B	PACKAGE = B::CV		PREFIX = Cv

B::HV
CvSTASH(cv)
	B::CV	cv

B::OP
CvSTART(cv)
	B::CV	cv

B::OP
CvROOT(cv)
	B::CV	cv

B::GV
CvGV(cv)
	B::CV	cv

B::GV
CvFILEGV(cv)
	B::CV	cv

long
CvDEPTH(cv)
	B::CV	cv

B::AV
CvPADLIST(cv)
	B::CV	cv

B::CV
CvOUTSIDE(cv)
	B::CV	cv

void
CvXSUB(cv)
	B::CV	cv
    CODE:
	ST(0) = sv_2mortal(newSViv((IV)CvXSUB(cv)));


void
CvXSUBANY(cv)
	B::CV	cv
    CODE:
	ST(0) = sv_2mortal(newSViv(CvXSUBANY(cv).any_iv));

MODULE = B    PACKAGE = B::CV

U8
CvFLAGS(cv)
      B::CV   cv


MODULE = B	PACKAGE = B::HV		PREFIX = Hv

STRLEN
HvFILL(hv)
	B::HV	hv

STRLEN
HvMAX(hv)
	B::HV	hv

I32
HvKEYS(hv)
	B::HV	hv

I32
HvRITER(hv)
	B::HV	hv

char *
HvNAME(hv)
	B::HV	hv

B::PMOP
HvPMROOT(hv)
	B::HV	hv

void
HvARRAY(hv)
	B::HV	hv
    PPCODE:
	if (HvKEYS(hv) > 0) {
	    SV *sv;
	    char *key;
	    I32 len;
	    (void)hv_iterinit(hv);
	    EXTEND(sp, HvKEYS(hv) * 2);
	    while (sv = hv_iternextsv(hv, &key, &len)) {
		PUSHs(newSVpv(key, len));
		PUSHs(make_sv_object(sv_newmortal(), sv));
	    }
	}
