char rcsid[] = "$RCSfile: perl.c,v $$Revision: 1.5 $$Date: 1996/06/02 19:59:24 $\nPatch level: ###\n";
/*
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: perl.c,v $
 * Revision 1.5  1996/06/02 19:59:24  gpalmer
 * Use setreuid instead of seteuid for permissions management
 *
 * Revision 1.4  1995/05/30 05:03:10  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.3  1995/05/28  19:21:54  ache
 * Fix $] variable value (version number), close PR 449
 * Submitted by: Bill Fenner <fenner@parc.xerox.com>
 *
 * Revision 1.2  1994/10/27  23:16:54  wollman
 * Convince Perl to that is is part of the system, as /usr/bin/perl (binary)
 * and /usr/share/perl (library).  The latter was chosen as analogous to other
 * directories already present in /usr/share, like /usr/share/groff_font and
 * (particularly) /usr/share/mk.
 *
 * Revision 1.1.1.1  1994/09/10  06:27:33  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:37  nate
 * PERL!
 *
 * Revision 4.0.1.8  1993/02/05  19:39:30  lwall
 * patch36: the taintanyway code wasn't tainting anyway
 * patch36: Malformed cmd links core dump apparently fixed
 *
 * Revision 4.0.1.7  92/06/08  14:50:39  lwall
 * patch20: PERLLIB now supports multiple directories
 * patch20: running taintperl explicitly now does checks even if $< == $>
 * patch20: -e 'cmd' no longer fails silently if /tmp runs out of space
 * patch20: perl -P now uses location of sed determined by Configure
 * patch20: form feed for formats is now specifiable via $^L
 * patch20: paragraph mode now skips extra newlines automatically
 * patch20: eval "1 #comment" didn't work
 * patch20: couldn't require . files
 * patch20: semantic compilation errors didn't abort execution
 *
 * Revision 4.0.1.6  91/11/11  16:38:45  lwall
 * patch19: default arg for shift was wrong after first subroutine definition
 * patch19: op/regexp.t failed from missing arg to bcmp()
 *
 * Revision 4.0.1.5  91/11/05  18:03:32  lwall
 * patch11: random cleanup
 * patch11: $0 was being truncated at times
 * patch11: cppstdin now installed outside of source directory
 * patch11: -P didn't allow use of #elif or #undef
 * patch11: prepared for ctype implementations that don't define isascii()
 * patch11: added eval {}
 * patch11: eval confused by string containing null
 *
 * Revision 4.0.1.4  91/06/10  01:23:07  lwall
 * patch10: perl -v printed incorrect copyright notice
 *
 * Revision 4.0.1.3  91/06/07  11:40:18  lwall
 * patch4: changed old $^P to $^X
 *
 * Revision 4.0.1.2  91/06/07  11:26:16  lwall
 * patch4: new copyright notice
 * patch4: added $^P variable to control calling of perldb routines
 * patch4: added $^F variable to specify maximum system fd, default 2
 * patch4: debugger lost track of lines in eval
 *
 * Revision 4.0.1.1  91/04/11  17:49:05  lwall
 * patch1: fixed undefined environ problem
 *
 * Revision 4.0  91/03/20  01:37:44  lwall
 * 4.0 baseline.
 *
 */

/*SUPPRESS 560*/

#include "EXTERN.h"
#include "perl.h"
#include "perly.h"
#include "patchlevel.h"

char *getenv();

#ifdef IAMSUID
#ifndef DOSUID
#define DOSUID
#endif
#endif

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef DOSUID
#undef DOSUID
#endif
#endif

static char* moreswitches();
static void incpush();
static char* cddir;
static bool minus_c;
static char patchlevel[6];
static char *nrs = "\n";
static int nrschar = '\n';      /* final char of rs, or 0777 if none */
static int nrslen = 1;
static int fdscript = -1;

main(argc,argv,env)
register int argc;
register char **argv;
register char **env;
{
    register STR *str;
    register char *s;
    char *scriptname;
    char *getenv();
    bool dosearch = FALSE;
#ifdef DOSUID
    char *validarg = "";
#endif
    int which;

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef IAMSUID
#undef IAMSUID
    fatal("suidperl is no longer needed since the kernel can now execute\n\
setuid perl scripts securely.\n");
#endif
#endif

    origargv = argv;
    origargc = argc;
    origenviron = environ;
    uid = (int)getuid();
    euid = (int)geteuid();
    gid = (int)getgid();
    egid = (int)getegid();
    sprintf(patchlevel,"%3.3s%2.2d", "4.0", PATCHLEVEL);
#ifdef MSDOS
    /*
     * There is no way we can refer to them from Perl so close them to save
     * space.  The other alternative would be to provide STDAUX and STDPRN
     * filehandles.
     */
    (void)fclose(stdaux);
    (void)fclose(stdprn);
#endif
    if (do_undump) {
	origfilename = savestr(argv[0]);
	do_undump = 0;
	loop_ptr = -1;		/* start label stack again */
	goto just_doit;
    }
#ifdef TAINT
#ifndef DOSUID
    if (uid == euid && gid == egid)
	taintanyway = TRUE;		/* running taintperl explicitly */
#endif
#endif
    (void)sprintf(index(rcsid,'#'), "%d\n", PATCHLEVEL);
    linestr = Str_new(65,80);
    str_nset(linestr,"",0);
    str = str_make("",0);		/* first used for -I flags */
    curstash = defstash = hnew(0);
    curstname = str_make("main",4);
    stab_xhash(stabent("_main",TRUE)) = defstash;
    defstash->tbl_name = "main";
    incstab = hadd(aadd(stabent("INC",TRUE)));
    incstab->str_pok |= SP_MULTI;
    for (argc--,argv++; argc > 0; argc--,argv++) {
	if (argv[0][0] != '-' || !argv[0][1])
	    break;
#ifdef DOSUID
    if (*validarg)
	validarg = " PHOOEY ";
    else
	validarg = argv[0];
#endif
	s = argv[0]+1;
      reswitch:
	switch (*s) {
	case '0':
	case 'a':
	case 'c':
	case 'd':
	case 'D':
	case 'i':
	case 'l':
	case 'n':
	case 'p':
	case 'u':
	case 'U':
	case 'v':
	case 'w':
	    if (s = moreswitches(s))
		goto reswitch;
	    break;

	case 'e':
#ifdef TAINT
	    if (euid != uid || egid != gid)
		fatal("No -e allowed in setuid scripts");
#endif
	    if (!e_fp) {
	        e_tmpname = savestr(TMPPATH);
		(void)mktemp(e_tmpname);
		if (!*e_tmpname)
		    fatal("Can't mktemp()");
		e_fp = fopen(e_tmpname,"w");
		if (!e_fp)
		    fatal("Cannot open temporary file");
	    }
	    if (argv[1]) {
		fputs(argv[1],e_fp);
		argc--,argv++;
	    }
	    (void)putc('\n', e_fp);
	    break;
	case 'I':
#ifdef TAINT
	    if (euid != uid || egid != gid)
		fatal("No -I allowed in setuid scripts");
#endif
	    str_cat(str,"-");
	    str_cat(str,s);
	    str_cat(str," ");
	    if (*++s) {
		(void)apush(stab_array(incstab),str_make(s,0));
	    }
	    else if (argv[1]) {
		(void)apush(stab_array(incstab),str_make(argv[1],0));
		str_cat(str,argv[1]);
		argc--,argv++;
		str_cat(str," ");
	    }
	    break;
	case 'P':
#ifdef TAINT
	    if (euid != uid || egid != gid)
		fatal("No -P allowed in setuid scripts");
#endif
	    preprocess = TRUE;
	    s++;
	    goto reswitch;
	case 's':
#ifdef TAINT
	    if (euid != uid || egid != gid)
		fatal("No -s allowed in setuid scripts");
#endif
	    doswitches = TRUE;
	    s++;
	    goto reswitch;
	case 'S':
#ifdef TAINT
	    if (euid != uid || egid != gid)
		fatal("No -S allowed in setuid scripts");
#endif
	    dosearch = TRUE;
	    s++;
	    goto reswitch;
	case 'x':
	    doextract = TRUE;
	    s++;
	    if (*s)
		cddir = savestr(s);
	    break;
	case '-':
	    argc--,argv++;
	    goto switch_end;
	case 0:
	    break;
	default:
	    fatal("Unrecognized switch: -%s",s);
	}
    }
  switch_end:
    scriptname = argv[0];
    if (e_fp) {
	if (fflush(e_fp) || ferror(e_fp) || fclose(e_fp))
	    fatal("Can't write to temp file for -e: %s", strerror(errno));
	argc++,argv--;
	scriptname = e_tmpname;
    }

#ifdef DOSISH
#define PERLLIB_SEP ';'
#else
#define PERLLIB_SEP ':'
#endif
#ifndef TAINT		/* Can't allow arbitrary PERLLIB in setuid script */
    incpush(getenv("PERLLIB"));
#endif /* TAINT */

#ifndef PRIVLIB
#define PRIVLIB "/usr/share/perl"
#endif
    incpush(PRIVLIB);
    (void)apush(stab_array(incstab),str_make(".",1));

    str_set(&str_no,No);
    str_set(&str_yes,Yes);

    /* open script */

    if (scriptname == Nullch)
#ifdef MSDOS
    {
	if ( isatty(fileno(stdin)) )
	  moreswitches("v");
	scriptname = "-";
    }
#else
	scriptname = "-";
#endif
    if (dosearch && !index(scriptname, '/') && (s = getenv("PATH"))) {
	char *xfound = Nullch, *xfailed = Nullch;
	int len;

	bufend = s + strlen(s);
	while (*s) {
#ifndef DOSISH
	    s = cpytill(tokenbuf,s,bufend,':',&len);
#else
#ifdef atarist
	    for (len = 0; *s && *s != ',' && *s != ';'; tokenbuf[len++] = *s++);
	    tokenbuf[len] = '\0';
#else
	    for (len = 0; *s && *s != ';'; tokenbuf[len++] = *s++);
	    tokenbuf[len] = '\0';
#endif
#endif
	    if (*s)
		s++;
#ifndef DOSISH
	    if (len && tokenbuf[len-1] != '/')
#else
#ifdef atarist
	    if (len && ((tokenbuf[len-1] != '\\') && (tokenbuf[len-1] != '/')))
#else
	    if (len && tokenbuf[len-1] != '\\')
#endif
#endif
		(void)strcat(tokenbuf+len,"/");
	    (void)strcat(tokenbuf+len,scriptname);
#ifdef DEBUGGING
	    if (debug & 1)
		fprintf(stderr,"Looking for %s\n",tokenbuf);
#endif
	    if (stat(tokenbuf,&statbuf) < 0)		/* not there? */
		continue;
	    if (S_ISREG(statbuf.st_mode)
	     && cando(S_IRUSR,TRUE,&statbuf) && cando(S_IXUSR,TRUE,&statbuf)) {
		xfound = tokenbuf;              /* bingo! */
		break;
	    }
	    if (!xfailed)
		xfailed = savestr(tokenbuf);
	}
	if (!xfound)
	    fatal("Can't execute %s", xfailed ? xfailed : scriptname );
	if (xfailed)
	    Safefree(xfailed);
	scriptname = savestr(xfound);
    }

    fdpid = anew(Nullstab);	/* for remembering popen pids by fd */
    pidstatus = hnew(COEFFSIZE);/* for remembering status of dead pids */

    if (strnEQ(scriptname, "/dev/fd/", 8) && isDIGIT(scriptname[8]) ) {
	char *s = scriptname + 8;
	fdscript = atoi(s);
	while (isDIGIT(*s))
	    s++;
	if (*s)
	    scriptname = s + 1;
    }
    else
	fdscript = -1;
    origfilename = savestr(scriptname);
    curcmd->c_filestab = fstab(origfilename);
    if (strEQ(origfilename,"-"))
	scriptname = "";
    if (fdscript >= 0) {
	rsfp = fdopen(fdscript,"r");
#if defined(HAS_FCNTL) && defined(F_SETFD)
	fcntl(fileno(rsfp),F_SETFD,1);	/* ensure close-on-exec */
#endif
    }
    else if (preprocess) {
	char *cpp = CPPSTDIN;

	if (strEQ(cpp,"cppstdin"))
	    sprintf(tokenbuf, "%s/%s", SCRIPTDIR, cpp);
	else
	    sprintf(tokenbuf, "%s", cpp);
	str_cat(str,"-I");
	str_cat(str,PRIVLIB);
#ifdef MSDOS
	(void)sprintf(buf, "\
sed %s -e \"/^[^#]/b\" \
 -e \"/^#[ 	]*include[ 	]/b\" \
 -e \"/^#[ 	]*define[ 	]/b\" \
 -e \"/^#[ 	]*if[ 	]/b\" \
 -e \"/^#[ 	]*ifdef[ 	]/b\" \
 -e \"/^#[ 	]*ifndef[ 	]/b\" \
 -e \"/^#[ 	]*else/b\" \
 -e \"/^#[ 	]*elif[ 	]/b\" \
 -e \"/^#[ 	]*undef[ 	]/b\" \
 -e \"/^#[ 	]*endif/b\" \
 -e \"s/^#.*//\" \
 %s | %s -C %s %s",
	  (doextract ? "-e \"1,/^#/d\n\"" : ""),
#else
	(void)sprintf(buf, "\
%s %s -e '/^[^#]/b' \
 -e '/^#[ 	]*include[ 	]/b' \
 -e '/^#[ 	]*define[ 	]/b' \
 -e '/^#[ 	]*if[ 	]/b' \
 -e '/^#[ 	]*ifdef[ 	]/b' \
 -e '/^#[ 	]*ifndef[ 	]/b' \
 -e '/^#[ 	]*else/b' \
 -e '/^#[ 	]*elif[ 	]/b' \
 -e '/^#[ 	]*undef[ 	]/b' \
 -e '/^#[ 	]*endif/b' \
 -e 's/^[ 	]*#.*//' \
 %s | %s -C %s %s",
#ifdef LOC_SED
	  LOC_SED,
#else
	  "sed",
#endif
	  (doextract ? "-e '1,/^#/d\n'" : ""),
#endif
	  scriptname, tokenbuf, str_get(str), CPPMINUS);
#ifdef DEBUGGING
	if (debug & 64) {
	    fputs(buf,stderr);
	    fputs("\n",stderr);
	}
#endif
	doextract = FALSE;
#ifdef IAMSUID				/* actually, this is caught earlier */
	if (euid != uid && !euid) {	/* if running suidperl */
#ifdef HAS_SETEUID
	    (void)seteuid(uid);		/* musn't stay setuid root */
#else
#ifdef HAS_SETREUID
	    (void)setreuid(-1, uid);
#else
	    setuid(uid);
#endif
#endif
	    if (geteuid() != uid)
		fatal("Can't do seteuid!\n");
	}
#endif /* IAMSUID */
	rsfp = mypopen(buf,"r");
    }
    else if (!*scriptname) {
#ifdef TAINT
	if (euid != uid || egid != gid)
	    fatal("Can't take set-id script from stdin");
#endif
	rsfp = stdin;
    }
    else {
	rsfp = fopen(scriptname,"r");
#if defined(HAS_FCNTL) && defined(F_SETFD)
	fcntl(fileno(rsfp),F_SETFD,1);	/* ensure close-on-exec */
#endif
    }
    if ((FILE*)rsfp == Nullfp) {
#ifdef DOSUID
#ifndef IAMSUID		/* in case script is not readable before setuid */
	if (euid && stat(stab_val(curcmd->c_filestab)->str_ptr,&statbuf) >= 0 &&
	  statbuf.st_mode & (S_ISUID|S_ISGID)) {
	    (void)sprintf(buf, "%s/sperl%s", BIN, patchlevel);
	    execv(buf, origargv);	/* try again */
	    fatal("Can't do setuid\n");
	}
#endif
#endif
	fatal("Can't open perl script \"%s\": %s\n",
	  stab_val(curcmd->c_filestab)->str_ptr, strerror(errno));
    }
    str_free(str);		/* free -I directories */
    str = Nullstr;

    /* do we need to emulate setuid on scripts? */

    /* This code is for those BSD systems that have setuid #! scripts disabled
     * in the kernel because of a security problem.  Merely defining DOSUID
     * in perl will not fix that problem, but if you have disabled setuid
     * scripts in the kernel, this will attempt to emulate setuid and setgid
     * on scripts that have those now-otherwise-useless bits set.  The setuid
     * root version must be called suidperl or sperlN.NNN.  If regular perl
     * discovers that it has opened a setuid script, it calls suidperl with
     * the same argv that it had.  If suidperl finds that the script it has
     * just opened is NOT setuid root, it sets the effective uid back to the
     * uid.  We don't just make perl setuid root because that loses the
     * effective uid we had before invoking perl, if it was different from the
     * uid.
     *
     * DOSUID must be defined in both perl and suidperl, and IAMSUID must
     * be defined in suidperl only.  suidperl must be setuid root.  The
     * Configure script will set this up for you if you want it.
     *
     * There is also the possibility of have a script which is running
     * set-id due to a C wrapper.  We want to do the TAINT checks
     * on these set-id scripts, but don't want to have the overhead of
     * them in normal perl, and can't use suidperl because it will lose
     * the effective uid info, so we have an additional non-setuid root
     * version called taintperl or tperlN.NNN that just does the TAINT checks.
     */

#ifdef DOSUID
    if (fstat(fileno(rsfp),&statbuf) < 0)	/* normal stat is insecure */
	fatal("Can't stat script \"%s\"",origfilename);
    if (fdscript < 0 && statbuf.st_mode & (S_ISUID|S_ISGID)) {
	int len;

#ifdef IAMSUID
#ifndef HAS_SETREUID
	/* On this access check to make sure the directories are readable,
	 * there is actually a small window that the user could use to make
	 * filename point to an accessible directory.  So there is a faint
	 * chance that someone could execute a setuid script down in a
	 * non-accessible directory.  I don't know what to do about that.
	 * But I don't think it's too important.  The manual lies when
	 * it says access() is useful in setuid programs.
	 */
	if (access(stab_val(curcmd->c_filestab)->str_ptr,1))	/*double check*/
	    fatal("Permission denied");
#else
	/* If we can swap euid and uid, then we can determine access rights
	 * with a simple stat of the file, and then compare device and
	 * inode to make sure we did stat() on the same file we opened.
	 * Then we just have to make sure he or she can execute it.
	 */
	{
	    struct stat tmpstatbuf;

	    if (setreuid(euid,uid) < 0 || getuid() != euid || geteuid() != uid)
		fatal("Can't swap uid and euid");	/* really paranoid */
	    if (stat(stab_val(curcmd->c_filestab)->str_ptr,&tmpstatbuf) < 0)
		fatal("Permission denied");	/* testing full pathname here */
	    if (tmpstatbuf.st_dev != statbuf.st_dev ||
		tmpstatbuf.st_ino != statbuf.st_ino) {
		(void)fclose(rsfp);
		if (rsfp = mypopen("/bin/mail root","w")) {	/* heh, heh */
		    fprintf(rsfp,
"User %d tried to run dev %d ino %d in place of dev %d ino %d!\n\
(Filename of set-id script was %s, uid %d gid %d.)\n\nSincerely,\nperl\n",
			uid,tmpstatbuf.st_dev, tmpstatbuf.st_ino,
			statbuf.st_dev, statbuf.st_ino,
			stab_val(curcmd->c_filestab)->str_ptr,
			statbuf.st_uid, statbuf.st_gid);
		    (void)mypclose(rsfp);
		}
		fatal("Permission denied\n");
	    }
	    if (setreuid(uid,euid) < 0 || getuid() != uid || geteuid() != euid)
		fatal("Can't reswap uid and euid");
	    if (!cando(S_IXUSR,FALSE,&statbuf))		/* can real uid exec? */
		fatal("Permission denied\n");
	}
#endif /* HAS_SETREUID */
#endif /* IAMSUID */

	if (!S_ISREG(statbuf.st_mode))
	    fatal("Permission denied");
	if (statbuf.st_mode & S_IWOTH)
	    fatal("Setuid/gid script is writable by world");
	doswitches = FALSE;		/* -s is insecure in suid */
	curcmd->c_line++;
	if (fgets(tokenbuf,sizeof tokenbuf, rsfp) == Nullch ||
	  strnNE(tokenbuf,"#!",2) )	/* required even on Sys V */
	    fatal("No #! line");
	s = tokenbuf+2;
	if (*s == ' ') s++;
	while (!isSPACE(*s)) s++;
	if (strnNE(s-4,"perl",4) && strnNE(s-9,"perl",4))  /* sanity check */
	    fatal("Not a perl script");
	while (*s == ' ' || *s == '\t') s++;
	/*
	 * #! arg must be what we saw above.  They can invoke it by
	 * mentioning suidperl explicitly, but they may not add any strange
	 * arguments beyond what #! says if they do invoke suidperl that way.
	 */
	len = strlen(validarg);
	if (strEQ(validarg," PHOOEY ") ||
	    strnNE(s,validarg,len) || !isSPACE(s[len]))
	    fatal("Args must match #! line");

#ifndef IAMSUID
	if (euid != uid && (statbuf.st_mode & S_ISUID) &&
	    euid == statbuf.st_uid)
	    if (!do_undump)
		fatal("YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
#endif /* IAMSUID */

	if (euid) {	/* oops, we're not the setuid root perl */
	    (void)fclose(rsfp);
#ifndef IAMSUID
	    (void)sprintf(buf, "%s/sperl%s", BIN, patchlevel);
	    execv(buf, origargv);	/* try again */
#endif
	    fatal("Can't do setuid\n");
	}

	if (statbuf.st_mode & S_ISGID && statbuf.st_gid != egid) {
#ifdef HAS_SETEGID
	    (void)setegid(statbuf.st_gid);
#else
#ifdef HAS_SETREGID
	    (void)setregid((GIDTYPE)gid,statbuf.st_gid);
#else
	    setgid(statbuf.st_gid);
#endif
#endif
	    if (getegid() != statbuf.st_gid)
		fatal("Can't do setegid!\n");
	}
	if (statbuf.st_mode & S_ISUID) {
	    if (statbuf.st_uid != euid)
#ifdef HAS_SETEUID
		(void)seteuid(statbuf.st_uid);	/* all that for this */
#else
#ifdef HAS_SETREUID
		(void)setreuid((UIDTYPE)uid,statbuf.st_uid);
#else
		setuid(statbuf.st_uid);
#endif
#endif
	    if (geteuid() != statbuf.st_uid)
		fatal("Can't do seteuid!\n");
	}
	else if (uid) {			/* oops, mustn't run as root */
#ifdef HAS_SETEUID
	    (void)seteuid((UIDTYPE)uid);
#else
#ifdef HAS_SETREUID
	    (void)setreuid((UIDTYPE)uid,(UIDTYPE)uid);
#else
	    setuid((UIDTYPE)uid);
#endif
#endif
	    if (geteuid() != uid)
		fatal("Can't do seteuid!\n");
	}
	uid = (int)getuid();
	euid = (int)geteuid();
	gid = (int)getgid();
	egid = (int)getegid();
	if (!cando(S_IXUSR,TRUE,&statbuf))
	    fatal("Permission denied\n");	/* they can't do this */
    }
#ifdef IAMSUID
    else if (preprocess)
	fatal("-P not allowed for setuid/setgid script\n");
    else if (fdscript >= 0)
	fatal("fd script not allowed in suidperl\n");
    else
	fatal("Script is not setuid/setgid in suidperl\n");

    /* We absolutely must clear out any saved ids here, so we */
    /* exec taintperl, substituting fd script for scriptname. */
    /* (We pass script name as "subdir" of fd, which taintperl will grok.) */
    rewind(rsfp);
    for (which = 1; origargv[which] && origargv[which] != scriptname; which++) ;
    if (!origargv[which])
	fatal("Permission denied");
    (void)sprintf(buf, "/dev/fd/%d/%.127s", fileno(rsfp), origargv[which]);
    origargv[which] = buf;

#if defined(HAS_FCNTL) && defined(F_SETFD)
    fcntl(fileno(rsfp),F_SETFD,0);	/* ensure no close-on-exec */
#endif

    (void)sprintf(tokenbuf, "%s/tperl%s", BIN, patchlevel);
    execv(tokenbuf, origargv);	/* try again */
    fatal("Can't do setuid\n");
#else
#ifndef TAINT		/* we aren't taintperl or suidperl */
    /* script has a wrapper--can't run suidperl or we lose euid */
    else if (euid != uid || egid != gid) {
	(void)fclose(rsfp);
	(void)sprintf(buf, "%s/tperl%s", BIN, patchlevel);
	execv(buf, origargv);	/* try again */
	fatal("Can't run setuid script with taint checks");
    }
#endif /* TAINT */
#endif /* IAMSUID */
#else /* !DOSUID */
#ifndef TAINT		/* we aren't taintperl or suidperl */
    if (euid != uid || egid != gid) {	/* (suidperl doesn't exist, in fact) */
#ifndef SETUID_SCRIPTS_ARE_SECURE_NOW
	fstat(fileno(rsfp),&statbuf);	/* may be either wrapped or real suid */
	if ((euid != uid && euid == statbuf.st_uid && statbuf.st_mode & S_ISUID)
	    ||
	    (egid != gid && egid == statbuf.st_gid && statbuf.st_mode & S_ISGID)
	   )
	    if (!do_undump)
		fatal("YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
#endif /* SETUID_SCRIPTS_ARE_SECURE_NOW */
	/* not set-id, must be wrapped */
	(void)fclose(rsfp);
	(void)sprintf(buf, "%s/tperl%s", BIN, patchlevel);
	execv(buf, origargv);	/* try again */
	fatal("Can't run setuid script with taint checks");
    }
#endif /* TAINT */
#endif /* DOSUID */

#if !defined(IAMSUID) && !defined(TAINT)

    /* skip forward in input to the real script? */

    while (doextract) {
	if ((s = str_gets(linestr, rsfp, 0)) == Nullch)
	    fatal("No Perl script found in input\n");
	if (*s == '#' && s[1] == '!' && instr(s,"perl")) {
	    ungetc('\n',rsfp);		/* to keep line count right */
	    doextract = FALSE;
	    if (s = instr(s,"perl -")) {
		s += 6;
		/*SUPPRESS 530*/
		while (s = moreswitches(s)) ;
	    }
	    if (cddir && chdir(cddir) < 0)
		fatal("Can't chdir to %s",cddir);
	}
    }
#endif /* !defined(IAMSUID) && !defined(TAINT) */

    defstab = stabent("_",TRUE);

    subname = str_make("main",4);
    if (perldb) {
	debstash = hnew(0);
	stab_xhash(stabent("_DB",TRUE)) = debstash;
	curstash = debstash;
	dbargs = stab_xarray(aadd((tmpstab = stabent("args",TRUE))));
	tmpstab->str_pok |= SP_MULTI;
	dbargs->ary_flags = 0;
	DBstab = stabent("DB",TRUE);
	DBstab->str_pok |= SP_MULTI;
	DBline = stabent("dbline",TRUE);
	DBline->str_pok |= SP_MULTI;
	DBsub = hadd(tmpstab = stabent("sub",TRUE));
	tmpstab->str_pok |= SP_MULTI;
	DBsingle = stab_val((tmpstab = stabent("single",TRUE)));
	tmpstab->str_pok |= SP_MULTI;
	DBtrace = stab_val((tmpstab = stabent("trace",TRUE)));
	tmpstab->str_pok |= SP_MULTI;
	DBsignal = stab_val((tmpstab = stabent("signal",TRUE)));
	tmpstab->str_pok |= SP_MULTI;
	curstash = defstash;
    }

    /* init tokener */

    bufend = bufptr = str_get(linestr);

    savestack = anew(Nullstab);		/* for saving non-local values */
    stack = anew(Nullstab);		/* for saving non-local values */
    stack->ary_flags = 0;		/* not a real array */
    afill(stack,63); afill(stack,-1);	/* preextend stack */
    afill(savestack,63); afill(savestack,-1);

    /* now parse the script */

    error_count = 0;
    if (yyparse() || error_count) {
	if (minus_c)
	    fatal("%s had compilation errors.\n", origfilename);
	else {
	    fatal("Execution of %s aborted due to compilation errors.\n",
		origfilename);
	}
    }

    New(50,loop_stack,128,struct loop);
#ifdef DEBUGGING
    if (debug) {
	New(51,debname,128,char);
	New(52,debdelim,128,char);
    }
#endif
    curstash = defstash;

    preprocess = FALSE;
    if (e_fp) {
	e_fp = Nullfp;
	(void)UNLINK(e_tmpname);
    }

    /* initialize everything that won't change if we undump */

    if (sigstab = stabent("SIG",allstabs)) {
	sigstab->str_pok |= SP_MULTI;
	(void)hadd(sigstab);
    }

    magicalize("!#?^~=-%.+&*()<>,\\/[|`':\004\t\020\024\027\006");
    userinit();		/* in case linked C routines want magical variables */

    amperstab = stabent("&",allstabs);
    leftstab = stabent("`",allstabs);
    rightstab = stabent("'",allstabs);
    sawampersand = (amperstab || leftstab || rightstab);
    if (tmpstab = stabent(":",allstabs))
	str_set(stab_val(tmpstab),chopset);
    if (tmpstab = stabent("\024",allstabs))
	time(&basetime);

    /* these aren't necessarily magical */
    if (tmpstab = stabent("\014",allstabs)) {
	str_set(stab_val(tmpstab),"\f");
	formfeed = stab_val(tmpstab);
    }
    if (tmpstab = stabent(";",allstabs))
	str_set(STAB_STR(tmpstab),"\034");
    if (tmpstab = stabent("]",allstabs)) {
	str = STAB_STR(tmpstab);
	str_set(str,rcsid);
	str->str_u.str_nval = atof(patchlevel);
	str->str_nok = 1;
    }
    str_nset(stab_val(stabent("\"", TRUE)), " ", 1);

    stdinstab = stabent("STDIN",TRUE);
    stdinstab->str_pok |= SP_MULTI;
    if (!stab_io(stdinstab))
	stab_io(stdinstab) = stio_new();
    stab_io(stdinstab)->ifp = stdin;
    tmpstab = stabent("stdin",TRUE);
    stab_io(tmpstab) = stab_io(stdinstab);
    tmpstab->str_pok |= SP_MULTI;

    tmpstab = stabent("STDOUT",TRUE);
    tmpstab->str_pok |= SP_MULTI;
    if (!stab_io(tmpstab))
	stab_io(tmpstab) = stio_new();
    stab_io(tmpstab)->ofp = stab_io(tmpstab)->ifp = stdout;
    defoutstab = tmpstab;
    tmpstab = stabent("stdout",TRUE);
    stab_io(tmpstab) = stab_io(defoutstab);
    tmpstab->str_pok |= SP_MULTI;

    curoutstab = stabent("STDERR",TRUE);
    curoutstab->str_pok |= SP_MULTI;
    if (!stab_io(curoutstab))
	stab_io(curoutstab) = stio_new();
    stab_io(curoutstab)->ofp = stab_io(curoutstab)->ifp = stderr;
    tmpstab = stabent("stderr",TRUE);
    stab_io(tmpstab) = stab_io(curoutstab);
    tmpstab->str_pok |= SP_MULTI;
    curoutstab = defoutstab;		/* switch back to STDOUT */

    statname = Str_new(66,0);		/* last filename we did stat on */

    /* now that script is parsed, we can modify record separator */

    rs = nrs;
    rslen = nrslen;
    rschar = nrschar;
    rspara = (nrslen == 2);
    str_nset(stab_val(stabent("/", TRUE)), rs, rslen);

    if (do_undump)
	my_unexec();

  just_doit:		/* come here if running an undumped a.out */
    argc--,argv++;	/* skip name of script */
    if (doswitches) {
	for (; argc > 0 && **argv == '-'; argc--,argv++) {
	    if (argv[0][1] == '-') {
		argc--,argv++;
		break;
	    }
	    if (s = index(argv[0], '=')) {
		*s++ = '\0';
		str_set(stab_val(stabent(argv[0]+1,TRUE)),s);
	    }
	    else
		str_numset(stab_val(stabent(argv[0]+1,TRUE)),(double)1.0);
	}
    }
#ifdef TAINT
    tainted = 1;
#endif
    if (tmpstab = stabent("0",allstabs)) {
	str_set(stab_val(tmpstab),origfilename);
	magicname("0", Nullch, 0);
    }
    if (tmpstab = stabent("\030",allstabs))
	str_set(stab_val(tmpstab),origargv[0]);
    if (argvstab = stabent("ARGV",allstabs)) {
	argvstab->str_pok |= SP_MULTI;
	(void)aadd(argvstab);
	aclear(stab_array(argvstab));
	for (; argc > 0; argc--,argv++) {
	    (void)apush(stab_array(argvstab),str_make(argv[0],0));
	}
    }
#ifdef TAINT
    (void) stabent("ENV",TRUE);		/* must test PATH and IFS */
#endif
    if (envstab = stabent("ENV",allstabs)) {
	envstab->str_pok |= SP_MULTI;
	(void)hadd(envstab);
	hclear(stab_hash(envstab), FALSE);
	if (env != environ)
	    environ[0] = Nullch;
	for (; *env; env++) {
	    if (!(s = index(*env,'=')))
		continue;
	    *s++ = '\0';
	    str = str_make(s--,0);
	    str_magic(str, envstab, 'E', *env, s - *env);
	    (void)hstore(stab_hash(envstab), *env, s - *env, str, 0);
	    *s = '=';
	}
    }
#ifdef TAINT
    tainted = 0;
#endif
    if (tmpstab = stabent("$",allstabs))
	str_numset(STAB_STR(tmpstab),(double)getpid());

    if (dowarn) {
	stab_check('A','Z');
	stab_check('a','z');
    }

    if (setjmp(top_env))	/* sets goto_targ on longjump */
	loop_ptr = -1;		/* start label stack again */

#ifdef DEBUGGING
    if (debug & 1024)
	dump_all();
    if (debug)
	fprintf(stderr,"\nEXECUTING...\n\n");
#endif

    if (minus_c) {
	fprintf(stderr,"%s syntax OK\n", origfilename);
	exit(0);
    }

    /* do it */

    (void) cmd_exec(main_root,G_SCALAR,-1);

    if (goto_targ)
	fatal("Can't find label \"%s\"--aborting",goto_targ);
    exit(0);
    /* NOTREACHED */
}

void
magicalize(list)
register char *list;
{
    char sym[2];

    sym[1] = '\0';
    while (*sym = *list++)
	magicname(sym, Nullch, 0);
}

void
magicname(sym,name,namlen)
char *sym;
char *name;
int namlen;
{
    register STAB *stab;

    if (stab = stabent(sym,allstabs)) {
	stab_flags(stab) = SF_VMAGIC;
	str_magic(stab_val(stab), stab, 0, name, namlen);
    }
}

static void
incpush(p)
char *p;
{
    char *s;

    if (!p)
	return;

    /* Break at all separators */
    while (*p) {
	/* First, skip any consecutive separators */
	while ( *p == PERLLIB_SEP ) {
	    /* Uncomment the next line for PATH semantics */
	    /* (void)apush(stab_array(incstab), str_make(".", 1)); */
	    p++;
	}
	if ( (s = index(p, PERLLIB_SEP)) != Nullch ) {
	    (void)apush(stab_array(incstab), str_make(p, (int)(s - p)));
	    p = s + 1;
	} else {
	    (void)apush(stab_array(incstab), str_make(p, 0));
	    break;
	}
    }
}

void
savelines(array, str)
ARRAY *array;
STR *str;
{
    register char *s = str->str_ptr;
    register char *send = str->str_ptr + str->str_cur;
    register char *t;
    register int line = 1;

    while (s && s < send) {
	STR *tmpstr = Str_new(85,0);

	t = index(s, '\n');
	if (t)
	    t++;
	else
	    t = send;

	str_nset(tmpstr, s, t - s);
	astore(array, line++, tmpstr);
	s = t;
    }
}

/* this routine is in perl.c by virtue of being sort of an alternate main() */

int
do_eval(str,optype,stash,savecmd,gimme,arglast)
STR *str;
int optype;
HASH *stash;
int savecmd;
int gimme;
int *arglast;
{
    STR **st = stack->ary_array;
    int retval;
    CMD *myroot = Nullcmd;
    ARRAY *ar;
    int i;
    CMD * VOLATILE oldcurcmd = curcmd;
    VOLATILE int oldtmps_base = tmps_base;
    VOLATILE int oldsave = savestack->ary_fill;
    VOLATILE int oldperldb = perldb;
    SPAT * VOLATILE oldspat = curspat;
    SPAT * VOLATILE oldlspat = lastspat;
    static char *last_eval = Nullch;
    static long last_elen = 0;
    static CMD *last_root = Nullcmd;
    VOLATILE int sp = arglast[0];
    char *specfilename;
    char *tmpfilename;
    int parsing = 1;

    tmps_base = tmps_max;
    if (curstash != stash) {
	(void)savehptr(&curstash);
	curstash = stash;
    }
    str_set(stab_val(stabent("@",TRUE)),"");
    if (curcmd->c_line == 0)		/* don't debug debugger... */
	perldb = FALSE;
    curcmd = &compiling;
    if (optype == O_EVAL) {		/* normal eval */
	curcmd->c_filestab = fstab("(eval)");
	curcmd->c_line = 1;
	str_sset(linestr,str);
	str_cat(linestr,";\n;\n");	/* be kind to them */
	if (perldb)
	    savelines(stab_xarray(curcmd->c_filestab), linestr);
    }
    else {
	if (last_root && !in_eval) {
	    Safefree(last_eval);
	    last_eval = Nullch;
	    cmd_free(last_root);
	    last_root = Nullcmd;
	}
	specfilename = str_get(str);
	str_set(linestr,"");
	if (optype == O_REQUIRE && &str_undef !=
	  hfetch(stab_hash(incstab), specfilename, strlen(specfilename), 0)) {
	    curcmd = oldcurcmd;
	    tmps_base = oldtmps_base;
	    st[++sp] = &str_yes;
	    perldb = oldperldb;
	    return sp;
	}
	tmpfilename = savestr(specfilename);
	if (*tmpfilename == '/' ||
	    (*tmpfilename == '.' &&
		(tmpfilename[1] == '/' ||
		 (tmpfilename[1] == '.' && tmpfilename[2] == '/'))))
	{
	    rsfp = fopen(tmpfilename,"r");
	}
	else {
	    ar = stab_array(incstab);
	    for (i = 0; i <= ar->ary_fill; i++) {
		(void)sprintf(buf, "%s/%s",
		  str_get(afetch(ar,i,TRUE)), specfilename);
		rsfp = fopen(buf,"r");
		if (rsfp) {
		    char *s = buf;

		    if (*s == '.' && s[1] == '/')
			s += 2;
		    Safefree(tmpfilename);
		    tmpfilename = savestr(s);
		    break;
		}
	    }
	}
	curcmd->c_filestab = fstab(tmpfilename);
	Safefree(tmpfilename);
	tmpfilename = Nullch;
	if (!rsfp) {
	    curcmd = oldcurcmd;
	    tmps_base = oldtmps_base;
	    if (optype == O_REQUIRE) {
		sprintf(tokenbuf,"Can't locate %s in @INC", specfilename);
		if (instr(tokenbuf,".h "))
		    strcat(tokenbuf," (change .h to .ph maybe?)");
		if (instr(tokenbuf,".ph "))
		    strcat(tokenbuf," (did you run h2ph?)");
		fatal("%s",tokenbuf);
	    }
	    if (gimme != G_ARRAY)
		st[++sp] = &str_undef;
	    perldb = oldperldb;
	    return sp;
	}
	curcmd->c_line = 0;
    }
    in_eval++;
    oldoldbufptr = oldbufptr = bufptr = str_get(linestr);
    bufend = bufptr + linestr->str_cur;
    if (++loop_ptr >= loop_max) {
	loop_max += 128;
	Renew(loop_stack, loop_max, struct loop);
    }
    loop_stack[loop_ptr].loop_label = "_EVAL_";
    loop_stack[loop_ptr].loop_sp = sp;
#ifdef DEBUGGING
    if (debug & 4) {
	deb("(Pushing label #%d _EVAL_)\n", loop_ptr);
    }
#endif
    eval_root = Nullcmd;
    if (setjmp(loop_stack[loop_ptr].loop_env)) {
	retval = 1;
    }
    else {
	error_count = 0;
	if (rsfp) {
	    retval = yyparse();
	    retval |= error_count;
	}
	else if (last_root && last_elen == bufend - bufptr
	  && *bufptr == *last_eval && !bcmp(bufptr,last_eval,last_elen)){
	    retval = 0;
	    eval_root = last_root;	/* no point in reparsing */
	}
	else if (in_eval == 1 && !savecmd) {
	    if (last_root) {
		Safefree(last_eval);
		last_eval = Nullch;
		cmd_free(last_root);
	    }
	    last_root = Nullcmd;
	    last_elen = bufend - bufptr;
	    last_eval = nsavestr(bufptr, last_elen);
	    retval = yyparse();
	    retval |= error_count;
	    if (!retval)
		last_root = eval_root;
	    if (!last_root) {
		Safefree(last_eval);
		last_eval = Nullch;
	    }
	}
	else
	    retval = yyparse();
    }
    myroot = eval_root;		/* in case cmd_exec does another eval! */

    if (retval || error_count) {
	st = stack->ary_array;
	sp = arglast[0];
	if (gimme != G_ARRAY)
	    st[++sp] = &str_undef;
	if (parsing) {
#ifndef MANGLEDPARSE
#ifdef DEBUGGING
	    if (debug & 128)
		fprintf(stderr,"Freeing eval_root %lx\n",(long)eval_root);
#endif
	    cmd_free(eval_root);
#endif
	    /*SUPPRESS 29*/ /*SUPPRESS 30*/
	    if ((CMD*)eval_root == last_root)
		last_root = Nullcmd;
	    eval_root = myroot = Nullcmd;
	}
	if (rsfp) {
	    fclose(rsfp);
	    rsfp = 0;
	}
    }
    else {
	parsing = 0;
	sp = cmd_exec(eval_root,gimme,sp);
	st = stack->ary_array;
	for (i = arglast[0] + 1; i <= sp; i++)
	    st[i] = str_mortal(st[i]);
				/* if we don't save result, free zaps it */
	if (savecmd)
	    eval_root = myroot;
	else if (in_eval != 1 && myroot != last_root)
	    cmd_free(myroot);
	    if (eval_root == myroot)
		eval_root = Nullcmd;
    }

    perldb = oldperldb;
    in_eval--;
#ifdef DEBUGGING
    if (debug & 4) {
	char *tmps = loop_stack[loop_ptr].loop_label;
	deb("(Popping label #%d %s)\n",loop_ptr,
	    tmps ? tmps : "" );
    }
#endif
    loop_ptr--;
    tmps_base = oldtmps_base;
    curspat = oldspat;
    lastspat = oldlspat;
    if (savestack->ary_fill > oldsave)	/* let them use local() */
	restorelist(oldsave);

    if (optype != O_EVAL) {
	if (retval) {
	    if (optype == O_REQUIRE)
		fatal("%s", str_get(stab_val(stabent("@",TRUE))));
	}
	else {
	    curcmd = oldcurcmd;
	    if (gimme == G_SCALAR ? str_true(st[sp]) : sp > arglast[0]) {
		(void)hstore(stab_hash(incstab), specfilename,
		  strlen(specfilename), str_smake(stab_val(curcmd->c_filestab)),
		      0 );
	    }
	    else if (optype == O_REQUIRE)
		fatal("%s did not return a true value", specfilename);
	}
    }
    curcmd = oldcurcmd;
    return sp;
}

int
do_try(cmd,gimme,arglast)
CMD *cmd;
int gimme;
int *arglast;
{
    STR **st = stack->ary_array;

    CMD * VOLATILE oldcurcmd = curcmd;
    VOLATILE int oldtmps_base = tmps_base;
    VOLATILE int oldsave = savestack->ary_fill;
    SPAT * VOLATILE oldspat = curspat;
    SPAT * VOLATILE oldlspat = lastspat;
    VOLATILE int sp = arglast[0];

    tmps_base = tmps_max;
    str_set(stab_val(stabent("@",TRUE)),"");
    in_eval++;
    if (++loop_ptr >= loop_max) {
	loop_max += 128;
	Renew(loop_stack, loop_max, struct loop);
    }
    loop_stack[loop_ptr].loop_label = "_EVAL_";
    loop_stack[loop_ptr].loop_sp = sp;
#ifdef DEBUGGING
    if (debug & 4) {
	deb("(Pushing label #%d _EVAL_)\n", loop_ptr);
    }
#endif
    if (setjmp(loop_stack[loop_ptr].loop_env)) {
	st = stack->ary_array;
	sp = arglast[0];
	if (gimme != G_ARRAY)
	    st[++sp] = &str_undef;
    }
    else {
	sp = cmd_exec(cmd,gimme,sp);
	st = stack->ary_array;
/*	for (i = arglast[0] + 1; i <= sp; i++)
	    st[i] = str_mortal(st[i]);  not needed, I think */
				/* if we don't save result, free zaps it */
    }

    in_eval--;
#ifdef DEBUGGING
    if (debug & 4) {
	char *tmps = loop_stack[loop_ptr].loop_label;
	deb("(Popping label #%d %s)\n",loop_ptr,
	    tmps ? tmps : "" );
    }
#endif
    loop_ptr--;
    tmps_base = oldtmps_base;
    curspat = oldspat;
    lastspat = oldlspat;
    curcmd = oldcurcmd;
    if (savestack->ary_fill > oldsave)	/* let them use local() */
	restorelist(oldsave);

    return sp;
}

/* This routine handles any switches that can be given during run */

static char *
moreswitches(s)
char *s;
{
    int numlen;

    switch (*s) {
    case '0':
	nrschar = scanoct(s, 4, &numlen);
	nrs = nsavestr("\n",1);
	*nrs = nrschar;
	if (nrschar > 0377) {
	    nrslen = 0;
	    nrs = "";
	}
	else if (!nrschar && numlen >= 2) {
	    nrslen = 2;
	    nrs = "\n\n";
	    nrschar = '\n';
	}
	return s + numlen;
    case 'a':
	minus_a = TRUE;
	s++;
	return s;
    case 'c':
	minus_c = TRUE;
	s++;
	return s;
    case 'd':
#ifdef TAINT
	if (euid != uid || egid != gid)
	    fatal("No -d allowed in setuid scripts");
#endif
	perldb = TRUE;
	s++;
	return s;
    case 'D':
#ifdef DEBUGGING
#ifdef TAINT
	if (euid != uid || egid != gid)
	    fatal("No -D allowed in setuid scripts");
#endif
	debug = atoi(s+1) | 32768;
#else
	warn("Recompile perl with -DDEBUGGING to use -D switch\n");
#endif
	/*SUPPRESS 530*/
	for (s++; isDIGIT(*s); s++) ;
	return s;
    case 'i':
	inplace = savestr(s+1);
	/*SUPPRESS 530*/
	for (s = inplace; *s && !isSPACE(*s); s++) ;
	*s = '\0';
	break;
    case 'I':
#ifdef TAINT
	if (euid != uid || egid != gid)
	    fatal("No -I allowed in setuid scripts");
#endif
	if (*++s) {
	    (void)apush(stab_array(incstab),str_make(s,0));
	}
	else
	    fatal("No space allowed after -I");
	break;
    case 'l':
	minus_l = TRUE;
	s++;
	if (isDIGIT(*s)) {
	    ors = savestr("\n");
	    orslen = 1;
	    *ors = scanoct(s, 3 + (*s == '0'), &numlen);
	    s += numlen;
	}
	else {
	    ors = nsavestr(nrs,nrslen);
	    orslen = nrslen;
	}
	return s;
    case 'n':
	minus_n = TRUE;
	s++;
	return s;
    case 'p':
	minus_p = TRUE;
	s++;
	return s;
    case 'u':
	do_undump = TRUE;
	s++;
	return s;
    case 'U':
	unsafe = TRUE;
	s++;
	return s;
    case 'v':
	fputs("\nThis is perl, version 4.0\n\n",stdout);
	fputs(rcsid,stdout);
	fputs("+ suidperl security patch\n", stdout);
	fputs("\nCopyright (c) 1989, 1990, 1991, Larry Wall\n",stdout);
#ifdef MSDOS
	fputs("MS-DOS port Copyright (c) 1989, 1990, Diomidis Spinellis\n",
	stdout);
#ifdef OS2
        fputs("OS/2 port Copyright (c) 1990, 1991, Raymond Chen, Kai Uwe Rommel\n",
        stdout);
#endif
#endif
#ifdef atarist
        fputs("atariST series port, ++jrb  bammi@cadence.com\n", stdout);
#endif
	fputs("\n\
Perl may be copied only under the terms of either the Artistic License or the\n\
GNU General Public License, which may be found in the Perl 4.0 source kit.\n",stdout);
#ifdef MSDOS
        usage(origargv[0]);
#endif
	exit(0);
    case 'w':
	dowarn = TRUE;
	s++;
	return s;
    case ' ':
    case '\n':
    case '\t':
	break;
    default:
	fatal("Switch meaningless after -x: -%s",s);
    }
    return Nullch;
}

/* compliments of Tom Christiansen */

/* unexec() can be found in the Gnu emacs distribution */

void
my_unexec()
{
#ifdef UNEXEC
    int    status;
    extern int etext;
    static char dumpname[BUFSIZ];
    static char perlpath[256];

    sprintf (dumpname, "%s.perldump", origfilename);
    sprintf (perlpath, "%s/perl", BIN);

    status = unexec(dumpname, perlpath, &etext, sbrk(0), 0);
    if (status)
	fprintf(stderr, "unexec of %s into %s failed!\n", perlpath, dumpname);
    exit(status);
#else
#ifdef DOSISH
    abort();	/* nothing else to do */
#else /* ! MSDOS */
#   ifndef SIGABRT
#	define SIGABRT SIGILL
#   endif
#   ifndef SIGILL
#	define SIGILL 6		/* blech */
#   endif
    kill(getpid(),SIGABRT);	/* for use with undump */
#endif /* ! MSDOS */
#endif
}
