/*    gv.c
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *   'Mercy!' cried Gandalf.  'If the giving of information is to be the cure
 * of your inquisitiveness, I shall spend all the rest of my days answering
 * you.  What more do you want to know?'
 *   'The names of all the stars, and of all living things, and the whole
 * history of Middle-earth and Over-heaven and of the Sundering Seas,'
 * laughed Pippin.
 */

#include "EXTERN.h"
#define PERL_IN_GV_C
#include "perl.h"

GV *
Perl_gv_AVadd(pTHX_ register GV *gv)
{
    if (!gv || SvTYPE((SV*)gv) != SVt_PVGV)
	Perl_croak(aTHX_ "Bad symbol for array");
    if (!GvAV(gv))
	GvAV(gv) = newAV();
    return gv;
}

GV *
Perl_gv_HVadd(pTHX_ register GV *gv)
{
    if (!gv || SvTYPE((SV*)gv) != SVt_PVGV)
	Perl_croak(aTHX_ "Bad symbol for hash");
    if (!GvHV(gv))
	GvHV(gv) = newHV();
    return gv;
}

GV *
Perl_gv_IOadd(pTHX_ register GV *gv)
{
    if (!gv || SvTYPE((SV*)gv) != SVt_PVGV)
	Perl_croak(aTHX_ "Bad symbol for filehandle");
    if (!GvIOp(gv))
	GvIOp(gv) = newIO();
    return gv;
}

GV *
Perl_gv_fetchfile(pTHX_ const char *name)
{
    char smallbuf[256];
    char *tmpbuf;
    STRLEN tmplen;
    GV *gv;

    if (!PL_defstash)
	return Nullgv;

    tmplen = strlen(name) + 2;
    if (tmplen < sizeof smallbuf)
	tmpbuf = smallbuf;
    else
	New(603, tmpbuf, tmplen + 1, char);
    tmpbuf[0] = '_';
    tmpbuf[1] = '<';
    strcpy(tmpbuf + 2, name);
    gv = *(GV**)hv_fetch(PL_defstash, tmpbuf, tmplen, TRUE);
    if (!isGV(gv)) {
	gv_init(gv, PL_defstash, tmpbuf, tmplen, FALSE);
	sv_setpv(GvSV(gv), name);
	if (PERLDB_LINE)
	    hv_magic(GvHVn(gv_AVadd(gv)), Nullgv, 'L');
    }
    if (tmpbuf != smallbuf)
	Safefree(tmpbuf);
    return gv;
}

void
Perl_gv_init(pTHX_ GV *gv, HV *stash, const char *name, STRLEN len, int multi)
{
    register GP *gp;
    bool doproto = SvTYPE(gv) > SVt_NULL;
    char *proto = (doproto && SvPOK(gv)) ? SvPVX(gv) : NULL;

    sv_upgrade((SV*)gv, SVt_PVGV);
    if (SvLEN(gv)) {
	if (proto) {
	    SvPVX(gv) = NULL;
	    SvLEN(gv) = 0;
	    SvPOK_off(gv);
	} else
	    Safefree(SvPVX(gv));
    }
    Newz(602, gp, 1, GP);
    GvGP(gv) = gp_ref(gp);
    GvSV(gv) = NEWSV(72,0);
    GvLINE(gv) = CopLINE(PL_curcop);
    GvFILE(gv) = CopFILE(PL_curcop) ? CopFILE(PL_curcop) : "";
    GvCVGEN(gv) = 0;
    GvEGV(gv) = gv;
    sv_magic((SV*)gv, (SV*)gv, '*', Nullch, 0);
    GvSTASH(gv) = (HV*)SvREFCNT_inc(stash);
    GvNAME(gv) = savepvn(name, len);
    GvNAMELEN(gv) = len;
    if (multi || doproto)              /* doproto means it _was_ mentioned */
	GvMULTI_on(gv);
    if (doproto) {			/* Replicate part of newSUB here. */
	SvIOK_off(gv);
	ENTER;
	/* XXX unsafe for threads if eval_owner isn't held */
	start_subparse(0,0);		/* Create CV in compcv. */
	GvCV(gv) = PL_compcv;
	LEAVE;

	PL_sub_generation++;
	CvGV(GvCV(gv)) = gv;
	CvFILE(GvCV(gv)) = CopFILE(PL_curcop);
	CvSTASH(GvCV(gv)) = PL_curstash;
#ifdef USE_THREADS
	CvOWNER(GvCV(gv)) = 0;
	if (!CvMUTEXP(GvCV(gv))) {
	    New(666, CvMUTEXP(GvCV(gv)), 1, perl_mutex);
	    MUTEX_INIT(CvMUTEXP(GvCV(gv)));
	}
#endif /* USE_THREADS */
	if (proto) {
	    sv_setpv((SV*)GvCV(gv), proto);
	    Safefree(proto);
	}
    }
}

STATIC void
S_gv_init_sv(pTHX_ GV *gv, I32 sv_type)
{
    switch (sv_type) {
    case SVt_PVIO:
	(void)GvIOn(gv);
	break;
    case SVt_PVAV:
	(void)GvAVn(gv);
	break;
    case SVt_PVHV:
	(void)GvHVn(gv);
	break;
    }
}

/*
=for apidoc gv_fetchmeth

Returns the glob with the given C<name> and a defined subroutine or
C<NULL>.  The glob lives in the given C<stash>, or in the stashes
accessible via @ISA and @UNIVERSAL.

The argument C<level> should be either 0 or -1.  If C<level==0>, as a
side-effect creates a glob with the given C<name> in the given C<stash>
which in the case of success contains an alias for the subroutine, and sets
up caching info for this glob.  Similarly for all the searched stashes.

This function grants C<"SUPER"> token as a postfix of the stash name. The
GV returned from C<gv_fetchmeth> may be a method cache entry, which is not
visible to Perl code.  So when calling C<call_sv>, you should not use
the GV directly; instead, you should use the method's CV, which can be
obtained from the GV with the C<GvCV> macro.

=cut
*/

GV *
Perl_gv_fetchmeth(pTHX_ HV *stash, const char *name, STRLEN len, I32 level)
{
    AV* av;
    GV* topgv;
    GV* gv;
    GV** gvp;
    CV* cv;

    if (!stash)
	return 0;
    if ((level > 100) || (level < -100))
	Perl_croak(aTHX_ "Recursive inheritance detected while looking for method '%s' in package '%s'",
	      name, HvNAME(stash));

    DEBUG_o( Perl_deb(aTHX_ "Looking for method %s in package %s\n",name,HvNAME(stash)) );

    gvp = (GV**)hv_fetch(stash, name, len, (level >= 0));
    if (!gvp)
	topgv = Nullgv;
    else {
	topgv = *gvp;
	if (SvTYPE(topgv) != SVt_PVGV)
	    gv_init(topgv, stash, name, len, TRUE);
	if ((cv = GvCV(topgv))) {
	    /* If genuine method or valid cache entry, use it */
	    if (!GvCVGEN(topgv) || GvCVGEN(topgv) == PL_sub_generation)
		return topgv;
	    /* Stale cached entry: junk it */
	    SvREFCNT_dec(cv);
	    GvCV(topgv) = cv = Nullcv;
	    GvCVGEN(topgv) = 0;
	}
	else if (GvCVGEN(topgv) == PL_sub_generation)
	    return 0;  /* cache indicates sub doesn't exist */
    }

    gvp = (GV**)hv_fetch(stash, "ISA", 3, FALSE);
    av = (gvp && (gv = *gvp) && gv != (GV*)&PL_sv_undef) ? GvAV(gv) : Nullav;

    /* create and re-create @.*::SUPER::ISA on demand */
    if (!av || !SvMAGIC(av)) {
	char* packname = HvNAME(stash);
	STRLEN packlen = strlen(packname);

	if (packlen >= 7 && strEQ(packname + packlen - 7, "::SUPER")) {
	    HV* basestash;

	    packlen -= 7;
	    basestash = gv_stashpvn(packname, packlen, TRUE);
	    gvp = (GV**)hv_fetch(basestash, "ISA", 3, FALSE);
	    if (gvp && (gv = *gvp) != (GV*)&PL_sv_undef && (av = GvAV(gv))) {
		gvp = (GV**)hv_fetch(stash, "ISA", 3, TRUE);
		if (!gvp || !(gv = *gvp))
		    Perl_croak(aTHX_ "Cannot create %s::ISA", HvNAME(stash));
		if (SvTYPE(gv) != SVt_PVGV)
		    gv_init(gv, stash, "ISA", 3, TRUE);
		SvREFCNT_dec(GvAV(gv));
		GvAV(gv) = (AV*)SvREFCNT_inc(av);
	    }
	}
    }

    if (av) {
	SV** svp = AvARRAY(av);
	/* NOTE: No support for tied ISA */
	I32 items = AvFILLp(av) + 1;
	while (items--) {
	    SV* sv = *svp++;
	    HV* basestash = gv_stashsv(sv, FALSE);
	    if (!basestash) {
		if (ckWARN(WARN_MISC))
		    Perl_warner(aTHX_ WARN_MISC, "Can't locate package %s for @%s::ISA",
			SvPVX(sv), HvNAME(stash));
		continue;
	    }
	    gv = gv_fetchmeth(basestash, name, len,
			      (level >= 0) ? level + 1 : level - 1);
	    if (gv)
		goto gotcha;
	}
    }

    /* if at top level, try UNIVERSAL */

    if (level == 0 || level == -1) {
	HV* lastchance;

	if ((lastchance = gv_stashpvn("UNIVERSAL", 9, FALSE))) {
	    if ((gv = gv_fetchmeth(lastchance, name, len,
				  (level >= 0) ? level + 1 : level - 1)))
	    {
	  gotcha:
		/*
		 * Cache method in topgv if:
		 *  1. topgv has no synonyms (else inheritance crosses wires)
		 *  2. method isn't a stub (else AUTOLOAD fails spectacularly)
		 */
		if (topgv &&
		    GvREFCNT(topgv) == 1 &&
		    (cv = GvCV(gv)) &&
		    (CvROOT(cv) || CvXSUB(cv)))
		{
		    if ((cv = GvCV(topgv)))
			SvREFCNT_dec(cv);
		    GvCV(topgv) = (CV*)SvREFCNT_inc(GvCV(gv));
		    GvCVGEN(topgv) = PL_sub_generation;
		}
		return gv;
	    }
	    else if (topgv && GvREFCNT(topgv) == 1) {
		/* cache the fact that the method is not defined */
		GvCVGEN(topgv) = PL_sub_generation;
	    }
	}
    }

    return 0;
}

/*
=for apidoc gv_fetchmethod

See L<gv_fetchmethod_autoload>.

=cut
*/

GV *
Perl_gv_fetchmethod(pTHX_ HV *stash, const char *name)
{
    return gv_fetchmethod_autoload(stash, name, TRUE);
}

/*
=for apidoc gv_fetchmethod_autoload

Returns the glob which contains the subroutine to call to invoke the method
on the C<stash>.  In fact in the presence of autoloading this may be the
glob for "AUTOLOAD".  In this case the corresponding variable $AUTOLOAD is
already setup.

The third parameter of C<gv_fetchmethod_autoload> determines whether
AUTOLOAD lookup is performed if the given method is not present: non-zero
means yes, look for AUTOLOAD; zero means no, don't look for AUTOLOAD.
Calling C<gv_fetchmethod> is equivalent to calling C<gv_fetchmethod_autoload>
with a non-zero C<autoload> parameter.

These functions grant C<"SUPER"> token as a prefix of the method name. Note
that if you want to keep the returned glob for a long time, you need to
check for it being "AUTOLOAD", since at the later time the call may load a
different subroutine due to $AUTOLOAD changing its value. Use the glob
created via a side effect to do this.

These functions have the same side-effects and as C<gv_fetchmeth> with
C<level==0>.  C<name> should be writable if contains C<':'> or C<'
''>. The warning against passing the GV returned by C<gv_fetchmeth> to
C<call_sv> apply equally to these functions.

=cut
*/

GV *
Perl_gv_fetchmethod_autoload(pTHX_ HV *stash, const char *name, I32 autoload)
{
    register const char *nend;
    const char *nsplit = 0;
    GV* gv;

    for (nend = name; *nend; nend++) {
	if (*nend == '\'')
	    nsplit = nend;
	else if (*nend == ':' && *(nend + 1) == ':')
	    nsplit = ++nend;
    }
    if (nsplit) {
	const char *origname = name;
	name = nsplit + 1;
	if (*nsplit == ':')
	    --nsplit;
	if ((nsplit - origname) == 5 && strnEQ(origname, "SUPER", 5)) {
	    /* ->SUPER::method should really be looked up in original stash */
	    SV *tmpstr = sv_2mortal(Perl_newSVpvf(aTHX_ "%s::SUPER",
						  CopSTASHPV(PL_curcop)));
	    stash = gv_stashpvn(SvPVX(tmpstr), SvCUR(tmpstr), TRUE);
	    DEBUG_o( Perl_deb(aTHX_ "Treating %s as %s::%s\n",
			 origname, HvNAME(stash), name) );
	}
	else
	    stash = gv_stashpvn(origname, nsplit - origname, TRUE);
    }

    gv = gv_fetchmeth(stash, name, nend - name, 0);
    if (!gv) {
	if (strEQ(name,"import") || strEQ(name,"unimport"))
	    gv = (GV*)&PL_sv_yes;
	else if (autoload)
	    gv = gv_autoload4(stash, name, nend - name, TRUE);
    }
    else if (autoload) {
	CV* cv = GvCV(gv);
	if (!CvROOT(cv) && !CvXSUB(cv)) {
	    GV* stubgv;
	    GV* autogv;

	    if (CvANON(cv))
		stubgv = gv;
	    else {
		stubgv = CvGV(cv);
		if (GvCV(stubgv) != cv)		/* orphaned import */
		    stubgv = gv;
	    }
	    autogv = gv_autoload4(GvSTASH(stubgv),
				  GvNAME(stubgv), GvNAMELEN(stubgv), TRUE);
	    if (autogv)
		gv = autogv;
	}
    }

    return gv;
}

GV*
Perl_gv_autoload4(pTHX_ HV *stash, const char *name, STRLEN len, I32 method)
{
    static char autoload[] = "AUTOLOAD";
    static STRLEN autolen = 8;
    GV* gv;
    CV* cv;
    HV* varstash;
    GV* vargv;
    SV* varsv;

    if (len == autolen && strnEQ(name, autoload, autolen))
	return Nullgv;
    if (!(gv = gv_fetchmeth(stash, autoload, autolen, FALSE)))
	return Nullgv;
    cv = GvCV(gv);

    if (!CvROOT(cv))
	return Nullgv;

    /*
     * Inheriting AUTOLOAD for non-methods works ... for now.
     */
    if (ckWARN(WARN_DEPRECATED) && !method &&
	(GvCVGEN(gv) || GvSTASH(gv) != stash))
	Perl_warner(aTHX_ WARN_DEPRECATED,
	  "Use of inherited AUTOLOAD for non-method %s::%.*s() is deprecated",
	     HvNAME(stash), (int)len, name);

    /*
     * Given &FOO::AUTOLOAD, set $FOO::AUTOLOAD to desired function name.
     * The subroutine's original name may not be "AUTOLOAD", so we don't
     * use that, but for lack of anything better we will use the sub's
     * original package to look up $AUTOLOAD.
     */
    varstash = GvSTASH(CvGV(cv));
    vargv = *(GV**)hv_fetch(varstash, autoload, autolen, TRUE);
    ENTER;

#ifdef USE_THREADS
    sv_lock((SV *)varstash);
#endif
    if (!isGV(vargv))
	gv_init(vargv, varstash, autoload, autolen, FALSE);
    LEAVE;
    varsv = GvSV(vargv);
#ifdef USE_THREADS
    sv_lock(varsv);
#endif
    sv_setpv(varsv, HvNAME(stash));
    sv_catpvn(varsv, "::", 2);
    sv_catpvn(varsv, name, len);
    SvTAINTED_off(varsv);
    return gv;
}

/*
=for apidoc gv_stashpv

Returns a pointer to the stash for a specified package.  C<name> should
be a valid UTF-8 string.  If C<create> is set then the package will be
created if it does not already exist.  If C<create> is not set and the
package does not exist then NULL is returned.

=cut
*/

HV*
Perl_gv_stashpv(pTHX_ const char *name, I32 create)
{
    return gv_stashpvn(name, strlen(name), create);
}

HV*
Perl_gv_stashpvn(pTHX_ const char *name, U32 namelen, I32 create)
{
    char smallbuf[256];
    char *tmpbuf;
    HV *stash;
    GV *tmpgv;

    if (namelen + 3 < sizeof smallbuf)
	tmpbuf = smallbuf;
    else
	New(606, tmpbuf, namelen + 3, char);
    Copy(name,tmpbuf,namelen,char);
    tmpbuf[namelen++] = ':';
    tmpbuf[namelen++] = ':';
    tmpbuf[namelen] = '\0';
    tmpgv = gv_fetchpv(tmpbuf, create, SVt_PVHV);
    if (tmpbuf != smallbuf)
	Safefree(tmpbuf);
    if (!tmpgv)
	return 0;
    if (!GvHV(tmpgv))
	GvHV(tmpgv) = newHV();
    stash = GvHV(tmpgv);
    if (!HvNAME(stash))
	HvNAME(stash) = savepv(name);
    return stash;
}

/*
=for apidoc gv_stashsv

Returns a pointer to the stash for a specified package, which must be a
valid UTF-8 string.  See C<gv_stashpv>.

=cut
*/

HV*
Perl_gv_stashsv(pTHX_ SV *sv, I32 create)
{
    register char *ptr;
    STRLEN len;
    ptr = SvPV(sv,len);
    return gv_stashpvn(ptr, len, create);
}


GV *
Perl_gv_fetchpv(pTHX_ const char *nambeg, I32 add, I32 sv_type)
{
    register const char *name = nambeg;
    register GV *gv = 0;
    GV**gvp;
    I32 len;
    register const char *namend;
    HV *stash = 0;

    if (*name == '*' && isALPHA(name[1])) /* accidental stringify on a GV? */
	name++;

    for (namend = name; *namend; namend++) {
	if ((*namend == ':' && namend[1] == ':')
	    || (*namend == '\'' && namend[1]))
	{
	    if (!stash)
		stash = PL_defstash;
	    if (!stash || !SvREFCNT(stash)) /* symbol table under destruction */
		return Nullgv;

	    len = namend - name;
	    if (len > 0) {
		char smallbuf[256];
		char *tmpbuf;

		if (len + 3 < sizeof smallbuf)
		    tmpbuf = smallbuf;
		else
		    New(601, tmpbuf, len+3, char);
		Copy(name, tmpbuf, len, char);
		tmpbuf[len++] = ':';
		tmpbuf[len++] = ':';
		tmpbuf[len] = '\0';
		gvp = (GV**)hv_fetch(stash,tmpbuf,len,add);
		gv = gvp ? *gvp : Nullgv;
		if (gv && gv != (GV*)&PL_sv_undef) {
		    if (SvTYPE(gv) != SVt_PVGV)
			gv_init(gv, stash, tmpbuf, len, (add & GV_ADDMULTI));
		    else
			GvMULTI_on(gv);
		}
		if (tmpbuf != smallbuf)
		    Safefree(tmpbuf);
		if (!gv || gv == (GV*)&PL_sv_undef)
		    return Nullgv;

		if (!(stash = GvHV(gv)))
		    stash = GvHV(gv) = newHV();

		if (!HvNAME(stash))
		    HvNAME(stash) = savepvn(nambeg, namend - nambeg);
	    }

	    if (*namend == ':')
		namend++;
	    namend++;
	    name = namend;
	    if (!*name)
		return gv ? gv : (GV*)*hv_fetch(PL_defstash, "main::", 6, TRUE);
	}
    }
    len = namend - name;
    if (!len)
	len = 1;

    /* No stash in name, so see how we can default */

    if (!stash) {
	if (isIDFIRST_lazy(name)) {
	    bool global = FALSE;

	    if (isUPPER(*name)) {
		if (*name == 'S' && (
		    strEQ(name, "SIG") ||
		    strEQ(name, "STDIN") ||
		    strEQ(name, "STDOUT") ||
		    strEQ(name, "STDERR")))
		    global = TRUE;
		else if (*name == 'I' && strEQ(name, "INC"))
		    global = TRUE;
		else if (*name == 'E' && strEQ(name, "ENV"))
		    global = TRUE;
		else if (*name == 'A' && (
		  strEQ(name, "ARGV") ||
		  strEQ(name, "ARGVOUT")))
		    global = TRUE;
	    }
	    else if (*name == '_' && !name[1])
		global = TRUE;

	    if (global)
		stash = PL_defstash;
	    else if ((COP*)PL_curcop == &PL_compiling) {
		stash = PL_curstash;
		if (add && (PL_hints & HINT_STRICT_VARS) &&
		    sv_type != SVt_PVCV &&
		    sv_type != SVt_PVGV &&
		    sv_type != SVt_PVFM &&
		    sv_type != SVt_PVIO &&
		    !(len == 1 && sv_type == SVt_PV && strchr("ab",*name)) )
		{
		    gvp = (GV**)hv_fetch(stash,name,len,0);
		    if (!gvp ||
			*gvp == (GV*)&PL_sv_undef ||
			SvTYPE(*gvp) != SVt_PVGV)
		    {
			stash = 0;
		    }
		    else if ((sv_type == SVt_PV   && !GvIMPORTED_SV(*gvp)) ||
			     (sv_type == SVt_PVAV && !GvIMPORTED_AV(*gvp)) ||
			     (sv_type == SVt_PVHV && !GvIMPORTED_HV(*gvp)) )
		    {
			Perl_warn(aTHX_ "Variable \"%c%s\" is not imported",
			    sv_type == SVt_PVAV ? '@' :
			    sv_type == SVt_PVHV ? '%' : '$',
			    name);
			if (GvCVu(*gvp))
			    Perl_warn(aTHX_ "\t(Did you mean &%s instead?)\n", name);
			stash = 0;
		    }
		}
	    }
	    else
		stash = CopSTASH(PL_curcop);
	}
	else
	    stash = PL_defstash;
    }

    /* By this point we should have a stash and a name */

    if (!stash) {
	if (add) {
	    qerror(Perl_mess(aTHX_
		 "Global symbol \"%s%s\" requires explicit package name",
		 (sv_type == SVt_PV ? "$"
		  : sv_type == SVt_PVAV ? "@"
		  : sv_type == SVt_PVHV ? "%"
		  : ""), name));
	    stash = PL_nullstash;
	}
	else
	    return Nullgv;
    }

    if (!SvREFCNT(stash))	/* symbol table under destruction */
	return Nullgv;

    gvp = (GV**)hv_fetch(stash,name,len,add);
    if (!gvp || *gvp == (GV*)&PL_sv_undef)
	return Nullgv;
    gv = *gvp;
    if (SvTYPE(gv) == SVt_PVGV) {
	if (add) {
	    GvMULTI_on(gv);
	    gv_init_sv(gv, sv_type);
	}
	return gv;
    } else if (add & GV_NOINIT) {
	return gv;
    }

    /* Adding a new symbol */

    if (add & GV_ADDWARN && ckWARN_d(WARN_INTERNAL))
	Perl_warner(aTHX_ WARN_INTERNAL, "Had to create %s unexpectedly", nambeg);
    gv_init(gv, stash, name, len, add & GV_ADDMULTI);
    gv_init_sv(gv, sv_type);

    if (isALPHA(name[0]) && ! (isLEXWARN_on ? ckWARN(WARN_ONCE) 
			                    : (PL_dowarn & G_WARN_ON ) ) )
        GvMULTI_on(gv) ;

    /* set up magic where warranted */
    switch (*name) {
    case 'A':
	if (strEQ(name, "ARGV")) {
	    IoFLAGS(GvIOn(gv)) |= IOf_ARGV|IOf_START;
	}
	break;
    case 'E':
	if (strnEQ(name, "EXPORT", 6))
	    GvMULTI_on(gv);
	break;
    case 'I':
	if (strEQ(name, "ISA")) {
	    AV* av = GvAVn(gv);
	    GvMULTI_on(gv);
	    sv_magic((SV*)av, (SV*)gv, 'I', Nullch, 0);
	    /* NOTE: No support for tied ISA */
	    if ((add & GV_ADDMULTI) && strEQ(nambeg,"AnyDBM_File::ISA")
		&& AvFILLp(av) == -1)
	    {
		char *pname;
		av_push(av, newSVpvn(pname = "NDBM_File",9));
		gv_stashpvn(pname, 9, TRUE);
		av_push(av, newSVpvn(pname = "DB_File",7));
		gv_stashpvn(pname, 7, TRUE);
		av_push(av, newSVpvn(pname = "GDBM_File",9));
		gv_stashpvn(pname, 9, TRUE);
		av_push(av, newSVpvn(pname = "SDBM_File",9));
		gv_stashpvn(pname, 9, TRUE);
		av_push(av, newSVpvn(pname = "ODBM_File",9));
		gv_stashpvn(pname, 9, TRUE);
	    }
	}
	break;
    case 'O':
        if (strEQ(name, "OVERLOAD")) {
            HV* hv = GvHVn(gv);
            GvMULTI_on(gv);
            hv_magic(hv, Nullgv, 'A');
        }
        break;
    case 'S':
	if (strEQ(name, "SIG")) {
	    HV *hv;
	    I32 i;
	    if (!PL_psig_ptr) {
		int sig_num[] = { SIG_NUM };
		New(73, PL_psig_ptr, sizeof(sig_num)/sizeof(*sig_num), SV*);
		New(73, PL_psig_name, sizeof(sig_num)/sizeof(*sig_num), SV*);
	    }
	    GvMULTI_on(gv);
	    hv = GvHVn(gv);
	    hv_magic(hv, Nullgv, 'S');
	    for (i = 1; PL_sig_name[i]; i++) {
	    	SV ** init;
	    	init = hv_fetch(hv, PL_sig_name[i], strlen(PL_sig_name[i]), 1);
	    	if (init)
		    sv_setsv(*init, &PL_sv_undef);
	    	PL_psig_ptr[i] = 0;
	    	PL_psig_name[i] = 0;
	    }
	}
	break;
    case 'V':
	if (strEQ(name, "VERSION"))
	    GvMULTI_on(gv);
	break;

    case '&':
	if (len > 1)
	    break;
	PL_sawampersand = TRUE;
	goto ro_magicalize;

    case '`':
	if (len > 1)
	    break;
	PL_sawampersand = TRUE;
	goto ro_magicalize;

    case '\'':
	if (len > 1)
	    break;
	PL_sawampersand = TRUE;
	goto ro_magicalize;

    case ':':
	if (len > 1)
	    break;
	sv_setpv(GvSV(gv),PL_chopset);
	goto magicalize;

    case '?':
	if (len > 1)
	    break;
#ifdef COMPLEX_STATUS
	(void)SvUPGRADE(GvSV(gv), SVt_PVLV);
#endif
	goto magicalize;

    case '!':
	if (len > 1)
	    break;
	if (sv_type > SVt_PV && PL_curcop != &PL_compiling) {
	    HV* stash = gv_stashpvn("Errno",5,FALSE);
	    if (!stash || !(gv_fetchmethod(stash, "TIEHASH"))) {
		dSP;
		PUTBACK;
		require_pv("Errno.pm");
		SPAGAIN;
		stash = gv_stashpvn("Errno",5,FALSE);
		if (!stash || !(gv_fetchmethod(stash, "TIEHASH")))
		    Perl_croak(aTHX_ "Can't use %%! because Errno.pm is not available");
	    }
	}
	goto magicalize;
    case '-':
	if (len > 1)
	    break;
	else {
            AV* av = GvAVn(gv);
            sv_magic((SV*)av, Nullsv, 'D', Nullch, 0);
	    SvREADONLY_on(av);
        }
	goto magicalize;
    case '#':
    case '*':
	if (ckWARN(WARN_DEPRECATED) && len == 1 && sv_type == SVt_PV)
	    Perl_warner(aTHX_ WARN_DEPRECATED, "Use of $%s is deprecated", name);
	/* FALL THROUGH */
    case '[':
    case '^':
    case '~':
    case '=':
    case '%':
    case '.':
    case '(':
    case ')':
    case '<':
    case '>':
    case ',':
    case '\\':
    case '/':
    case '\001':	/* $^A */
    case '\003':	/* $^C */
    case '\004':	/* $^D */
    case '\005':	/* $^E */
    case '\006':	/* $^F */
    case '\010':	/* $^H */
    case '\011':	/* $^I, NOT \t in EBCDIC */
    case '\017':	/* $^O */
    case '\020':	/* $^P */
    case '\024':	/* $^T */
	if (len > 1)
	    break;
	goto magicalize;
    case '|':
	if (len > 1)
	    break;
	sv_setiv(GvSV(gv), (IV)(IoFLAGS(GvIOp(PL_defoutgv)) & IOf_FLUSH) != 0);
	goto magicalize;
    case '\023':	/* $^S */
	if (len > 1)
	    break;
	goto ro_magicalize;
    case '\027':	/* $^W & $^WARNING_BITS */
	if (len > 1 && strNE(name, "\027ARNING_BITS")
	    && strNE(name, "\027IDE_SYSTEM_CALLS"))
	    break;
	goto magicalize;

    case '+':
	if (len > 1)
	    break;
	else {
            AV* av = GvAVn(gv);
            sv_magic((SV*)av, (SV*)av, 'D', Nullch, 0);
	    SvREADONLY_on(av);
        }
	/* FALL THROUGH */
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      ro_magicalize:
	SvREADONLY_on(GvSV(gv));
      magicalize:
	sv_magic(GvSV(gv), (SV*)gv, 0, name, len);
	break;

    case '\014':	/* $^L */
	if (len > 1)
	    break;
	sv_setpv(GvSV(gv),"\f");
	PL_formfeed = GvSV(gv);
	break;
    case ';':
	if (len > 1)
	    break;
	sv_setpv(GvSV(gv),"\034");
	break;
    case ']':
	if (len == 1) {
	    SV *sv = GvSV(gv);
	    (void)SvUPGRADE(sv, SVt_PVNV);
	    Perl_sv_setpvf(aTHX_ sv,
#if defined(PERL_SUBVERSION) && (PERL_SUBVERSION > 0)
			    "%8.6"
#else
			    "%5.3"
#endif
			    NVff,
			    SvNVX(PL_patchlevel));
	    SvNVX(sv) = SvNVX(PL_patchlevel);
	    SvNOK_on(sv);
	    SvREADONLY_on(sv);
	}
	break;
    case '\026':	/* $^V */
	if (len == 1) {
	    SV *sv = GvSV(gv);
	    GvSV(gv) = SvREFCNT_inc(PL_patchlevel);
	    SvREFCNT_dec(sv);
	}
	break;
    }
    return gv;
}

void
Perl_gv_fullname4(pTHX_ SV *sv, GV *gv, const char *prefix, bool keepmain)
{
    HV *hv = GvSTASH(gv);
    if (!hv) {
	(void)SvOK_off(sv);
	return;
    }
    sv_setpv(sv, prefix ? prefix : "");
    if (keepmain || strNE(HvNAME(hv), "main")) {
	sv_catpv(sv,HvNAME(hv));
	sv_catpvn(sv,"::", 2);
    }
    sv_catpvn(sv,GvNAME(gv),GvNAMELEN(gv));
}

void
Perl_gv_fullname3(pTHX_ SV *sv, GV *gv, const char *prefix)
{
    HV *hv = GvSTASH(gv);
    if (!hv) {
	(void)SvOK_off(sv);
	return;
    }
    sv_setpv(sv, prefix ? prefix : "");
    sv_catpv(sv,HvNAME(hv));
    sv_catpvn(sv,"::", 2);
    sv_catpvn(sv,GvNAME(gv),GvNAMELEN(gv));
}

void
Perl_gv_efullname4(pTHX_ SV *sv, GV *gv, const char *prefix, bool keepmain)
{
    GV *egv = GvEGV(gv);
    if (!egv)
	egv = gv;
    gv_fullname4(sv, egv, prefix, keepmain);
}

void
Perl_gv_efullname3(pTHX_ SV *sv, GV *gv, const char *prefix)
{
    GV *egv = GvEGV(gv);
    if (!egv)
	egv = gv;
    gv_fullname3(sv, egv, prefix);
}

/* XXX compatibility with versions <= 5.003. */
void
Perl_gv_fullname(pTHX_ SV *sv, GV *gv)
{
    gv_fullname3(sv, gv, sv == (SV*)gv ? "*" : "");
}

/* XXX compatibility with versions <= 5.003. */
void
Perl_gv_efullname(pTHX_ SV *sv, GV *gv)
{
    gv_efullname3(sv, gv, sv == (SV*)gv ? "*" : "");
}

IO *
Perl_newIO(pTHX)
{
    IO *io;
    GV *iogv;

    io = (IO*)NEWSV(0,0);
    sv_upgrade((SV *)io,SVt_PVIO);
    SvREFCNT(io) = 1;
    SvOBJECT_on(io);
    iogv = gv_fetchpv("FileHandle::", FALSE, SVt_PVHV);
    /* unless exists($main::{FileHandle}) and defined(%main::FileHandle::) */
    if (!(iogv && GvHV(iogv) && HvARRAY(GvHV(iogv))))
      iogv = gv_fetchpv("IO::Handle::", TRUE, SVt_PVHV);
    SvSTASH(io) = (HV*)SvREFCNT_inc(GvHV(iogv));
    return io;
}

void
Perl_gv_check(pTHX_ HV *stash)
{
    register HE *entry;
    register I32 i;
    register GV *gv;
    HV *hv;

    if (!HvARRAY(stash))
	return;
    for (i = 0; i <= (I32) HvMAX(stash); i++) {
	for (entry = HvARRAY(stash)[i]; entry; entry = HeNEXT(entry)) {
	    if (HeKEY(entry)[HeKLEN(entry)-1] == ':' &&
		(gv = (GV*)HeVAL(entry)) && (hv = GvHV(gv)) && HvNAME(hv))
	    {
		if (hv != PL_defstash && hv != stash)
		     gv_check(hv);              /* nested package */
	    }
	    else if (isALPHA(*HeKEY(entry))) {
		char *file;
		gv = (GV*)HeVAL(entry);
		if (SvTYPE(gv) != SVt_PVGV || GvMULTI(gv))
		    continue;
		file = GvFILE(gv);
		/* performance hack: if filename is absolute and it's a standard
		 * module, don't bother warning */
		if (file
		    && PERL_FILE_IS_ABSOLUTE(file)
		    && (instr(file, "/lib/") || instr(file, ".pm")))
		{
		    continue;
		}
		CopLINE_set(PL_curcop, GvLINE(gv));
#ifdef USE_ITHREADS
		CopFILE(PL_curcop) = file;	/* set for warning */
#else
		CopFILEGV(PL_curcop) = gv_fetchfile(file);
#endif
		Perl_warner(aTHX_ WARN_ONCE,
			"Name \"%s::%s\" used only once: possible typo",
			HvNAME(stash), GvNAME(gv));
	    }
	}
    }
}

GV *
Perl_newGVgen(pTHX_ char *pack)
{
    return gv_fetchpv(Perl_form(aTHX_ "%s::_GEN_%ld", pack, (long)PL_gensym++),
		      TRUE, SVt_PVGV);
}

/* hopefully this is only called on local symbol table entries */

GP*
Perl_gp_ref(pTHX_ GP *gp)
{
    if (!gp)
	return (GP*)NULL;
    gp->gp_refcnt++;
    if (gp->gp_cv) {
	if (gp->gp_cvgen) {
	    /* multi-named GPs cannot be used for method cache */
	    SvREFCNT_dec(gp->gp_cv);
	    gp->gp_cv = Nullcv;
	    gp->gp_cvgen = 0;
	}
	else {
	    /* Adding a new name to a subroutine invalidates method cache */
	    PL_sub_generation++;
	}
    }
    return gp;
}

void
Perl_gp_free(pTHX_ GV *gv)
{
    GP* gp;

    if (!gv || !(gp = GvGP(gv)))
	return;
    if (gp->gp_refcnt == 0) {
	if (ckWARN_d(WARN_INTERNAL))
	    Perl_warner(aTHX_ WARN_INTERNAL,
			"Attempt to free unreferenced glob pointers");
        return;
    }
    if (gp->gp_cv) {
	/* Deleting the name of a subroutine invalidates method cache */
	PL_sub_generation++;
    }
    if (--gp->gp_refcnt > 0) {
	if (gp->gp_egv == gv)
	    gp->gp_egv = 0;
        return;
    }

    SvREFCNT_dec(gp->gp_sv);
    SvREFCNT_dec(gp->gp_av);
    SvREFCNT_dec(gp->gp_hv);
    SvREFCNT_dec(gp->gp_io);
    SvREFCNT_dec(gp->gp_cv);
    SvREFCNT_dec(gp->gp_form);

    Safefree(gp);
    GvGP(gv) = 0;
}

#if defined(CRIPPLED_CC) && (defined(iAPX286) || defined(M_I286) || defined(I80286))
#define MICROPORT
#endif

#ifdef	MICROPORT	/* Microport 2.4 hack */
AV *GvAVn(gv)
register GV *gv;
{
    if (GvGP(gv)->gp_av)
	return GvGP(gv)->gp_av;
    else
	return GvGP(gv_AVadd(gv))->gp_av;
}

HV *GvHVn(gv)
register GV *gv;
{
    if (GvGP(gv)->gp_hv)
	return GvGP(gv)->gp_hv;
    else
	return GvGP(gv_HVadd(gv))->gp_hv;
}
#endif			/* Microport 2.4 hack */

/* Updates and caches the CV's */

bool
Perl_Gv_AMupdate(pTHX_ HV *stash)
{
  GV* gv;
  CV* cv;
  MAGIC* mg=mg_find((SV*)stash,'c');
  AMT *amtp = (mg) ? (AMT*)mg->mg_ptr: (AMT *) NULL;
  AMT amt;
  STRLEN n_a;
#ifdef OVERLOAD_VIA_HASH
  GV** gvp;
  HV* hv;
#endif

  if (mg && amtp->was_ok_am == PL_amagic_generation
      && amtp->was_ok_sub == PL_sub_generation)
      return AMT_AMAGIC(amtp);
  if (amtp && AMT_AMAGIC(amtp)) {	/* Have table. */
    int i;
    for (i=1; i<NofAMmeth; i++) {
      if (amtp->table[i]) {
	SvREFCNT_dec(amtp->table[i]);
      }
    }
  }
  sv_unmagic((SV*)stash, 'c');

  DEBUG_o( Perl_deb(aTHX_ "Recalcing overload magic in package %s\n",HvNAME(stash)) );

  amt.was_ok_am = PL_amagic_generation;
  amt.was_ok_sub = PL_sub_generation;
  amt.fallback = AMGfallNO;
  amt.flags = 0;

#ifdef OVERLOAD_VIA_HASH
  gvp=(GV**)hv_fetch(stash,"OVERLOAD",8,FALSE);	/* A shortcut */
  if (gvp && ((gv = *gvp) != (GV*)&PL_sv_undef && (hv = GvHV(gv)))) {
    int filled=0;
    int i;
    char *cp;
    SV* sv;
    SV** svp;

    /* Work with "fallback" key, which we assume to be first in PL_AMG_names */

    if (( cp = (char *)PL_AMG_names[0] ) &&
	(svp = (SV**)hv_fetch(hv,cp,strlen(cp),FALSE)) && (sv = *svp)) {
      if (SvTRUE(sv)) amt.fallback=AMGfallYES;
      else if (SvOK(sv)) amt.fallback=AMGfallNEVER;
    }
    for (i = 1; i < NofAMmeth; i++) {
      cv = 0;
      cp = (char *)PL_AMG_names[i];

        svp = (SV**)hv_fetch(hv, cp, strlen(cp), FALSE);
        if (svp && ((sv = *svp) != &PL_sv_undef)) {
          switch (SvTYPE(sv)) {
            default:
              if (!SvROK(sv)) {
                if (!SvOK(sv)) break;
		gv = gv_fetchmethod(stash, SvPV(sv, n_a));
                if (gv) cv = GvCV(gv);
                break;
              }
              cv = (CV*)SvRV(sv);
              if (SvTYPE(cv) == SVt_PVCV)
                  break;
                /* FALL THROUGH */
            case SVt_PVHV:
            case SVt_PVAV:
	      Perl_croak(aTHX_ "Not a subroutine reference in overload table");
	      return FALSE;
            case SVt_PVCV:
              cv = (CV*)sv;
              break;
            case SVt_PVGV:
              if (!(cv = GvCVu((GV*)sv)))
                cv = sv_2cv(sv, &stash, &gv, FALSE);
              break;
          }
          if (cv) filled=1;
	  else {
	    Perl_croak(aTHX_ "Method for operation %s not found in package %.256s during blessing\n",
		cp,HvNAME(stash));
	    return FALSE;
	  }
        }
#else
  {
    int filled = 0;
    int i;
    const char *cp;
    SV* sv = NULL;

    /* Work with "fallback" key, which we assume to be first in PL_AMG_names */

    if ((cp = PL_AMG_names[0])) {
	/* Try to find via inheritance. */
	gv = gv_fetchmeth(stash, "()", 2, -1); /* A cookie: "()". */
	if (gv)
	    sv = GvSV(gv);

	if (!gv)
	    goto no_table;
	else if (SvTRUE(sv))
	    amt.fallback=AMGfallYES;
	else if (SvOK(sv))
	    amt.fallback=AMGfallNEVER;
    }

    for (i = 1; i < NofAMmeth; i++) {
	SV *cookie = sv_2mortal(Perl_newSVpvf(aTHX_ "(%s", cp = PL_AMG_names[i]));
	DEBUG_o( Perl_deb(aTHX_ "Checking overloading of `%s' in package `%.256s'\n",
		     cp, HvNAME(stash)) );
	/* don't fill the cache while looking up! */
	gv = gv_fetchmeth(stash, SvPVX(cookie), SvCUR(cookie), -1);
        cv = 0;
        if(gv && (cv = GvCV(gv))) {
	    if (GvNAMELEN(CvGV(cv)) == 3 && strEQ(GvNAME(CvGV(cv)), "nil")
		&& strEQ(HvNAME(GvSTASH(CvGV(cv))), "overload")) {
		/* GvSV contains the name of the method. */
		GV *ngv;
		
		DEBUG_o( Perl_deb(aTHX_ "Resolving method `%.256s' for overloaded `%s' in package `%.256s'\n",
			     SvPV(GvSV(gv), n_a), cp, HvNAME(stash)) );
		if (!SvPOK(GvSV(gv))
		    || !(ngv = gv_fetchmethod_autoload(stash, SvPVX(GvSV(gv)),
						       FALSE)))
		{
		    /* Can be an import stub (created by `can'). */
		    if (GvCVGEN(gv)) {
			Perl_croak(aTHX_ "Stub found while resolving method `%.256s' overloading `%s' in package `%.256s'",
			      (SvPOK(GvSV(gv)) ?  SvPVX(GvSV(gv)) : "???" ),
			      cp, HvNAME(stash));
		    } else
			Perl_croak(aTHX_ "Can't resolve method `%.256s' overloading `%s' in package `%.256s'",
			      (SvPOK(GvSV(gv)) ?  SvPVX(GvSV(gv)) : "???" ),
			      cp, HvNAME(stash));
		}
		cv = GvCV(gv = ngv);
	    }
	    DEBUG_o( Perl_deb(aTHX_ "Overloading `%s' in package `%.256s' via `%.256s::%.256s' \n",
			 cp, HvNAME(stash), HvNAME(GvSTASH(CvGV(cv))),
			 GvNAME(CvGV(cv))) );
	    filled = 1;
	}
#endif
	amt.table[i]=(CV*)SvREFCNT_inc(cv);
    }
    if (filled) {
      AMT_AMAGIC_on(&amt);
      sv_magic((SV*)stash, 0, 'c', (char*)&amt, sizeof(AMT));
      return TRUE;
    }
  }
  /* Here we have no table: */
 no_table:
  AMT_AMAGIC_off(&amt);
  sv_magic((SV*)stash, 0, 'c', (char*)&amt, sizeof(AMTS));
  return FALSE;
}

SV*
Perl_amagic_call(pTHX_ SV *left, SV *right, int method, int flags)
{
  MAGIC *mg;
  CV *cv;
  CV **cvp=NULL, **ocvp=NULL;
  AMT *amtp, *oamtp;
  int fl=0, off, off1, lr=0, assign=AMGf_assign & flags, notfound=0;
  int postpr = 0, force_cpy = 0, assignshift = assign ? 1 : 0;
  HV* stash;
  if (!(AMGf_noleft & flags) && SvAMAGIC(left)
      && (mg = mg_find((SV*)(stash=SvSTASH(SvRV(left))),'c'))
      && (ocvp = cvp = (AMT_AMAGIC((AMT*)mg->mg_ptr)
			? (oamtp = amtp = (AMT*)mg->mg_ptr)->table
			: (CV **) NULL))
      && ((cv = cvp[off=method+assignshift])
	  || (assign && amtp->fallback > AMGfallNEVER && /* fallback to
						          * usual method */
		  (fl = 1, cv = cvp[off=method])))) {
    lr = -1;			/* Call method for left argument */
  } else {
    if (cvp && amtp->fallback > AMGfallNEVER && flags & AMGf_unary) {
      int logic;

      /* look for substituted methods */
      /* In all the covered cases we should be called with assign==0. */
	 switch (method) {
	 case inc_amg:
	   force_cpy = 1;
	   if ((cv = cvp[off=add_ass_amg])
	       || ((cv = cvp[off = add_amg]) && (force_cpy = 0, postpr = 1))) {
	     right = &PL_sv_yes; lr = -1; assign = 1;
	   }
	   break;
	 case dec_amg:
	   force_cpy = 1;
	   if ((cv = cvp[off = subtr_ass_amg])
	       || ((cv = cvp[off = subtr_amg]) && (force_cpy = 0, postpr=1))) {
	     right = &PL_sv_yes; lr = -1; assign = 1;
	   }
	   break;
	 case bool__amg:
	   (void)((cv = cvp[off=numer_amg]) || (cv = cvp[off=string_amg]));
	   break;
	 case numer_amg:
	   (void)((cv = cvp[off=string_amg]) || (cv = cvp[off=bool__amg]));
	   break;
	 case string_amg:
	   (void)((cv = cvp[off=numer_amg]) || (cv = cvp[off=bool__amg]));
	   break;
 case not_amg:
   (void)((cv = cvp[off=bool__amg])
	  || (cv = cvp[off=numer_amg])
	  || (cv = cvp[off=string_amg]));
   postpr = 1;
   break;
	 case copy_amg:
	   {
	     /*
		  * SV* ref causes confusion with the interpreter variable of
		  * the same name
		  */
	     SV* tmpRef=SvRV(left);
	     if (!SvROK(tmpRef) && SvTYPE(tmpRef) <= SVt_PVMG) {
		/*
		 * Just to be extra cautious.  Maybe in some
		 * additional cases sv_setsv is safe, too.
		 */
		SV* newref = newSVsv(tmpRef);
		SvOBJECT_on(newref);
		SvSTASH(newref) = (HV*)SvREFCNT_inc(SvSTASH(tmpRef));
		return newref;
	     }
	   }
	   break;
	 case abs_amg:
	   if ((cvp[off1=lt_amg] || cvp[off1=ncmp_amg])
	       && ((cv = cvp[off=neg_amg]) || (cv = cvp[off=subtr_amg]))) {
	     SV* nullsv=sv_2mortal(newSViv(0));
	     if (off1==lt_amg) {
	       SV* lessp = amagic_call(left,nullsv,
				       lt_amg,AMGf_noright);
	       logic = SvTRUE(lessp);
	     } else {
	       SV* lessp = amagic_call(left,nullsv,
				       ncmp_amg,AMGf_noright);
	       logic = (SvNV(lessp) < 0);
	     }
	     if (logic) {
	       if (off==subtr_amg) {
		 right = left;
		 left = nullsv;
		 lr = 1;
	       }
	     } else {
	       return left;
	     }
	   }
	   break;
	 case neg_amg:
	   if ((cv = cvp[off=subtr_amg])) {
	     right = left;
	     left = sv_2mortal(newSViv(0));
	     lr = 1;
	   }
	   break;
	 case iter_amg:			/* XXXX Eventually should do to_gv. */
	     /* FAIL safe */
	     return NULL;	/* Delegate operation to standard mechanisms. */
	     break;
	 case to_sv_amg:
	 case to_av_amg:
	 case to_hv_amg:
	 case to_gv_amg:
	 case to_cv_amg:
	     /* FAIL safe */
	     return left;	/* Delegate operation to standard mechanisms. */
	     break;
	 default:
	   goto not_found;
	 }
	 if (!cv) goto not_found;
    } else if (!(AMGf_noright & flags) && SvAMAGIC(right)
	       && (mg = mg_find((SV*)(stash=SvSTASH(SvRV(right))),'c'))
	       && (cvp = (AMT_AMAGIC((AMT*)mg->mg_ptr)
			  ? (amtp = (AMT*)mg->mg_ptr)->table
			  : (CV **) NULL))
	       && (cv = cvp[off=method])) { /* Method for right
					     * argument found */
      lr=1;
    } else if (((ocvp && oamtp->fallback > AMGfallNEVER
		 && (cvp=ocvp) && (lr = -1))
		|| (cvp && amtp->fallback > AMGfallNEVER && (lr=1)))
	       && !(flags & AMGf_unary)) {
				/* We look for substitution for
				 * comparison operations and
				 * concatenation */
      if (method==concat_amg || method==concat_ass_amg
	  || method==repeat_amg || method==repeat_ass_amg) {
	return NULL;		/* Delegate operation to string conversion */
      }
      off = -1;
      switch (method) {
	 case lt_amg:
	 case le_amg:
	 case gt_amg:
	 case ge_amg:
	 case eq_amg:
	 case ne_amg:
	   postpr = 1; off=ncmp_amg; break;
	 case slt_amg:
	 case sle_amg:
	 case sgt_amg:
	 case sge_amg:
	 case seq_amg:
	 case sne_amg:
	   postpr = 1; off=scmp_amg; break;
	 }
      if (off != -1) cv = cvp[off];
      if (!cv) {
	goto not_found;
      }
    } else {
    not_found:			/* No method found, either report or croak */
      switch (method) {
	 case to_sv_amg:
	 case to_av_amg:
	 case to_hv_amg:
	 case to_gv_amg:
	 case to_cv_amg:
	     /* FAIL safe */
	     return left;	/* Delegate operation to standard mechanisms. */
	     break;
      }
      if (ocvp && (cv=ocvp[nomethod_amg])) { /* Call report method */
	notfound = 1; lr = -1;
      } else if (cvp && (cv=cvp[nomethod_amg])) {
	notfound = 1; lr = 1;
      } else {
	SV *msg;
	if (off==-1) off=method;
	msg = sv_2mortal(Perl_newSVpvf(aTHX_
		      "Operation `%s': no method found,%sargument %s%s%s%s",
		      PL_AMG_names[method + assignshift],
		      (flags & AMGf_unary ? " " : "\n\tleft "),
		      SvAMAGIC(left)?
		        "in overloaded package ":
		        "has no overloaded magic",
		      SvAMAGIC(left)?
		        HvNAME(SvSTASH(SvRV(left))):
		        "",
		      SvAMAGIC(right)?
		        ",\n\tright argument in overloaded package ":
		        (flags & AMGf_unary
			 ? ""
			 : ",\n\tright argument has no overloaded magic"),
		      SvAMAGIC(right)?
		        HvNAME(SvSTASH(SvRV(right))):
		        ""));
	if (amtp && amtp->fallback >= AMGfallYES) {
	  DEBUG_o( Perl_deb(aTHX_ "%s", SvPVX(msg)) );
	} else {
	  Perl_croak(aTHX_ "%"SVf, msg);
	}
	return NULL;
      }
      force_cpy = force_cpy || assign;
    }
  }
  if (!notfound) {
    DEBUG_o( Perl_deb(aTHX_
  "Overloaded operator `%s'%s%s%s:\n\tmethod%s found%s in package %s%s\n",
		 PL_AMG_names[off],
		 method+assignshift==off? "" :
		             " (initially `",
		 method+assignshift==off? "" :
		             PL_AMG_names[method+assignshift],
		 method+assignshift==off? "" : "')",
		 flags & AMGf_unary? "" :
		   lr==1 ? " for right argument": " for left argument",
		 flags & AMGf_unary? " for argument" : "",
		 HvNAME(stash),
		 fl? ",\n\tassignment variant used": "") );
  }
    /* Since we use shallow copy during assignment, we need
     * to dublicate the contents, probably calling user-supplied
     * version of copy operator
     */
    /* We need to copy in following cases:
     * a) Assignment form was called.
     * 		assignshift==1,  assign==T, method + 1 == off
     * b) Increment or decrement, called directly.
     * 		assignshift==0,  assign==0, method + 0 == off
     * c) Increment or decrement, translated to assignment add/subtr.
     * 		assignshift==0,  assign==T,
     *		force_cpy == T
     * d) Increment or decrement, translated to nomethod.
     * 		assignshift==0,  assign==0,
     *		force_cpy == T
     * e) Assignment form translated to nomethod.
     * 		assignshift==1,  assign==T, method + 1 != off
     *		force_cpy == T
     */
    /*	off is method, method+assignshift, or a result of opcode substitution.
     *	In the latter case assignshift==0, so only notfound case is important.
     */
  if (( (method + assignshift == off)
	&& (assign || (method == inc_amg) || (method == dec_amg)))
      || force_cpy)
    RvDEEPCP(left);
  {
    dSP;
    BINOP myop;
    SV* res;
    bool oldcatch = CATCH_GET;

    CATCH_SET(TRUE);
    Zero(&myop, 1, BINOP);
    myop.op_last = (OP *) &myop;
    myop.op_next = Nullop;
    myop.op_flags = OPf_WANT_SCALAR | OPf_STACKED;

    PUSHSTACKi(PERLSI_OVERLOAD);
    ENTER;
    SAVEOP();
    PL_op = (OP *) &myop;
    if (PERLDB_SUB && PL_curstash != PL_debstash)
	PL_op->op_private |= OPpENTERSUB_DB;
    PUTBACK;
    pp_pushmark();

    EXTEND(SP, notfound + 5);
    PUSHs(lr>0? right: left);
    PUSHs(lr>0? left: right);
    PUSHs( lr > 0 ? &PL_sv_yes : ( assign ? &PL_sv_undef : &PL_sv_no ));
    if (notfound) {
      PUSHs( sv_2mortal(newSVpv((char *)PL_AMG_names[method + assignshift],0)));
    }
    PUSHs((SV*)cv);
    PUTBACK;

    if ((PL_op = Perl_pp_entersub(aTHX)))
      CALLRUNOPS(aTHX);
    LEAVE;
    SPAGAIN;

    res=POPs;
    PUTBACK;
    POPSTACK;
    CATCH_SET(oldcatch);

    if (postpr) {
      int ans;
      switch (method) {
      case le_amg:
      case sle_amg:
	ans=SvIV(res)<=0; break;
      case lt_amg:
      case slt_amg:
	ans=SvIV(res)<0; break;
      case ge_amg:
      case sge_amg:
	ans=SvIV(res)>=0; break;
      case gt_amg:
      case sgt_amg:
	ans=SvIV(res)>0; break;
      case eq_amg:
      case seq_amg:
	ans=SvIV(res)==0; break;
      case ne_amg:
      case sne_amg:
	ans=SvIV(res)!=0; break;
      case inc_amg:
      case dec_amg:
	SvSetSV(left,res); return left;
      case not_amg:
	ans=!SvTRUE(res); break;
      }
      return boolSV(ans);
    } else if (method==copy_amg) {
      if (!SvROK(res)) {
	Perl_croak(aTHX_ "Copy method did not return a reference");
      }
      return SvREFCNT_inc(SvRV(res));
    } else {
      return res;
    }
  }
}

/*
=for apidoc is_gv_magical

Returns C<TRUE> if given the name of a magical GV.

Currently only useful internally when determining if a GV should be
created even in rvalue contexts.

C<flags> is not used at present but available for future extension to
allow selecting particular classes of magical variable.

=cut
*/
bool
Perl_is_gv_magical(pTHX_ char *name, STRLEN len, U32 flags)
{
    if (!len)
	return FALSE;

    switch (*name) {
    case 'I':
	if (len == 3 && strEQ(name, "ISA"))
	    goto yes;
	break;
    case 'O':
	if (len == 8 && strEQ(name, "OVERLOAD"))
	    goto yes;
	break;
    case 'S':
	if (len == 3 && strEQ(name, "SIG"))
	    goto yes;
	break;
    case '\027':   /* $^W & $^WARNING_BITS */
	if (len == 1
	    || (len == 12 && strEQ(name, "\027ARNING_BITS"))
	    || (len == 17 && strEQ(name, "\027IDE_SYSTEM_CALLS")))
	{
	    goto yes;
	}
	break;

    case '&':
    case '`':
    case '\'':
    case ':':
    case '?':
    case '!':
    case '-':
    case '#':
    case '*':
    case '[':
    case '^':
    case '~':
    case '=':
    case '%':
    case '.':
    case '(':
    case ')':
    case '<':
    case '>':
    case ',':
    case '\\':
    case '/':
    case '|':
    case '+':
    case ';':
    case ']':
    case '\001':   /* $^A */
    case '\003':   /* $^C */
    case '\004':   /* $^D */
    case '\005':   /* $^E */
    case '\006':   /* $^F */
    case '\010':   /* $^H */
    case '\011':   /* $^I, NOT \t in EBCDIC */
    case '\014':   /* $^L */
    case '\017':   /* $^O */
    case '\020':   /* $^P */
    case '\023':   /* $^S */
    case '\024':   /* $^T */
    case '\026':   /* $^V */
	if (len == 1)
	    goto yes;
	break;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
	if (len > 1) {
	    char *end = name + len;
	    while (--end > name) {
		if (!isDIGIT(*end))
		    return FALSE;
	    }
	}
    yes:
	return TRUE;
    default:
	break;
    }
    return FALSE;
}
