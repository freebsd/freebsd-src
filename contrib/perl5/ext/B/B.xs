/*	B.xs
 *
 *	Copyright (c) 1996 Malcolm Beattie
 *
 *	You may distribute under the terms of either the GNU General Public
 *	License or the Artistic License, as specified in the README file.
 *
 */

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef PERL_OBJECT
#undef PL_op_name
#undef PL_opargs 
#undef PL_op_desc
#define PL_op_name (get_op_names())
#define PL_opargs (get_opargs())
#define PL_op_desc (get_op_descs())
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
    OPc_LISTOP,	/* 5 */
    OPc_PMOP,	/* 6 */
    OPc_SVOP,	/* 7 */
    OPc_PADOP,	/* 8 */
    OPc_PVOP,	/* 9 */
    OPc_CVOP,	/* 10 */
    OPc_LOOP,	/* 11 */
    OPc_COP	/* 12 */
} opclass;

static char *opclassnames[] = {
    "B::NULL",
    "B::OP",
    "B::UNOP",
    "B::BINOP",
    "B::LOGOP",
    "B::LISTOP",
    "B::PMOP",
    "B::SVOP",
    "B::PADOP",
    "B::PVOP",
    "B::CVOP",
    "B::LOOP",
    "B::COP"	
};

static int walkoptree_debug = 0;	/* Flag for walkoptree debug hook */

static SV *specialsv_list[6];

static opclass
cc_opclass(pTHX_ OP *o)
{
    if (!o)
	return OPc_NULL;

    if (o->op_type == 0)
	return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;

    if (o->op_type == OP_SASSIGN)
	return ((o->op_private & OPpASSIGN_BACKWARDS) ? OPc_UNOP : OPc_BINOP);

#ifdef USE_ITHREADS
    if (o->op_type == OP_GV || o->op_type == OP_GVSV || o->op_type == OP_AELEMFAST)
	return OPc_PADOP;
#endif

    switch (PL_opargs[o->op_type] & OA_CLASS_MASK) {
    case OA_BASEOP:
	return OPc_BASEOP;

    case OA_UNOP:
	return OPc_UNOP;

    case OA_BINOP:
	return OPc_BINOP;

    case OA_LOGOP:
	return OPc_LOGOP;

    case OA_LISTOP:
	return OPc_LISTOP;

    case OA_PMOP:
	return OPc_PMOP;

    case OA_SVOP:
	return OPc_SVOP;

    case OA_PADOP:
	return OPc_PADOP;

    case OA_PVOP_OR_SVOP:
        /*
         * Character translations (tr///) are usually a PVOP, keeping a 
         * pointer to a table of shorts used to look up translations.
         * Under utf8, however, a simple table isn't practical; instead,
         * the OP is an SVOP, and the SV is a reference to a swash
         * (i.e., an RV pointing to an HV).
         */
	return (o->op_private & (OPpTRANS_TO_UTF|OPpTRANS_FROM_UTF))
		? OPc_SVOP : OPc_PVOP;

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
	 * an SVOP (and op_sv is the GV for the filehandle argument).
	 */
	return ((o->op_flags & OPf_KIDS) ? OPc_UNOP :
#ifdef USE_ITHREADS
		(o->op_flags & OPf_REF) ? OPc_PADOP : OPc_BASEOP);
#else
		(o->op_flags & OPf_REF) ? OPc_SVOP : OPc_BASEOP);
#endif
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
	 PL_op_name[o->op_type]);
    return OPc_BASEOP;
}

static char *
cc_opclassname(pTHX_ OP *o)
{
    return opclassnames[cc_opclass(aTHX_ o)];
}

static SV *
make_sv_object(pTHX_ SV *arg, SV *sv)
{
    char *type = 0;
    IV iv;
    
    for (iv = 0; iv < sizeof(specialsv_list)/sizeof(SV*); iv++) {
	if (sv == specialsv_list[iv]) {
	    type = "B::SPECIAL";
	    break;
	}
    }
    if (!type) {
	type = svclassnames[SvTYPE(sv)];
	iv = PTR2IV(sv);
    }
    sv_setiv(newSVrv(arg, type), iv);
    return arg;
}

static SV *
make_mg_object(pTHX_ SV *arg, MAGIC *mg)
{
    sv_setiv(newSVrv(arg, "B::MAGIC"), PTR2IV(mg));
    return arg;
}

static SV *
cstring(pTHX_ SV *sv)
{
    SV *sstr = newSVpvn("", 0);
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
cchar(pTHX_ SV *sv)
{
    SV *sstr = newSVpvn("'", 1);
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

void
walkoptree(pTHX_ SV *opsv, char *method)
{
    dSP;
    OP *o;
    
    if (!SvROK(opsv))
	croak("opsv is not a reference");
    opsv = sv_mortalcopy(opsv);
    o = INT2PTR(OP*,SvIV((SV*)SvRV(opsv)));
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
	    sv_setiv(newSVrv(opsv, cc_opclassname(aTHX_ kid)), PTR2IV(kid));
	    walkoptree(aTHX_ opsv, method);
	}
    }
}

typedef OP	*B__OP;
typedef UNOP	*B__UNOP;
typedef BINOP	*B__BINOP;
typedef LOGOP	*B__LOGOP;
typedef LISTOP	*B__LISTOP;
typedef PMOP	*B__PMOP;
typedef SVOP	*B__SVOP;
typedef PADOP	*B__PADOP;
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
{
    HV *stash = gv_stashpvn("B", 1, TRUE);
    AV *export_ok = perl_get_av("B::EXPORT_OK",TRUE);
    specialsv_list[0] = Nullsv;
    specialsv_list[1] = &PL_sv_undef;
    specialsv_list[2] = &PL_sv_yes;
    specialsv_list[3] = &PL_sv_no;
    specialsv_list[4] = pWARN_ALL;
    specialsv_list[5] = pWARN_NONE;
#include "defsubs.h"
}

#define B_main_cv()	PL_main_cv
#define B_init_av()	PL_initav
#define B_begin_av()	PL_beginav_save
#define B_end_av()	PL_endav
#define B_main_root()	PL_main_root
#define B_main_start()	PL_main_start
#define B_amagic_generation()	PL_amagic_generation
#define B_comppadlist()	(PL_main_cv ? CvPADLIST(PL_main_cv) : CvPADLIST(PL_compcv))
#define B_sv_undef()	&PL_sv_undef
#define B_sv_yes()	&PL_sv_yes
#define B_sv_no()	&PL_sv_no

B::AV
B_init_av()

B::AV
B_begin_av()

B::AV
B_end_av()

B::CV
B_main_cv()

B::OP
B_main_root()

B::OP
B_main_start()

long 
B_amagic_generation()

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
    CODE:
	walkoptree(aTHX_ opsv, method);

int
walkoptree_debug(...)
    CODE:
	RETVAL = walkoptree_debug;
	if (items > 0 && SvTRUE(ST(1)))
	    walkoptree_debug = 1;
    OUTPUT:
	RETVAL

#define address(sv) PTR2IV(sv)

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
opnumber(name)
char *	name
CODE:
{
 int i; 
 IV  result = -1;
 ST(0) = sv_newmortal();
 if (strncmp(name,"pp_",3) == 0)
   name += 3;
 for (i = 0; i < PL_maxo; i++)
  {
   if (strcmp(name, PL_op_name[i]) == 0)
    {
     result = i;
     break;
    }
  }
 sv_setiv(ST(0),result);
}

void
ppname(opnum)
	int	opnum
    CODE:
	ST(0) = sv_newmortal();
	if (opnum >= 0 && opnum < PL_maxo) {
	    sv_setpvn(ST(0), "pp_", 3);
	    sv_catpv(ST(0), PL_op_name[opnum]);
	}

void
hash(sv)
	SV *	sv
    CODE:
	char *s;
	STRLEN len;
	U32 hash = 0;
	char hexhash[19]; /* must fit "0xffffffffffffffff" plus trailing \0 */
	s = SvPV(sv, len);
	PERL_HASH(hash, s, len);
	sprintf(hexhash, "0x%"UVxf, (UV)hash);
	ST(0) = sv_2mortal(newSVpv(hexhash, 0));

#define cast_I32(foo) (I32)foo
IV
cast_I32(i)
	IV	i

void
minus_c()
    CODE:
	PL_minus_c = TRUE;

void
save_BEGINs()
    CODE:
	PL_minus_c |= 0x10;

SV *
cstring(sv)
	SV *	sv
    CODE:
	RETVAL = cstring(aTHX_ sv);
    OUTPUT:
	RETVAL

SV *
cchar(sv)
	SV *	sv
    CODE:
	RETVAL = cchar(aTHX_ sv);
    OUTPUT:
	RETVAL

void
threadsv_names()
    PPCODE:
#ifdef USE_THREADS
	int i;
	STRLEN len = strlen(PL_threadsv_names);

	EXTEND(sp, len);
	for (i = 0; i < len; i++)
	    PUSHs(sv_2mortal(newSVpvn(&PL_threadsv_names[i], 1)));
#endif


#define OP_next(o)	o->op_next
#define OP_sibling(o)	o->op_sibling
#define OP_desc(o)	PL_op_desc[o->op_type]
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
OP_name(o)
	B::OP		o
    CODE:
	RETVAL = PL_op_name[o->op_type];
    OUTPUT:
	RETVAL


void
OP_ppaddr(o)
	B::OP		o
    PREINIT:
	int i;
	SV *sv = sv_newmortal();
    CODE:
	sv_setpvn(sv, "PL_ppaddr[OP_", 13);
	sv_catpv(sv, PL_op_name[o->op_type]);
	for (i=13; i<SvCUR(sv); ++i)
	    SvPVX(sv)[i] = toUPPER(SvPVX(sv)[i]);
	sv_catpv(sv, "]");
	ST(0) = sv;

char *
OP_desc(o)
	B::OP		o

PADOFFSET
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

MODULE = B	PACKAGE = B::LISTOP		PREFIX = LISTOP_

U32
LISTOP_children(o)
	B::LISTOP	o
	OP *		kid = NO_INIT
	int		i = NO_INIT
    CODE:
	i = 0;
	for (kid = o->op_first; kid; kid = kid->op_sibling)
	    i++;
	RETVAL = i;
    OUTPUT:
        RETVAL

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
		     PTR2IV(root));
	}
	else {
	    sv_setiv(newSVrv(ST(0), cc_opclassname(aTHX_ root)), PTR2IV(root));
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

#define SVOP_sv(o)     cSVOPo->op_sv
#define SVOP_gv(o)     ((GV*)cSVOPo->op_sv)

MODULE = B	PACKAGE = B::SVOP		PREFIX = SVOP_

B::SV
SVOP_sv(o)
	B::SVOP	o

B::GV
SVOP_gv(o)
	B::SVOP	o

#define PADOP_padix(o)	o->op_padix
#define PADOP_sv(o)	(o->op_padix ? PL_curpad[o->op_padix] : Nullsv)
#define PADOP_gv(o)	((o->op_padix \
			  && SvTYPE(PL_curpad[o->op_padix]) == SVt_PVGV) \
			 ? (GV*)PL_curpad[o->op_padix] : Nullgv)

MODULE = B	PACKAGE = B::PADOP		PREFIX = PADOP_

PADOFFSET
PADOP_padix(o)
	B::PADOP o

B::SV
PADOP_sv(o)
	B::PADOP o

B::GV
PADOP_gv(o)
	B::PADOP o

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
#define COP_stashpv(o)	CopSTASHPV(o)
#define COP_stash(o)	CopSTASH(o)
#define COP_file(o)	CopFILE(o)
#define COP_cop_seq(o)	o->cop_seq
#define COP_arybase(o)	o->cop_arybase
#define COP_line(o)	CopLINE(o)
#define COP_warnings(o)	o->cop_warnings

MODULE = B	PACKAGE = B::COP		PREFIX = COP_

char *
COP_label(o)
	B::COP	o

char *
COP_stashpv(o)
	B::COP	o

B::HV
COP_stash(o)
	B::COP	o

char *
COP_file(o)
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

B::SV
COP_warnings(o)
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

UV 
SvUVX(sv) 
	B::IV   sv
                      

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
#ifdef UV_IS_QUAD
	    wp[0] = htonl(((UV)iv) >> (sizeof(UV)*4));
#else
	    wp[0] = htonl(((U32)iv) >> (sizeof(UV)*4));
#endif
	    wp[1] = htonl(iv & 0xffffffff);
	    ST(0) = sv_2mortal(newSVpvn((char *)wp, 8));
	} else {
	    U32 w = htonl((U32)SvIVX(sv));
	    ST(0) = sv_2mortal(newSVpvn((char *)&w, 4));
	}

MODULE = B	PACKAGE = B::NV		PREFIX = Sv

NV
SvNV(sv)
	B::NV	sv

NV
SvNVX(sv)
	B::NV	sv

MODULE = B	PACKAGE = B::RV		PREFIX = Sv

B::SV
SvRV(sv)
	B::RV	sv

MODULE = B	PACKAGE = B::PV		PREFIX = Sv

char*
SvPVX(sv)
	B::PV	sv

void
SvPV(sv)
	B::PV	sv
    CODE:
	ST(0) = sv_newmortal();
	sv_setpvn(ST(0), SvPVX(sv), SvCUR(sv));

STRLEN
SvLEN(sv)
	B::PV	sv

STRLEN
SvCUR(sv)
	B::PV	sv

MODULE = B	PACKAGE = B::PVMG	PREFIX = Sv

void
SvMAGIC(sv)
	B::PVMG	sv
	MAGIC *	mg = NO_INIT
    PPCODE:
	for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic)
	    XPUSHs(make_mg_object(aTHX_ sv_newmortal(), mg));

MODULE = B	PACKAGE = B::PVMG

B::HV
SvSTASH(sv)
	B::PVMG	sv

#define MgMOREMAGIC(mg) mg->mg_moremagic
#define MgPRIVATE(mg) mg->mg_private
#define MgTYPE(mg) mg->mg_type
#define MgFLAGS(mg) mg->mg_flags
#define MgOBJ(mg) mg->mg_obj
#define MgLENGTH(mg) mg->mg_len

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

I32 
MgLENGTH(mg)
	B::MAGIC	mg
 
void
MgPTR(mg)
	B::MAGIC	mg
    CODE:
	ST(0) = sv_newmortal();
 	if (mg->mg_ptr){
		if (mg->mg_len >= 0){
	    		sv_setpvn(ST(0), mg->mg_ptr, mg->mg_len);
		} else {
			if (mg->mg_len == HEf_SVKEY)	
				sv_setsv(ST(0),newRV((SV*)mg->mg_ptr));
		}
	}

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
	ST(0) = sv_2mortal(newSVpvn(str + len + 1, 256));

MODULE = B	PACKAGE = B::GV		PREFIX = Gv

void
GvNAME(gv)
	B::GV	gv
    CODE:
	ST(0) = sv_2mortal(newSVpvn(GvNAME(gv), GvNAMELEN(gv)));

bool
is_empty(gv)
        B::GV   gv
    CODE:
        RETVAL = GvGP(gv) == Null(GP*);
    OUTPUT:
        RETVAL

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

char *
GvFILE(gv)
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
		XPUSHs(make_sv_object(aTHX_ sv_newmortal(), svp[i]));
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

char *
CvFILE(cv)
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
	ST(0) = sv_2mortal(newSViv(PTR2IV(CvXSUB(cv))));


void
CvXSUBANY(cv)
	B::CV	cv
    CODE:
	ST(0) = sv_2mortal(newSViv(CvXSUBANY(cv).any_iv));

MODULE = B    PACKAGE = B::CV

U16
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
	    while ((sv = hv_iternextsv(hv, &key, &len))) {
		PUSHs(newSVpvn(key, len));
		PUSHs(make_sv_object(aTHX_ sv_newmortal(), sv));
	    }
	}
