/*    gv.c
 *
 *    Copyright (c) 1991-1999, Larry Wall
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
#include "perl.h"

GV *
gv_AVadd(register GV *gv)
{
    if (!gv || SvTYPE((SV*)gv) != SVt_PVGV)
	croak("Bad symbol for array");
    if (!GvAV(gv))
	GvAV(gv) = newAV();
    return gv;
}

GV *
gv_HVadd(register GV *gv)
{
    if (!gv || SvTYPE((SV*)gv) != SVt_PVGV)
	croak("Bad symbol for hash");
    if (!GvHV(gv))
	GvHV(gv) = newHV();
    return gv;
}

GV *
gv_IOadd(register GV *gv)
{
    if (!gv || SvTYPE((SV*)gv) != SVt_PVGV)
	croak("Bad symbol for filehandle");
    if (!GvIOp(gv))
	GvIOp(gv) = newIO();
    return gv;
}

GV *
gv_fetchfile(char *name)
{
    dTHR;
    char smallbuf[256];
    char *tmpbuf;
    STRLEN tmplen;
    GV *gv;

    tmplen = strlen(name) + 2;
    if (tmplen < sizeof smallbuf)
	tmpbuf = smallbuf;
    else
	New(603, tmpbuf, tmplen + 1, char);
    tmpbuf[0] = '_';
    tmpbuf[1] = '<';
    strcpy(tmpbuf + 2, name);
    gv = *(GV**)hv_fetch(PL_defstash, tmpbuf, tmplen, TRUE);
    if (!isGV(gv))
	gv_init(gv, PL_defstash, tmpbuf, tmplen, FALSE);
    if (tmpbuf != smallbuf)
	Safefree(tmpbuf);
    sv_setpv(GvSV(gv), name);
    if (*name == '/' && (instr(name, "/lib/") || instr(name, ".pm")))
	GvMULTI_on(gv);
    if (PERLDB_LINE)
	hv_magic(GvHVn(gv_AVadd(gv)), gv, 'L');
    return gv;
}

void
gv_init(GV *gv, HV *stash, char *name, STRLEN len, int multi)
{
    dTHR;
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
    GvLINE(gv) = PL_curcop->cop_line;
    GvFILEGV(gv) = PL_curcop->cop_filegv;
    GvCVGEN(gv) = 0;
    GvEGV(gv) = gv;
    sv_magic((SV*)gv, (SV*)gv, '*', name, len);
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
	CvGV(GvCV(gv)) = (GV*)SvREFCNT_inc(gv);
	CvFILEGV(GvCV(gv)) = PL_curcop->cop_filegv;
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
gv_init_sv(GV *gv, I32 sv_type)
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

GV *
gv_fetchmeth(HV *stash, char *name, STRLEN len, I32 level)
{
    AV* av;
    GV* topgv;
    GV* gv;
    GV** gvp;
    CV* cv;

    if (!stash)
	return 0;
    if ((level > 100) || (level < -100))
	croak("Recursive inheritance detected while looking for method '%s' in package '%s'",
	      name, HvNAME(stash));

    DEBUG_o( deb("Looking for method %s in package %s\n",name,HvNAME(stash)) );

    gvp = (GV**)hv_fetch(stash, name, len, (level >= 0));
    if (!gvp)
	topgv = Nullgv;
    else {
	topgv = *gvp;
	if (SvTYPE(topgv) != SVt_PVGV)
	    gv_init(topgv, stash, name, len, TRUE);
	if (cv = GvCV(topgv)) {
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
		dTHR;		/* just for SvREFCNT_dec */
		gvp = (GV**)hv_fetch(stash, "ISA", 3, TRUE);
		if (!gvp || !(gv = *gvp))
		    croak("Cannot create %s::ISA", HvNAME(stash));
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
		if (PL_dowarn)
		    warn("Can't locate package %s for @%s::ISA",
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

	if (lastchance = gv_stashpvn("UNIVERSAL", 9, FALSE)) {
	    if (gv = gv_fetchmeth(lastchance, name, len,
				  (level >= 0) ? level + 1 : level - 1)) {
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
		    if (cv = GvCV(topgv))
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

GV *
gv_fetchmethod(HV *stash, char *name)
{
    return gv_fetchmethod_autoload(stash, name, TRUE);
}

GV *
gv_fetchmethod_autoload(HV *stash, char *name, I32 autoload)
{
    dTHR;
    register char *nend;
    char *nsplit = 0;
    GV* gv;
    
    for (nend = name; *nend; nend++) {
	if (*nend == '\'')
	    nsplit = nend;
	else if (*nend == ':' && *(nend + 1) == ':')
	    nsplit = ++nend;
    }
    if (nsplit) {
	char *origname = name;
	name = nsplit + 1;
	if (*nsplit == ':')
	    --nsplit;
	if ((nsplit - origname) == 5 && strnEQ(origname, "SUPER", 5)) {
	    /* ->SUPER::method should really be looked up in original stash */
	    SV *tmpstr = sv_2mortal(newSVpvf("%s::SUPER",
					     HvNAME(PL_curcop->cop_stash)));
	    stash = gv_stashpvn(SvPVX(tmpstr), SvCUR(tmpstr), TRUE);
	    DEBUG_o( deb("Treating %s as %s::%s\n",
			 origname, HvNAME(stash), name) );
	}
	else
	    stash = gv_stashpvn(origname, nsplit - origname, TRUE);
    }

    gv = gv_fetchmeth(stash, name, nend - name, 0);
    if (!gv) {
	if (strEQ(name,"import"))
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
gv_autoload4(HV *stash, char *name, STRLEN len, I32 method)
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

    /*
     * Inheriting AUTOLOAD for non-methods works ... for now.
     */
    if (PL_dowarn && !method && (GvCVGEN(gv) || GvSTASH(gv) != stash))
	warn(
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
    if (!isGV(vargv))
	gv_init(vargv, varstash, autoload, autolen, FALSE);
    varsv = GvSV(vargv);
    sv_setpv(varsv, HvNAME(stash));
    sv_catpvn(varsv, "::", 2);
    sv_catpvn(varsv, name, len);
    SvTAINTED_off(varsv);
    return gv;
}

HV*
gv_stashpv(char *name, I32 create)
{
    return gv_stashpvn(name, strlen(name), create);
}

HV*
gv_stashpvn(char *name, U32 namelen, I32 create)
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

HV*
gv_stashsv(SV *sv, I32 create)
{
    register char *ptr;
    STRLEN len;
    ptr = SvPV(sv,len);
    return gv_stashpvn(ptr, len, create);
}


GV *
gv_fetchpv(char *nambeg, I32 add, I32 sv_type)
{
    dTHR;
    register char *name = nambeg;
    register GV *gv = 0;
    GV**gvp;
    I32 len;
    register char *namend;
    HV *stash = 0;
    U32 add_gvflags = 0;

    if (*name == '*' && isALPHA(name[1])) /* accidental stringify on a GV? */
	name++;

    for (namend = name; *namend; namend++) {
	if ((*namend == '\'' && namend[1]) ||
	    (*namend == ':' && namend[1] == ':'))
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
	if (isIDFIRST(*name)) {
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
		    else if (sv_type == SVt_PV   && !GvIMPORTED_SV(*gvp) ||
			     sv_type == SVt_PVAV && !GvIMPORTED_AV(*gvp) ||
			     sv_type == SVt_PVHV && !GvIMPORTED_HV(*gvp) )
		    {
			warn("Variable \"%c%s\" is not imported",
			    sv_type == SVt_PVAV ? '@' :
			    sv_type == SVt_PVHV ? '%' : '$',
			    name);
			if (GvCVu(*gvp))
			    warn("(Did you mean &%s instead?)\n", name);
			stash = 0;
		    }
		}
	    }
	    else
		stash = PL_curcop->cop_stash;
	}
	else
	    stash = PL_defstash;
    }

    /* By this point we should have a stash and a name */

    if (!stash) {
	if (!add)
	    return Nullgv;
	if (add & ~GV_ADDMULTI) {
	    char sv_type_char = ((sv_type == SVt_PV) ? '$'
				 : (sv_type == SVt_PVAV) ? '@'
				 : (sv_type == SVt_PVHV) ? '%'
				 : 0);
	    if (sv_type_char) 
		warn("Global symbol \"%c%s\" requires explicit package name",
		     sv_type_char, name);
	    else
		warn("Global symbol \"%s\" requires explicit package name",
		     name);
	}
	++PL_error_count;
	stash = PL_curstash ? PL_curstash : PL_defstash;	/* avoid core dumps */
	add_gvflags = ((sv_type == SVt_PV) ? GVf_IMPORTED_SV
		       : (sv_type == SVt_PVAV) ? GVf_IMPORTED_AV
		       : (sv_type == SVt_PVHV) ? GVf_IMPORTED_HV
		       : 0);
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

    if (add & GV_ADDWARN)
	warn("Had to create %s unexpectedly", nambeg);
    gv_init(gv, stash, name, len, add & GV_ADDMULTI);
    gv_init_sv(gv, sv_type);
    GvFLAGS(gv) |= add_gvflags;

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
		av_push(av, newSVpv(pname = "NDBM_File",0));
		gv_stashpvn(pname, 9, TRUE);
		av_push(av, newSVpv(pname = "DB_File",0));
		gv_stashpvn(pname, 7, TRUE);
		av_push(av, newSVpv(pname = "GDBM_File",0));
		gv_stashpvn(pname, 9, TRUE);
		av_push(av, newSVpv(pname = "SDBM_File",0));
		gv_stashpvn(pname, 9, TRUE);
		av_push(av, newSVpv(pname = "ODBM_File",0));
		gv_stashpvn(pname, 9, TRUE);
	    }
	}
	break;
#ifdef OVERLOAD
    case 'O':
        if (strEQ(name, "OVERLOAD")) {
            HV* hv = GvHVn(gv);
            GvMULTI_on(gv);
            hv_magic(hv, gv, 'A');
        }
        break;
#endif /* OVERLOAD */
    case 'S':
	if (strEQ(name, "SIG")) {
	    HV *hv;
	    I32 i;
	    PL_siggv = gv;
	    GvMULTI_on(PL_siggv);
	    hv = GvHVn(PL_siggv);
	    hv_magic(hv, PL_siggv, 'S');
	    for(i=1;sig_name[i];i++) {
	    	SV ** init;
	    	init=hv_fetch(hv,sig_name[i],strlen(sig_name[i]),1);
	    	if(init)
	    		sv_setsv(*init,&PL_sv_undef);
	    	psig_ptr[i] = 0;
	    	psig_name[i] = 0;
	    }
	}
	break;

    case '&':
	if (len > 1)
	    break;
	PL_ampergv = gv;
	PL_sawampersand = TRUE;
	goto ro_magicalize;

    case '`':
	if (len > 1)
	    break;
	PL_leftgv = gv;
	PL_sawampersand = TRUE;
	goto ro_magicalize;

    case '\'':
	if (len > 1)
	    break;
	PL_rightgv = gv;
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
	sv_upgrade(GvSV(gv), SVt_PVLV);
#endif
	goto magicalize;

    case '!':
	if (len > 1)
	    break;
	if (sv_type > SVt_PV && PL_curcop != &PL_compiling) {
	    HV* stash = gv_stashpvn("Errno",5,FALSE);
	    if(!stash || !(gv_fetchmethod(stash, "TIEHASH"))) {
		dSP;
		PUTBACK;
		perl_require_pv("Errno.pm");
		SPAGAIN;
		stash = gv_stashpvn("Errno",5,FALSE);
		if (!stash || !(gv_fetchmethod(stash, "TIEHASH")))
		    croak("Can't use %%! because Errno.pm is not available");
	    }
	}
	goto magicalize;
    case '#':
    case '*':
	if (PL_dowarn && len == 1 && sv_type == SVt_PV)
	    warn("Use of $%s is deprecated", name);
	/* FALL THROUGH */
    case '[':
    case '^':
    case '~':
    case '=':
    case '-':
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
    case '\001':
    case '\003':
    case '\004':
    case '\005':
    case '\006':
    case '\010':
    case '\011':	/* NOT \t in EBCDIC */
    case '\017':
    case '\020':
    case '\024':
    case '\027':
	if (len > 1)
	    break;
	goto magicalize;

    case '+':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '\023':
      ro_magicalize:
	SvREADONLY_on(GvSV(gv));
      magicalize:
	sv_magic(GvSV(gv), (SV*)gv, 0, name, len);
	break;

    case '\014':
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
	    sv_upgrade(sv, SVt_PVNV);
	    sv_setpv(sv, PL_patchlevel);
	    (void)sv_2nv(sv);
	    SvREADONLY_on(sv);
	}
	break;
    }
    return gv;
}

void
gv_fullname3(SV *sv, GV *gv, char *prefix)
{
    HV *hv = GvSTASH(gv);
    if (!hv) {
	SvOK_off(sv);
	return;
    }
    sv_setpv(sv, prefix ? prefix : "");
    sv_catpv(sv,HvNAME(hv));
    sv_catpvn(sv,"::", 2);
    sv_catpvn(sv,GvNAME(gv),GvNAMELEN(gv));
}

void
gv_efullname3(SV *sv, GV *gv, char *prefix)
{
    GV *egv = GvEGV(gv);
    if (!egv)
	egv = gv;
    gv_fullname3(sv, egv, prefix);
}

/* XXX compatibility with versions <= 5.003. */
void
gv_fullname(SV *sv, GV *gv)
{
    gv_fullname3(sv, gv, sv == (SV*)gv ? "*" : "");
}

/* XXX compatibility with versions <= 5.003. */
void
gv_efullname(SV *sv, GV *gv)
{
    gv_efullname3(sv, gv, sv == (SV*)gv ? "*" : "");
}

IO *
newIO(void)
{
    dTHR;
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
gv_check(HV *stash)
{
    dTHR;
    register HE *entry;
    register I32 i;
    register GV *gv;
    HV *hv;
    GV *filegv;

    if (!HvARRAY(stash))
	return;
    for (i = 0; i <= (I32) HvMAX(stash); i++) {
	for (entry = HvARRAY(stash)[i]; entry; entry = HeNEXT(entry)) {
	    if (HeKEY(entry)[HeKLEN(entry)-1] == ':' &&
		(gv = (GV*)HeVAL(entry)) && (hv = GvHV(gv)) && HvNAME(hv))
	    {
		if (hv != PL_defstash)
		     gv_check(hv);              /* nested package */
	    }
	    else if (isALPHA(*HeKEY(entry))) {
		gv = (GV*)HeVAL(entry);
		if (SvTYPE(gv) != SVt_PVGV || GvMULTI(gv))
		    continue;
		PL_curcop->cop_line = GvLINE(gv);
		filegv = GvFILEGV(gv);
		PL_curcop->cop_filegv = filegv;
		if (filegv && GvMULTI(filegv))	/* Filename began with slash */
		    continue;
		warn("Name \"%s::%s\" used only once: possible typo",
			HvNAME(stash), GvNAME(gv));
	    }
	}
    }
}

GV *
newGVgen(char *pack)
{
    return gv_fetchpv(form("%s::_GEN_%ld", pack, (long)PL_gensym++),
		      TRUE, SVt_PVGV);
}

/* hopefully this is only called on local symbol table entries */

GP*
gp_ref(GP *gp)
{
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
gp_free(GV *gv)
{
    GP* gp;
    CV* cv;

    if (!gv || !(gp = GvGP(gv)))
	return;
    if (gp->gp_refcnt == 0) {
        warn("Attempt to free unreferenced glob pointers");
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

#ifdef OVERLOAD
/* Updates and caches the CV's */

bool
Gv_AMupdate(HV *stash)
{
  dTHR;  
  GV** gvp;
  HV* hv;
  GV* gv;
  CV* cv;
  MAGIC* mg=mg_find((SV*)stash,'c');
  AMT *amtp = (mg) ? (AMT*)mg->mg_ptr: (AMT *) NULL;
  AMT amt;
  STRLEN n_a;

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

  DEBUG_o( deb("Recalcing overload magic in package %s\n",HvNAME(stash)) );

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

    /* Work with "fallback" key, which we assume to be first in AMG_names */

    if (( cp = (char *)AMG_names[0] ) &&
	(svp = (SV**)hv_fetch(hv,cp,strlen(cp),FALSE)) && (sv = *svp)) {
      if (SvTRUE(sv)) amt.fallback=AMGfallYES;
      else if (SvOK(sv)) amt.fallback=AMGfallNEVER;
    }
    for (i = 1; i < NofAMmeth; i++) {
      cv = 0;
      cp = (char *)AMG_names[i];
      
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
	      croak("Not a subroutine reference in overload table");
	      return FALSE;
            case SVt_PVCV:
              cv = (CV*)sv;
              break;
            case SVt_PVGV:
              if (!(cv = GvCVu((GV*)sv)))
                cv = sv_2cv(sv, &stash, &gv, TRUE);
              break;
          }
          if (cv) filled=1;
	  else {
	    croak("Method for operation %s not found in package %.256s during blessing\n",
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
    SV** svp;

    /* Work with "fallback" key, which we assume to be first in AMG_names */

    if ( cp = AMG_names[0] ) {
	/* Try to find via inheritance. */
	gv = gv_fetchmeth(stash, "()", 2, -1); /* A cookie: "()". */
	if (gv) sv = GvSV(gv);

	if (!gv) goto no_table;
	else if (SvTRUE(sv)) amt.fallback=AMGfallYES;
	else if (SvOK(sv)) amt.fallback=AMGfallNEVER;
    }

    for (i = 1; i < NofAMmeth; i++) {
	SV *cookie = sv_2mortal(newSVpvf("(%s", cp = AMG_names[i]));
	DEBUG_o( deb("Checking overloading of `%s' in package `%.256s'\n",
		     cp, HvNAME(stash)) );
	/* don't fill the cache while looking up! */
	gv = gv_fetchmeth(stash, SvPVX(cookie), SvCUR(cookie), -1);
        cv = 0;
        if(gv && (cv = GvCV(gv))) {
	    if (GvNAMELEN(CvGV(cv)) == 3 && strEQ(GvNAME(CvGV(cv)), "nil")
		&& strEQ(HvNAME(GvSTASH(CvGV(cv))), "overload")) {
		/* GvSV contains the name of the method. */
		GV *ngv;
		
		DEBUG_o( deb("Resolving method `%.256s' for overloaded `%s' in package `%.256s'\n", 
			     SvPV(GvSV(gv), n_a), cp, HvNAME(stash)) );
		if (!SvPOK(GvSV(gv)) 
		    || !(ngv = gv_fetchmethod_autoload(stash, SvPVX(GvSV(gv)),
						       FALSE)))
		{
		    /* Can be an import stub (created by `can'). */
		    if (GvCVGEN(gv)) {
			croak("Stub found while resolving method `%.256s' overloading `%s' in package `%.256s'", 
			      (SvPOK(GvSV(gv)) ?  SvPVX(GvSV(gv)) : "???" ),
			      cp, HvNAME(stash));
		    } else
			croak("Cannot resolve method `%.256s' overloading `%s' in package `%.256s'", 
			      (SvPOK(GvSV(gv)) ?  SvPVX(GvSV(gv)) : "???" ),
			      cp, HvNAME(stash));
		}
		cv = GvCV(gv = ngv);
	    }
	    DEBUG_o( deb("Overloading `%s' in package `%.256s' via `%.256s::%.256s' \n",
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
amagic_call(SV *left, SV *right, int method, int flags)
{
  dTHR;
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
	   if (cv = cvp[off=subtr_amg]) {
	     right = left;
	     left = sv_2mortal(newSViv(0));
	     lr = 1;
	   }
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
      if (ocvp && (cv=ocvp[nomethod_amg])) { /* Call report method */
	notfound = 1; lr = -1;
      } else if (cvp && (cv=cvp[nomethod_amg])) {
	notfound = 1; lr = 1;
      } else {
	SV *msg;
	if (off==-1) off=method;
	msg = sv_2mortal(newSVpvf(
		      "Operation `%s': no method found,%sargument %s%s%s%s",
		      AMG_names[method + assignshift],
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
	  DEBUG_o( deb("%s", SvPVX(msg)) );
	} else {
	  croak("%_", msg);
	}
	return NULL;
      }
      force_cpy = force_cpy || assign;
    }
  }
  if (!notfound) {
    DEBUG_o( deb(
  "Overloaded operator `%s'%s%s%s:\n\tmethod%s found%s in package %s%s\n",
		 AMG_names[off],
		 method+assignshift==off? "" :
		             " (initially `",
		 method+assignshift==off? "" :
		             AMG_names[method+assignshift],
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
    pp_pushmark(ARGS);

    EXTEND(SP, notfound + 5);
    PUSHs(lr>0? right: left);
    PUSHs(lr>0? left: right);
    PUSHs( lr > 0 ? &PL_sv_yes : ( assign ? &PL_sv_undef : &PL_sv_no ));
    if (notfound) {
      PUSHs( sv_2mortal(newSVpv((char *)AMG_names[method + assignshift],0)) );
    }
    PUSHs((SV*)cv);
    PUTBACK;

    if (PL_op = pp_entersub(ARGS))
      CALLRUNOPS();
    LEAVE;
    SPAGAIN;

    res=POPs;
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
	ans=!SvOK(res); break;
      }
      return boolSV(ans);
    } else if (method==copy_amg) {
      if (!SvROK(res)) {
	croak("Copy method did not return a reference");
      }
      return SvREFCNT_inc(SvRV(res));
    } else {
      return res;
    }
  }
}
#endif /* OVERLOAD */

