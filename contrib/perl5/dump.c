/*    dump.c
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "'You have talked long in your sleep, Frodo,' said Gandalf gently, 'and
 * it has not been hard for me to read your mind and memory.'"
 */

#include "EXTERN.h"
#include "perl.h"

#ifndef PERL_OBJECT
static void dump(char *pat, ...);
#endif /* PERL_OBJECT */

void
dump_all(void)
{
#ifdef DEBUGGING
    dTHR;
    PerlIO_setlinebuf(Perl_debug_log);
    if (PL_main_root)
	dump_op(PL_main_root);
    dump_packsubs(PL_defstash);
#endif	/* DEBUGGING */
}

void
dump_packsubs(HV *stash)
{
#ifdef DEBUGGING
    dTHR;
    I32	i;
    HE	*entry;

    if (!HvARRAY(stash))
	return;
    for (i = 0; i <= (I32) HvMAX(stash); i++) {
	for (entry = HvARRAY(stash)[i]; entry; entry = HeNEXT(entry)) {
	    GV *gv = (GV*)HeVAL(entry);
	    HV *hv;
	    if (SvTYPE(gv) != SVt_PVGV || !GvGP(gv))
		continue;
	    if (GvCVu(gv))
		dump_sub(gv);
	    if (GvFORM(gv))
		dump_form(gv);
	    if (HeKEY(entry)[HeKLEN(entry)-1] == ':' &&
	      (hv = GvHV(gv)) && HvNAME(hv) && hv != PL_defstash)
		dump_packsubs(hv);		/* nested package */
	}
    }
#endif	/* DEBUGGING */
}

void
dump_sub(GV *gv)
{
#ifdef DEBUGGING
    SV *sv = sv_newmortal();

    gv_fullname3(sv, gv, Nullch);
    dump("\nSUB %s = ", SvPVX(sv));
    if (CvXSUB(GvCV(gv)))
	dump("(xsub 0x%x %d)\n",
	    (long)CvXSUB(GvCV(gv)),
	    CvXSUBANY(GvCV(gv)).any_i32);
    else if (CvROOT(GvCV(gv)))
	dump_op(CvROOT(GvCV(gv)));
    else
	dump("<undef>\n");
#endif	/* DEBUGGING */
}

void
dump_form(GV *gv)
{
#ifdef DEBUGGING
    SV *sv = sv_newmortal();

    gv_fullname3(sv, gv, Nullch);
    dump("\nFORMAT %s = ", SvPVX(sv));
    if (CvROOT(GvFORM(gv)))
	dump_op(CvROOT(GvFORM(gv)));
    else
	dump("<undef>\n");
#endif	/* DEBUGGING */
}

void
dump_eval(void)
{
#ifdef DEBUGGING
    dump_op(PL_eval_root);
#endif	/* DEBUGGING */
}

void
dump_op(OP *o)
{
#ifdef DEBUGGING
    dump("{\n");
    if (o->op_seq)
	PerlIO_printf(Perl_debug_log, "%-4d", o->op_seq);
    else
	PerlIO_printf(Perl_debug_log, "    ");
    dump("TYPE = %s  ===> ", op_name[o->op_type]);
    if (o->op_next) {
	if (o->op_seq)
	    PerlIO_printf(Perl_debug_log, "%d\n", o->op_next->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "(%d)\n", o->op_next->op_seq);
    }
    else
	PerlIO_printf(Perl_debug_log, "DONE\n");
    PL_dumplvl++;
    if (o->op_targ) {
	if (o->op_type == OP_NULL)
	    dump("  (was %s)\n", op_name[o->op_targ]);
	else
	    dump("TARG = %d\n", o->op_targ);
    }
#ifdef DUMPADDR
    dump("ADDR = 0x%lx => 0x%lx\n",o, o->op_next);
#endif
    if (o->op_flags) {
	SV *tmpsv = newSVpv("", 0);
	switch (o->op_flags & OPf_WANT) {
	case OPf_WANT_VOID:
	    sv_catpv(tmpsv, ",VOID");
	    break;
	case OPf_WANT_SCALAR:
	    sv_catpv(tmpsv, ",SCALAR");
	    break;
	case OPf_WANT_LIST:
	    sv_catpv(tmpsv, ",LIST");
	    break;
	default:
	    sv_catpv(tmpsv, ",UNKNOWN");
	    break;
	}
	if (o->op_flags & OPf_KIDS)
	    sv_catpv(tmpsv, ",KIDS");
	if (o->op_flags & OPf_PARENS)
	    sv_catpv(tmpsv, ",PARENS");
	if (o->op_flags & OPf_STACKED)
	    sv_catpv(tmpsv, ",STACKED");
	if (o->op_flags & OPf_REF)
	    sv_catpv(tmpsv, ",REF");
	if (o->op_flags & OPf_MOD)
	    sv_catpv(tmpsv, ",MOD");
	if (o->op_flags & OPf_SPECIAL)
	    sv_catpv(tmpsv, ",SPECIAL");
	dump("FLAGS = (%s)\n", SvCUR(tmpsv) ? SvPVX(tmpsv) + 1 : "");
	SvREFCNT_dec(tmpsv);
    }
    if (o->op_private) {
	SV *tmpsv = newSVpv("", 0);
	if (o->op_type == OP_AASSIGN) {
	    if (o->op_private & OPpASSIGN_COMMON)
		sv_catpv(tmpsv, ",COMMON");
	}
	else if (o->op_type == OP_SASSIGN) {
	    if (o->op_private & OPpASSIGN_BACKWARDS)
		sv_catpv(tmpsv, ",BACKWARDS");
	}
	else if (o->op_type == OP_TRANS) {
	    if (o->op_private & OPpTRANS_SQUASH)
		sv_catpv(tmpsv, ",SQUASH");
	    if (o->op_private & OPpTRANS_DELETE)
		sv_catpv(tmpsv, ",DELETE");
	    if (o->op_private & OPpTRANS_COMPLEMENT)
		sv_catpv(tmpsv, ",COMPLEMENT");
	}
	else if (o->op_type == OP_REPEAT) {
	    if (o->op_private & OPpREPEAT_DOLIST)
		sv_catpv(tmpsv, ",DOLIST");
	}
	else if (o->op_type == OP_ENTERSUB ||
		 o->op_type == OP_RV2SV ||
		 o->op_type == OP_RV2AV ||
		 o->op_type == OP_RV2HV ||
		 o->op_type == OP_RV2GV ||
		 o->op_type == OP_AELEM ||
		 o->op_type == OP_HELEM )
	{
	    if (o->op_type == OP_ENTERSUB) {
		if (o->op_private & OPpENTERSUB_AMPER)
		    sv_catpv(tmpsv, ",AMPER");
		if (o->op_private & OPpENTERSUB_DB)
		    sv_catpv(tmpsv, ",DB");
	    }
	    switch (o->op_private & OPpDEREF) {
	    case OPpDEREF_SV:
		sv_catpv(tmpsv, ",SV");
		break;
	    case OPpDEREF_AV:
		sv_catpv(tmpsv, ",AV");
		break;
	    case OPpDEREF_HV:
		sv_catpv(tmpsv, ",HV");
		break;
	    }
	    if (o->op_type == OP_AELEM || o->op_type == OP_HELEM) {
		if (o->op_private & OPpLVAL_DEFER)
		    sv_catpv(tmpsv, ",LVAL_DEFER");
	    }
	    else {
		if (o->op_private & HINT_STRICT_REFS)
		    sv_catpv(tmpsv, ",STRICT_REFS");
	    }
	}
	else if (o->op_type == OP_CONST) {
	    if (o->op_private & OPpCONST_BARE)
		sv_catpv(tmpsv, ",BARE");
	}
	else if (o->op_type == OP_FLIP) {
	    if (o->op_private & OPpFLIP_LINENUM)
		sv_catpv(tmpsv, ",LINENUM");
	}
	else if (o->op_type == OP_FLOP) {
	    if (o->op_private & OPpFLIP_LINENUM)
		sv_catpv(tmpsv, ",LINENUM");
	}
	if (o->op_flags & OPf_MOD && o->op_private & OPpLVAL_INTRO)
	    sv_catpv(tmpsv, ",INTRO");
	if (SvCUR(tmpsv))
	    dump("PRIVATE = (%s)\n", SvPVX(tmpsv) + 1);
	SvREFCNT_dec(tmpsv);
    }

    switch (o->op_type) {
    case OP_GVSV:
    case OP_GV:
	if (cGVOPo->op_gv) {
	    STRLEN n_a;
	    SV *tmpsv = NEWSV(0,0);
	    ENTER;
	    SAVEFREESV(tmpsv);
	    gv_fullname3(tmpsv, cGVOPo->op_gv, Nullch);
	    dump("GV = %s\n", SvPV(tmpsv, n_a));
	    LEAVE;
	}
	else
	    dump("GV = NULL\n");
	break;
    case OP_CONST:
	dump("SV = %s\n", SvPEEK(cSVOPo->op_sv));
	break;
    case OP_NEXTSTATE:
    case OP_DBSTATE:
	if (cCOPo->cop_line)
	    dump("LINE = %d\n",cCOPo->cop_line);
	if (cCOPo->cop_label)
	    dump("LABEL = \"%s\"\n",cCOPo->cop_label);
	break;
    case OP_ENTERLOOP:
	dump("REDO ===> ");
	if (cLOOPo->op_redoop)
	    PerlIO_printf(Perl_debug_log, "%d\n", cLOOPo->op_redoop->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	dump("NEXT ===> ");
	if (cLOOPo->op_nextop)
	    PerlIO_printf(Perl_debug_log, "%d\n", cLOOPo->op_nextop->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	dump("LAST ===> ");
	if (cLOOPo->op_lastop)
	    PerlIO_printf(Perl_debug_log, "%d\n", cLOOPo->op_lastop->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	break;
    case OP_COND_EXPR:
	dump("TRUE ===> ");
	if (cCONDOPo->op_true)
	    PerlIO_printf(Perl_debug_log, "%d\n", cCONDOPo->op_true->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	dump("FALSE ===> ");
	if (cCONDOPo->op_false)
	    PerlIO_printf(Perl_debug_log, "%d\n", cCONDOPo->op_false->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	break;
    case OP_MAPWHILE:
    case OP_GREPWHILE:
    case OP_OR:
    case OP_AND:
	dump("OTHER ===> ");
	if (cLOGOPo->op_other)
	    PerlIO_printf(Perl_debug_log, "%d\n", cLOGOPo->op_other->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	break;
    case OP_PUSHRE:
    case OP_MATCH:
    case OP_QR:
    case OP_SUBST:
	dump_pm(cPMOPo);
	break;
    default:
	break;
    }
    if (o->op_flags & OPf_KIDS) {
	OP *kid;
	for (kid = cUNOPo->op_first; kid; kid = kid->op_sibling)
	    dump_op(kid);
    }
    PL_dumplvl--;
    dump("}\n");
#endif	/* DEBUGGING */
}

void
dump_gv(GV *gv)
{
#ifdef DEBUGGING
    SV *sv;

    if (!gv) {
	PerlIO_printf(Perl_debug_log, "{}\n");
	return;
    }
    sv = sv_newmortal();
    PL_dumplvl++;
    PerlIO_printf(Perl_debug_log, "{\n");
    gv_fullname3(sv, gv, Nullch);
    dump("GV_NAME = %s", SvPVX(sv));
    if (gv != GvEGV(gv)) {
	gv_efullname3(sv, GvEGV(gv), Nullch);
	dump("-> %s", SvPVX(sv));
    }
    dump("\n");
    PL_dumplvl--;
    dump("}\n");
#endif	/* DEBUGGING */
}

void
dump_pm(PMOP *pm)
{
#ifdef DEBUGGING
    char ch;

    if (!pm) {
	dump("{}\n");
	return;
    }
    dump("{\n");
    PL_dumplvl++;
    if (pm->op_pmflags & PMf_ONCE)
	ch = '?';
    else
	ch = '/';
    if (pm->op_pmregexp)
	dump("PMf_PRE %c%s%c%s\n",
	     ch, pm->op_pmregexp->precomp, ch,
	     (pm->op_private & OPpRUNTIME) ? " (RUNTIME)" : "");
    else
	dump("PMf_PRE (RUNTIME)\n");
    if (pm->op_type != OP_PUSHRE && pm->op_pmreplroot) {
	dump("PMf_REPL = ");
	dump_op(pm->op_pmreplroot);
    }
    if (pm->op_pmflags || (pm->op_pmregexp && pm->op_pmregexp->check_substr)) {
	SV *tmpsv = newSVpv("", 0);
	if (pm->op_pmdynflags & PMdf_USED)
	    sv_catpv(tmpsv, ",USED");
	if (pm->op_pmdynflags & PMdf_TAINTED)
	    sv_catpv(tmpsv, ",TAINTED");
	if (pm->op_pmflags & PMf_ONCE)
	    sv_catpv(tmpsv, ",ONCE");
	if (pm->op_pmregexp && pm->op_pmregexp->check_substr
	    && !(pm->op_pmregexp->reganch & ROPT_NOSCAN))
	    sv_catpv(tmpsv, ",SCANFIRST");
	if (pm->op_pmregexp && pm->op_pmregexp->check_substr
	    && pm->op_pmregexp->reganch & ROPT_CHECK_ALL)
	    sv_catpv(tmpsv, ",ALL");
	if (pm->op_pmflags & PMf_SKIPWHITE)
	    sv_catpv(tmpsv, ",SKIPWHITE");
	if (pm->op_pmflags & PMf_CONST)
	    sv_catpv(tmpsv, ",CONST");
	if (pm->op_pmflags & PMf_KEEP)
	    sv_catpv(tmpsv, ",KEEP");
	if (pm->op_pmflags & PMf_GLOBAL)
	    sv_catpv(tmpsv, ",GLOBAL");
	if (pm->op_pmflags & PMf_CONTINUE)
	    sv_catpv(tmpsv, ",CONTINUE");
	if (pm->op_pmflags & PMf_RETAINT)
	    sv_catpv(tmpsv, ",RETAINT");
	if (pm->op_pmflags & PMf_EVAL)
	    sv_catpv(tmpsv, ",EVAL");
	dump("PMFLAGS = (%s)\n", SvCUR(tmpsv) ? SvPVX(tmpsv) + 1 : "");
	SvREFCNT_dec(tmpsv);
    }

    PL_dumplvl--;
    dump("}\n");
#endif	/* DEBUGGING */
}


STATIC void
dump(char *pat,...)
{
#ifdef DEBUGGING
    I32 i;
    va_list args;

    va_start(args, pat);
    for (i = PL_dumplvl*4; i; i--)
	(void)PerlIO_putc(Perl_debug_log,' ');
    PerlIO_vprintf(Perl_debug_log,pat,args);
    va_end(args);
#endif	/* DEBUGGING */
}
