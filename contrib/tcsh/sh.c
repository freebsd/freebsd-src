/* $Header: /src/pub/tcsh/sh.c,v 3.92 2000/11/11 23:03:35 christos Exp $ */
/*
 * sh.c: Main shell routines
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#define EXTERN	/* Intern */
#include "sh.h"

#ifndef lint
char    copyright[] =
"@(#) Copyright (c) 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

RCSID("$Id: sh.c,v 3.92 2000/11/11 23:03:35 christos Exp $")

#include "tc.h"
#include "ed.h"
#include "tw.h"

extern bool MapsAreInited;
extern bool NLSMapsAreInited;
extern bool NoNLSRebind;

/*
 * C Shell
 *
 * Bill Joy, UC Berkeley, California, USA
 * October 1978, May 1980
 *
 * Jim Kulp, IIASA, Laxenburg, Austria
 * April 1980
 *
 * Filename recognition added:
 * Ken Greer, Ind. Consultant, Palo Alto CA
 * October 1983.
 *
 * Karl Kleinpaste, Computer Consoles, Inc.
 * Added precmd, periodic/tperiod, prompt changes,
 * directory stack hack, and login watch.
 * Sometime March 1983 - Feb 1984.
 *
 * Added scheduled commands, including the "sched" command,
 * plus the call to sched_run near the precmd et al
 * routines.
 * Upgraded scheduled events for running events while
 * sitting idle at command input.
 *
 * Paul Placeway, Ohio State
 * added stuff for running with twenex/inputl  9 Oct 1984.
 *
 * ported to Apple Unix (TM) (OREO)  26 -- 29 Jun 1987
 */

jmp_buf_t reslab INIT_ZERO_STRUCT;

static const char tcshstr[] = "tcsh";
#ifdef WINNT_NATIVE
static const char tcshstr_nt[] = "tcsh.exe";
#endif /* WINNT_NATIVE */

signalfun_t parintr = 0;	/* Parents interrupt catch */
signalfun_t parterm = 0;	/* Parents terminate catch */

#ifdef TESLA
int do_logout = 0;
#endif /* TESLA */


bool    use_fork = 0;		/* use fork() instead of vfork()? */

/*
 * Magic pointer values. Used to specify other invalid conditions aside
 * from null.
 */
static Char	INVCHAR;
Char    *INVPTR = &INVCHAR;
Char    **INVPPTR = &INVPTR;

static int     nofile = 0;
static bool    reenter = 0;
static bool    nverbose = 0;
static bool    nexececho = 0;
static bool    quitit = 0;
static bool    rdirs = 0;
bool    fast = 0;
static bool    batch = 0;
static bool    mflag = 0;
static bool    prompt = 1;
static int     enterhist = 0;
bool    tellwhat = 0;
time_t  t_period;
Char  *ffile = NULL;
bool	dolzero = 0;
int	insource = 0;
int	exitset = 0;
static time_t  chktim;		/* Time mail last checked */
char *progname;
int tcsh;
extern char **environ;

/*
 * This preserves the input state of the shell. It is used by
 * st_save and st_restore to manupulate shell state.
 */
struct saved_state {
    int		  insource;
    int		  SHIN;
    int		  intty;
    struct whyle *whyles;
    Char 	 *gointr;
    Char 	 *arginp;
    Char	 *evalp;
    Char	**evalvec;
    Char	 *alvecp;
    Char	**alvec;
    int		  onelflg;
    bool	  enterhist;
    Char	**argv;
    Char	  HIST;
    bool	  cantell;
    struct Bin	  B;
    /* These keep signal state and setjump state */
#ifdef BSDSIGS
    sigmask_t	  mask;
#endif
    jmp_buf_t	  oldexit;
    int		  reenter;
};

static	int		  srccat	__P((Char *, Char *));
static	int		  srcfile	__P((char *, bool, int, Char **));
static	sigret_t	  phup		__P((int));
static	void		  srcunit	__P((int, bool, int, Char **));
static	void		  mailchk	__P((void));
#ifndef _PATH_DEFPATH
static	Char	 	**defaultpath	__P((void));
#endif
static	void		  record	__P((void));
static	void		  st_save	__P((struct saved_state *, int, int,
					     Char **, Char **));
static	void		  st_restore	__P((struct saved_state *, Char **));

	int		  main		__P((int, char **));

int
main(argc, argv)
    int     argc;
    char  **argv;
{
    register Char *cp;
#ifdef AUTOLOGOUT
    register Char *cp2;
#endif
    register char *tcp, *ttyn;
    register int f;
    register char **tempv;

#ifdef BSDSIGS
    sigvec_t osv;
#endif /* BSDSIGS */

#ifdef WINNT_NATIVE
    nt_init();
#endif /* WINNT_NATIVE */
#if defined(NLS_CATALOGS) && defined(LC_MESSAGES)
    (void) setlocale(LC_MESSAGES, "");
#endif /* NLS_CATALOGS && LC_MESSAGES */

#ifdef NLS
# ifdef LC_CTYPE
    (void) setlocale(LC_CTYPE, ""); /* for iscntrl */
# endif /* LC_CTYPE */
#endif /* NLS */

    nlsinit();

#ifdef MALLOC_TRACE
     mal_setstatsfile(fdopen(dup2(open("/tmp/tcsh.trace", 
				       O_WRONLY|O_CREAT, 0666), 25), "w"));
     mal_trace(1);
#endif /* MALLOC_TRACE */

#if !(defined(BSDTIMES) || defined(_SEQUENT_)) && defined(POSIX)
# ifdef _SC_CLK_TCK
    clk_tck = (clock_t) sysconf(_SC_CLK_TCK);
# else /* ! _SC_CLK_TCK */
#  ifdef CLK_TCK
    clk_tck = CLK_TCK;
#  else /* !CLK_TCK */
    clk_tck = HZ;
#  endif /* CLK_TCK */
# endif /* _SC_CLK_TCK */
#endif /* !BSDTIMES && POSIX */

    settimes();			/* Immed. estab. timing base */
#ifdef TESLA
    do_logout = 0;
#endif /* TESLA */

    /*
     * Make sure we have 0, 1, 2 open
     * Otherwise `` jobs will not work... (From knaff@poly.polytechnique.fr)
     */
    {
	do 
	    if ((f = open(_PATH_DEVNULL, O_RDONLY)) == -1 &&
		(f = open("/", O_RDONLY)) == -1) 
		exit(1);
	while (f < 3);
	(void) close(f);
    }

    osinit();			/* Os dependent initialization */

    
    {
	char *t;

	t = strrchr(argv[0], '/');
#ifdef WINNT_NATIVE
	{
	    char *s = strrchr(argv[0], '\\');
	    if (s)
		t = s;
	}
#endif /* WINNT_NATIVE */
	t = t ? t + 1 : argv[0];
	if (*t == '-') t++;
	progname = strsave((t && *t) ? t : tcshstr);    /* never want a null */
	tcsh = strcmp(progname, tcshstr) == 0;
    }

    /*
     * Initialize non constant strings
     */
#ifdef _PATH_BSHELL
    STR_BSHELL = SAVE(_PATH_BSHELL);
#endif
#ifdef _PATH_TCSHELL
    STR_SHELLPATH = SAVE(_PATH_TCSHELL);
#else
# ifdef _PATH_CSHELL
    STR_SHELLPATH = SAVE(_PATH_CSHELL);
# endif
#endif
    STR_environ = blk2short(environ);
    environ = short2blk(STR_environ);	/* So that we can free it */
    STR_WORD_CHARS = SAVE(WORD_CHARS);

    HIST = '!';
    HISTSUB = '^';
    PRCH = '>';
    PRCHROOT = '#';
    word_chars = STR_WORD_CHARS;
    bslash_quote = 0;		/* PWP: do tcsh-style backslash quoting? */

    /* Default history size to 100 */
    set(STRhistory, SAVE("100"), VAR_READWRITE);

    tempv = argv;
    ffile = SAVE(tempv[0]);
    dolzero = 0;
    if (eq(ffile, STRaout))	/* A.out's are quittable */
	quitit = 1;
    uid = getuid();
    gid = getgid();
    euid = geteuid();
    egid = getegid();
#if defined(OREO) || defined(DT_SUPPORT)
    /*
     * We are a login shell if: 1. we were invoked as -<something> with
     * optional arguments 2. or we were invoked only with the -l flag
     */
    loginsh = (**tempv == '-') || (argc == 2 &&
				   tempv[1][0] == '-' && tempv[1][1] == 'l' &&
						tempv[1][2] == '\0');
#else
    /*
     * We are a login shell if: 1. we were invoked as -<something> and we had
     * no arguments 2. or we were invoked only with the -l flag
     */
    loginsh = (**tempv == '-' && argc == 1) || (argc == 2 &&
				   tempv[1][0] == '-' && tempv[1][1] == 'l' &&
						tempv[1][2] == '\0');
#endif

#ifdef _VMS_POSIX
    /* No better way to find if we are a login shell */
    if (!loginsh) {
	loginsh = (argc == 1 && getppid() == 1);
	**tempv = '-';	/* Avoid giving VMS an acidic stomach */
    }
#endif /* _VMS_POSIX */

    if (loginsh && **tempv != '-') {
	/*
	 * Mangle the argv space
	 */
	tempv[1][0] = '\0';
	tempv[1][1] = '\0';
	tempv[1] = NULL;
	for (tcp = *tempv; *tcp++;)
	     continue;
	for (tcp--; tcp >= *tempv; tcp--)
	    tcp[1] = tcp[0];
	*++tcp = '-';
	argc--;
    }
    if (loginsh) {
	(void) time(&chktim);
	set(STRloginsh, Strsave(STRNULL), VAR_READWRITE);
    }

    AsciiOnly = 1;
    NoNLSRebind = getenv("NOREBIND") != NULL;
#ifdef NLS
# ifdef SETLOCALEBUG
    dont_free = 1;
# endif /* SETLOCALEBUG */
    (void) setlocale(LC_ALL, "");
# ifdef LC_COLLATE
    (void) setlocale(LC_COLLATE, "");
# endif
# ifdef SETLOCALEBUG
    dont_free = 0;
# endif /* SETLOCALEBUG */
# ifdef STRCOLLBUG
    fix_strcoll_bug();
# endif /* STRCOLLBUG */

    {
	int     k;

	for (k = 0200; k <= 0377 && !Isprint(k); k++)
	    continue;
	AsciiOnly = k > 0377;
    }
#else
    AsciiOnly = getenv("LANG") == NULL && getenv("LC_CTYPE") == NULL;
#endif				/* NLS */
    if (MapsAreInited && !NLSMapsAreInited)
	ed_InitNLSMaps();
    ResetArrowKeys();

    /*
     * Initialize for periodic command intervals. Also, initialize the dummy
     * tty list for login-watch.
     */
    (void) time(&t_period);
#ifndef HAVENOUTMP
    initwatch();
#endif /* !HAVENOUTMP */

#if defined(alliant)
    /*
     * From:  Jim Pace <jdp@research.att.com>
     * tcsh does not work properly on the alliants through an rlogin session.
     * The shell generally hangs.  Also, reference to the controlling terminal
     * does not work ( ie: echo foo > /dev/tty ).
     *
     * A security feature was added to rlogind affecting FX/80's Concentrix
     * from revision 5.5.xx upwards (through 5.7 where this fix was implemented)
     * This security change also affects the FX/2800 series.
     * The security change to rlogind requires the process group of an rlogin
     * session become disassociated with the tty in rlogind.
     *
     * The changes needed are:
     * 1. set the process group
     * 2. reenable the control terminal
     */
     if (loginsh && isatty(SHIN)) {
	 ttyn = (char *) ttyname(SHIN);
	 (void) close(SHIN);
	 SHIN = open(ttyn, O_RDWR);
	 shpgrp = getpid();
	 (void) ioctl (SHIN, TIOCSPGRP, (ioctl_t) &shpgrp);
	 (void) setpgid(0, shpgrp);
     }
#endif /* alliant */

    /*
     * Move the descriptors to safe places. The variable didfds is 0 while we
     * have only FSH* to work with. When didfds is true, we have 0,1,2 and
     * prefer to use these.
     */
    initdesc();

    /*
     * Get and set the tty now
     */
    if ((ttyn = ttyname(SHIN)) != NULL) {
	/*
	 * Could use rindex to get rid of other possible path components, but
	 * hpux preserves the subdirectory /pty/ when storing the tty name in
	 * utmp, so we keep it too.
	 */
	if (strncmp(ttyn, "/dev/", 5) == 0)
	    set(STRtty, cp = SAVE(ttyn + 5), VAR_READWRITE);
	else
	    set(STRtty, cp = SAVE(ttyn), VAR_READWRITE);
    }
    else
	set(STRtty, cp = SAVE(""), VAR_READWRITE);
    /*
     * Initialize the shell variables. ARGV and PROMPT are initialized later.
     * STATUS is also munged in several places. CHILD is munged when
     * forking/waiting
     */

    /*
     * 7-10-87 Paul Placeway autologout should be set ONLY on login shells and
     * on shells running as root.  Out of these, autologout should NOT be set
     * for any psudo-terminals (this catches most window systems) and not for
     * any terminal running X windows.
     * 
     * At Ohio State, we have had problems with a user having his X session 
     * drop out from under him (on a Sun) because the shell in his master 
     * xterm timed out and exited.
     * 
     * Really, this should be done with a program external to the shell, that
     * watches for no activity (and NO running programs, such as dump) on a
     * terminal for a long peroid of time, and then SIGHUPS the shell on that
     * terminal.
     * 
     * bugfix by Rich Salz <rsalz@PINEAPPLE.BBN.COM>: For root rsh things 
     * allways first check to see if loginsh or really root, then do things 
     * with ttyname()
     * 
     * Also by Jean-Francois Lamy <lamy%ai.toronto.edu@RELAY.CS.NET>: check the
     * value of cp before using it! ("root can rsh too")
     * 
     * PWP: keep the nested ifs; the order of the tests matters and a good 
     * (smart) C compiler might re-arange things wrong.
     */
#ifdef AUTOLOGOUT
# ifdef convex
    if (uid == 0) {
	/*  root always has a 15 minute autologout  */
	set(STRautologout, Strsave(STRrootdefautologout), VAR_READWRITE);
    }
    else
	if (loginsh)
	    /*  users get autologout set to 0  */
	    set(STRautologout, Strsave(STR0), VAR_READWRITE);
# else /* convex */
    if (loginsh || (uid == 0)) {
	if (*cp) {
	    /* only for login shells or root and we must have a tty */
	    if ((cp2 = Strrchr(cp, (Char) '/')) != NULL) {
		cp = cp2 + 1;
	    }
	    else
		cp2 = cp;
	    if (!(((Strncmp(cp2, STRtty, 3) == 0) && Isalpha(cp2[3])) ||
		  ((Strncmp(cp, STRpts, 3) == 0) && cp[3] == '/'))) {
		if (getenv("DISPLAY") == NULL) {
		    /* NOT on X window shells */
		    set(STRautologout, Strsave(STRdefautologout), 
			VAR_READWRITE);
		}
	    }
	}
    }
# endif /* convex */
#endif /* AUTOLOGOUT */

    (void) sigset(SIGALRM, alrmcatch);

    set(STRstatus, Strsave(STR0), VAR_READWRITE);

    /*
     * get and set machine specific envirnment variables
     */
    getmachine();

    fix_version();		/* publish the shell version */

    /*
     * Publish the selected echo style
     */
#if ECHO_STYLE == NONE_ECHO
    set(STRecho_style, Strsave(STRnone), VAR_READWRITE);
#endif /* ECHO_STYLE == NONE_ECHO */
#if ECHO_STYLE == BSD_ECHO
    set(STRecho_style, Strsave(STRbsd), VAR_READWRITE);
#endif /* ECHO_STYLE == BSD_ECHO */
#if ECHO_STYLE == SYSV_ECHO
    set(STRecho_style, Strsave(STRsysv), VAR_READWRITE);
#endif /* ECHO_STYLE == SYSV_ECHO */
#if ECHO_STYLE == BOTH_ECHO
    set(STRecho_style, Strsave(STRboth), VAR_READWRITE);
#endif /* ECHO_STYLE == BOTH_ECHO */

    /*
     * increment the shell level.
     */
    shlvl(1);

    if ((tcp = getenv("HOME")) != NULL)
	cp = quote(SAVE(tcp));
    else
	cp = NULL;
    if (cp == NULL)
	fast = 1;		/* No home -> can't read scripts */
    else
	set(STRhome, cp, VAR_READWRITE);
    dinit(cp);			/* dinit thinks that HOME == cwd in a login
				 * shell */
    /*
     * Grab other useful things from the environment. Should we grab
     * everything??
     */
    {
	char *cln, *cus, *cgr;
	Char    buff[BUFSIZE];
	struct passwd *pw;
	struct group *gr;


#ifdef apollo
	int     oid = getoid();

	(void) Itoa(oid, buff, 0, 0);
	set(STRoid, Strsave(buff), VAR_READWRITE);
#endif /* apollo */

	(void) Itoa(uid, buff, 0, 0);
	set(STRuid, Strsave(buff), VAR_READWRITE);

	(void) Itoa(gid, buff, 0, 0);
	set(STRgid, Strsave(buff), VAR_READWRITE);

	cln = getenv("LOGNAME");
	cus = getenv("USER");
	if (cus != NULL)
	    set(STRuser, quote(SAVE(cus)), VAR_READWRITE);
	else if (cln != NULL)
	    set(STRuser, quote(SAVE(cln)), VAR_READWRITE);
	else if ((pw = getpwuid(uid)) == NULL)
	    set(STRuser, SAVE("unknown"), VAR_READWRITE);
	else
	    set(STRuser, SAVE(pw->pw_name), VAR_READWRITE);
	if (cln == NULL)
	    tsetenv(STRLOGNAME, varval(STRuser));
	if (cus == NULL)
	    tsetenv(STRKUSER, varval(STRuser));
	
	cgr = getenv("GROUP");
	if (cgr != NULL)
	    set(STRgroup, quote(SAVE(cgr)), VAR_READWRITE);
	else if ((gr = getgrgid(gid)) == NULL)
	    set(STRgroup, SAVE("unknown"), VAR_READWRITE);
	else
	    set(STRgroup, SAVE(gr->gr_name), VAR_READWRITE);
	if (cgr == NULL)
	    tsetenv(STRKGROUP, varval(STRgroup));
    }

    /*
     * HOST may be wrong, since rexd transports the entire environment on sun
     * 3.x Just set it again
     */
    {
	char    cbuff[MAXHOSTNAMELEN];

	if (gethostname(cbuff, sizeof(cbuff)) >= 0) {
	    cbuff[sizeof(cbuff) - 1] = '\0';	/* just in case */
	    tsetenv(STRHOST, str2short(cbuff));
	}
	else
	    tsetenv(STRHOST, str2short("unknown"));
    }


#ifdef REMOTEHOST
    /*
     * Try to determine the remote host we were logged in from.
     */
    remotehost();
#endif /* REMOTEHOST */
 
#ifdef apollo
    if ((tcp = getenv("SYSTYPE")) == NULL)
	tcp = "bsd4.3";
    tsetenv(STRSYSTYPE, quote(str2short(tcp)));
#endif /* apollo */

    /*
     * set editing on by default, unless running under Emacs as an inferior
     * shell.
     * We try to do this intelligently. If $TERM is available, then it
     * should determine if we should edit or not. $TERM is preserved
     * across rlogin sessions, so we will not get confused if we rlogin
     * under an emacs shell. Another advantage is that if we run an
     * xterm under an emacs shell, then the $TERM will be set to 
     * xterm, so we are going to want to edit. Unfortunately emacs
     * does not restore all the tty modes, so xterm is not very well
     * set up. But this is not the shell's fault.
     * Also don't edit if $TERM == wm, for when we're running under an ATK app.
     * Finally, emacs compiled under terminfo, sets the terminal to dumb,
     * so disable editing for that too.
     * 
     * Unfortunately, in some cases the initial $TERM setting is "unknown",
     * "dumb", or "network" which is then changed in the user's startup files.
     * We fix this by setting noediting here if $TERM is unknown/dumb and
     * if noediting is set, we switch on editing if $TERM is changed.
     */
    if ((tcp = getenv("TERM")) != NULL) {
	set(STRterm, quote(SAVE(tcp)), VAR_READWRITE);
	noediting = strcmp(tcp, "unknown") == 0 || strcmp(tcp, "dumb") == 0 ||
		    strcmp(tcp, "network") == 0;
	editing = strcmp(tcp, "emacs") != 0 && strcmp(tcp, "wm") != 0 &&
		  !noediting;
    }
    else {
	noediting = 0;
	editing = ((tcp = getenv("EMACS")) == NULL || strcmp(tcp, "t") != 0);
    }

    /* 
     * The 'edit' variable is either set or unset.  It doesn't 
     * need a value.  Making it 'emacs' might be confusing. 
     */
    if (editing)
	set(STRedit, Strsave(STRNULL), VAR_READWRITE);


    /*
     * still more mutability: make the complete routine automatically add the
     * suffix of file names...
     */
    set(STRaddsuffix, Strsave(STRNULL), VAR_READWRITE);

    /*
     * Re-initialize path if set in environment
     */
    if ((tcp = getenv("PATH")) == NULL)
#ifdef _PATH_DEFPATH
	importpath(str2short(_PATH_DEFPATH));
#else /* !_PATH_DEFPATH */
	setq(STRpath, defaultpath(), &shvhed, VAR_READWRITE);
#endif /* _PATH_DEFPATH */
    else
	/* Importpath() allocates memory for the path, and the
	 * returned pointer from SAVE() was discarded, so
	 * this was a memory leak.. (sg)
	 *
	 * importpath(SAVE(tcp));
	 */
	importpath(str2short(tcp));


    {
	/* If the SHELL environment variable ends with "tcsh", set
	 * STRshell to the same path.  This is to facilitate using
	 * the executable in environments where the compiled-in
	 * default isn't appropriate (sg).
	 */

	int sh_len = 0;

	if ((tcp = getenv("SHELL")) != NULL) {
	    sh_len = strlen(tcp);
	    if ((sh_len >= 5 && strcmp(tcp + (sh_len - 5), "/tcsh") == 0) || 
	        (!tcsh && sh_len >= 4 && strcmp(tcp + (sh_len - 4), "/csh") == 0))
		set(STRshell, quote(SAVE(tcp)), VAR_READWRITE);
	    else
		sh_len = 0;
	}
	if (sh_len == 0)
	    set(STRshell, Strsave(STR_SHELLPATH), VAR_READWRITE);
    }

#ifdef COLOR_LS_F
    if ((tcp = getenv("LS_COLORS")) != NULL)
	parseLS_COLORS(str2short(tcp));
#endif /* COLOR_LS_F */

    doldol = putn((int) getpid());	/* For $$ */
#ifdef WINNT_NATIVE
    {
	char *strtmp1, strtmp2[MAXPATHLEN];
	if ((strtmp1 = getenv("TMP")) != NULL)
	    wsprintf(strtmp2, "%s/%s", strtmp1, "sh");
	shtemp = Strspl(SAVE(strtmp2), doldol);	/* For << */
    }
#else /* !WINNT_NATIVE */
    shtemp = Strspl(STRtmpsh, doldol);	/* For << */
#endif /* WINNT_NATIVE */

    /*
     * Record the interrupt states from the parent process. If the parent is
     * non-interruptible our hand must be forced or we (and our children) won't
     * be either. Our children inherit termination from our parent. We catch it
     * only if we are the login shell.
     */
#ifdef BSDSIGS
    /* 
     * PURIFY-2 claims that osv does not get 
     * initialized after the sigvec call
     */
    setzero((char*) &osv, sizeof(osv));
    /* parents interruptibility */
    (void) mysigvec(SIGINT, NULL, &osv);
    parintr = (signalfun_t) osv.sv_handler;
    (void) mysigvec(SIGTERM, NULL, &osv);
    parterm = (signalfun_t) osv.sv_handler;
#else				/* BSDSIGS */
    parintr = signal(SIGINT, SIG_IGN);	/* parents interruptibility */
    (void) sigset(SIGINT, parintr);	/* ... restore */

# ifdef COHERENT
    if (loginsh) /* it seems that SIGTERM is always set to SIG_IGN by */
                 /* init/getty so it should be set to SIG_DFL - there may be */
                 /* a better fix for this. */
         parterm = SIG_DFL;
    else
# else /* !COHERENT */
    parterm = signal(SIGTERM, SIG_IGN);	/* parents terminability */
# endif /* COHERENT */
    (void) sigset(SIGTERM, parterm);	/* ... restore */

#endif /* BSDSIGS */


#ifdef TCF
    /* Enable process migration on ourselves and our progeny */
    (void) signal(SIGMIGRATE, SIG_DFL);
#endif /* TCF */

    /*
     * dspkanji/dspmbyte autosetting
     */
    /* PATCH IDEA FROM Issei.Suzuki VERY THANKS */
#if defined(DSPMBYTE)
#if defined(NLS) && defined(LC_CTYPE)
    if (((tcp = setlocale(LC_CTYPE, NULL)) != NULL || (tcp = getenv("LANG")) != NULL) && !adrof(CHECK_MBYTEVAR)) {
#else
    if ((tcp = getenv("LANG")) != NULL && !adrof(CHECK_MBYTEVAR)) {
#endif
	autoset_dspmbyte(str2short(tcp));
    }
#if defined(WINNT_NATIVE)
    else if (!adrof(CHECK_MBYTEVAR))
      nt_autoset_dspmbyte();
#endif /* WINNT_NATIVE */
#endif

    /*
     * Process the arguments.
     * 
     * Note that processing of -v/-x is actually delayed till after script
     * processing.
     * 
     * We set the first character of our name to be '-' if we are a shell 
     * running interruptible commands.  Many programs which examine ps'es 
     * use this to filter such shells out.
     */
    argc--, tempv++;
    while (argc > 0 && (tcp = tempv[0])[0] == '-' &&
	   *++tcp != '\0' && !batch) {
	do
	    switch (*tcp++) {

	    case 0:		/* -	Interruptible, no prompt */
		prompt = 0;
		setintr = 1;
		nofile = 1;
		break;

	    case 'b':		/* -b	Next arg is input file */
		batch = 1;
		break;

	    case 'c':		/* -c	Command input from arg */
		if (argc == 1)
		    xexit(0);
		argc--, tempv++;
#ifdef M_XENIX
		/* Xenix Vi bug:
		   it relies on a 7 bit environment (/bin/sh), so it
		   pass ascii arguments with the 8th bit set */
		if (!strcmp(argv[0], "sh"))
		  {
		    char *p;

		    for (p = tempv[0]; *p; ++p)
		      *p &= ASCII;
		  }
#endif
		arginp = SAVE(tempv[0]);

		/*
		 * we put the command into a variable
		 */
		if (arginp != NULL)
		  set(STRcommand, quote(Strsave(arginp)), VAR_READWRITE);

		/*
		 * * Give an error on -c arguments that end in * backslash to
		 * ensure that you don't make * nonportable csh scripts.
		 */
		{
		    register int count;

		    cp = arginp + Strlen(arginp);
		    count = 0;
		    while (cp > arginp && *--cp == '\\')
			++count;
		    if ((count & 1) != 0) {
			exiterr = 1;
			stderror(ERR_ARGC);
		    }
		}
		prompt = 0;
		nofile = 1;
		break;
	    case 'd':		/* -d	Load directory stack from file */
		rdirs = 1;
		break;

#ifdef apollo
	    case 'D':		/* -D	Define environment variable */
		{
		    register Char *dp;

		    cp = str2short(tcp);
		    if (dp = Strchr(cp, '=')) {
			*dp++ = '\0';
			tsetenv(cp, dp);
		    }
		    else
			tsetenv(cp, STRNULL);
		}
		*tcp = '\0'; 	/* done with this argument */
		break;
#endif /* apollo */

	    case 'e':		/* -e	Exit on any error */
		exiterr = 1;
		break;

	    case 'f':		/* -f	Fast start */
		fast = 1;
		break;

	    case 'i':		/* -i	Interactive, even if !intty */
		intact = 1;
		nofile = 1;
		break;

	    case 'm':		/* -m	read .cshrc (from su) */
		mflag = 1;
		break;

	    case 'n':		/* -n	Don't execute */
		noexec = 1;
		break;

	    case 'q':		/* -q	(Undoc'd) ... die on quit */
		quitit = 1;
		break;

	    case 's':		/* -s	Read from std input */
		nofile = 1;
		break;

	    case 't':		/* -t	Read one line from input */
		onelflg = 2;
		prompt = 0;
		nofile = 1;
		break;

	    case 'v':		/* -v	Echo hist expanded input */
		nverbose = 1;	/* ... later */
		break;

	    case 'x':		/* -x	Echo just before execution */
		nexececho = 1;	/* ... later */
		break;

	    case 'V':		/* -V	Echo hist expanded input */
		setNS(STRverbose);	/* NOW! */
		break;

	    case 'X':		/* -X	Echo just before execution */
		setNS(STRecho);	/* NOW! */
		break;

	    case 'F':		/* Undocumented flag */
		/*
		 * This will cause children to be created using fork instead of
		 * vfork.
		 */
		use_fork = 1;
		break;

	    case ' ':
	    case '\t':
		/* 
		 * for O/S's that don't do the argument parsing right in 
		 * "#!/foo -f " scripts
		 */
		break;

	    default:		/* Unknown command option */
		exiterr = 1;
		stderror(ERR_TCSHUSAGE, tcp-1, progname);
		break;

	} while (*tcp);
	tempv++, argc--;
    }

    if (quitit)			/* With all due haste, for debugging */
	(void) signal(SIGQUIT, SIG_DFL);

    /*
     * Unless prevented by -, -c, -i, -s, or -t, if there are remaining
     * arguments the first of them is the name of a shell file from which to
     * read commands.
     */
    if (nofile == 0 && argc > 0) {
	nofile = open(tempv[0], O_RDONLY);
	if (nofile < 0) {
	    child = 1;		/* So this ... */
	    /* ... doesn't return */
	    stderror(ERR_SYSTEM, tempv[0], strerror(errno));
	}
	if (ffile != NULL)
	    xfree((ptr_t) ffile);
	dolzero = 1;
	ffile = SAVE(tempv[0]);
	/* 
	 * Replace FSHIN. Handle /dev/std{in,out,err} specially
	 * since once they are closed we cannot open them again.
	 * In that case we use our own saved descriptors
	 */
	if ((SHIN = dmove(nofile, FSHIN)) < 0) 
	    switch(nofile) {
	    case 0:
		SHIN = FSHIN;
		break;
	    case 1:
		SHIN = FSHOUT;
		break;
	    case 2:
		SHIN = FSHDIAG;
		break;
	    default:
		stderror(ERR_SYSTEM, tempv[0], strerror(errno));
		break;
	    }
	(void) close_on_exec(SHIN, 1);
	prompt = 0;
	 /* argc not used any more */ tempv++;
    }


    /*
     * Consider input a tty if it really is or we are interactive. but not for
     * editing (christos)
     */
    if (!(intty = isatty(SHIN))) {
	if (adrof(STRedit))
	    unsetv(STRedit);
	editing = 0;
    }
    intty |= intact;
#ifndef convex
    if (intty || (intact && isatty(SHOUT))) {
	if (!batch && (uid != euid || gid != egid)) {
	    errno = EACCES;
	    child = 1;		/* So this ... */
	    /* ... doesn't return */
	    stderror(ERR_SYSTEM, progname, strerror(errno));
	}
    }
#endif /* convex */
    isoutatty = isatty(SHOUT);
    isdiagatty = isatty(SHDIAG);
    /*
     * Decide whether we should play with signals or not. If we are explicitly
     * told (via -i, or -) or we are a login shell (arg0 starts with -) or the
     * input and output are both the ttys("csh", or "csh</dev/ttyx>/dev/ttyx")
     * Note that in only the login shell is it likely that parent may have set
     * signals to be ignored
     */
    if (loginsh || intact || (intty && isatty(SHOUT)))
	setintr = 1;
    settell();
    /*
     * Save the remaining arguments in argv.
     */
    setq(STRargv, blk2short(tempv), &shvhed, VAR_READWRITE);

    /*
     * Set up the prompt.
     */
    if (prompt) {
	if (tcsh)
	    set(STRprompt, Strsave(STRdeftcshprompt), VAR_READWRITE);
	else
	    set(STRprompt, Strsave(STRdefcshprompt), VAR_READWRITE);
	/* that's a meta-questionmark */
	set(STRprompt2, Strsave(STRmquestion), VAR_READWRITE);
	set(STRprompt3, Strsave(STRKCORRECT), VAR_READWRITE);
    }

    /*
     * If we are an interactive shell, then start fiddling with the signals;
     * this is a tricky game.
     */
    shpgrp = mygetpgrp();
    opgrp = tpgrp = -1;
    if (setintr) {
	signalfun_t osig;
	**argv = '-';
	if (!quitit)		/* Wary! */
	    (void) signal(SIGQUIT, SIG_IGN);
	(void) sigset(SIGINT, pintr);
	(void) sighold(SIGINT);
	(void) signal(SIGTERM, SIG_IGN);

	/* 
	 * No reason I can see not to save history on all these events..
	 * Most usual occurrence is in a window system, where we're not a login
	 * shell, but might as well be... (sg)
	 * But there might be races when lots of shells exit together...
	 * [this is also incompatible].
	 * We have to be mre careful here. If the parent wants to 
	 * ignore the signals then we leave them untouched...
	 * We also only setup the handlers for shells that are trully
	 * interactive.
	 */
	osig = signal(SIGHUP, phup);	/* exit processing on HUP */
	if (!loginsh && osig == SIG_IGN)
	    (void) signal(SIGHUP, osig);
#ifdef SIGXCPU
	osig = signal(SIGXCPU, phup);	/* exit processing on XCPU */
	if (!loginsh && osig == SIG_IGN)
	    (void) signal(SIGXCPU, osig);
#endif
#ifdef SIGXFSZ
	osig = signal(SIGXFSZ, phup);	/* exit processing on XFSZ */
	if (!loginsh && osig == SIG_IGN)
	    (void) signal(SIGXFSZ, osig);
#endif

	if (quitit == 0 && arginp == 0) {
#ifdef SIGTSTP
	    (void) signal(SIGTSTP, SIG_IGN);
#endif
#ifdef SIGTTIN
	    (void) signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTTOU
	    (void) signal(SIGTTOU, SIG_IGN);
#endif
	    /*
	     * Wait till in foreground, in case someone stupidly runs csh &
	     * dont want to try to grab away the tty.
	     */
	    if (isatty(FSHDIAG))
		f = FSHDIAG;
	    else if (isatty(FSHOUT))
		f = FSHOUT;
	    else if (isatty(OLDSTD))
		f = OLDSTD;
	    else
		f = -1;

#ifdef NeXT
	    /* NeXT 2.0 /usr/etc/rlogind, does not set our process group! */
	    if (shpgrp == 0) {
	        shpgrp = getpid();
		(void) setpgid(0, shpgrp);
	        (void) tcsetpgrp(f, shpgrp);
	    }
#endif /* NeXT */
#ifdef BSDJOBS			/* if we have tty job control */
    retry:
	    if ((tpgrp = tcgetpgrp(f)) != -1) {
		if (tpgrp != shpgrp) {
		    signalfun_t old = signal(SIGTTIN, SIG_DFL);
		    (void) kill(0, SIGTTIN);
		    (void) signal(SIGTTIN, old);
		    goto retry;
		}
		/*
		 * Thanks to Matt Day for the POSIX references, and to
		 * Paul Close for the SGI clarification.
		 */
		if (setdisc(f) != -1) {
		    opgrp = shpgrp;
		    shpgrp = getpid();
		    tpgrp = shpgrp;
		    if (tcsetpgrp(f, shpgrp) == -1) {
			/*
			 * On hpux 7.03 this fails with EPERM. This happens on
			 * the 800 when opgrp != shpgrp at this point. (we were
			 * forked from a non job control shell)
			 * POSIX 7.2.4, says we failed because the process
			 * group specified did not belong to a process
			 * in the same session with the tty. So we set our
			 * process group and try again.
			 */
			if (setpgid(0, shpgrp) == -1) {
			    xprintf("setpgid:");
			    goto notty;
			}
			if (tcsetpgrp(f, shpgrp) == -1) {
			    xprintf("tcsetpgrp:");
			    goto notty;
			}
		    }
		    /*
		     * We check the process group now. If it is the same, then
		     * we don't need to set it again. On hpux 7.0 on the 300's
		     * if we set it again it fails with EPERM. This is the
		     * correct behavior according to POSIX 4.3.3 if the process
		     * was a session leader .
		     */
		    else if (shpgrp != mygetpgrp()) {
			if(setpgid(0, shpgrp) == -1) {
			    xprintf("setpgid:");
			    goto notty;
			}
		    }
#ifdef IRIS4D
		    /*
		     * But on irix 3.3 we need to set it again, even if it is
		     * the same. We do that to tell the system that we
		     * need BSD process group compatibility.
		     */
		    else
			(void) setpgid(0, shpgrp);
#endif
		    (void) close_on_exec(dcopy(f, FSHTTY), 1);
		}
		else
		    tpgrp = -1;
	    }
	    if (tpgrp == -1) {
	notty:
		xprintf(CGETS(11, 1, "Warning: no access to tty (%s).\n"),
			strerror(errno));
		xprintf(CGETS(11, 2, "Thus no job control in this shell.\n"));
		/*
		 * Fix from:Sakari Jalovaara <sja@sirius.hut.fi> if we don't
		 * have access to tty, disable editing too
		 */
		if (adrof(STRedit))
		    unsetv(STRedit);
		editing = 0;
	    }
#else	/* BSDJOBS */		/* don't have job control, so frotz it */
	    tpgrp = -1;
#endif				/* BSDJOBS */
	}
    }
    if ((setintr == 0) && (parintr == SIG_DFL))
	setintr = 1;

/*
 * SVR4 doesn't send a SIGCHLD when a child is stopped or continued if the
 * handler is installed with signal(2) or sigset(2).  sigaction(2) must
 * be used instead.
 *
 * David Dawes (dawes@physics.su.oz.au) Sept 1991
 */

#if SYSVREL > 3
    {
	struct sigaction act;
        act.sa_handler=pchild;
	(void) sigemptyset(&(act.sa_mask));	/* Don't block any extra sigs
						 * when the handler is called
						 */
        act.sa_flags=0;	           /* want behaviour of sigset() without
                                    * SA_NOCLDSTOP
				    */

        if ((sigaction(SIGCHLD,&act,(struct sigaction *)NULL)) == -1)
	    stderror(ERR_SYSTEM, "sigaction", strerror(errno));
    }
#else /* SYSVREL <= 3 */
    (void) sigset(SIGCHLD, pchild);	/* while signals not ready */
#endif /* SYSVREL <= 3 */


    if (intty && !arginp) 	
	(void) ed_Setup(editing);/* Get the tty state, and set defaults */
				 /* Only alter the tty state if editing */
    
    /*
     * Set an exit here in case of an interrupt or error reading the shell
     * start-up scripts.
     */
    reenter = setexit();	/* PWP */
    exitset++;
    haderr = 0;			/* In case second time through */
    if (!fast && reenter == 0) {
	/* Will have varval(STRhome) here because set fast if don't */
	{
	    int     osetintr = setintr;
	    signalfun_t oparintr = parintr;

#ifdef BSDSIGS
	    sigmask_t omask = sigblock(sigmask(SIGINT));
#else
	    (void) sighold(SIGINT);
#endif
	    setintr = 0;
	    parintr = SIG_IGN;	/* onintr in /etc/ files has no effect */
#ifdef LOGINFIRST
#ifdef _PATH_DOTLOGIN
	    if (loginsh)
	      (void) srcfile(_PATH_DOTLOGIN, 0, 0, NULL);
#endif
#endif

#ifdef _PATH_DOTCSHRC
	    (void) srcfile(_PATH_DOTCSHRC, 0, 0, NULL);
#endif
	    if (!arginp && !onelflg && !havhash)
		dohash(NULL,NULL);
#ifndef LOGINFIRST
#ifdef _PATH_DOTLOGIN
	    if (loginsh)
		(void) srcfile(_PATH_DOTLOGIN, 0, 0, NULL);
#endif
#endif
#ifdef BSDSIGS
	    (void) sigsetmask(omask);
#else
	    (void) sigrelse(SIGINT);
#endif
	    setintr = osetintr;
	    parintr = oparintr;
	}
#ifdef LOGINFIRST
	if (loginsh)
	    (void) srccat(varval(STRhome), STRsldotlogin);
#endif
	/* upward compat. */
	if (!srccat(varval(STRhome), STRsldottcshrc))
	    (void) srccat(varval(STRhome), STRsldotcshrc);

	if (!fast && !arginp && !onelflg && !havhash)
	    dohash(NULL,NULL);

	/*
	 * Source history before .login so that it is available in .login
	 */
	loadhist(NULL, 0);
#ifndef LOGINFIRST
	if (loginsh)
	    (void) srccat(varval(STRhome), STRsldotlogin);
#endif
	if (!fast && (loginsh || rdirs))
	    loaddirs(NULL);
    }
    /* Initing AFTER .cshrc is the Right Way */
    if (intty && !arginp) {	/* PWP setup stuff */
	ed_Init();		/* init the new line editor */
#ifdef SIG_WINDOW
	check_window_size(1);	/* mung environment */
#endif				/* SIG_WINDOW */
    }

    /*
     * Now are ready for the -v and -x flags
     */
    if (nverbose)
	setNS(STRverbose);
    if (nexececho)
	setNS(STRecho);
    
    /*
     * All the rest of the world is inside this call. The argument to process
     * indicates whether it should catch "error unwinds".  Thus if we are a
     * interactive shell our call here will never return by being blown past on
     * an error.
     */
    process(setintr);

    /*
     * Mop-up.
     */
    if (intty) {
	if (loginsh) {
	    xprintf("logout\n");
	    (void) close(SHIN);
	    child = 1;
#ifdef TESLA
	    do_logout = 1;
#endif				/* TESLA */
	    goodbye(NULL, NULL);
	}
	else {
	    xprintf("exit\n");
	}
    }
    record();
    exitstat();
    return (0);
}

void
untty()
{
#ifdef BSDJOBS
    if (tpgrp > 0 && opgrp != shpgrp) {
	(void) setpgid(0, opgrp);
	(void) tcsetpgrp(FSHTTY, opgrp);
	(void) resetdisc(FSHTTY);
    }
#endif /* BSDJOBS */
}

void
importpath(cp)
    Char   *cp;
{
    register int i = 0;
    register Char *dp;
    register Char **pv;
    int     c;

    for (dp = cp; *dp; dp++)
	if (*dp == PATHSEP)
	    i++;
    /*
     * i+2 where i is the number of colons in the path. There are i+1
     * directories in the path plus we need room for a zero terminator.
     */
    pv = (Char **) xcalloc((size_t) (i + 2), sizeof(Char *));
    dp = cp;
    i = 0;
    if (*dp)
	for (;;) {
	    if ((c = *dp) == PATHSEP || c == 0) {
		*dp = 0;
		pv[i++] = Strsave(*cp ? cp : STRdot);
		if (c) {
		    cp = dp + 1;
		    *dp = PATHSEP;
		}
		else
		    break;
	    }
#ifdef WINNT_NATIVE
	    else if (*dp == '\\')
		*dp = '/';
#endif /* WINNT_NATIVE */
	    dp++;
	}
    pv[i] = 0;
    setq(STRpath, pv, &shvhed, VAR_READWRITE);
}

/*
 * Source to the file which is the catenation of the argument names.
 */
static int
srccat(cp, dp)
    Char   *cp, *dp;
{
    if (cp[0] == '/' && cp[1] == '\0') 
	return srcfile(short2str(dp), (mflag ? 0 : 1), 0, NULL);
    else {
	register Char *ep;
	char   *ptr;
	int rv;

#ifdef WINNT_NATIVE
	ep = cp;
	while(*ep)
	    ep++;
	if (ep[-1] == '/' && dp[0] == '/') /* silly win95 */
	    dp++;
#endif /* WINNT_NATIVE */

	ep = Strspl(cp, dp);
	ptr = short2str(ep);

	rv = srcfile(ptr, (mflag ? 0 : 1), 0, NULL);
	xfree((ptr_t) ep);
	return rv;
    }
}

/*
 * Source to a file putting the file descriptor in a safe place (> 2).
 */
static int
srcfile(f, onlyown, flag, av)
    char   *f;
    bool    onlyown;
    int flag;
    Char **av;
{
    register int unit;

    if ((unit = open(f, O_RDONLY)) == -1) 
	return 0;
    unit = dmove(unit, -1);

    (void) close_on_exec(unit, 1);
    srcunit(unit, onlyown, flag, av);
    return 1;
}


/*
 * Save the shell state, and establish new argument vector, and new input
 * fd.
 */
static void
st_save(st, unit, hflg, al, av)
    struct saved_state *st;
    int unit, hflg;
    Char **al, **av;
{
    st->insource	= insource;
    st->SHIN		= SHIN;
    st->intty		= intty;
    st->whyles		= whyles;
    st->gointr		= gointr;
    st->arginp		= arginp;
    st->evalp		= evalp;
    st->evalvec		= evalvec;
    st->alvecp		= alvecp;
    st->alvec		= alvec;
    st->onelflg		= onelflg;
    st->enterhist	= enterhist;
    if (hflg)
	st->HIST	= HIST;
    else
	st->HIST	= '\0';
    st->cantell		= cantell;
    cpybin(st->B, B);

    /*
     * we can now pass arguments to source. 
     * For compatibility we do that only if arguments were really
     * passed, otherwise we keep the old, global $argv like before.
     */
    if (av != NULL && *av != NULL) {
	struct varent *vp;
	if ((vp = adrof(STRargv)) != NULL)
	    st->argv = saveblk(vp->vec);
	else
	    st->argv = NULL;
	setq(STRargv, saveblk(av), &shvhed, VAR_READWRITE);
    }
    else
	st->argv = NULL;

    SHIN	= unit;	/* Do this first */

    /* Establish new input arena */
    {
	fbuf = NULL;
	fseekp = feobp = fblocks = 0;
	settell();
    }

    arginp	= 0;
    onelflg	= 0;
    intty	= isatty(SHIN);
    whyles	= 0;
    gointr	= 0;
    evalvec	= 0;
    evalp	= 0;
    alvec	= al;
    alvecp	= 0;
    enterhist	= hflg;
    if (enterhist)
	HIST	= '\0';
    insource	= 1;
}


/*
 * Restore the shell to a saved state
 */
static void
st_restore(st, av)
    struct saved_state *st;
    Char **av;
{
    if (st->SHIN == -1)
	return;

    /* Reset input arena */
    {
	register int i;
	register Char** nfbuf = fbuf;
	register int nfblocks = fblocks;

	fblocks = 0;
	fbuf = NULL;
	for (i = 0; i < nfblocks; i++)
	    xfree((ptr_t) nfbuf[i]);
	xfree((ptr_t) nfbuf);
    }
    cpybin(B, st->B);

    (void) close(SHIN);

    insource	= st->insource;
    SHIN	= st->SHIN;
    arginp	= st->arginp;
    onelflg	= st->onelflg;
    evalp	= st->evalp;
    evalvec	= st->evalvec;
    alvecp	= st->alvecp;
    alvec	= st->alvec;
    intty	= st->intty;
    whyles	= st->whyles;
    gointr	= st->gointr;
    if (st->HIST != '\0')
	HIST	= st->HIST;
    enterhist	= st->enterhist;
    cantell	= st->cantell;

    if (st->argv != NULL)
	setq(STRargv, st->argv, &shvhed, VAR_READWRITE);
    else if (av != NULL  && *av != NULL && adrof(STRargv) != NULL)
	unsetv(STRargv);
}

/*
 * Source to a unit.  If onlyown it must be our file or our group or
 * we don't chance it.	This occurs on ".cshrc"s and the like.
 */
static void
srcunit(unit, onlyown, hflg, av)
    register int unit;
    bool    onlyown;
    int hflg;
    Char **av;
{
    struct saved_state st;
    st.SHIN = -1;	/* st_restore checks this */

    if (unit < 0)
	return;

    if (didfds)
	donefds();

    if (onlyown) {
	struct stat stb;

	if (fstat(unit, &stb) < 0) {
	    (void) close(unit);
	    return;
	}
    }

    getexit(st.oldexit);

    if (setintr)
#ifdef BSDSIGS
	st.mask = sigblock(sigmask(SIGINT));
#else
	(void) sighold(SIGINT);
#endif

    /* Save the current state and move us to a new state */
    st_save(&st, unit, hflg, NULL, av);

    /*
     * Now if we are allowing commands to be interrupted, we let ourselves be
     * interrupted.
     */
    if (setintr)
#ifdef BSDSIGS
	(void) sigsetmask(st.mask);
#else
	(void) sigrelse(SIGINT);
#endif

    /*
     * Bugfix for running out of memory by: Jak Kirman
     * <jak%cs.brown.edu@RELAY.CS.NET>.  Solution: pay attention to what
     * setexit() is returning because reenter _may_ be in a register, and
     * thus restored to 0 on a longjump(). (PWP: insert flames about
     * compiler-dependant code here) PWP: THANKS LOTS !!!
     *
     * PWP: think of this as like a LISP (unwind-protect ...)
     * thanks to Diana Smetters for pointing out how this _should_ be written
     */
#ifdef cray
    st.reenter = 1;		/* assume non-zero return val */
    if (setexit() == 0) {
	st.reenter = 0;		/* Oh well, we were wrong */
#else
    if ((st.reenter = setexit()) == 0) {
#endif
	process(0);		/* 0 -> blow away on errors */
    }

    if (setintr)
#ifdef BSDSIGS
	(void) sigsetmask(st.mask);
#else
	(void) sigrelse(SIGINT);
#endif

    /* Restore the old state */
    st_restore(&st, av);
    resexit(st.oldexit);
    /*
     * If process reset() (effectively an unwind) then we must also unwind.
     */
    if (st.reenter)
	stderror(ERR_SILENT);
}


/*ARGSUSED*/
void
goodbye(v, c)
    Char **v;
    struct command *c;
{
    USE(c);
    record();

    if (loginsh) {
	(void) sigset(SIGQUIT, SIG_IGN);
	(void) sigset(SIGINT, SIG_IGN);
	(void) sigset(SIGTERM, SIG_IGN);
	(void) sigset(SIGHUP, SIG_IGN);
	setintr = 0;		/* No interrupts after "logout" */
	/* Trap errors inside .logout */
	reenter = setexit();
	if (reenter != 0)
	    exitstat();
	if (!(adrof(STRlogout)))
	    set(STRlogout, Strsave(STRnormal), VAR_READWRITE);
#ifdef _PATH_DOTLOGOUT
	(void) srcfile(_PATH_DOTLOGOUT, 0, 0, NULL);
#endif
	if (adrof(STRhome))
	    (void) srccat(varval(STRhome), STRsldtlogout);
#ifdef TESLA
	do_logout = 1;
#endif /* TESLA */
    }
    exitstat();
}

void
exitstat()
{
#ifdef PROF
    monitor(0);
#endif
    /*
     * Note that if STATUS is corrupted (i.e. getn bombs) then error will exit
     * directly because we poke child here. Otherwise we might continue
     * unwarrantedly (sic).
     */
    child = 1;

    xexit(getn(varval(STRstatus)));
}

/*
 * in the event of a HUP we want to save the history
 */
static  sigret_t
phup(snum)
int snum;
{
    /*
     * There is no return from here,
     * so we are not going to release SIGHUP
     * anymore
     */
#ifdef UNRELSIGS
    if (snum)
	(void) sigset(snum, SIG_IGN);
#else
# ifdef BSDSIGS
    (void) sigblock(sigmask(SIGHUP));
# else
    (void) sighold(SIGHUP);
# endif /* BSDSIGS */
#endif /* UNRELSIGS */

    if (loginsh) {
	set(STRlogout, Strsave(STRhangup), VAR_READWRITE);
#ifdef _PATH_DOTLOGOUT
	(void) srcfile(_PATH_DOTLOGOUT, 0, 0, NULL);
#endif
	if (adrof(STRhome))
	    (void) srccat(varval(STRhome), STRsldtlogout);
    }

    record();

#ifdef POSIXJOBS 
    /*
     * We kill the last foreground process group. It then becomes
     * responsible to propagate the SIGHUP to its progeny. 
     */
    {
	struct process *pp, *np;

	for (pp = proclist.p_next; pp; pp = pp->p_next) {
	    np = pp;
	    /* 
	     * Find if this job is in the foreground. It could be that
	     * the process leader has exited and the foreground flag
	     * is cleared for it.
	     */
	    do 
		/*
		 * If a process is in the foreground we try to kill
		 * it's process group. If we succeed, then the 
		 * whole job is gone. Otherwise we keep going...
		 * But avoid sending HUP to the shell again.
		 */
		if (((np->p_flags & PFOREGND) != 0) && np->p_jobid != shpgrp) {
		    np->p_flags &= ~PHUP;
		    if (killpg(np->p_jobid, SIGHUP) != -1) {
			/* In case the job was suspended... */
#ifdef SIGCONT
			(void) killpg(np->p_jobid, SIGCONT);
#endif
			break;
		    }
		}
	    while ((np = np->p_friends) != pp);
	}
    }
#endif /* POSIXJOBS */

    xexit(snum);
#ifndef SIGVOID
    return (snum);
#endif
}

static Char   *jobargv[2] = {STRjobs, 0};

/*
 * Catch an interrupt, e.g. during lexical input.
 * If we are an interactive shell, we reset the interrupt catch
 * immediately.  In any case we drain the shell output,
 * and finally go through the normal error mechanism, which
 * gets a chance to make the shell go away.
 */
int     just_signaled;		/* bugfix by Michael Bloom (mg@ttidca.TTI.COM) */

#ifdef SIGVOID
/*ARGSUSED*/
#endif
sigret_t
pintr(snum)
int snum;
{
#ifdef UNRELSIGS
    if (snum)
	(void) sigset(snum, pintr);
#endif /* UNRELSIGS */
    just_signaled = 1;
    pintr1(1);
#ifndef SIGVOID
    return (snum);
#endif
}

void
pintr1(wantnl)
    bool    wantnl;
{
    register Char **v;
#ifdef BSDSIGS
    sigmask_t omask;
#endif

#ifdef BSDSIGS
    omask = sigblock((sigmask_t) 0);
#endif
    if (setintr) {
#ifdef BSDSIGS
	(void) sigsetmask(omask & ~sigmask(SIGINT));
#else
	(void) sigrelse(SIGINT);
#endif
	if (pjobs) {
	    pjobs = 0;
	    xputchar('\n');
	    dojobs(jobargv, NULL);
	    stderror(ERR_NAME | ERR_INTR);
	}
    }
    /* MH - handle interrupted completions specially */
    {
	extern int InsideCompletion;

	if (InsideCompletion)
	    stderror(ERR_SILENT);
    }
    /* JV - Make sure we shut off inputl */
    {
	extern Char GettingInput;

	(void) Cookedmode();
	GettingInput = 0;
    }
#ifdef BSDSIGS
    (void) sigsetmask(omask & ~sigmask(SIGCHLD));
#else
    if (setintr)
	(void) sighold(SIGINT);
    (void) sigrelse(SIGCHLD);
#endif
    drainoline();
#if !defined(_VMS_POSIX) && !defined(WINNT_NATIVE)
    (void) endpwent();
#endif /* !_VMS_POSIX && !WINNT_NATIVE */

    /*
     * If we have an active "onintr" then we search for the label. Note that if
     * one does "onintr -" then we shan't be interruptible so we needn't worry
     * about that here.
     */
    if (gointr) {
	gotolab(gointr);
	timflg = 0;
	if ((v = pargv) != 0)
	    pargv = 0, blkfree(v);
	if ((v = gargv) != 0)
	    gargv = 0, blkfree(v);
	reset();
    }
    else if (intty && wantnl) {
	if (editing) {
	    /* 
	     * If we are editing a multi-line input command, and move to
	     * the beginning of the line, we don't want to trash it when
	     * we hit ^C
	     */
	    PastBottom();
	    ClearLines();
	    ClearDisp();
	}
	else {
	    /* xputchar('\n'); *//* Some like this, others don't */
	    (void) putraw('\r');
	    (void) putraw('\n');
	}
    }
    stderror(ERR_SILENT);
}

/*
 * Process is the main driving routine for the shell.
 * It runs all command processing, except for those within { ... }
 * in expressions (which is run by a routine evalav in sh.exp.c which
 * is a stripped down process), and `...` evaluation which is run
 * also by a subset of this code in sh.glob.c in the routine backeval.
 *
 * The code here is a little strange because part of it is interruptible
 * and hence freeing of structures appears to occur when none is necessary
 * if this is ignored.
 *
 * Note that if catch is not set then we will unwind on any error.
 * If an end-of-file occurs, we return.
 */
static struct command *savet = NULL;
void
process(catch)
    bool    catch;
{
    extern char Expand;
    jmp_buf_t osetexit;
    /* PWP: This might get nuked my longjmp so don't make it a register var */
    struct command *t = savet;

    savet = NULL;
    getexit(osetexit);
    for (;;) {

	pendjob();

	/* This was leaking memory badly, particularly when sourcing
	 * files, etc.. For whatever reason we were arriving here with
	 * allocated pointers still active, and the code was simply
	 * overwriting them.  I can't say I fully understand the
	 * control flow here, but according to Purify this particular
	 * leak has been plugged, and I haven't noticed any ill
	 * effects.. (sg)
	 */
	if (paraml.next && paraml.next != &paraml)
	    freelex(&paraml);

	paraml.next = paraml.prev = &paraml;
	paraml.word = STRNULL;
	(void) setexit();
	justpr = enterhist;	/* execute if not entering history */

	/*
	 * Interruptible during interactive reads
	 */
	if (setintr)
#ifdef BSDSIGS
	    (void) sigsetmask(sigblock((sigmask_t) 0) & ~sigmask(SIGINT));
#else
	    (void) sigrelse(SIGINT);
#endif


	/*
	 * For the sake of reset()
	 */
	freelex(&paraml);
	if (savet)
	    freesyn(savet), savet = NULL;

	if (haderr) {
	    if (!catch) {
		/* unwind */
		doneinp = 0;
		savet = t;
		resexit(osetexit);
		reset();
	    }
	    haderr = 0;
	    /*
	     * Every error is eventually caught here or the shell dies.  It is
	     * at this point that we clean up any left-over open files, by
	     * closing all but a fixed number of pre-defined files.  Thus
	     * routines don't have to worry about leaving files open due to
	     * deeper errors... they will get closed here.
	     */
	    closem();
	    continue;
	}
	if (doneinp) {
	    doneinp = 0;
	    break;
	}
	if (chkstop)
	    chkstop--;
	if (neednote)
	    pnote();
	if (intty && prompt && evalvec == 0) {
	    just_signaled = 0;
	    mailchk();
	    /*
	     * Watch for logins/logouts. Next is scheduled commands stored
	     * previously using "sched." Then execute periodic commands.
	     * Following that, the prompt precmd is run.
	     */
#ifndef HAVENOUTMP
	    watch_login(0);
#endif /* !HAVENOUTMP */
	    sched_run(0);
	    period_cmd();
	    precmd();
	    /*
	     * If we are at the end of the input buffer then we are going to
	     * read fresh stuff. Otherwise, we are rereading input and don't
	     * need or want to prompt.
	     */
	    if (fseekp == feobp && aret == TCSH_F_SEEK)
		printprompt(0, NULL);
	    flush();
	    setalarm(1);
	}
	if (seterr) {
	    xfree((ptr_t) seterr);
	    seterr = NULL;
	}

	/*
	 * Echo not only on VERBOSE, but also with history expansion. If there
	 * is a lexical error then we forego history echo.
	 */
	if ((lex(&paraml) && !seterr && intty && !tellwhat && !Expand && 
	     !whyles) || adrof(STRverbose)) {
	    haderr = 1;
	    prlex(&paraml);
	    haderr = 0;
	}
	(void) alarm(0);	/* Autologout OFF */

	/*
	 * The parser may lose space if interrupted.
	 */
	if (setintr)
#ifdef BSDSIGS
	    (void) sigblock(sigmask(SIGINT));
#else
	    (void) sighold(SIGINT);
#endif

	/*
	 * Save input text on the history list if reading in old history, or it
	 * is from the terminal at the top level and not in a loop.
	 * 
	 * PWP: entry of items in the history list while in a while loop is done
	 * elsewhere...
	 */
	if (enterhist || (catch && intty && !whyles && !tellwhat && !arun))
	    savehist(&paraml, enterhist > 1);

	if (Expand && seterr)
	    Expand = 0;

	/*
	 * Print lexical error messages, except when sourcing history lists.
	 */
	if (!enterhist && seterr)
	    stderror(ERR_OLD);

	/*
	 * If had a history command :p modifier then this is as far as we
	 * should go
	 */
	if (justpr)
	    reset();

	/*
	 * If had a tellwhat from twenex() then do
	 */
	if (tellwhat) {
	    (void) tellmewhat(&paraml, NULL);
	    reset();
	}

	alias(&paraml);

#ifdef BSDJOBS
	/*
	 * If we are interactive, try to continue jobs that we have stopped
	 */
	if (prompt)
	    continue_jobs(&paraml);
#endif				/* BSDJOBS */

	/*
	 * Check to see if the user typed "rm * .o" or something
	 */
	if (prompt)
	    rmstar(&paraml);
	/*
	 * Parse the words of the input into a parse tree.
	 */
	savet = syntax(paraml.next, &paraml, 0);
	if (seterr)
	    stderror(ERR_OLD);

	postcmd();
	/*
	 * Execute the parse tree From: Michael Schroeder
	 * <mlschroe@immd4.informatik.uni-erlangen.de> was execute(t, tpgrp);
	 */
	execute(savet, (tpgrp > 0 ? tpgrp : -1), NULL, NULL);

	/*
	 * Made it!
	 */
	freelex(&paraml);
	freesyn(savet), savet = NULL;
#ifdef SIG_WINDOW
	if (catch && intty && !whyles && !tellwhat)
	    (void) window_change(0);	/* for window systems */
#endif /* SIG_WINDOW */
	set(STR_, Strsave(InputBuf), VAR_READWRITE | VAR_NOGLOB);
    }
    savet = t;
    resexit(osetexit);
}

/*ARGSUSED*/
void
dosource(t, c)
    register Char **t;
    struct command *c;
{
    register Char *f;
    bool    hflg = 0;
    extern int bequiet;
    char    buf[BUFSIZE];

    USE(c);
    t++;
    if (*t && eq(*t, STRmh)) {
	if (*++t == NULL)
	    stderror(ERR_NAME | ERR_HFLAG);
	hflg++;
    }
    else if (*t && eq(*t, STRmm)) {
    	if (*++t == NULL)
	    stderror(ERR_NAME | ERR_MFLAG);
	hflg = 2;
    }

    f = globone(*t++, G_ERROR);
    (void) strcpy(buf, short2str(f));
    xfree((ptr_t) f);
    if ((!srcfile(buf, 0, hflg, t)) && (!hflg) && (!bequiet))
	stderror(ERR_SYSTEM, buf, strerror(errno));
}

/*
 * Check for mail.
 * If we are a login shell, then we don't want to tell
 * about any mail file unless its been modified
 * after the time we started.
 * This prevents us from telling the user things he already
 * knows, since the login program insists on saying
 * "You have mail."
 */

/*
 * The AMS version.
 * This version checks if the file is a directory, and if so,
 * tells you the number of files in it, otherwise do the old thang.
 * The magic "+1" in the time calculation is to compensate for
 * an AFS bug where directory mtimes are set to 1 second in
 * the future.
 */

static void
mailchk()
{
    register struct varent *v;
    register Char **vp;
    time_t  t;
    int     intvl, cnt;
    struct stat stb;
    bool    new;

    v = adrof(STRmail);
    if (v == 0)
	return;
    (void) time(&t);
    vp = v->vec;
    cnt = blklen(vp);
    intvl = (cnt && number(*vp)) ? (--cnt, getn(*vp++)) : MAILINTVL;
    if (intvl < 1)
	intvl = 1;
    if (chktim + intvl > t)
	return;
    for (; *vp; vp++) {
	char *filename = short2str(*vp);
	char *mboxdir = filename;

	if (stat(filename, &stb) < 0)
	    continue;
#if defined(BSDTIMES) || defined(_SEQUENT_)
	new = stb.st_mtime > time0.tv_sec;
#else
	new = stb.st_mtime > seconds0;
#endif
	if (S_ISDIR(stb.st_mode)) {
	    DIR *mailbox;
	    int mailcount = 0;
	    char tempfilename[MAXPATHLEN];
	    struct stat stc;

	    xsnprintf(tempfilename, MAXPATHLEN, "%s/new", filename);

	    if (stat(tempfilename, &stc) != -1 && S_ISDIR(stc.st_mode)) {
		/*
		 * "filename/new" exists and is a directory; you are
		 * using Qmail.
		 */
		stb = stc;
#if defined(BSDTIMES) || defined(_SEQUENT_)
		new = stb.st_mtime > time0.tv_sec;
#else
		new = stb.st_mtime > seconds0;
#endif
		mboxdir = tempfilename;
	    }

	    if (stb.st_mtime <= chktim + 1 || (loginsh && !new))
		continue;

	    if ((mailbox = opendir(mboxdir)) == NULL)
		continue;

	    /* skip . and .. */
	    if (!readdir(mailbox) || !readdir(mailbox))
		continue;

	    while (readdir(mailbox))
		mailcount++;

	    if (mailcount == 0)
		continue;

	    if (cnt == 1)
		xprintf(CGETS(11, 3, "You have %d mail messages.\n"),
			mailcount);
	    else
		xprintf(CGETS(11, 4, "You have %d mail messages in %s.\n"),
			mailcount, filename);
	}
	else {
	    if (stb.st_size == 0 || stb.st_atime > stb.st_mtime ||
		(stb.st_atime <= chktim && stb.st_mtime <= chktim) ||
		(loginsh && !new))
		continue;
	    if (cnt == 1)
		xprintf(CGETS(11, 5, "You have %smail.\n"),
			new ? CGETS(11, 6, "new ") : "");
	    else
	        xprintf(CGETS(11, 7, "You have %smail in %s.\n"),
			new ? CGETS(11, 6, "new ") : "", filename);
	}
    }
    chktim = t;
}

/*
 * Extract a home directory from the password file
 * The argument points to a buffer where the name of the
 * user whose home directory is sought is currently.
 * We write the home directory of the user back there.
 */
int
gethdir(home)
    Char   *home;
{
    Char   *h;

    /*
     * Is it us?
     */
    if (*home == '\0') {
	if ((h = varval(STRhome)) != STRNULL) {
	    (void) Strcpy(home, h);
	    return 0;
	}
	else
	    return 1;
    }

    /*
     * Look in the cache
     */
    if ((h = gettilde(home)) == NULL)
	return 1;
    else {
	(void) Strcpy(home, h);
	return 0;
    }
}

/*
 * Move the initial descriptors to their eventual
 * resting places, closing all other units.
 */
void
initdesc()
{
#ifdef NLS_BUGS
#ifdef NLS_CATALOGS
    (void)catclose(catd);
#endif /* NLS_CATALOGS */
#endif /* NLS_BUGS */


    didfds = 0;			/* 0, 1, 2 aren't set up */
    (void) close_on_exec(SHIN = dcopy(0, FSHIN), 1);
    (void) close_on_exec(SHOUT = dcopy(1, FSHOUT), 1);
    (void) close_on_exec(SHDIAG = dcopy(2, FSHDIAG), 1);
    (void) close_on_exec(OLDSTD = dcopy(SHIN, FOLDSTD), 1);
#ifndef CLOSE_ON_EXEC
    didcch = 0;			/* Havent closed for child */
#endif /* CLOSE_ON_EXEC */
    isdiagatty = isatty(SHDIAG);
    isoutatty = isatty(SHOUT);
    closem();
#ifdef NLS_BUGS
#ifdef NLS_CATALOGS
    nlsinit();
#endif /* NLS_CATALOGS */
#endif /* NLS_BUGS */
}


void
#ifdef PROF
done(i)
#else
xexit(i)
#endif
    int     i;
{
#ifdef TESLA
    if (loginsh && do_logout) {
	/* this is to send hangup signal to the develcon */
	/* we toggle DTR. clear dtr - sleep 1 - set dtr */
	/* ioctl will return ENOTTY for pty's but we ignore it 	 */
	/* exitstat will run after disconnect */
	/* we sleep for 2 seconds to let things happen in */
	/* .logout and rechist() */
#ifdef TIOCCDTR
	(void) sleep(2);
	(void) ioctl(FSHTTY, TIOCCDTR, NULL);
	(void) sleep(1);
	(void) ioctl(FSHTTY, TIOCSDTR, NULL);
#endif /* TIOCCDTR */
    }
#endif /* TESLA */

    {
	struct process *pp, *np;

	/* Kill all processes marked for hup'ing */
	for (pp = proclist.p_next; pp; pp = pp->p_next) {
	    np = pp;
	    do 
		if ((np->p_flags & PHUP) && np->p_jobid != shpgrp) {
		    if (killpg(np->p_jobid, SIGHUP) != -1) {
			/* In case the job was suspended... */
#ifdef SIGCONT
			(void) killpg(np->p_jobid, SIGCONT);
#endif
			break;
		    }
		}
	    while ((np = np->p_friends) != pp);
	}
    }
    untty();
#ifdef NLS_CATALOGS
    /*
     * We need to call catclose, because SVR4 leaves symlinks behind otherwise
     * in the catalog directories. We cannot close on a vforked() child,
     * because messages will stop working on the parent too.
     */
    if (child == 0)
	(void) catclose(catd);
#endif /* NLS_CATALOGS */
#ifdef WINNT_NATIVE
    nt_cleanup();
#endif /* WINNT_NATIVE */
    _exit(i);
}

#ifndef _PATH_DEFPATH
static Char **
defaultpath()
{
    char   *ptr;
    Char  **blk, **blkp;
    struct stat stb;

    blkp = blk = (Char **) xmalloc((size_t) sizeof(Char *) * 10);

#ifndef NODOT
# ifndef DOTLAST
    *blkp++ = Strsave(STRdot);
# endif
#endif

#define DIRAPPEND(a)  \
	if (stat(ptr = a, &stb) == 0 && S_ISDIR(stb.st_mode)) \
		*blkp++ = SAVE(ptr)

#ifdef _PATH_LOCAL
    DIRAPPEND(_PATH_LOCAL);
#endif

#ifdef _PATH_USRUCB
    DIRAPPEND(_PATH_USRUCB);
#endif

#ifdef _PATH_USRBSD
    DIRAPPEND(_PATH_USRBSD);
#endif

#ifdef _PATH_BIN
    DIRAPPEND(_PATH_BIN);
#endif

#ifdef _PATH_USRBIN
    DIRAPPEND(_PATH_USRBIN);
#endif

#undef DIRAPPEND

#ifndef NODOT
# ifdef DOTLAST
    *blkp++ = Strsave(STRdot);
# endif
#endif
    *blkp = NULL;
    return (blk);
}
#endif

static void
record()
{
    if (!fast) {
	recdirs(NULL, adrof(STRsavedirs) != NULL);
	rechist(NULL, adrof(STRsavehist) != NULL);
    }
}
