/* $RCSfile: stab.c,v $$Revision: 1.2 $$Date: 1995/05/30 05:03:19 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: stab.c,v $
 * Revision 1.2  1995/05/30 05:03:19  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1994/09/10  06:27:33  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:39  nate
 * PERL!
 *
 * Revision 4.0.1.5  1993/02/05  19:42:47  lwall
 * patch36: length returned wrong value on certain semi-magical variables
 *
 * Revision 4.0.1.4  92/06/08  15:32:19  lwall
 * patch20: fixed confusion between a *var's real name and its effective name
 * patch20: the debugger now warns you on lines that can't set a breakpoint
 * patch20: the debugger made perl forget the last pattern used by //
 * patch20: paragraph mode now skips extra newlines automatically
 * patch20: ($<,$>) = ... didn't work on some architectures
 *
 * Revision 4.0.1.3  91/11/05  18:35:33  lwall
 * patch11: length($x) was sometimes wrong for numeric $x
 * patch11: perl now issues warning if $SIG{'ALARM'} is referenced
 * patch11: *foo = undef coredumped
 * patch11: solitary subroutine references no longer trigger typo warnings
 * patch11: local(*FILEHANDLE) had a memory leak
 *
 * Revision 4.0.1.2  91/06/07  11:55:53  lwall
 * patch4: new copyright notice
 * patch4: added $^P variable to control calling of perldb routines
 * patch4: added $^F variable to specify maximum system fd, default 2
 * patch4: $` was busted inside s///
 * patch4: default top-of-form format is now FILEHANDLE_TOP
 * patch4: length($`), length($&), length($') now optimized to avoid string copy
 * patch4: $^D |= 1024 now does syntax tree dump at run-time
 *
 * Revision 4.0.1.1  91/04/12  09:10:24  lwall
 * patch1: Configure now differentiates getgroups() type from getgid() type
 * patch1: you may now use "die" and "caller" in a signal handler
 *
 * Revision 4.0  91/03/20  01:39:41  lwall
 * 4.0 baseline.
 *
 */

#include "EXTERN.h"
#include "perl.h"

#if !defined(NSIG) || defined(M_UNIX) || defined(M_XENIX)
#include <signal.h>
#endif

static char *sig_name[] = {
    SIG_NAME,0
};

#ifdef VOIDSIG
#define handlertype void
#else
#define handlertype int
#endif

static handlertype sighandler();

static int origalen = 0;

STR *
stab_str(str)
STR *str;
{
    STAB *stab = str->str_u.str_stab;
    register int paren;
    register char *s;
    register int i;

    if (str->str_rare)
	return stab_val(stab);

    switch (*stab->str_magic->str_ptr) {
    case '\004':		/* ^D */
#ifdef DEBUGGING
	str_numset(stab_val(stab),(double)(debug & 32767));
#endif
	break;
    case '\006':		/* ^F */
	str_numset(stab_val(stab),(double)maxsysfd);
	break;
    case '\t':			/* ^I */
	if (inplace)
	    str_set(stab_val(stab), inplace);
	else
	    str_sset(stab_val(stab),&str_undef);
	break;
    case '\020':		/* ^P */
	str_numset(stab_val(stab),(double)perldb);
	break;
    case '\024':		/* ^T */
	str_numset(stab_val(stab),(double)basetime);
	break;
    case '\027':		/* ^W */
	str_numset(stab_val(stab),(double)dowarn);
	break;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': case '&':
	if (curspat) {
	    paren = atoi(stab_ename(stab));
	  getparen:
	    if (curspat->spat_regexp &&
	      paren <= curspat->spat_regexp->nparens &&
	      (s = curspat->spat_regexp->startp[paren]) ) {
		i = curspat->spat_regexp->endp[paren] - s;
		if (i >= 0)
		    str_nset(stab_val(stab),s,i);
		else
		    str_sset(stab_val(stab),&str_undef);
	    }
	    else
		str_sset(stab_val(stab),&str_undef);
	}
	break;
    case '+':
	if (curspat) {
	    paren = curspat->spat_regexp->lastparen;
	    goto getparen;
	}
	break;
    case '`':
	if (curspat) {
	    if (curspat->spat_regexp &&
	      (s = curspat->spat_regexp->subbeg) ) {
		i = curspat->spat_regexp->startp[0] - s;
		if (i >= 0)
		    str_nset(stab_val(stab),s,i);
		else
		    str_nset(stab_val(stab),"",0);
	    }
	    else
		str_nset(stab_val(stab),"",0);
	}
	break;
    case '\'':
	if (curspat) {
	    if (curspat->spat_regexp &&
	      (s = curspat->spat_regexp->endp[0]) ) {
		str_nset(stab_val(stab),s, curspat->spat_regexp->subend - s);
	    }
	    else
		str_nset(stab_val(stab),"",0);
	}
	break;
    case '.':
#ifndef lint
	if (last_in_stab && stab_io(last_in_stab)) {
	    str_numset(stab_val(stab),(double)stab_io(last_in_stab)->lines);
	}
#endif
	break;
    case '?':
	str_numset(stab_val(stab),(double)statusvalue);
	break;
    case '^':
	s = stab_io(curoutstab)->top_name;
	if (s)
	    str_set(stab_val(stab),s);
	else {
	    str_set(stab_val(stab),stab_ename(curoutstab));
	    str_cat(stab_val(stab),"_TOP");
	}
	break;
    case '~':
	s = stab_io(curoutstab)->fmt_name;
	if (!s)
	    s = stab_ename(curoutstab);
	str_set(stab_val(stab),s);
	break;
#ifndef lint
    case '=':
	str_numset(stab_val(stab),(double)stab_io(curoutstab)->page_len);
	break;
    case '-':
	str_numset(stab_val(stab),(double)stab_io(curoutstab)->lines_left);
	break;
    case '%':
	str_numset(stab_val(stab),(double)stab_io(curoutstab)->page);
	break;
#endif
    case ':':
	break;
    case '/':
	break;
    case '[':
	str_numset(stab_val(stab),(double)arybase);
	break;
    case '|':
	if (!stab_io(curoutstab))
	    stab_io(curoutstab) = stio_new();
	str_numset(stab_val(stab),
	   (double)((stab_io(curoutstab)->flags & IOF_FLUSH) != 0) );
	break;
    case ',':
	str_nset(stab_val(stab),ofs,ofslen);
	break;
    case '\\':
	str_nset(stab_val(stab),ors,orslen);
	break;
    case '#':
	str_set(stab_val(stab),ofmt);
	break;
    case '!':
	str_numset(stab_val(stab), (double)errno);
	str_set(stab_val(stab), errno ? strerror(errno) : "");
	stab_val(stab)->str_nok = 1;	/* what a wonderful hack! */
	break;
    case '<':
	str_numset(stab_val(stab),(double)uid);
	break;
    case '>':
	str_numset(stab_val(stab),(double)euid);
	break;
    case '(':
	s = buf;
	(void)sprintf(s,"%d",(int)gid);
	goto add_groups;
    case ')':
	s = buf;
	(void)sprintf(s,"%d",(int)egid);
      add_groups:
	while (*s) s++;
#ifdef HAS_GETGROUPS
#ifndef NGROUPS
#define NGROUPS 32
#endif
	{
	    GROUPSTYPE gary[NGROUPS];

	    i = getgroups(NGROUPS,gary);
	    while (--i >= 0) {
		(void)sprintf(s," %ld", (long)gary[i]);
		while (*s) s++;
	    }
	}
#endif
	str_set(stab_val(stab),buf);
	break;
    case '*':
	break;
    case '0':
	break;
    default:
	{
	    struct ufuncs *uf = (struct ufuncs *)str->str_ptr;

	    if (uf && uf->uf_val)
		(*uf->uf_val)(uf->uf_index, stab_val(stab));
	}
	break;
    }
    return stab_val(stab);
}

STRLEN
stab_len(str)
STR *str;
{
    STAB *stab = str->str_u.str_stab;
    int paren;
    int i;
    char *s;

    if (str->str_rare)
	return str_len(stab_val(stab));

    switch (*stab->str_magic->str_ptr) {
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': case '&':
	if (curspat) {
	    paren = atoi(stab_ename(stab));
	  getparen:
	    if (curspat->spat_regexp &&
	      paren <= curspat->spat_regexp->nparens &&
	      (s = curspat->spat_regexp->startp[paren]) ) {
		i = curspat->spat_regexp->endp[paren] - s;
		if (i >= 0)
		    return i;
		else
		    return 0;
	    }
	    else
		return 0;
	}
	break;
    case '+':
	if (curspat) {
	    paren = curspat->spat_regexp->lastparen;
	    goto getparen;
	}
	break;
    case '`':
	if (curspat) {
	    if (curspat->spat_regexp &&
	      (s = curspat->spat_regexp->subbeg) ) {
		i = curspat->spat_regexp->startp[0] - s;
		if (i >= 0)
		    return i;
		else
		    return 0;
	    }
	    else
		return 0;
	}
	break;
    case '\'':
	if (curspat) {
	    if (curspat->spat_regexp &&
	      (s = curspat->spat_regexp->endp[0]) ) {
		return (STRLEN) (curspat->spat_regexp->subend - s);
	    }
	    else
		return 0;
	}
	break;
    case ',':
	return (STRLEN)ofslen;
    case '\\':
	return (STRLEN)orslen;
    }
    return str_len(stab_str(str));
}

void
stabset(mstr,str)
register STR *mstr;
STR *str;
{
    STAB *stab;
    register char *s;
    int i;

    switch (mstr->str_rare) {
    case 'E':
	my_setenv(mstr->str_ptr,str_get(str));
				/* And you'll never guess what the dog had */
				/*   in its mouth... */
#ifdef TAINT
	if (strEQ(mstr->str_ptr,"PATH")) {
	    char *strend = str->str_ptr + str->str_cur;

	    s = str->str_ptr;
	    while (s < strend) {
		s = cpytill(tokenbuf,s,strend,':',&i);
		s++;
		if (*tokenbuf != '/'
		  || (stat(tokenbuf,&statbuf) && (statbuf.st_mode & 2)) )
		    str->str_tainted = 2;
	    }
	}
#endif
	break;
    case 'S':
	s = str_get(str);
	i = whichsig(mstr->str_ptr);	/* ...no, a brick */
	if (!i && (dowarn || strEQ(mstr->str_ptr,"ALARM")))
	    warn("No such signal: SIG%s", mstr->str_ptr);
	if (strEQ(s,"IGNORE"))
#ifndef lint
	    (void)signal(i,SIG_IGN);
#else
	    ;
#endif
	else if (strEQ(s,"DEFAULT") || !*s)
	    (void)signal(i,SIG_DFL);
	else {
	    (void)signal(i,sighandler);
	    if (!index(s,'\'')) {
		sprintf(tokenbuf, "main'%s",s);
		str_set(str,tokenbuf);
	    }
	}
	break;
#ifdef SOME_DBM
    case 'D':
	stab = mstr->str_u.str_stab;
	hdbmstore(stab_hash(stab),mstr->str_ptr,mstr->str_cur,str);
	break;
#endif
    case 'L':
	{
	    CMD *cmd;

	    stab = mstr->str_u.str_stab;
	    i = str_true(str);
	    str = afetch(stab_xarray(stab),atoi(mstr->str_ptr), FALSE);
	    if (str->str_magic && (cmd = str->str_magic->str_u.str_cmd)) {
		cmd->c_flags &= ~CF_OPTIMIZE;
		cmd->c_flags |= i? CFT_D1 : CFT_D0;
	    }
	    else
		warn("Can't break at that line\n");
	}
	break;
    case '#':
	stab = mstr->str_u.str_stab;
	afill(stab_array(stab), (int)str_gnum(str) - arybase);
	break;
    case 'X':	/* merely a copy of a * string */
	break;
    case '*':
	s = str->str_pok ? str_get(str) : "";
	if (strNE(s,"StB") || str->str_cur != sizeof(STBP)) {
	    stab = mstr->str_u.str_stab;
	    if (!*s) {
		STBP *stbp;

		/*SUPPRESS 701*/
		(void)savenostab(stab);	/* schedule a free of this stab */
		if (stab->str_len)
		    Safefree(stab->str_ptr);
		Newz(601,stbp, 1, STBP);
		stab->str_ptr = stbp;
		stab->str_len = stab->str_cur = sizeof(STBP);
		stab->str_pok = 1;
		strcpy(stab_magic(stab),"StB");
		stab_val(stab) = Str_new(70,0);
		stab_line(stab) = curcmd->c_line;
		stab_estab(stab) = stab;
	    }
	    else {
		stab = stabent(s,TRUE);
		if (!stab_xarray(stab))
		    aadd(stab);
		if (!stab_xhash(stab))
		    hadd(stab);
		if (!stab_io(stab))
		    stab_io(stab) = stio_new();
	    }
	    str_sset(str, (STR*) stab);
	}
	break;
    case 's': {
	    struct lstring *lstr = (struct lstring*)str;
	    char *tmps;

	    mstr->str_rare = 0;
	    str->str_magic = Nullstr;
	    tmps = str_get(str);
	    str_insert(mstr,lstr->lstr_offset,lstr->lstr_len,
	      tmps,str->str_cur);
	}
	break;

    case 'v':
	do_vecset(mstr,str);
	break;

    case 0:
	/*SUPPRESS 560*/
	if (!(stab = mstr->str_u.str_stab))
	    break;
	switch (*stab->str_magic->str_ptr) {
	case '\004':	/* ^D */
#ifdef DEBUGGING
	    debug = (int)(str_gnum(str)) | 32768;
	    if (debug & 1024)
		dump_all();
#endif
	    break;
	case '\006':	/* ^F */
	    maxsysfd = (int)str_gnum(str);
	    break;
	case '\t':	/* ^I */
	    if (inplace)
		Safefree(inplace);
	    if (str->str_pok || str->str_nok)
		inplace = savestr(str_get(str));
	    else
		inplace = Nullch;
	    break;
	case '\020':	/* ^P */
	    i = (int)str_gnum(str);
	    if (i != perldb) {
		static SPAT *oldlastspat;

		if (perldb)
		    oldlastspat = lastspat;
		else
		    lastspat = oldlastspat;
	    }
	    perldb = i;
	    break;
	case '\024':	/* ^T */
	    basetime = (time_t)str_gnum(str);
	    break;
	case '\027':	/* ^W */
	    dowarn = (bool)str_gnum(str);
	    break;
	case '.':
	    if (localizing)
		savesptr((STR**)&last_in_stab);
	    break;
	case '^':
	    Safefree(stab_io(curoutstab)->top_name);
	    stab_io(curoutstab)->top_name = s = savestr(str_get(str));
	    stab_io(curoutstab)->top_stab = stabent(s,TRUE);
	    break;
	case '~':
	    Safefree(stab_io(curoutstab)->fmt_name);
	    stab_io(curoutstab)->fmt_name = s = savestr(str_get(str));
	    stab_io(curoutstab)->fmt_stab = stabent(s,TRUE);
	    break;
	case '=':
	    stab_io(curoutstab)->page_len = (long)str_gnum(str);
	    break;
	case '-':
	    stab_io(curoutstab)->lines_left = (long)str_gnum(str);
	    if (stab_io(curoutstab)->lines_left < 0L)
		stab_io(curoutstab)->lines_left = 0L;
	    break;
	case '%':
	    stab_io(curoutstab)->page = (long)str_gnum(str);
	    break;
	case '|':
	    if (!stab_io(curoutstab))
		stab_io(curoutstab) = stio_new();
	    stab_io(curoutstab)->flags &= ~IOF_FLUSH;
	    if (str_gnum(str) != 0.0) {
		stab_io(curoutstab)->flags |= IOF_FLUSH;
	    }
	    break;
	case '*':
	    i = (int)str_gnum(str);
	    multiline = (i != 0);
	    break;
	case '/':
	    if (str->str_pok) {
		rs = str_get(str);
		rslen = str->str_cur;
		if (rspara = !rslen) {
		    rs = "\n\n";
		    rslen = 2;
		}
		rschar = rs[rslen - 1];
	    }
	    else {
		rschar = 0777;	/* fake a non-existent char */
		rslen = 1;
	    }
	    break;
	case '\\':
	    if (ors)
		Safefree(ors);
	    ors = savestr(str_get(str));
	    orslen = str->str_cur;
	    break;
	case ',':
	    if (ofs)
		Safefree(ofs);
	    ofs = savestr(str_get(str));
	    ofslen = str->str_cur;
	    break;
	case '#':
	    if (ofmt)
		Safefree(ofmt);
	    ofmt = savestr(str_get(str));
	    break;
	case '[':
	    arybase = (int)str_gnum(str);
	    break;
	case '?':
	    statusvalue = U_S(str_gnum(str));
	    break;
	case '!':
	    errno = (int)str_gnum(str);		/* will anyone ever use this? */
	    break;
	case '<':
	    uid = (int)str_gnum(str);
	    if (delaymagic) {
		delaymagic |= DM_RUID;
		break;				/* don't do magic till later */
	    }
#ifdef HAS_SETRUID
	    (void)setruid((UIDTYPE)uid);
#else
#ifdef HAS_SETREUID
	    (void)setreuid((UIDTYPE)uid, (UIDTYPE)-1);
#else
	    if (uid == euid)		/* special case $< = $> */
		(void)setuid(uid);
	    else
		fatal("setruid() not implemented");
#endif
#endif
	    uid = (int)getuid();
	    break;
	case '>':
	    euid = (int)str_gnum(str);
	    if (delaymagic) {
		delaymagic |= DM_EUID;
		break;				/* don't do magic till later */
	    }
#ifdef HAS_SETEUID
	    (void)seteuid((UIDTYPE)euid);
#else
#ifdef HAS_SETREUID
	    (void)setreuid((UIDTYPE)-1, (UIDTYPE)euid);
#else
	    if (euid == uid)		/* special case $> = $< */
		setuid(euid);
	    else
		fatal("seteuid() not implemented");
#endif
#endif
	    euid = (int)geteuid();
	    break;
	case '(':
	    gid = (int)str_gnum(str);
	    if (delaymagic) {
		delaymagic |= DM_RGID;
		break;				/* don't do magic till later */
	    }
#ifdef HAS_SETRGID
	    (void)setrgid((GIDTYPE)gid);
#else
#ifdef HAS_SETREGID
	    (void)setregid((GIDTYPE)gid, (GIDTYPE)-1);
#else
	    if (gid == egid)			/* special case $( = $) */
		(void)setgid(gid);
	    else
		fatal("setrgid() not implemented");
#endif
#endif
	    gid = (int)getgid();
	    break;
	case ')':
	    egid = (int)str_gnum(str);
	    if (delaymagic) {
		delaymagic |= DM_EGID;
		break;				/* don't do magic till later */
	    }
#ifdef HAS_SETEGID
	    (void)setegid((GIDTYPE)egid);
#else
#ifdef HAS_SETREGID
	    (void)setregid((GIDTYPE)-1, (GIDTYPE)egid);
#else
	    if (egid == gid)			/* special case $) = $( */
		(void)setgid(egid);
	    else
		fatal("setegid() not implemented");
#endif
#endif
	    egid = (int)getegid();
	    break;
	case ':':
	    chopset = str_get(str);
	    break;
	case '0':
	    if (!origalen) {
		s = origargv[0];
		s += strlen(s);
		/* See if all the arguments are contiguous in memory */
		for (i = 1; i < origargc; i++) {
		    if (origargv[i] == s + 1)
			s += strlen(++s);	/* this one is ok too */
		}
		if (origenviron[0] == s + 1) {	/* can grab env area too? */
		    my_setenv("NoNeSuCh", Nullch);
						/* force copy of environment */
		    for (i = 0; origenviron[i]; i++)
			if (origenviron[i] == s + 1)
			    s += strlen(++s);
		}
		origalen = s - origargv[0];
	    }
	    s = str_get(str);
	    i = str->str_cur;
	    if (i >= origalen) {
		i = origalen;
		str->str_cur = i;
		str->str_ptr[i] = '\0';
		Copy(s, origargv[0], i, char);
	    }
	    else {
		Copy(s, origargv[0], i, char);
		s = origargv[0]+i;
		*s++ = '\0';
		while (++i < origalen)
		    *s++ = ' ';
	    }
	    break;
	default:
	    {
		struct ufuncs *uf = (struct ufuncs *)str->str_magic->str_ptr;

		if (uf && uf->uf_set)
		    (*uf->uf_set)(uf->uf_index, str);
	    }
	    break;
	}
	break;
    }
}

int
whichsig(sig)
char *sig;
{
    register char **sigv;

    for (sigv = sig_name+1; *sigv; sigv++)
	if (strEQ(sig,*sigv))
	    return sigv - sig_name;
#ifdef SIGCLD
    if (strEQ(sig,"CHLD"))
	return SIGCLD;
#endif
#ifdef SIGCHLD
    if (strEQ(sig,"CLD"))
	return SIGCHLD;
#endif
    return 0;
}

static handlertype
sighandler(sig)
int sig;
{
    STAB *stab;
    STR *str;
    int oldsave = savestack->ary_fill;
    int oldtmps_base = tmps_base;
    register CSV *csv;
    SUBR *sub;

#ifdef OS2		/* or anybody else who requires SIG_ACK */
    signal(sig, SIG_ACK);
#endif
    stab = stabent(
	str_get(hfetch(stab_hash(sigstab),sig_name[sig],strlen(sig_name[sig]),
	  TRUE)), TRUE);
    sub = stab_sub(stab);
    if (!sub && *sig_name[sig] == 'C' && instr(sig_name[sig],"LD")) {
	if (sig_name[sig][1] == 'H')
	    stab = stabent(str_get(hfetch(stab_hash(sigstab),"CLD",3,TRUE)),
	      TRUE);
	else
	    stab = stabent(str_get(hfetch(stab_hash(sigstab),"CHLD",4,TRUE)),
	      TRUE);
	sub = stab_sub(stab);	/* gag */
    }
    if (!sub) {
	if (dowarn)
	    warn("SIG%s handler \"%s\" not defined.\n",
		sig_name[sig], stab_ename(stab) );
	return;
    }
    /*SUPPRESS 701*/
    saveaptr(&stack);
    str = Str_new(15, sizeof(CSV));
    str->str_state = SS_SCSV;
    (void)apush(savestack,str);
    csv = (CSV*)str->str_ptr;
    csv->sub = sub;
    csv->stab = stab;
    csv->curcsv = curcsv;
    csv->curcmd = curcmd;
    csv->depth = sub->depth;
    csv->wantarray = G_SCALAR;
    csv->hasargs = TRUE;
    csv->savearray = stab_xarray(defstab);
    csv->argarray = stab_xarray(defstab) = stack = anew(defstab);
    stack->ary_flags = 0;
    curcsv = csv;
    str = str_mortal(&str_undef);
    str_set(str,sig_name[sig]);
    (void)apush(stab_xarray(defstab),str);
    sub->depth++;
    if (sub->depth >= 2) {	/* save temporaries on recursion? */
	if (sub->depth == 100 && dowarn)
	    warn("Deep recursion on subroutine \"%s\"",stab_ename(stab));
	savelist(sub->tosave->ary_array,sub->tosave->ary_fill);
    }

    tmps_base = tmps_max;		/* protect our mortal string */
    (void)cmd_exec(sub->cmd,G_SCALAR,0);		/* so do it already */
    tmps_base = oldtmps_base;

    restorelist(oldsave);		/* put everything back */
}

STAB *
aadd(stab)
register STAB *stab;
{
    if (!stab_xarray(stab))
	stab_xarray(stab) = anew(stab);
    return stab;
}

STAB *
hadd(stab)
register STAB *stab;
{
    if (!stab_xhash(stab))
	stab_xhash(stab) = hnew(COEFFSIZE);
    return stab;
}

STAB *
fstab(name)
char *name;
{
    char tmpbuf[1200];
    STAB *stab;

    snprintf(tmpbuf,sizeof(tmpbuf), "'_<%s", name);
    stab = stabent(tmpbuf, TRUE);
    str_set(stab_val(stab), name);
    if (perldb)
	(void)hadd(aadd(stab));
    return stab;
}

STAB *
stabent(name,add)
register char *name;
int add;
{
    register STAB *stab;
    register STBP *stbp;
    int len;
    register char *namend;
    HASH *stash;
    char *sawquote = Nullch;
    char *prevquote = Nullch;
    bool global = FALSE;

    if (isUPPER(*name)) {
	if (*name > 'I') {
	    if (*name == 'S' && (
	      strEQ(name, "SIG") ||
	      strEQ(name, "STDIN") ||
	      strEQ(name, "STDOUT") ||
	      strEQ(name, "STDERR") ))
		global = TRUE;
	}
	else if (*name > 'E') {
	    if (*name == 'I' && strEQ(name, "INC"))
		global = TRUE;
	}
	else if (*name > 'A') {
	    if (*name == 'E' && strEQ(name, "ENV"))
		global = TRUE;
	}
	else if (*name == 'A' && (
	  strEQ(name, "ARGV") ||
	  strEQ(name, "ARGVOUT") ))
	    global = TRUE;
    }
    for (namend = name; *namend; namend++) {
	if (*namend == '\'' && namend[1])
	    prevquote = sawquote, sawquote = namend;
    }
    if (sawquote == name && name[1]) {
	stash = defstash;
	sawquote = Nullch;
	name++;
    }
    else if (!isALPHA(*name) || global)
	stash = defstash;
    else if ((CMD*)curcmd == &compiling)
	stash = curstash;
    else
	stash = curcmd->c_stash;
    if (sawquote) {
	char tmpbuf[256];
	char *s, *d;

	*sawquote = '\0';
	/*SUPPRESS 560*/
	if (s = prevquote) {
	    strncpy(tmpbuf,name,s-name+1);
	    d = tmpbuf+(s-name+1);
	    *d++ = '_';
	    strcpy(d,s+1);
	}
	else {
	    *tmpbuf = '_';
	    strcpy(tmpbuf+1,name);
	}
	stab = stabent(tmpbuf,TRUE);
	if (!(stash = stab_xhash(stab)))
	    stash = stab_xhash(stab) = hnew(0);
	if (!stash->tbl_name)
	    stash->tbl_name = savestr(name);
	name = sawquote+1;
	*sawquote = '\'';
    }
    len = namend - name;
    stab = (STAB*)hfetch(stash,name,len,add);
    if (stab == (STAB*)&str_undef)
	return Nullstab;
    if (stab->str_pok) {
	stab->str_pok |= SP_MULTI;
	return stab;
    }
    else {
	if (stab->str_len)
	    Safefree(stab->str_ptr);
	Newz(602,stbp, 1, STBP);
	stab->str_ptr = stbp;
	stab->str_len = stab->str_cur = sizeof(STBP);
	stab->str_pok = 1;
	strcpy(stab_magic(stab),"StB");
	stab_val(stab) = Str_new(72,0);
	stab_line(stab) = curcmd->c_line;
	stab_estab(stab) = stab;
	str_magic((STR*)stab, stab, '*', name, len);
	stab_stash(stab) = stash;
	if (isDIGIT(*name) && *name != '0') {
	    stab_flags(stab) = SF_VMAGIC;
	    str_magic(stab_val(stab), stab, 0, Nullch, 0);
	}
	if (add & 2)
	    stab->str_pok |= SP_MULTI;
	return stab;
    }
}

void
stab_fullname(str,stab)
STR *str;
STAB *stab;
{
    HASH *tb = stab_stash(stab);

    if (!tb)
	return;
    str_set(str,tb->tbl_name);
    str_ncat(str,"'", 1);
    str_scat(str,stab->str_magic);
}

void
stab_efullname(str,stab)
STR *str;
STAB *stab;
{
    HASH *tb = stab_estash(stab);

    if (!tb)
	return;
    str_set(str,tb->tbl_name);
    str_ncat(str,"'", 1);
    str_scat(str,stab_estab(stab)->str_magic);
}

STIO *
stio_new()
{
    STIO *stio;

    Newz(603,stio,1,STIO);
    stio->page_len = 60;
    return stio;
}

void
stab_check(min,max)
int min;
register int max;
{
    register HENT *entry;
    register int i;
    register STAB *stab;

    for (i = min; i <= max; i++) {
	for (entry = defstash->tbl_array[i]; entry; entry = entry->hent_next) {
	    stab = (STAB*)entry->hent_val;
	    if (stab->str_pok & SP_MULTI)
		continue;
	    curcmd->c_line = stab_line(stab);
	    warn("Possible typo: \"%s\"", stab_name(stab));
	}
    }
}

static int gensym = 0;

STAB *
genstab()
{
    (void)sprintf(tokenbuf,"_GEN_%d",gensym++);
    return stabent(tokenbuf,TRUE);
}

/* hopefully this is only called on local symbol table entries */

void
stab_clear(stab)
register STAB *stab;
{
    STIO *stio;
    SUBR *sub;

    if (!stab || !stab->str_ptr)
	return;
    afree(stab_xarray(stab));
    stab_xarray(stab) = Null(ARRAY*);
    (void)hfree(stab_xhash(stab), FALSE);
    stab_xhash(stab) = Null(HASH*);
    str_free(stab_val(stab));
    stab_val(stab) = Nullstr;
    /*SUPPRESS 560*/
    if (stio = stab_io(stab)) {
	do_close(stab,FALSE);
	Safefree(stio->top_name);
	Safefree(stio->fmt_name);
	Safefree(stio);
    }
    /*SUPPRESS 560*/
    if (sub = stab_sub(stab)) {
	afree(sub->tosave);
	cmd_free(sub->cmd);
    }
    Safefree(stab->str_ptr);
    stab->str_ptr = Null(STBP*);
    stab->str_len = 0;
    stab->str_cur = 0;
}

#if defined(CRIPPLED_CC) && (defined(iAPX286) || defined(M_I286) || defined(I80286))
#define MICROPORT
#endif

#ifdef	MICROPORT	/* Microport 2.4 hack */
ARRAY *stab_array(stab)
register STAB *stab;
{
    if (((STBP*)(stab->str_ptr))->stbp_array)
	return ((STBP*)(stab->str_ptr))->stbp_array;
    else
	return ((STBP*)(aadd(stab)->str_ptr))->stbp_array;
}

HASH *stab_hash(stab)
register STAB *stab;
{
    if (((STBP*)(stab->str_ptr))->stbp_hash)
	return ((STBP*)(stab->str_ptr))->stbp_hash;
    else
	return ((STBP*)(hadd(stab)->str_ptr))->stbp_hash;
}
#endif			/* Microport 2.4 hack */
