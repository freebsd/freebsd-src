/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.\n\
	All rights reserved.\n\
     Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.\n\
     Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* ! lint */

#ifndef lint
static char id[] = "@(#)$Id: main.c,v 8.485.4.65 2001/07/20 00:53:00 gshapiro Exp $";
#endif /* ! lint */

#define	_DEFINE

#include <sendmail.h>


#if NETINET || NETINET6
# include <arpa/inet.h>
#endif /* NETINET || NETINET6 */

static SIGFUNC_DECL	intindebug __P((int));
static SIGFUNC_DECL	quiesce __P((int));
#ifdef SIGUSR1
static SIGFUNC_DECL	sigusr1 __P((int));
# endif /* SIGUSR1 */
static SIGFUNC_DECL	term_daemon __P((int));
static void	dump_class __P((STAB *, int));
static void	obsolete __P((char **));
static void	testmodeline __P((char *, ENVELOPE *));

/*
**  SENDMAIL -- Post mail to a set of destinations.
**
**	This is the basic mail router.  All user mail programs should
**	call this routine to actually deliver mail.  Sendmail in
**	turn calls a bunch of mail servers that do the real work of
**	delivering the mail.
**
**	Sendmail is driven by settings read in from /etc/mail/sendmail.cf
**	(read by readcf.c).
**
**	Usage:
**		/usr/lib/sendmail [flags] addr ...
**
**		See the associated documentation for details.
**
**	Author:
**		Eric Allman, UCB/INGRES (until 10/81).
**			     Britton-Lee, Inc., purveyors of fine
**				database computers (11/81 - 10/88).
**			     International Computer Science Institute
**				(11/88 - 9/89).
**			     UCB/Mammoth Project (10/89 - 7/95).
**			     InReference, Inc. (8/95 - 1/97).
**			     Sendmail, Inc. (1/98 - present).
**		The support of the my employers is gratefully acknowledged.
**			Few of them (Britton-Lee in particular) have had
**			anything to gain from my involvement in this project.
*/


int		NextMailer;	/* "free" index into Mailer struct */
char		*FullName;	/* sender's full name */
ENVELOPE	BlankEnvelope;	/* a "blank" envelope */
static ENVELOPE	MainEnvelope;	/* the envelope around the basic letter */
ADDRESS		NullAddress =	/* a null address */
		{ "", "", NULL, "" };
char		*CommandLineArgs;	/* command line args for pid file */
bool		Warn_Q_option = FALSE;	/* warn about Q option use */
static int	MissingFds = 0;	/* bit map of fds missing on startup */

#ifdef NGROUPS_MAX
GIDSET_T	InitialGidSet[NGROUPS_MAX];
#endif /* NGROUPS_MAX */

#if DAEMON && !SMTP
ERROR %%%%   Cannot have DAEMON mode without SMTP   %%%% ERROR
#endif /* DAEMON && !SMTP */
#if SMTP && !QUEUE
ERROR %%%%   Cannot have SMTP mode without QUEUE   %%%% ERROR
#endif /* SMTP && !QUEUE */

#define MAXCONFIGLEVEL	9	/* highest config version level known */

#if SASL
static sasl_callback_t srvcallbacks[] =
{
	{	SASL_CB_VERIFYFILE,	&safesaslfile,	NULL	},
	{	SASL_CB_PROXY_POLICY,	&proxy_policy,	NULL	},
	{	SASL_CB_LIST_END,	NULL,		NULL	}
};

#endif /* SASL */

int SubmitMode;

int
main(argc, argv, envp)
	int argc;
	char **argv;
	char **envp;
{
	register char *p;
	char **av;
	extern char Version[];
	char *ep, *from;
	STAB *st;
	register int i;
	int j;
	int dp;
	bool safecf = TRUE;
	BITMAP256 *p_flags = NULL;	/* daemon flags */
	bool warn_C_flag = FALSE;
	bool auth = TRUE;		/* whether to set e_auth_param */
	char warn_f_flag = '\0';
	bool run_in_foreground = FALSE;	/* -bD mode */
	static bool reenter = FALSE;
	struct passwd *pw;
	struct hostent *hp;
	char *nullserver = NULL;
	char *authinfo = NULL;
	char *sysloglabel = NULL;	/* label for syslog */
	bool forged;
	struct stat traf_st;		/* for TrafficLog FIFO check */
	char jbuf[MAXHOSTNAMELEN];	/* holds MyHostName */
	static char rnamebuf[MAXNAME];	/* holds RealUserName */
	char *emptyenviron[1];
# if STARTTLS
	bool tls_ok;
# endif /* STARTTLS */
	QUEUE_CHAR *new;
	extern int DtableSize;
	extern int optind;
	extern int opterr;
	extern char *optarg;
	extern char **environ;

	/*
	**  Check to see if we reentered.
	**	This would normally happen if e_putheader or e_putbody
	**	were NULL when invoked.
	*/

	if (reenter)
	{
		syserr("main: reentered!");
		abort();
	}
	reenter = TRUE;

	/* avoid null pointer dereferences */
	TermEscape.te_rv_on = TermEscape.te_rv_off = "";

	/*
	**  Seed the random number generator.
	**  Used for queue file names, picking a queue directory, and
	**  MX randomization.
	*/

	seed_random();

	/* do machine-dependent initializations */
	init_md(argc, argv);


	/* in 4.4BSD, the table can be huge; impose a reasonable limit */
	DtableSize = getdtsize();
	if (DtableSize > 256)
		DtableSize = 256;

	/*
	**  Be sure we have enough file descriptors.
	**	But also be sure that 0, 1, & 2 are open.
	*/

	fill_fd(STDIN_FILENO, NULL);
	fill_fd(STDOUT_FILENO, NULL);
	fill_fd(STDERR_FILENO, NULL);

	i = DtableSize;
	while (--i > 0)
	{
		if (i != STDIN_FILENO && i != STDOUT_FILENO && i != STDERR_FILENO)
			(void) close(i);
	}
	errno = 0;

#if LOG
#  ifdef LOG_MAIL
	openlog("sendmail", LOG_PID, LOG_MAIL);
#  else /* LOG_MAIL */
	openlog("sendmail", LOG_PID);
#  endif /* LOG_MAIL */
#endif /* LOG */

	if (MissingFds != 0)
	{
		char mbuf[MAXLINE];

		mbuf[0] = '\0';
		if (bitset(1 << STDIN_FILENO, MissingFds))
			(void) strlcat(mbuf, ", stdin", sizeof mbuf);
		if (bitset(1 << STDOUT_FILENO, MissingFds))
			(void) strlcat(mbuf, ", stdout", sizeof mbuf);
		if (bitset(1 << STDERR_FILENO, MissingFds))
			(void) strlcat(mbuf, ", stderr", sizeof mbuf);
		syserr("File descriptors missing on startup: %s", &mbuf[2]);
	}

	/* reset status from syserr() calls for missing file descriptors */
	Errors = 0;
	ExitStat = EX_OK;

	SubmitMode = SUBMIT_UNKNOWN;
#if XDEBUG
	checkfd012("after openlog");
#endif /* XDEBUG */

	tTsetup(tTdvect, sizeof tTdvect, "0-99.1");

#ifdef NGROUPS_MAX
	/* save initial group set for future checks */
	i = getgroups(NGROUPS_MAX, InitialGidSet);
	if (i == 0)
		InitialGidSet[0] = (GID_T) -1;
	while (i < NGROUPS_MAX)
		InitialGidSet[i++] = InitialGidSet[0];
#endif /* NGROUPS_MAX */

	/* drop group id privileges (RunAsUser not yet set) */
	dp = drop_privileges(FALSE);
	setstat(dp);

# ifdef SIGUSR1
	/* Only allow root (or non-set-*-ID binaries) to use SIGUSR1 */
	if (getuid() == 0 ||
	    (getuid() == geteuid() && getgid() == getegid()))
	{
		/* arrange to dump state on user-1 signal */
		(void) setsignal(SIGUSR1, sigusr1);
	}
# endif /* SIGUSR1 */

	/* initialize for setproctitle */
	initsetproctitle(argc, argv, envp);

	/* Handle any non-getoptable constructions. */
	obsolete(argv);

	/*
	**  Do a quick prescan of the argument list.
	*/


#if defined(__osf__) || defined(_AIX3)
# define OPTIONS	"B:b:C:cd:e:F:f:Gh:IiL:M:mN:nO:o:p:q:R:r:sTtUV:vX:x"
#endif /* defined(__osf__) || defined(_AIX3) */
#if defined(sony_news)
# define OPTIONS	"B:b:C:cd:E:e:F:f:Gh:IiJ:L:M:mN:nO:o:p:q:R:r:sTtUV:vX:"
#endif /* defined(sony_news) */
#ifndef OPTIONS
# define OPTIONS	"B:b:C:cd:e:F:f:Gh:IiL:M:mN:nO:o:p:q:R:r:sTtUV:vX:"
#endif /* ! OPTIONS */
	opterr = 0;
	while ((j = getopt(argc, argv, OPTIONS)) != -1)
	{
		switch (j)
		{
		  case 'd':
			/* hack attack -- see if should use ANSI mode */
			if (strcmp(optarg, "ANSI") == 0)
			{
				TermEscape.te_rv_on = "\033[7m";
				TermEscape.te_rv_off = "\033[0m";
				break;
			}
			tTflag(optarg);
			setbuf(stdout, (char *) NULL);
			break;

		  case 'G':	/* relay (gateway) submission */
			SubmitMode |= SUBMIT_MTA;
			break;

		  case 'L':
			j = min(strlen(optarg), 24) + 1;
			sysloglabel = xalloc(j);
			(void) strlcpy(sysloglabel, optarg, j);
			break;

		  case 'U':	/* initial (user) submission */
			SubmitMode |= SUBMIT_MSA;
			break;
		}
	}
	opterr = 1;

#if LOG
	if (sysloglabel != NULL)
	{
		/* Sanitize the string */
		for (p = sysloglabel; *p != '\0'; p++)
		{
			if (!isascii(*p) || !isprint(*p) || *p == '%')
				*p = '*';
		}
		closelog();
#  ifdef LOG_MAIL
		openlog(sysloglabel, LOG_PID, LOG_MAIL);
#  else /* LOG_MAIL */
		openlog(sysloglabel, LOG_PID);
#  endif /* LOG_MAIL */
	}
#endif /* LOG */

	/* set up the blank envelope */
	BlankEnvelope.e_puthdr = putheader;
	BlankEnvelope.e_putbody = putbody;
	BlankEnvelope.e_xfp = NULL;
	STRUCTCOPY(NullAddress, BlankEnvelope.e_from);
	CurEnv = &BlankEnvelope;
	STRUCTCOPY(NullAddress, MainEnvelope.e_from);

	/*
	**  Set default values for variables.
	**	These cannot be in initialized data space.
	*/

	setdefaults(&BlankEnvelope);

	RealUid = getuid();
	RealGid = getgid();

	pw = sm_getpwuid(RealUid);
	if (pw != NULL)
		(void) snprintf(rnamebuf, sizeof rnamebuf, "%s", pw->pw_name);
	else
		(void) snprintf(rnamebuf, sizeof rnamebuf, "Unknown UID %d",
				(int) RealUid);

	RealUserName = rnamebuf;

	if (tTd(0, 101))
	{
		dprintf("Version %s\n", Version);
		finis(FALSE, EX_OK);
	}

	/*
	**  if running non-setuid binary as non-root, pretend
	**  we are the RunAsUid
	*/

	if (RealUid != 0 && geteuid() == RealUid)
	{
		if (tTd(47, 1))
			dprintf("Non-setuid binary: RunAsUid = RealUid = %d\n",
				(int)RealUid);
		RunAsUid = RealUid;
	}
	else if (geteuid() != 0)
		RunAsUid = geteuid();

	if (RealUid != 0 && getegid() == RealGid)
		RunAsGid = RealGid;

	if (tTd(47, 5))
	{
		dprintf("main: e/ruid = %d/%d e/rgid = %d/%d\n",
			(int)geteuid(), (int)getuid(),
			(int)getegid(), (int)getgid());
		dprintf("main: RunAsUser = %d:%d\n",
			(int)RunAsUid, (int)RunAsGid);
	}

	/* save command line arguments */
	j = 0;
	for (av = argv; *av != NULL; )
		j += strlen(*av++) + 1;
	SaveArgv = (char **) xalloc(sizeof (char *) * (argc + 1));
	CommandLineArgs = xalloc(j);
	p = CommandLineArgs;
	for (av = argv, i = 0; *av != NULL; )
	{
		int h;

		SaveArgv[i++] = newstr(*av);
		if (av != argv)
			*p++ = ' ';
		(void) strlcpy(p, *av++, j);
		h = strlen(p);
		p += h;
		j -= h + 1;
	}
	SaveArgv[i] = NULL;

	if (tTd(0, 1))
	{
		int ll;
		extern char *CompileOptions[];

		dprintf("Version %s\n Compiled with:", Version);
		av = CompileOptions;
		ll = 7;
		while (*av != NULL)
		{
			if (ll + strlen(*av) > 63)
			{
				dprintf("\n");
				ll = 0;
			}
			if (ll == 0)
				dprintf("\t\t");
			else
				dprintf(" ");
			dprintf("%s", *av);
			ll += strlen(*av++) + 1;
		}
		dprintf("\n");
	}
	if (tTd(0, 10))
	{
		int ll;
		extern char *OsCompileOptions[];

		dprintf("    OS Defines:");
		av = OsCompileOptions;
		ll = 7;
		while (*av != NULL)
		{
			if (ll + strlen(*av) > 63)
			{
				dprintf("\n");
				ll = 0;
			}
			if (ll == 0)
				dprintf("\t\t");
			else
				dprintf(" ");
			dprintf("%s", *av);
			ll += strlen(*av++) + 1;
		}
		dprintf("\n");
#ifdef _PATH_UNIX
		dprintf("Kernel symbols:\t%s\n", _PATH_UNIX);
#endif /* _PATH_UNIX */
		dprintf(" Def Conf file:\t%s\n", getcfname());
		dprintf("  Def Pid file:\t%s\n", PidFile);
	}

	InChannel = stdin;
	OutChannel = stdout;

	/* clear sendmail's environment */
	ExternalEnviron = environ;
	emptyenviron[0] = NULL;
	environ = emptyenviron;

	/*
	**  restore any original TZ setting until TimeZoneSpec has been
	**  determined - or early log messages may get bogus time stamps
	*/
	if ((p = getextenv("TZ")) != NULL)
	{
		char *tz;
		int tzlen;

		tzlen = strlen(p) + 4;
		tz = xalloc(tzlen);
		(void) snprintf(tz, tzlen, "TZ=%s", p);
		(void) putenv(tz);
	}

	/* prime the child environment */
	setuserenv("AGENT", "sendmail");
	(void) setsignal(SIGPIPE, SIG_IGN);

	OldUmask = umask(022);
	OpMode = MD_DELIVER;
	FullName = getextenv("NAME");

	/*
	**  Initialize name server if it is going to be used.
	*/

#if NAMED_BIND
	if (!bitset(RES_INIT, _res.options))
		(void) res_init();

	/*
	**  hack to avoid crashes when debugging for the resolver is
	**  turned on and sfio is used
	*/
	if (tTd(8, 8))
# if !SFIO || SFIO_STDIO_COMPAT
		_res.options |= RES_DEBUG;
# else /* !SFIO || SFIO_STDIO_COMPAT */
		dprintf("RES_DEBUG not available due to SFIO\n");
# endif /* !SFIO || SFIO_STDIO_COMPAT */
	else
		_res.options &= ~RES_DEBUG;
# ifdef RES_NOALIASES
	_res.options |= RES_NOALIASES;
# endif /* RES_NOALIASES */
	TimeOuts.res_retry[RES_TO_DEFAULT] = _res.retry;
	TimeOuts.res_retry[RES_TO_FIRST] = _res.retry;
	TimeOuts.res_retry[RES_TO_NORMAL] = _res.retry;
	TimeOuts.res_retrans[RES_TO_DEFAULT] = _res.retrans;
	TimeOuts.res_retrans[RES_TO_FIRST] = _res.retrans;
	TimeOuts.res_retrans[RES_TO_NORMAL] = _res.retrans;
#endif /* NAMED_BIND */

	errno = 0;
	from = NULL;

	/* initialize some macros, etc. */
	initmacros(CurEnv);
	init_vendor_macros(CurEnv);

	/* version */
	define('v', Version, CurEnv);

	/* hostname */
	hp = myhostname(jbuf, sizeof jbuf);
	if (jbuf[0] != '\0')
	{
		struct	utsname	utsname;

		if (tTd(0, 4))
			dprintf("canonical name: %s\n", jbuf);
		define('w', newstr(jbuf), CurEnv);	/* must be new string */
		define('j', newstr(jbuf), CurEnv);
		setclass('w', jbuf);

		p = strchr(jbuf, '.');
		if (p != NULL)
		{
			if (p[1] != '\0')
			{
				define('m', newstr(&p[1]), CurEnv);
			}
			while (p != NULL && strchr(&p[1], '.') != NULL)
			{
				*p = '\0';
				if (tTd(0, 4))
					dprintf("\ta.k.a.: %s\n", jbuf);
				setclass('w', jbuf);
				*p++ = '.';
				p = strchr(p, '.');
			}
		}

		if (uname(&utsname) >= 0)
			p = utsname.nodename;
		else
		{
			if (tTd(0, 22))
				dprintf("uname failed (%s)\n",
					errstring(errno));
			makelower(jbuf);
			p = jbuf;
		}
		if (tTd(0, 4))
			dprintf(" UUCP nodename: %s\n", p);
		p = newstr(p);
		define('k', p, CurEnv);
		setclass('k', p);
		setclass('w', p);
	}
	if (hp != NULL)
	{
		for (av = hp->h_aliases; av != NULL && *av != NULL; av++)
		{
			if (tTd(0, 4))
				dprintf("\ta.k.a.: %s\n", *av);
			setclass('w', *av);
		}
#if NETINET || NETINET6
		for (i = 0; hp->h_addr_list[i] != NULL; i++)
		{
# if NETINET6
			char *addr;
			char buf6[INET6_ADDRSTRLEN];
			struct in6_addr ia6;
# endif /* NETINET6 */
# if NETINET
			struct in_addr ia;
# endif /* NETINET */
			char ipbuf[103];

			ipbuf[0] = '\0';
			switch (hp->h_addrtype)
			{
# if NETINET
			  case AF_INET:
				if (hp->h_length != INADDRSZ)
					break;

				memmove(&ia, hp->h_addr_list[i], INADDRSZ);
				(void) snprintf(ipbuf,	 sizeof ipbuf,
						"[%.100s]", inet_ntoa(ia));
				break;
# endif /* NETINET */

# if NETINET6
			  case AF_INET6:
				if (hp->h_length != IN6ADDRSZ)
					break;

				memmove(&ia6, hp->h_addr_list[i], IN6ADDRSZ);
				addr = anynet_ntop(&ia6, buf6, sizeof buf6);
				if (addr != NULL)
					(void) snprintf(ipbuf, sizeof ipbuf,
							"[%.100s]", addr);
				break;
# endif /* NETINET6 */
			}
			if (ipbuf[0] == '\0')
				break;

			if (tTd(0, 4))
				dprintf("\ta.k.a.: %s\n", ipbuf);
			setclass('w', ipbuf);
		}
#endif /* NETINET || NETINET6 */
#if _FFR_FREEHOSTENT && NETINET6
		freehostent(hp);
		hp = NULL;
#endif /* _FFR_FREEHOSTENT && NETINET6 */
	}

	/* current time */
	define('b', arpadate((char *) NULL), CurEnv);
	/* current load average */
	CurrentLA = sm_getla(CurEnv);

	QueueLimitRecipient = (QUEUE_CHAR *) NULL;
	QueueLimitSender = (QUEUE_CHAR *) NULL;
	QueueLimitId = (QUEUE_CHAR *) NULL;

	/*
	**  Crack argv.
	*/

	av = argv;
	p = strrchr(*av, '/');
	if (p++ == NULL)
		p = *av;
	if (strcmp(p, "newaliases") == 0)
		OpMode = MD_INITALIAS;
	else if (strcmp(p, "mailq") == 0)
		OpMode = MD_PRINT;
	else if (strcmp(p, "smtpd") == 0)
		OpMode = MD_DAEMON;
	else if (strcmp(p, "hoststat") == 0)
		OpMode = MD_HOSTSTAT;
	else if (strcmp(p, "purgestat") == 0)
		OpMode = MD_PURGESTAT;

	optind = 1;
	while ((j = getopt(argc, argv, OPTIONS)) != -1)
	{
		switch (j)
		{
		  case 'b':	/* operations mode */
			switch (j = *optarg)
			{
			  case MD_DAEMON:
			  case MD_FGDAEMON:
#if !DAEMON
				usrerr("Daemon mode not implemented");
				ExitStat = EX_USAGE;
				break;
#endif /* !DAEMON */
			  case MD_SMTP:
#if !SMTP
				usrerr("I don't speak SMTP");
				ExitStat = EX_USAGE;
				break;
#endif /* !SMTP */

			  case MD_INITALIAS:
			  case MD_DELIVER:
			  case MD_VERIFY:
			  case MD_TEST:
			  case MD_PRINT:
			  case MD_HOSTSTAT:
			  case MD_PURGESTAT:
			  case MD_ARPAFTP:
				OpMode = j;
				break;

			  case MD_FREEZE:
				usrerr("Frozen configurations unsupported");
				ExitStat = EX_USAGE;
				break;

			  default:
				usrerr("Invalid operation mode %c", j);
				ExitStat = EX_USAGE;
				break;
			}
			break;

		  case 'B':	/* body type */
			CurEnv->e_bodytype = newstr(optarg);
			break;

		  case 'C':	/* select configuration file (already done) */
			if (RealUid != 0)
				warn_C_flag = TRUE;
			ConfFile = newstr(optarg);
			dp = drop_privileges(TRUE);
			setstat(dp);
			safecf = FALSE;
			break;

		  case 'd':	/* debugging -- already done */
			break;

		  case 'f':	/* from address */
		  case 'r':	/* obsolete -f flag */
			if (from != NULL)
			{
				usrerr("More than one \"from\" person");
				ExitStat = EX_USAGE;
				break;
			}
			from = newstr(denlstring(optarg, TRUE, TRUE));
			if (strcmp(RealUserName, from) != 0)
				warn_f_flag = j;
			break;

		  case 'F':	/* set full name */
			FullName = newstr(optarg);
			break;

		  case 'G':	/* relay (gateway) submission */
			/* already set */
			break;

		  case 'h':	/* hop count */
			CurEnv->e_hopcount = (short) strtol(optarg, &ep, 10);
			if (*ep)
			{
				usrerr("Bad hop count (%s)", optarg);
				ExitStat = EX_USAGE;
			}
			break;

		  case 'L':	/* program label */
			/* already set */
			break;

		  case 'n':	/* don't alias */
			NoAlias = TRUE;
			break;

		  case 'N':	/* delivery status notifications */
			DefaultNotify |= QHASNOTIFY;
			define(macid("{dsn_notify}", NULL),
			       newstr(optarg), CurEnv);
			if (strcasecmp(optarg, "never") == 0)
				break;
			for (p = optarg; p != NULL; optarg = p)
			{
				p = strchr(p, ',');
				if (p != NULL)
					*p++ = '\0';
				if (strcasecmp(optarg, "success") == 0)
					DefaultNotify |= QPINGONSUCCESS;
				else if (strcasecmp(optarg, "failure") == 0)
					DefaultNotify |= QPINGONFAILURE;
				else if (strcasecmp(optarg, "delay") == 0)
					DefaultNotify |= QPINGONDELAY;
				else
				{
					usrerr("Invalid -N argument");
					ExitStat = EX_USAGE;
				}
			}
			break;

		  case 'o':	/* set option */
			setoption(*optarg, optarg + 1, FALSE, TRUE, CurEnv);
			break;

		  case 'O':	/* set option (long form) */
			setoption(' ', optarg, FALSE, TRUE, CurEnv);
			break;

		  case 'p':	/* set protocol */
			p = strchr(optarg, ':');
			if (p != NULL)
			{
				*p++ = '\0';
				if (*p != '\0')
				{
					ep = xalloc(strlen(p) + 1);
					cleanstrcpy(ep, p, MAXNAME);
					define('s', ep, CurEnv);
				}
			}
			if (*optarg != '\0')
			{
				ep = xalloc(strlen(optarg) + 1);
				cleanstrcpy(ep, optarg, MAXNAME);
				define('r', ep, CurEnv);
			}
			break;

		  case 'q':	/* run queue files at intervals */
#if QUEUE
			/* sanity check */
			if (OpMode != MD_DELIVER &&
			    OpMode != MD_DAEMON &&
			    OpMode != MD_FGDAEMON &&
			    OpMode != MD_PRINT &&
			    OpMode != MD_QUEUERUN)
			{
				usrerr("Can not use -q with -b%c", OpMode);
				ExitStat = EX_USAGE;
				break;
			}

			/* don't override -bd, -bD or -bp */
			if (OpMode == MD_DELIVER)
				OpMode = MD_QUEUERUN;

			FullName = NULL;

			switch (optarg[0])
			{
			  case 'I':
				new = (QUEUE_CHAR *) xalloc(sizeof *new);
				new->queue_match = newstr(&optarg[1]);
				new->queue_next = QueueLimitId;
				QueueLimitId = new;
				break;

			  case 'R':
				new = (QUEUE_CHAR *) xalloc(sizeof *new);
				new->queue_match = newstr(&optarg[1]);
				new->queue_next = QueueLimitRecipient;
				QueueLimitRecipient = new;
				break;

			  case 'S':
				new = (QUEUE_CHAR *) xalloc(sizeof *new);
				new->queue_match = newstr(&optarg[1]);
				new->queue_next = QueueLimitSender;
				QueueLimitSender = new;
				break;

			  default:
				i = Errors;
				QueueIntvl = convtime(optarg, 'm');

				/* check for bad conversion */
				if (i < Errors)
					ExitStat = EX_USAGE;
				break;
			}
#else /* QUEUE */
			usrerr("I don't know about queues");
			ExitStat = EX_USAGE;
#endif /* QUEUE */
			break;

		  case 'R':	/* DSN RET: what to return */
			if (bitset(EF_RET_PARAM, CurEnv->e_flags))
			{
				usrerr("Duplicate -R flag");
				ExitStat = EX_USAGE;
				break;
			}
			CurEnv->e_flags |= EF_RET_PARAM;
			if (strcasecmp(optarg, "hdrs") == 0)
				CurEnv->e_flags |= EF_NO_BODY_RETN;
			else if (strcasecmp(optarg, "full") != 0)
			{
				usrerr("Invalid -R value");
				ExitStat = EX_USAGE;
			}
			define(macid("{dsn_ret}", NULL),
			       newstr(optarg), CurEnv);
			break;

		  case 't':	/* read recipients from message */
			GrabTo = TRUE;
			break;

		  case 'U':	/* initial (user) submission */
			/* already set */
			break;

		  case 'V':	/* DSN ENVID: set "original" envelope id */
			if (!xtextok(optarg))
			{
				usrerr("Invalid syntax in -V flag");
				ExitStat = EX_USAGE;
			}
			else
			{
				CurEnv->e_envid = newstr(optarg);
				define(macid("{dsn_envid}", NULL),
				       newstr(optarg), CurEnv);
			}
			break;

		  case 'X':	/* traffic log file */
			dp = drop_privileges(TRUE);
			setstat(dp);
			if (stat(optarg, &traf_st) == 0 &&
			    S_ISFIFO(traf_st.st_mode))
				TrafficLogFile = fopen(optarg, "w");
			else
				TrafficLogFile = fopen(optarg, "a");
			if (TrafficLogFile == NULL)
			{
				syserr("cannot open %s", optarg);
				ExitStat = EX_CANTCREAT;
				break;
			}
#if HASSETVBUF
			(void) setvbuf(TrafficLogFile, NULL, _IOLBF, 0);
#else /* HASSETVBUF */
			(void) setlinebuf(TrafficLogFile);
#endif /* HASSETVBUF */
			break;

			/* compatibility flags */
		  case 'c':	/* connect to non-local mailers */
		  case 'i':	/* don't let dot stop me */
		  case 'm':	/* send to me too */
		  case 'T':	/* set timeout interval */
		  case 'v':	/* give blow-by-blow description */
			setoption(j, "T", FALSE, TRUE, CurEnv);
			break;

		  case 'e':	/* error message disposition */
		  case 'M':	/* define macro */
			setoption(j, optarg, FALSE, TRUE, CurEnv);
			break;

		  case 's':	/* save From lines in headers */
			setoption('f', "T", FALSE, TRUE, CurEnv);
			break;

#ifdef DBM
		  case 'I':	/* initialize alias DBM file */
			OpMode = MD_INITALIAS;
			break;
#endif /* DBM */

#if defined(__osf__) || defined(_AIX3)
		  case 'x':	/* random flag that OSF/1 & AIX mailx passes */
			break;
#endif /* defined(__osf__) || defined(_AIX3) */
#if defined(sony_news)
		  case 'E':
		  case 'J':	/* ignore flags for Japanese code conversion
				   implemented on Sony NEWS */
			break;
#endif /* defined(sony_news) */

		  default:
			finis(TRUE, EX_USAGE);
			break;
		}
	}
	av += optind;

	if (bitset(SUBMIT_MTA, SubmitMode) &&
	    bitset(SUBMIT_MSA, SubmitMode))
	{
		/* sanity check */
		errno = 0;	/* reset to avoid bogus error messages */
		syserr("Cannot use both -G and -U together");
	}
	else if (bitset(SUBMIT_MTA, SubmitMode))
		define(macid("{daemon_flags}", NULL), "CC f", CurEnv);
	else if (bitset(SUBMIT_MSA, SubmitMode))
	{
		define(macid("{daemon_flags}", NULL), "c u", CurEnv);

		/* check for wrong OpMode */
		if (OpMode != MD_DELIVER && OpMode != MD_SMTP)
		{
			errno = 0;	/* reset to avoid bogus error msgs */
			syserr("Cannot use -U and -b%c", OpMode);
		}
	}
	else
	{
#if _FFR_DEFAULT_SUBMIT_TO_MSA
		define(macid("{daemon_flags}", NULL), "c u", CurEnv);
#else /* _FFR_DEFAULT_SUBMIT_TO_MSA */
		/* EMPTY */
#endif /* _FFR_DEFAULT_SUBMIT_TO_MSA */
	}

	/*
	**  Do basic initialization.
	**	Read system control file.
	**	Extract special fields for local use.
	*/

	/* set up ${opMode} for use in config file */
	{
		char mbuf[2];

		mbuf[0] = OpMode;
		mbuf[1] = '\0';
		define(MID_OPMODE, newstr(mbuf), CurEnv);
	}

#if XDEBUG
	checkfd012("before readcf");
#endif /* XDEBUG */
	vendor_pre_defaults(CurEnv);

	readcf(getcfname(), safecf, CurEnv);
	ConfigFileRead = TRUE;
	vendor_post_defaults(CurEnv);

	/* Remove the ability for a normal user to send signals */
	if (RealUid != 0 &&
	    RealUid != geteuid())
	{
		uid_t new_uid = geteuid();

#if HASSETREUID
		/*
		**  Since we can differentiate between uid and euid,
		**  make the uid a different user so the real user
		**  can't send signals.  However, it doesn't need to be
		**  root (euid has root).
		*/

		if (new_uid == 0)
			new_uid = DefUid;
		if (tTd(47, 5))
			dprintf("Changing real uid to %d\n", (int) new_uid);
		if (setreuid(new_uid, geteuid()) < 0)
		{
			syserr("main: setreuid(%d, %d) failed",
			       (int) new_uid, (int) geteuid());
			finis(FALSE, EX_OSERR);
			/* NOTREACHED */
		}
		if (tTd(47, 10))
			dprintf("Now running as e/ruid %d:%d\n",
				(int) geteuid(), (int) getuid());
#else /* HASSETREUID */
		/*
		**  Have to change both effective and real so need to
		**  change them both to effective to keep privs.
		*/

		if (tTd(47, 5))
			dprintf("Changing uid to %d\n", (int) new_uid);
		if (setuid(new_uid) < 0)
		{
			syserr("main: setuid(%d) failed", (int) new_uid);
			finis(FALSE, EX_OSERR);
			/* NOTREACHED */
		}
		if (tTd(47, 10))
			dprintf("Now running as e/ruid %d:%d\n",
				(int) geteuid(), (int) getuid());
#endif /* HASSETREUID */
	}

	/* set up the basic signal handlers */
	if (setsignal(SIGINT, SIG_IGN) != SIG_IGN)
		(void) setsignal(SIGINT, intsig);
	(void) setsignal(SIGTERM, intsig);

	/* Enforce use of local time (null string overrides this) */
	if (TimeZoneSpec == NULL)
		unsetenv("TZ");
	else if (TimeZoneSpec[0] != '\0')
		setuserenv("TZ", TimeZoneSpec);
	else
		setuserenv("TZ", NULL);
	tzset();

	/* avoid denial-of-service attacks */
	resetlimits();

	if (OpMode != MD_DAEMON && OpMode != MD_FGDAEMON)
	{
		/* drop privileges -- daemon mode done after socket/bind */
		dp = drop_privileges(FALSE);
		setstat(dp);
	}

#if NAMED_BIND
	_res.retry = TimeOuts.res_retry[RES_TO_DEFAULT];
	_res.retrans = TimeOuts.res_retrans[RES_TO_DEFAULT];
#endif /* NAMED_BIND */

	/*
	**  Find our real host name for future logging.
	*/

	authinfo = getauthinfo(STDIN_FILENO, &forged);
	define('_', authinfo, CurEnv);

	/* suppress error printing if errors mailed back or whatever */
	if (CurEnv->e_errormode != EM_PRINT)
		HoldErrs = TRUE;

	/* set up the $=m class now, after .cf has a chance to redefine $m */
	expand("\201m", jbuf, sizeof jbuf, CurEnv);
	if (jbuf[0] != '\0')
		setclass('m', jbuf);

	/* probe interfaces and locate any additional names */
	if (!DontProbeInterfaces)
		load_if_names();

	if (tTd(0, 1))
	{
		dprintf("\n============ SYSTEM IDENTITY (after readcf) ============");
		dprintf("\n      (short domain name) $w = ");
		xputs(macvalue('w', CurEnv));
		dprintf("\n  (canonical domain name) $j = ");
		xputs(macvalue('j', CurEnv));
		dprintf("\n         (subdomain name) $m = ");
		xputs(macvalue('m', CurEnv));
		dprintf("\n              (node name) $k = ");
		xputs(macvalue('k', CurEnv));
		dprintf("\n========================================================\n\n");
	}

	/*
	**  Do more command line checking -- these are things that
	**  have to modify the results of reading the config file.
	*/

	/* process authorization warnings from command line */
	if (warn_C_flag)
		auth_warning(CurEnv, "Processed by %s with -C %s",
			RealUserName, ConfFile);
	if (Warn_Q_option && !wordinclass(RealUserName, 't'))
		auth_warning(CurEnv, "Processed from queue %s", QueueDir);

	/* check body type for legality */
	if (CurEnv->e_bodytype == NULL)
		/* EMPTY */
		/* nothing */ ;
	else if (strcasecmp(CurEnv->e_bodytype, "7BIT") == 0)
		SevenBitInput = TRUE;
	else if (strcasecmp(CurEnv->e_bodytype, "8BITMIME") == 0)
		SevenBitInput = FALSE;
	else
	{
		usrerr("Illegal body type %s", CurEnv->e_bodytype);
		CurEnv->e_bodytype = NULL;
	}

	/* tweak default DSN notifications */
	if (DefaultNotify == 0)
		DefaultNotify = QPINGONFAILURE|QPINGONDELAY;

	/* be sure we don't pick up bogus HOSTALIASES environment variable */
	if (OpMode == MD_QUEUERUN && RealUid != 0)
		(void) unsetenv("HOSTALIASES");

	/* check for sane configuration level */
	if (ConfigLevel > MAXCONFIGLEVEL)
	{
		syserr("Warning: .cf version level (%d) exceeds sendmail version %s functionality (%d)",
			ConfigLevel, Version, MAXCONFIGLEVEL);
	}

	/* need MCI cache to have persistence */
	if (HostStatDir != NULL && MaxMciCache == 0)
	{
		HostStatDir = NULL;
		printf("Warning: HostStatusDirectory disabled with ConnectionCacheSize = 0\n");
	}

	/* need HostStatusDir in order to have SingleThreadDelivery */
	if (SingleThreadDelivery && HostStatDir == NULL)
	{
		SingleThreadDelivery = FALSE;
		printf("Warning: HostStatusDirectory required for SingleThreadDelivery\n");
	}

	/* check for permissions */
	if ((OpMode == MD_DAEMON ||
	     OpMode == MD_FGDAEMON ||
	     OpMode == MD_PURGESTAT) &&
	    RealUid != 0 &&
	    RealUid != TrustedUid)
	{
		if (LogLevel > 1)
			sm_syslog(LOG_ALERT, NOQID,
				  "user %d attempted to %s",
				  RealUid,
				  OpMode != MD_PURGESTAT ? "run daemon"
							 : "purge host status");
		usrerr("Permission denied");
		finis(FALSE, EX_USAGE);
	}
	if (OpMode == MD_INITALIAS &&
	    RealUid != 0 &&
	    RealUid != TrustedUid &&
	    !wordinclass(RealUserName, 't'))
	{
		if (LogLevel > 1)
			sm_syslog(LOG_ALERT, NOQID,
				  "user %d attempted to rebuild the alias map",
				  RealUid);
		usrerr("Permission denied");
		finis(FALSE, EX_USAGE);
	}

	if (MeToo)
		BlankEnvelope.e_flags |= EF_METOO;

	switch (OpMode)
	{
	  case MD_TEST:
		/* don't have persistent host status in test mode */
		HostStatDir = NULL;
		if (Verbose == 0)
			Verbose = 2;
		CurEnv->e_errormode = EM_PRINT;
		HoldErrs = FALSE;
		break;

	  case MD_VERIFY:
		CurEnv->e_errormode = EM_PRINT;
		HoldErrs = FALSE;

		/* arrange to exit cleanly on hangup signal */
		if (setsignal(SIGHUP, SIG_IGN) == (sigfunc_t) SIG_DFL)
			(void) setsignal(SIGHUP, intsig);
		break;

	  case MD_FGDAEMON:
		run_in_foreground = TRUE;
		OpMode = MD_DAEMON;
		/* FALLTHROUGH */

	  case MD_DAEMON:
		vendor_daemon_setup(CurEnv);

		/* remove things that don't make sense in daemon mode */
		FullName = NULL;
		GrabTo = FALSE;

		/* arrange to restart on hangup signal */
		if (SaveArgv[0] == NULL || SaveArgv[0][0] != '/')
			sm_syslog(LOG_WARNING, NOQID,
				  "daemon invoked without full pathname; kill -1 won't work");
		(void) setsignal(SIGTERM, term_daemon);
		break;

	  case MD_INITALIAS:
		Verbose = 2;
		CurEnv->e_errormode = EM_PRINT;
		HoldErrs = FALSE;
		/* FALLTHROUGH */

	  default:
		/* arrange to exit cleanly on hangup signal */
		if (setsignal(SIGHUP, SIG_IGN) == (sigfunc_t) SIG_DFL)
			(void) setsignal(SIGHUP, intsig);
		break;
	}

	/* special considerations for FullName */
	if (FullName != NULL)
	{
		char *full = NULL;

		/* full names can't have newlines */
		if (strchr(FullName, '\n') != NULL)
		{
			full = newstr(denlstring(FullName, TRUE, TRUE));
			FullName = full;
		}

		/* check for characters that may have to be quoted */
		if (!rfc822_string(FullName))
		{
			/*
			**  Quote a full name with special characters
			**  as a comment so crackaddr() doesn't destroy
			**  the name portion of the address.
			*/

			FullName = addquotes(FullName);
			if (full != NULL)
				sm_free(full);
		}
	}

	/* do heuristic mode adjustment */
	if (Verbose)
	{
		/* turn off noconnect option */
		setoption('c', "F", TRUE, FALSE, CurEnv);

		/* turn on interactive delivery */
		setoption('d', "", TRUE, FALSE, CurEnv);
	}

#ifdef VENDOR_CODE
	/* check for vendor mismatch */
	if (VendorCode != VENDOR_CODE)
	{
		message("Warning: .cf file vendor code mismatch: sendmail expects vendor %s, .cf file vendor is %s",
			getvendor(VENDOR_CODE), getvendor(VendorCode));
	}
#endif /* VENDOR_CODE */

	/* check for out of date configuration level */
	if (ConfigLevel < MAXCONFIGLEVEL)
	{
		message("Warning: .cf file is out of date: sendmail %s supports version %d, .cf file is version %d",
			Version, MAXCONFIGLEVEL, ConfigLevel);
	}

	if (ConfigLevel < 3)
		UseErrorsTo = TRUE;

	/* set options that were previous macros */
	if (SmtpGreeting == NULL)
	{
		if (ConfigLevel < 7 && (p = macvalue('e', CurEnv)) != NULL)
			SmtpGreeting = newstr(p);
		else
			SmtpGreeting = "\201j Sendmail \201v ready at \201b";
	}
	if (UnixFromLine == NULL)
	{
		if (ConfigLevel < 7 && (p = macvalue('l', CurEnv)) != NULL)
			UnixFromLine = newstr(p);
		else
			UnixFromLine = "From \201g  \201d";
	}
	SmtpError[0] = '\0';

	/* our name for SMTP codes */
	expand("\201j", jbuf, sizeof jbuf, CurEnv);
	if (jbuf[0] == '\0')
		MyHostName = newstr("localhost");
	else
		MyHostName = jbuf;
	if (strchr(MyHostName, '.') == NULL)
		message("WARNING: local host name (%s) is not qualified; fix $j in config file",
			MyHostName);

	/* make certain that this name is part of the $=w class */
	setclass('w', MyHostName);

	/* the indices of built-in mailers */
	st = stab("local", ST_MAILER, ST_FIND);
	if (st != NULL)
		LocalMailer = st->s_mailer;
	else if (OpMode != MD_TEST || !warn_C_flag)
		syserr("No local mailer defined");

	st = stab("prog", ST_MAILER, ST_FIND);
	if (st == NULL)
		syserr("No prog mailer defined");
	else
	{
		ProgMailer = st->s_mailer;
		clrbitn(M_MUSER, ProgMailer->m_flags);
	}

	st = stab("*file*", ST_MAILER, ST_FIND);
	if (st == NULL)
		syserr("No *file* mailer defined");
	else
	{
		FileMailer = st->s_mailer;
		clrbitn(M_MUSER, FileMailer->m_flags);
	}

	st = stab("*include*", ST_MAILER, ST_FIND);
	if (st == NULL)
		syserr("No *include* mailer defined");
	else
		InclMailer = st->s_mailer;

	if (ConfigLevel < 6)
	{
		/* heuristic tweaking of local mailer for back compat */
		if (LocalMailer != NULL)
		{
			setbitn(M_ALIASABLE, LocalMailer->m_flags);
			setbitn(M_HASPWENT, LocalMailer->m_flags);
			setbitn(M_TRYRULESET5, LocalMailer->m_flags);
			setbitn(M_CHECKINCLUDE, LocalMailer->m_flags);
			setbitn(M_CHECKPROG, LocalMailer->m_flags);
			setbitn(M_CHECKFILE, LocalMailer->m_flags);
			setbitn(M_CHECKUDB, LocalMailer->m_flags);
		}
		if (ProgMailer != NULL)
			setbitn(M_RUNASRCPT, ProgMailer->m_flags);
		if (FileMailer != NULL)
			setbitn(M_RUNASRCPT, FileMailer->m_flags);
	}
	if (ConfigLevel < 7)
	{
		if (LocalMailer != NULL)
			setbitn(M_VRFY250, LocalMailer->m_flags);
		if (ProgMailer != NULL)
			setbitn(M_VRFY250, ProgMailer->m_flags);
		if (FileMailer != NULL)
			setbitn(M_VRFY250, FileMailer->m_flags);
	}

	/* MIME Content-Types that cannot be transfer encoded */
	setclass('n', "multipart/signed");

	/* MIME message/xxx subtypes that can be treated as messages */
	setclass('s', "rfc822");

	/* MIME Content-Transfer-Encodings that can be encoded */
	setclass('e', "7bit");
	setclass('e', "8bit");
	setclass('e', "binary");

#ifdef USE_B_CLASS
	/* MIME Content-Types that should be treated as binary */
	setclass('b', "image");
	setclass('b', "audio");
	setclass('b', "video");
	setclass('b', "application/octet-stream");
#endif /* USE_B_CLASS */

	/* MIME headers which have fields to check for overflow */
	setclass(macid("{checkMIMEFieldHeaders}", NULL), "content-disposition");
	setclass(macid("{checkMIMEFieldHeaders}", NULL), "content-type");

	/* MIME headers to check for length overflow */
	setclass(macid("{checkMIMETextHeaders}", NULL), "content-description");

	/* MIME headers to check for overflow and rebalance */
	setclass(macid("{checkMIMEHeaders}", NULL), "content-disposition");
	setclass(macid("{checkMIMEHeaders}", NULL), "content-id");
	setclass(macid("{checkMIMEHeaders}", NULL), "content-transfer-encoding");
	setclass(macid("{checkMIMEHeaders}", NULL), "content-type");
	setclass(macid("{checkMIMEHeaders}", NULL), "mime-version");

	/* Macros to save in the qf file -- don't remove any */
	setclass(macid("{persistentMacros}", NULL), "r");
	setclass(macid("{persistentMacros}", NULL), "s");
	setclass(macid("{persistentMacros}", NULL), "_");
	setclass(macid("{persistentMacros}", NULL), "{if_addr}");
	setclass(macid("{persistentMacros}", NULL), "{daemon_flags}");
	setclass(macid("{persistentMacros}", NULL), "{client_flags}");

	/* operate in queue directory */
	if (QueueDir == NULL)
	{
		if (OpMode != MD_TEST)
		{
			syserr("QueueDirectory (Q) option must be set");
			ExitStat = EX_CONFIG;
		}
	}
	else
	{
		/*
		**  If multiple queues wildcarded, use one for
		**  the daemon's home. Note that this preconditions
		**  a wildcarded QueueDir to a real pathname.
		*/

		if (OpMode != MD_TEST)
			multiqueue_cache();
	}

	/* check host status directory for validity */
	if (HostStatDir != NULL && !path_is_dir(HostStatDir, FALSE))
	{
		/* cannot use this value */
		if (tTd(0, 2))
			dprintf("Cannot use HostStatusDirectory = %s: %s\n",
				HostStatDir, errstring(errno));
		HostStatDir = NULL;
	}

#if QUEUE
	if (OpMode == MD_QUEUERUN && RealUid != 0 &&
	    bitset(PRIV_RESTRICTQRUN, PrivacyFlags))
	{
		struct stat stbuf;

		/* check to see if we own the queue directory */
		if (stat(".", &stbuf) < 0)
			syserr("main: cannot stat %s", QueueDir);
		if (stbuf.st_uid != RealUid)
		{
			/* nope, really a botch */
			usrerr("You do not have permission to process the queue");
			finis(FALSE, EX_NOPERM);
		}
	}
#endif /* QUEUE */

#if _FFR_MILTER
	/* sanity checks on milter filters */
	if (OpMode == MD_DAEMON || OpMode == MD_SMTP)
		milter_parse_list(InputFilterList, InputFilters, MAXFILTERS);
#endif /* _FFR_MILTER */


	/* if we've had errors so far, exit now */
	if (ExitStat != EX_OK && OpMode != MD_TEST)
		finis(FALSE, ExitStat);

#if XDEBUG
	checkfd012("before main() initmaps");
#endif /* XDEBUG */

	/*
	**  Do operation-mode-dependent initialization.
	*/

	switch (OpMode)
	{
	  case MD_PRINT:
		/* print the queue */
#if QUEUE
		dropenvelope(CurEnv, TRUE);
		(void) setsignal(SIGPIPE, quiesce);
		printqueue();
		finis(FALSE, EX_OK);
#else /* QUEUE */
		usrerr("No queue to print");
		finis(FALSE, EX_UNAVAILABLE);
#endif /* QUEUE */
		break;

	  case MD_HOSTSTAT:
		(void) setsignal(SIGPIPE, quiesce);
		(void) mci_traverse_persistent(mci_print_persistent, NULL);
		finis(FALSE, EX_OK);
		break;

	  case MD_PURGESTAT:
		(void) mci_traverse_persistent(mci_purge_persistent, NULL);
		finis(FALSE, EX_OK);
		break;

	  case MD_INITALIAS:
		/* initialize maps */
		initmaps();
		finis(FALSE, ExitStat);
		break;

	  case MD_SMTP:
	  case MD_DAEMON:
		/* reset DSN parameters */
		DefaultNotify = QPINGONFAILURE|QPINGONDELAY;
		define(macid("{dsn_notify}", NULL), NULL, CurEnv);
		CurEnv->e_envid = NULL;
		define(macid("{dsn_envid}", NULL), NULL, CurEnv);
		CurEnv->e_flags &= ~(EF_RET_PARAM|EF_NO_BODY_RETN);
		define(macid("{dsn_ret}", NULL), NULL, CurEnv);

		/* don't open maps for daemon -- done below in child */
		break;
	}

	if (tTd(0, 15))
	{
		/* print configuration table (or at least part of it) */
		if (tTd(0, 90))
			printrules();
		for (i = 0; i < MAXMAILERS; i++)
		{
			if (Mailer[i] != NULL)
				printmailer(Mailer[i]);
		}
	}

	/*
	**  Switch to the main envelope.
	*/

	CurEnv = newenvelope(&MainEnvelope, CurEnv);
	MainEnvelope.e_flags = BlankEnvelope.e_flags;

	/*
	**  If test mode, read addresses from stdin and process.
	*/

	if (OpMode == MD_TEST)
	{
		char buf[MAXLINE];

#if _FFR_TESTMODE_DROP_PRIVS
		dp = drop_privileges(TRUE);
		if (dp != EX_OK)
		{
			CurEnv->e_id = NULL;
			finis(TRUE, dp);
		}
#endif /* _FFR_TESTMODE_DROP_PRIVS */

		if (isatty(fileno(stdin)))
			Verbose = 2;

		if (Verbose)
		{
			printf("ADDRESS TEST MODE (ruleset 3 NOT automatically invoked)\n");
			printf("Enter <ruleset> <address>\n");
		}
		if (setjmp(TopFrame) > 0)
			printf("\n");
		(void) setsignal(SIGINT, intindebug);
		for (;;)
		{
			if (Verbose == 2)
				printf("> ");
			(void) fflush(stdout);
			if (fgets(buf, sizeof buf, stdin) == NULL)
				testmodeline("/quit", CurEnv);
			p = strchr(buf, '\n');
			if (p != NULL)
				*p = '\0';
			if (Verbose < 2)
				printf("> %s\n", buf);
			testmodeline(buf, CurEnv);
		}
	}

#if SMTP
# if STARTTLS
	tls_ok = init_tls_library();
# endif /* STARTTLS */
#endif /* SMTP */

#if QUEUE
	/*
	**  If collecting stuff from the queue, go start doing that.
	*/

	if (OpMode == MD_QUEUERUN && QueueIntvl == 0)
	{
# if SMTP
#  if STARTTLS
		if (tls_ok
		   )
		{
			/* init TLS for client, ignore result for now */
			(void) initclttls();
		}
#  endif /* STARTTLS */
# endif /* SMTP */
		(void) runqueue(FALSE, Verbose);
		finis(TRUE, ExitStat);
	}
#endif /* QUEUE */

# if SASL
	if (OpMode == MD_SMTP || OpMode == MD_DAEMON)
	{
		/* give a syserr or just disable AUTH ? */
		if ((i = sasl_server_init(srvcallbacks, "Sendmail")) != SASL_OK)
			syserr("!sasl_server_init failed! [%s]",
			       sasl_errstring(i, NULL, NULL));
	}
# endif /* SASL */

	/*
	**  If a daemon, wait for a request.
	**	getrequests will always return in a child.
	**	If we should also be processing the queue, start
	**		doing it in background.
	**	We check for any errors that might have happened
	**		during startup.
	*/

	if (OpMode == MD_DAEMON || QueueIntvl != 0)
	{
		char dtype[200];

		if (!run_in_foreground && !tTd(99, 100))
		{
			/* put us in background */
			i = fork();
			if (i < 0)
				syserr("daemon: cannot fork");
			if (i != 0)
				finis(FALSE, EX_OK);

			/* disconnect from our controlling tty */
			disconnect(2, CurEnv);
		}

		dtype[0] = '\0';
		if (OpMode == MD_DAEMON)
			(void) strlcat(dtype, "+SMTP", sizeof dtype);
		if (QueueIntvl != 0)
		{
			(void) strlcat(dtype, "+queueing@", sizeof dtype);
			(void) strlcat(dtype, pintvl(QueueIntvl, TRUE),
				       sizeof dtype);
		}
		if (tTd(0, 1))
			(void) strlcat(dtype, "+debugging", sizeof dtype);

		sm_syslog(LOG_INFO, NOQID,
			  "starting daemon (%s): %s", Version, dtype + 1);
#ifdef XLA
		xla_create_file();
#endif /* XLA */

		/* save daemon type in a macro for possible PidFile use */
		define(macid("{daemon_info}", NULL),
		       newstr(dtype + 1), &BlankEnvelope);

		/* save queue interval in a macro for possible PidFile use */
		define(macid("{queue_interval}", NULL),
		       newstr(pintvl(QueueIntvl, TRUE)), CurEnv);

#if QUEUE
		if (QueueIntvl != 0)
		{
			(void) runqueue(TRUE, FALSE);
			if (OpMode != MD_DAEMON)
			{
				/* write the pid to file */
				log_sendmail_pid(CurEnv);
				(void) setsignal(SIGTERM, term_daemon);
				for (;;)
				{
					(void) pause();
					if (ShutdownRequest != NULL)
						shutdown_daemon();
					else if (DoQueueRun)
						(void) runqueue(TRUE, FALSE);
				}
			}
		}
#endif /* QUEUE */
		dropenvelope(CurEnv, TRUE);

#if DAEMON
# if STARTTLS
		/* init TLS for server, ignore result for now */
		(void) initsrvtls();
# endif /* STARTTLS */
		p_flags = getrequests(CurEnv);

		/* drop privileges */
		(void) drop_privileges(FALSE);

		/* at this point we are in a child: reset state */
		(void) newenvelope(CurEnv, CurEnv);

		/*
		**  Get authentication data
		*/

		authinfo = getauthinfo(fileno(InChannel), &forged);
		define('_', authinfo, &BlankEnvelope);
#endif /* DAEMON */
	}

	if (LogLevel > 9)
	{
		/* log connection information */
		sm_syslog(LOG_INFO, NULL, "connect from %.100s", authinfo);
	}

#if SMTP
	/*
	**  If running SMTP protocol, start collecting and executing
	**  commands.  This will never return.
	*/

	if (OpMode == MD_SMTP || OpMode == MD_DAEMON)
	{
		char pbuf[20];

		/*
		**  Save some macros for check_* rulesets.
		*/

		if (forged)
		{
			char ipbuf[103];

			(void) snprintf(ipbuf, sizeof ipbuf, "[%.100s]",
					anynet_ntoa(&RealHostAddr));
			define(macid("{client_name}", NULL),
			       newstr(ipbuf), &BlankEnvelope);
			define(macid("{client_resolve}", NULL),
			       "FORGED", &BlankEnvelope);
		}
		else
			define(macid("{client_name}", NULL), RealHostName,
			       &BlankEnvelope);
		define(macid("{client_addr}", NULL),
		       newstr(anynet_ntoa(&RealHostAddr)), &BlankEnvelope);
		(void)sm_getla(&BlankEnvelope);

		switch(RealHostAddr.sa.sa_family)
		{
# if NETINET
		  case AF_INET:
			(void) snprintf(pbuf, sizeof pbuf, "%d",
					RealHostAddr.sin.sin_port);
			break;
# endif /* NETINET */
# if NETINET6
		  case AF_INET6:
			(void) snprintf(pbuf, sizeof pbuf, "%d",
					RealHostAddr.sin6.sin6_port);
			break;
# endif /* NETINET6 */
		  default:
			(void) snprintf(pbuf, sizeof pbuf, "0");
			break;
		}
		define(macid("{client_port}", NULL),
		       newstr(pbuf), &BlankEnvelope);

		if (OpMode == MD_DAEMON)
		{
			/* validate the connection */
			HoldErrs = TRUE;
			nullserver = validate_connection(&RealHostAddr,
							 RealHostName, CurEnv);
			HoldErrs = FALSE;
		}
		else if (p_flags == NULL)
		{
			p_flags = (BITMAP256 *) xalloc(sizeof *p_flags);
			clrbitmap(p_flags);
		}
# if STARTTLS
		if (OpMode == MD_SMTP)
			(void) initsrvtls();
# endif /* STARTTLS */


		smtp(nullserver, *p_flags, CurEnv);
	}
#endif /* SMTP */

	clearenvelope(CurEnv, FALSE);
	if (OpMode == MD_VERIFY)
	{
		set_delivery_mode(SM_VERIFY, CurEnv);
		PostMasterCopy = NULL;
	}
	else
	{
		/* interactive -- all errors are global */
		CurEnv->e_flags |= EF_GLOBALERRS|EF_LOGSENDER;
	}

	/*
	**  Do basic system initialization and set the sender
	*/

	initsys(CurEnv);
	define(macid("{ntries}", NULL), "0", CurEnv);
	setsender(from, CurEnv, NULL, '\0', FALSE);
	if (warn_f_flag != '\0' && !wordinclass(RealUserName, 't') &&
	    (!bitnset(M_LOCALMAILER, CurEnv->e_from.q_mailer->m_flags) ||
	     strcmp(CurEnv->e_from.q_user, RealUserName) != 0))
	{
		auth_warning(CurEnv, "%s set sender to %s using -%c",
			RealUserName, from, warn_f_flag);
#if SASL
		auth = FALSE;
#endif /* SASL */
	}
	if (auth)
	{
		char *fv;

		/* set the initial sender for AUTH= to $f@$j */
		fv = macvalue('f', CurEnv);
		if (fv == NULL || *fv == '\0')
			CurEnv->e_auth_param = NULL;
		else
		{
			if (strchr(fv, '@') == NULL)
			{
				i = strlen(fv) + strlen(macvalue('j', CurEnv))
				    + 2;
				p = xalloc(i);
				(void) snprintf(p, i, "%s@%s", fv,
						macvalue('j', CurEnv));
			}
			else
				p = newstr(fv);
			CurEnv->e_auth_param = newstr(xtextify(p, "="));
		}
	}
	if (macvalue('s', CurEnv) == NULL)
		define('s', RealHostName, CurEnv);

	if (*av == NULL && !GrabTo)
	{
		CurEnv->e_to = NULL;
		CurEnv->e_flags |= EF_GLOBALERRS;
		HoldErrs = FALSE;
		SuperSafe = FALSE;
		usrerr("Recipient names must be specified");

		/* collect body for UUCP return */
		if (OpMode != MD_VERIFY)
			collect(InChannel, FALSE, NULL, CurEnv);
		finis(TRUE, EX_USAGE);
	}

	/*
	**  Scan argv and deliver the message to everyone.
	*/

	sendtoargv(av, CurEnv);

	/* if we have had errors sofar, arrange a meaningful exit stat */
	if (Errors > 0 && ExitStat == EX_OK)
		ExitStat = EX_USAGE;

#if _FFR_FIX_DASHT
	/*
	**  If using -t, force not sending to argv recipients, even
	**  if they are mentioned in the headers.
	*/

	if (GrabTo)
	{
		ADDRESS *q;

		for (q = CurEnv->e_sendqueue; q != NULL; q = q->q_next)
			q->q_state = QS_REMOVED;
	}
#endif /* _FFR_FIX_DASHT */

	/*
	**  Read the input mail.
	*/

	CurEnv->e_to = NULL;
	if (OpMode != MD_VERIFY || GrabTo)
	{
		int savederrors = Errors;
		long savedflags = CurEnv->e_flags & EF_FATALERRS;

		CurEnv->e_flags |= EF_GLOBALERRS;
		CurEnv->e_flags &= ~EF_FATALERRS;
		Errors = 0;
		buffer_errors();
		collect(InChannel, FALSE, NULL, CurEnv);

		/* header checks failed */
		if (Errors > 0)
		{
			/* Log who the mail would have gone to */
			if (LogLevel > 8 && CurEnv->e_message != NULL &&
			    !GrabTo)
			{
				ADDRESS *a;

				for (a = CurEnv->e_sendqueue;
				     a != NULL;
				     a = a->q_next)
				{
					if (!QS_IS_UNDELIVERED(a->q_state))
						continue;

					CurEnv->e_to = a->q_paddr;
					logdelivery(NULL, NULL, NULL,
						    CurEnv->e_message,
						    NULL, (time_t) 0, CurEnv);
				}
				CurEnv->e_to = NULL;
			}
			flush_errors(TRUE);
			finis(TRUE, ExitStat);
			/* NOTREACHED */
			return -1;
		}

		/* bail out if message too large */
		if (bitset(EF_CLRQUEUE, CurEnv->e_flags))
		{
			finis(TRUE, ExitStat != EX_OK ? ExitStat : EX_DATAERR);
			/* NOTREACHED */
			return -1;
		}
		Errors = savederrors;
		CurEnv->e_flags |= savedflags;
	}
	errno = 0;

	if (tTd(1, 1))
		dprintf("From person = \"%s\"\n", CurEnv->e_from.q_paddr);

	/*
	**  Actually send everything.
	**	If verifying, just ack.
	*/

	CurEnv->e_from.q_state = QS_SENDER;
	if (tTd(1, 5))
	{
		dprintf("main: QS_SENDER ");
		printaddr(&CurEnv->e_from, FALSE);
	}
	CurEnv->e_to = NULL;
	CurrentLA = sm_getla(CurEnv);
	GrabTo = FALSE;
#if NAMED_BIND
	_res.retry = TimeOuts.res_retry[RES_TO_FIRST];
	_res.retrans = TimeOuts.res_retrans[RES_TO_FIRST];
#endif /* NAMED_BIND */
	sendall(CurEnv, SM_DEFAULT);

	/*
	**  All done.
	**	Don't send return error message if in VERIFY mode.
	*/

	finis(TRUE, ExitStat);
	/* NOTREACHED */
	return ExitStat;
}
/*
**  QUIESCE -- signal handler for SIGPIPE
**
**	Parameters:
**		sig -- incoming signal.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Sets StopRequest which should cause the mailq/hoststatus
**		display to stop.
**
**	NOTE:	THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
**		ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
**		DOING.
*/

/* ARGSUSED */
static SIGFUNC_DECL
quiesce(sig)
	int sig;
{
	int save_errno = errno;

	FIX_SYSV_SIGNAL(sig, quiesce);
	StopRequest = TRUE;
	errno = save_errno;
	return SIGFUNC_RETURN;
}
/*
**  STOP_SENDMAIL -- Stop the running program
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		exits.
*/

void
stop_sendmail()
{
	/* reset uid for process accounting */
	endpwent();
	(void) setuid(RealUid);
	exit(EX_OK);
}

/*
**  INTINDEBUG -- signal handler for SIGINT in -bt mode
**
**	Parameters:
**		sig -- incoming signal.
**
**	Returns:
**		none.
**
**	Side Effects:
**		longjmps back to test mode loop.
**
**	NOTE:	THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
**		ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
**		DOING.
**
**	XXX: More work is needed for this signal handler.
*/

/* ARGSUSED */
static SIGFUNC_DECL
intindebug(sig)
	int sig;
{
	int save_errno = errno;

	FIX_SYSV_SIGNAL(sig, intindebug);
	errno = save_errno;
	CHECK_CRITICAL(sig);

	errno = save_errno;
	longjmp(TopFrame, 1);
	return SIGFUNC_RETURN;
}
/*
**  FINIS -- Clean up and exit.
**
**	Parameters:
**		drop -- whether or not to drop CurEnv envelope
**		exitstat -- exit status to use for exit() call
**
**	Returns:
**		never
**
**	Side Effects:
**		exits sendmail
*/

void
finis(drop, exitstat)
	bool drop;
	volatile int exitstat;
{
	/* Still want to process new timeouts added below */
	clear_events();
	(void) releasesignal(SIGALRM);

	if (tTd(2, 1))
	{
		dprintf("\n====finis: stat %d e_id=%s e_flags=",
			exitstat,
			CurEnv->e_id == NULL ? "NOQUEUE" : CurEnv->e_id);
		printenvflags(CurEnv);
	}
	if (tTd(2, 9))
		printopenfds(FALSE);

	/* if we fail in finis(), just exit */
	if (setjmp(TopFrame) != 0)
	{
		/* failed -- just give it up */
		goto forceexit;
	}

	/* clean up temp files */
	CurEnv->e_to = NULL;
	if (drop)
	{
		if (CurEnv->e_id != NULL)
			dropenvelope(CurEnv, TRUE);
		else
			poststats(StatFile);
	}

	/* flush any cached connections */
	mci_flush(TRUE, NULL);

	/* close maps belonging to this pid */
	closemaps();

#if USERDB
	/* close UserDatabase */
	_udbx_close();
#endif /* USERDB */

#ifdef XLA
	/* clean up extended load average stuff */
	xla_all_end();
#endif /* XLA */

	/* and exit */
  forceexit:
	if (LogLevel > 78)
		sm_syslog(LOG_DEBUG, CurEnv->e_id,
			  "finis, pid=%d",
			  (int) getpid());
	if (exitstat == EX_TEMPFAIL || CurEnv->e_errormode == EM_BERKNET)
		exitstat = EX_OK;

	sync_queue_time();

	/* reset uid for process accounting */
	endpwent();
	(void) setuid(RealUid);
	exit(exitstat);
}
/*
**  TERM_DEAMON -- SIGTERM handler for the daemon
**
**	Parameters:
**		sig -- signal number.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Sets ShutdownRequest which will hopefully trigger
**		the daemon to exit.
**
**	NOTE:	THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
**		ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
**		DOING.
*/

/* ARGSUSED */
static SIGFUNC_DECL
term_daemon(sig)
	int sig;
{
	int save_errno = errno;

	FIX_SYSV_SIGNAL(sig, term_daemon);
	ShutdownRequest = "signal";
	errno = save_errno;
	return SIGFUNC_RETURN;
}
/*
**  SHUTDOWN_DAEMON -- Performs a clean shutdown of the daemon
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		closes control socket, exits.
*/

void
shutdown_daemon()
{
	char *reason;

	allsignals(TRUE);

	reason = ShutdownRequest;
	ShutdownRequest = NULL;
	PendingSignal = 0;

	if (LogLevel > 79)
		sm_syslog(LOG_DEBUG, CurEnv->e_id, "interrupt (%s)",
			  reason == NULL ? "implicit call" : reason);

	FileName = NULL;
	closecontrolsocket(TRUE);
#ifdef XLA
	xla_all_end();
#endif /* XLA */

	finis(FALSE, EX_OK);
}
/*
**  INTSIG -- clean up on interrupt
**
**	This just arranges to exit.  It pessimizes in that it
**	may resend a message.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Unlocks the current job.
**
**	NOTE:	THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
**		ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
**		DOING.
**
**		XXX: More work is needed for this signal handler.
*/

/* ARGSUSED */
SIGFUNC_DECL
intsig(sig)
	int sig;
{
	bool drop = FALSE;
	int save_errno = errno;

	FIX_SYSV_SIGNAL(sig, intsig);
	errno = save_errno;
	CHECK_CRITICAL(sig);
	allsignals(TRUE);
	if (sig != 0 && LogLevel > 79)
		sm_syslog(LOG_DEBUG, CurEnv->e_id, "interrupt");
	FileName = NULL;

	/* Clean-up on aborted stdin message submission */
	if (CurEnv->e_id != NULL &&
	    (OpMode == MD_SMTP ||
	     OpMode == MD_DELIVER ||
	     OpMode == MD_ARPAFTP))
	{
		register ADDRESS *q;

		/* don't return an error indication */
		CurEnv->e_to = NULL;
		CurEnv->e_flags &= ~EF_FATALERRS;
		CurEnv->e_flags |= EF_CLRQUEUE;

		/*
		**  Spin through the addresses and
		**  mark them dead to prevent bounces
		*/

		for (q = CurEnv->e_sendqueue; q != NULL; q = q->q_next)
			q->q_state = QS_DONTSEND;

		/* and don't try to deliver the partial message either */
		if (InChild)
			ExitStat = EX_QUIT;

		drop = TRUE;
	}
	else if (OpMode != MD_TEST)
		unlockqueue(CurEnv);

	finis(drop, EX_OK);
}
/*
**  INITMACROS -- initialize the macro system
**
**	This just involves defining some macros that are actually
**	used internally as metasymbols to be themselves.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		initializes several macros to be themselves.
*/

struct metamac	MetaMacros[] =
{
	/* LHS pattern matching characters */
	{ '*', MATCHZANY },	{ '+', MATCHANY },	{ '-', MATCHONE },
	{ '=', MATCHCLASS },	{ '~', MATCHNCLASS },

	/* these are RHS metasymbols */
	{ '#', CANONNET },	{ '@', CANONHOST },	{ ':', CANONUSER },
	{ '>', CALLSUBR },

	/* the conditional operations */
	{ '?', CONDIF },	{ '|', CONDELSE },	{ '.', CONDFI },

	/* the hostname lookup characters */
	{ '[', HOSTBEGIN },	{ ']', HOSTEND },
	{ '(', LOOKUPBEGIN },	{ ')', LOOKUPEND },

	/* miscellaneous control characters */
	{ '&', MACRODEXPAND },

	{ '\0', '\0' }
};

#define MACBINDING(name, mid) \
		stab(name, ST_MACRO, ST_ENTER)->s_macro = mid; \
		MacroName[mid] = name;

void
initmacros(e)
	register ENVELOPE *e;
{
	register struct metamac *m;
	register int c;
	char buf[5];
	extern char *MacroName[MAXMACROID + 1];

	for (m = MetaMacros; m->metaname != '\0'; m++)
	{
		buf[0] = m->metaval;
		buf[1] = '\0';
		define(m->metaname, newstr(buf), e);
	}
	buf[0] = MATCHREPL;
	buf[2] = '\0';
	for (c = '0'; c <= '9'; c++)
	{
		buf[1] = c;
		define(c, newstr(buf), e);
	}

	/* set defaults for some macros sendmail will use later */
	define('n', "MAILER-DAEMON", e);

	/* set up external names for some internal macros */
	MACBINDING("opMode", MID_OPMODE);
	/*XXX should probably add equivalents for all short macros here XXX*/
}
/*
**  DISCONNECT -- remove our connection with any foreground process
**
**	Parameters:
**		droplev -- how "deeply" we should drop the line.
**			0 -- ignore signals, mail back errors, make sure
**			     output goes to stdout.
**			1 -- also, make stdout go to /dev/null.
**			2 -- also, disconnect from controlling terminal
**			     (only for daemon mode).
**		e -- the current envelope.
**
**	Returns:
**		none
**
**	Side Effects:
**		Trys to insure that we are immune to vagaries of
**		the controlling tty.
*/

void
disconnect(droplev, e)
	int droplev;
	register ENVELOPE *e;
{
	int fd;

	if (tTd(52, 1))
		dprintf("disconnect: In %d Out %d, e=%lx\n",
			fileno(InChannel), fileno(OutChannel), (u_long) e);
	if (tTd(52, 100))
	{
		dprintf("don't\n");
		return;
	}
	if (LogLevel > 93)
		sm_syslog(LOG_DEBUG, e->e_id,
			  "disconnect level %d",
			  droplev);

	/* be sure we don't get nasty signals */
	(void) setsignal(SIGINT, SIG_IGN);
	(void) setsignal(SIGQUIT, SIG_IGN);

	/* we can't communicate with our caller, so.... */
	HoldErrs = TRUE;
	CurEnv->e_errormode = EM_MAIL;
	Verbose = 0;
	DisConnected = TRUE;

	/* all input from /dev/null */
	if (InChannel != stdin)
	{
		(void) fclose(InChannel);
		InChannel = stdin;
	}
	if (freopen("/dev/null", "r", stdin) == NULL)
		sm_syslog(LOG_ERR, e->e_id,
			  "disconnect: freopen(\"/dev/null\") failed: %s",
			  errstring(errno));

	/* output to the transcript */
	if (OutChannel != stdout)
	{
		(void) fclose(OutChannel);
		OutChannel = stdout;
	}
	if (droplev > 0)
	{
		fd = open("/dev/null", O_WRONLY, 0666);
		if (fd == -1)
			sm_syslog(LOG_ERR, e->e_id,
				  "disconnect: open(\"/dev/null\") failed: %s",
				  errstring(errno));
		(void) fflush(stdout);
		(void) dup2(fd, STDOUT_FILENO);
		(void) dup2(fd, STDERR_FILENO);
		(void) close(fd);
	}

	/* drop our controlling TTY completely if possible */
	if (droplev > 1)
	{
		(void) setsid();
		errno = 0;
	}

#if XDEBUG
	checkfd012("disconnect");
#endif /* XDEBUG */

	if (LogLevel > 71)
		sm_syslog(LOG_DEBUG, e->e_id,
			  "in background, pid=%d",
			  (int) getpid());

	errno = 0;
}

static void
obsolete(argv)
	char *argv[];
{
	register char *ap;
	register char *op;

	while ((ap = *++argv) != NULL)
	{
		/* Return if "--" or not an option of any form. */
		if (ap[0] != '-' || ap[1] == '-')
			return;

		/* skip over options that do have a value */
		op = strchr(OPTIONS, ap[1]);
		if (op != NULL && *++op == ':' && ap[2] == '\0' &&
		    ap[1] != 'd' &&
#if defined(sony_news)
		    ap[1] != 'E' && ap[1] != 'J' &&
#endif /* defined(sony_news) */
		    argv[1] != NULL && argv[1][0] != '-')
		{
			argv++;
			continue;
		}

		/* If -C doesn't have an argument, use sendmail.cf. */
#define	__DEFPATH	"sendmail.cf"
		if (ap[1] == 'C' && ap[2] == '\0')
		{
			*argv = xalloc(sizeof(__DEFPATH) + 2);
			(void) snprintf(argv[0], sizeof(__DEFPATH) + 2, "-C%s",
					__DEFPATH);
		}

		/* If -q doesn't have an argument, run it once. */
		if (ap[1] == 'q' && ap[2] == '\0')
			*argv = "-q0";

		/* if -d doesn't have an argument, use 0-99.1 */
		if (ap[1] == 'd' && ap[2] == '\0')
			*argv = "-d0-99.1";

#if defined(sony_news)
		/* if -E doesn't have an argument, use -EC */
		if (ap[1] == 'E' && ap[2] == '\0')
			*argv = "-EC";

		/* if -J doesn't have an argument, use -JJ */
		if (ap[1] == 'J' && ap[2] == '\0')
			*argv = "-JJ";
#endif /* defined(sony_news) */
	}
}
/*
**  AUTH_WARNING -- specify authorization warning
**
**	Parameters:
**		e -- the current envelope.
**		msg -- the text of the message.
**		args -- arguments to the message.
**
**	Returns:
**		none.
*/

void
#ifdef __STDC__
auth_warning(register ENVELOPE *e, const char *msg, ...)
#else /* __STDC__ */
auth_warning(e, msg, va_alist)
	register ENVELOPE *e;
	const char *msg;
	va_dcl
#endif /* __STDC__ */
{
	char buf[MAXLINE];
	VA_LOCAL_DECL

	if (bitset(PRIV_AUTHWARNINGS, PrivacyFlags))
	{
		register char *p;
		static char hostbuf[48];

		if (hostbuf[0] == '\0')
		{
			struct hostent *hp;

			hp = myhostname(hostbuf, sizeof hostbuf);
#if _FFR_FREEHOSTENT && NETINET6
			if (hp != NULL)
			{
				freehostent(hp);
				hp = NULL;
			}
#endif /* _FFR_FREEHOSTENT && NETINET6 */
		}

		(void) snprintf(buf, sizeof buf, "%s: ", hostbuf);
		p = &buf[strlen(buf)];
		VA_START(msg);
		vsnprintf(p, SPACELEFT(buf, p), msg, ap);
		VA_END;
		addheader("X-Authentication-Warning", buf, 0, &e->e_header);
		if (LogLevel > 3)
			sm_syslog(LOG_INFO, e->e_id,
				  "Authentication-Warning: %.400s",
				  buf);
	}
}
/*
**  GETEXTENV -- get from external environment
**
**	Parameters:
**		envar -- the name of the variable to retrieve
**
**	Returns:
**		The value, if any.
*/

char *
getextenv(envar)
	const char *envar;
{
	char **envp;
	int l;

	l = strlen(envar);
	for (envp = ExternalEnviron; *envp != NULL; envp++)
	{
		if (strncmp(*envp, envar, l) == 0 && (*envp)[l] == '=')
			return &(*envp)[l + 1];
	}
	return NULL;
}
/*
**  SETUSERENV -- set an environment in the propogated environment
**
**	Parameters:
**		envar -- the name of the environment variable.
**		value -- the value to which it should be set.  If
**			null, this is extracted from the incoming
**			environment.  If that is not set, the call
**			to setuserenv is ignored.
**
**	Returns:
**		none.
*/

void
setuserenv(envar, value)
	const char *envar;
	const char *value;
{
	int i, l;
	char **evp = UserEnviron;
	char *p;

	if (value == NULL)
	{
		value = getextenv(envar);
		if (value == NULL)
			return;
	}

	i = strlen(envar) + 1;
	l = strlen(value) + i + 1;
	p = (char *) xalloc(l);
	(void) snprintf(p, l, "%s=%s", envar, value);

	while (*evp != NULL && strncmp(*evp, p, i) != 0)
		evp++;
	if (*evp != NULL)
	{
		*evp++ = p;
	}
	else if (evp < &UserEnviron[MAXUSERENVIRON])
	{
		*evp++ = p;
		*evp = NULL;
	}

	/* make sure it is in our environment as well */
	if (putenv(p) < 0)
		syserr("setuserenv: putenv(%s) failed", p);
}
/*
**  DUMPSTATE -- dump state
**
**	For debugging.
*/

void
dumpstate(when)
	char *when;
{
	register char *j = macvalue('j', CurEnv);
	int rs;
	extern int NextMacroId;

	sm_syslog(LOG_DEBUG, CurEnv->e_id,
		  "--- dumping state on %s: $j = %s ---",
		  when,
		  j == NULL ? "<NULL>" : j);
	if (j != NULL)
	{
		if (!wordinclass(j, 'w'))
			sm_syslog(LOG_DEBUG, CurEnv->e_id,
				  "*** $j not in $=w ***");
	}
	sm_syslog(LOG_DEBUG, CurEnv->e_id, "CurChildren = %d", CurChildren);
	sm_syslog(LOG_DEBUG, CurEnv->e_id, "NextMacroId = %d (Max %d)\n",
		  NextMacroId, MAXMACROID);
	sm_syslog(LOG_DEBUG, CurEnv->e_id, "--- open file descriptors: ---");
	printopenfds(TRUE);
	sm_syslog(LOG_DEBUG, CurEnv->e_id, "--- connection cache: ---");
	mci_dump_all(TRUE);
	rs = strtorwset("debug_dumpstate", NULL, ST_FIND);
	if (rs > 0)
	{
		int status;
		register char **pvp;
		char *pv[MAXATOM + 1];

		pv[0] = NULL;
		status = rewrite(pv, rs, 0, CurEnv);
		sm_syslog(LOG_DEBUG, CurEnv->e_id,
			  "--- ruleset debug_dumpstate returns stat %d, pv: ---",
			  status);
		for (pvp = pv; *pvp != NULL; pvp++)
			sm_syslog(LOG_DEBUG, CurEnv->e_id, "%s", *pvp);
	}
	sm_syslog(LOG_DEBUG, CurEnv->e_id, "--- end of state dump ---");
}
#ifdef SIGUSR1
/*
**  SIGUSR1 -- Signal a request to dump state.
**
**	Parameters:
**		sig -- calling signal.
**
**	Returns:
**		none.
**
**	NOTE:	THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
**		ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
**		DOING.
**
**		XXX: More work is needed for this signal handler.
*/

/* ARGSUSED */
static SIGFUNC_DECL
sigusr1(sig)
	int sig;
{
	int save_errno = errno;

	FIX_SYSV_SIGNAL(sig, sigusr1);
	errno = save_errno;
	CHECK_CRITICAL(sig);
	dumpstate("user signal");
	errno = save_errno;
	return SIGFUNC_RETURN;
}
# endif /* SIGUSR1 */
/*
**  DROP_PRIVILEGES -- reduce privileges to those of the RunAsUser option
**
**	Parameters:
**		to_real_uid -- if set, drop to the real uid instead
**			of the RunAsUser.
**
**	Returns:
**		EX_OSERR if the setuid failed.
**		EX_OK otherwise.
*/

int
drop_privileges(to_real_uid)
	bool to_real_uid;
{
	int rval = EX_OK;
	GIDSET_T emptygidset[1];

	if (tTd(47, 1))
		dprintf("drop_privileges(%d): Real[UG]id=%d:%d, RunAs[UG]id=%d:%d\n",
			(int)to_real_uid, (int)RealUid,
			(int)RealGid, (int)RunAsUid, (int)RunAsGid);

	if (to_real_uid)
	{
		RunAsUserName = RealUserName;
		RunAsUid = RealUid;
		RunAsGid = RealGid;
	}

	/* make sure no one can grab open descriptors for secret files */
	endpwent();

	/* reset group permissions; these can be set later */
	emptygidset[0] = (to_real_uid || RunAsGid != 0) ? RunAsGid : getegid();
	if (setgroups(1, emptygidset) == -1 && geteuid() == 0)
	{
		syserr("drop_privileges: setgroups(1, %d) failed",
		       (int)emptygidset[0]);
		rval = EX_OSERR;
	}

	/* reset primary group and user id */
	if ((to_real_uid || RunAsGid != 0) && setgid(RunAsGid) < 0)
	{
		syserr("drop_privileges: setgid(%d) failed", (int)RunAsGid);
		rval = EX_OSERR;
	}
	if (to_real_uid || RunAsUid != 0)
	{
		uid_t euid = geteuid();

		if (setuid(RunAsUid) < 0)
		{
			syserr("drop_privileges: setuid(%d) failed",
			       (int)RunAsUid);
			rval = EX_OSERR;
		}
		else if (RunAsUid != 0 && setuid(0) == 0)
		{
			/*
			**  Believe it or not, the Linux capability model
			**  allows a non-root process to override setuid()
			**  on a process running as root and prevent that
			**  process from dropping privileges.
			*/

			syserr("drop_privileges: setuid(0) succeeded (when it should not)");
			rval = EX_OSERR;
		}
		else if (RunAsUid != euid && setuid(euid) == 0)
		{
			/*
			**  Some operating systems will keep the saved-uid
			**  if a non-root effective-uid calls setuid(real-uid)
			**  making it possible to set it back again later.
			*/

			syserr("drop_privileges: Unable to drop non-root set-user-id privileges");
			rval = EX_OSERR;
		}
	}
	if (tTd(47, 5))
	{
		dprintf("drop_privileges: e/ruid = %d/%d e/rgid = %d/%d\n",
			(int)geteuid(), (int)getuid(),
			(int)getegid(), (int)getgid());
		dprintf("drop_privileges: RunAsUser = %d:%d\n",
			(int)RunAsUid, (int)RunAsGid);
		if (tTd(47, 10))
			dprintf("drop_privileges: rval = %d\n", rval);
	}
	return rval;
}
/*
**  FILL_FD -- make sure a file descriptor has been properly allocated
**
**	Used to make sure that stdin/out/err are allocated on startup
**
**	Parameters:
**		fd -- the file descriptor to be filled.
**		where -- a string used for logging.  If NULL, this is
**			being called on startup, and logging should
**			not be done.
**
**	Returns:
**		none
*/

void
fill_fd(fd, where)
	int fd;
	char *where;
{
	int i;
	struct stat stbuf;

	if (fstat(fd, &stbuf) >= 0 || errno != EBADF)
		return;

	if (where != NULL)
		syserr("fill_fd: %s: fd %d not open", where, fd);
	else
		MissingFds |= 1 << fd;
	i = open("/dev/null", fd == 0 ? O_RDONLY : O_WRONLY, 0666);
	if (i < 0)
	{
		syserr("!fill_fd: %s: cannot open /dev/null",
			where == NULL ? "startup" : where);
	}
	if (fd != i)
	{
		(void) dup2(i, fd);
		(void) close(i);
	}
}
/*
**  TESTMODELINE -- process a test mode input line
**
**	Parameters:
**		line -- the input line.
**		e -- the current environment.
**	Syntax:
**		#  a comment
**		.X process X as a configuration line
**		=X dump a configuration item (such as mailers)
**		$X dump a macro or class
**		/X try an activity
**		X  normal process through rule set X
*/

static void
testmodeline(line, e)
	char *line;
	ENVELOPE *e;
{
	register char *p;
	char *q;
	auto char *delimptr;
	int mid;
	int i, rs;
	STAB *map;
	char **s;
	struct rewrite *rw;
	ADDRESS a;
	static int tryflags = RF_COPYNONE;
	char exbuf[MAXLINE];
	extern u_char TokTypeNoC[];

#if _FFR_ADDR_TYPE
	define(macid("{addr_type}", NULL), "e r", e);
#endif /* _FFR_ADDR_TYPE */

	/* skip leading spaces */
	while (*line == ' ')
		line++;

	switch (line[0])
	{
	  case '#':
	  case '\0':
		return;

	  case '?':
		help("-bt", e);
		return;

	  case '.':		/* config-style settings */
		switch (line[1])
		{
		  case 'D':
			mid = macid(&line[2], &delimptr);
			if (mid == 0)
				return;
			translate_dollars(delimptr);
			define(mid, newstr(delimptr), e);
			break;

		  case 'C':
			if (line[2] == '\0')	/* not to call syserr() */
				return;

			mid = macid(&line[2], &delimptr);
			if (mid == 0)
				return;
			translate_dollars(delimptr);
			expand(delimptr, exbuf, sizeof exbuf, e);
			p = exbuf;
			while (*p != '\0')
			{
				register char *wd;
				char delim;

				while (*p != '\0' && isascii(*p) && isspace(*p))
					p++;
				wd = p;
				while (*p != '\0' && !(isascii(*p) && isspace(*p)))
					p++;
				delim = *p;
				*p = '\0';
				if (wd[0] != '\0')
					setclass(mid, wd);
				*p = delim;
			}
			break;

		  case '\0':
			printf("Usage: .[DC]macro value(s)\n");
			break;

		  default:
			printf("Unknown \".\" command %s\n", line);
			break;
		}
		return;

	  case '=':		/* config-style settings */
		switch (line[1])
		{
		  case 'S':		/* dump rule set */
			rs = strtorwset(&line[2], NULL, ST_FIND);
			if (rs < 0)
			{
				printf("Undefined ruleset %s\n", &line[2]);
				return;
			}
			rw = RewriteRules[rs];
			if (rw == NULL)
				return;
			do
			{
				(void) putchar('R');
				s = rw->r_lhs;
				while (*s != NULL)
				{
					xputs(*s++);
					(void) putchar(' ');
				}
				(void) putchar('\t');
				(void) putchar('\t');
				s = rw->r_rhs;
				while (*s != NULL)
				{
					xputs(*s++);
					(void) putchar(' ');
				}
				(void) putchar('\n');
			} while ((rw = rw->r_next) != NULL);
			break;

		  case 'M':
			for (i = 0; i < MAXMAILERS; i++)
			{
				if (Mailer[i] != NULL)
					printmailer(Mailer[i]);
			}
			break;

		  case '\0':
			printf("Usage: =Sruleset or =M\n");
			break;

		  default:
			printf("Unknown \"=\" command %s\n", line);
			break;
		}
		return;

	  case '-':		/* set command-line-like opts */
		switch (line[1])
		{
		  case 'd':
			tTflag(&line[2]);
			break;

		  case '\0':
			printf("Usage: -d{debug arguments}\n");
			break;

		  default:
			printf("Unknown \"-\" command %s\n", line);
			break;
		}
		return;

	  case '$':
		if (line[1] == '=')
		{
			mid = macid(&line[2], NULL);
			if (mid != 0)
				stabapply(dump_class, mid);
			return;
		}
		mid = macid(&line[1], NULL);
		if (mid == 0)
			return;
		p = macvalue(mid, e);
		if (p == NULL)
			printf("Undefined\n");
		else
		{
			xputs(p);
			printf("\n");
		}
		return;

	  case '/':		/* miscellaneous commands */
		p = &line[strlen(line)];
		while (--p >= line && isascii(*p) && isspace(*p))
			*p = '\0';
		p = strpbrk(line, " \t");
		if (p != NULL)
		{
			while (isascii(*p) && isspace(*p))
				*p++ = '\0';
		}
		else
			p = "";
		if (line[1] == '\0')
		{
			printf("Usage: /[canon|map|mx|parse|try|tryflags]\n");
			return;
		}
		if (strcasecmp(&line[1], "quit") == 0)
		{
			CurEnv->e_id = NULL;
			finis(TRUE, ExitStat);
		}
		if (strcasecmp(&line[1], "mx") == 0)
		{
#if NAMED_BIND
			/* look up MX records */
			int nmx;
			auto int rcode;
			char *mxhosts[MAXMXHOSTS + 1];

			if (*p == '\0')
			{
				printf("Usage: /mx address\n");
				return;
			}
			nmx = getmxrr(p, mxhosts, NULL, FALSE, &rcode);
			printf("getmxrr(%s) returns %d value(s):\n", p, nmx);
			for (i = 0; i < nmx; i++)
				printf("\t%s\n", mxhosts[i]);
#else /* NAMED_BIND */
			printf("No MX code compiled in\n");
#endif /* NAMED_BIND */
		}
		else if (strcasecmp(&line[1], "canon") == 0)
		{
			char host[MAXHOSTNAMELEN];

			if (*p == '\0')
			{
				printf("Usage: /canon address\n");
				return;
			}
			else if (strlcpy(host, p, sizeof host) >= sizeof host)
			{
				printf("Name too long\n");
				return;
			}
			(void) getcanonname(host, sizeof host, HasWildcardMX);
			printf("getcanonname(%s) returns %s\n", p, host);
		}
		else if (strcasecmp(&line[1], "map") == 0)
		{
			auto int rcode = EX_OK;
			char *av[2];

			if (*p == '\0')
			{
				printf("Usage: /map mapname key\n");
				return;
			}
			for (q = p; *q != '\0' && !(isascii(*q) && isspace(*q)); q++)
				continue;
			if (*q == '\0')
			{
				printf("No key specified\n");
				return;
			}
			*q++ = '\0';
			map = stab(p, ST_MAP, ST_FIND);
			if (map == NULL)
			{
				printf("Map named \"%s\" not found\n", p);
				return;
			}
			if (!bitset(MF_OPEN, map->s_map.map_mflags) &&
			    !openmap(&(map->s_map)))
			{
				printf("Map named \"%s\" not open\n", p);
				return;
			}
			printf("map_lookup: %s (%s) ", p, q);
			av[0] = q;
			av[1] = NULL;
			p = (*map->s_map.map_class->map_lookup)
					(&map->s_map, q, av, &rcode);
			if (p == NULL)
				printf("no match (%d)\n", rcode);
			else
				printf("returns %s (%d)\n", p, rcode);
		}
		else if (strcasecmp(&line[1], "try") == 0)
		{
			MAILER *m;
			STAB *st;
			auto int rcode = EX_OK;

			q = strpbrk(p, " \t");
			if (q != NULL)
			{
				while (isascii(*q) && isspace(*q))
					*q++ = '\0';
			}
			if (q == NULL || *q == '\0')
			{
				printf("Usage: /try mailer address\n");
				return;
			}
			st = stab(p, ST_MAILER, ST_FIND);
			if (st == NULL)
			{
				printf("Unknown mailer %s\n", p);
				return;
			}
			m = st->s_mailer;
			printf("Trying %s %s address %s for mailer %s\n",
				bitset(RF_HEADERADDR, tryflags) ? "header" : "envelope",
				bitset(RF_SENDERADDR, tryflags) ? "sender" : "recipient",
				q, p);
			p = remotename(q, m, tryflags, &rcode, CurEnv);
			printf("Rcode = %d, addr = %s\n",
				rcode, p == NULL ? "<NULL>" : p);
			e->e_to = NULL;
		}
		else if (strcasecmp(&line[1], "tryflags") == 0)
		{
			if (*p == '\0')
			{
				printf("Usage: /tryflags [Hh|Ee][Ss|Rr]\n");
				return;
			}
			for (; *p != '\0'; p++)
			{
				switch (*p)
				{
				  case 'H':
				  case 'h':
					tryflags |= RF_HEADERADDR;
					break;

				  case 'E':
				  case 'e':
					tryflags &= ~RF_HEADERADDR;
					break;

				  case 'S':
				  case 's':
					tryflags |= RF_SENDERADDR;
					break;

				  case 'R':
				  case 'r':
					tryflags &= ~RF_SENDERADDR;
					break;
				}
			}
#if _FFR_ADDR_TYPE
			exbuf[0] = bitset(RF_HEADERADDR, tryflags) ? 'h' : 'e';
			exbuf[1] = ' ';
			exbuf[2] = bitset(RF_SENDERADDR, tryflags) ? 's' : 'r';
			exbuf[3] = '\0';
			define(macid("{addr_type}", NULL), newstr(exbuf), e);
#endif /* _FFR_ADDR_TYPE */
		}
		else if (strcasecmp(&line[1], "parse") == 0)
		{
			if (*p == '\0')
			{
				printf("Usage: /parse address\n");
				return;
			}
			q = crackaddr(p);
			printf("Cracked address = ");
			xputs(q);
			printf("\nParsing %s %s address\n",
				bitset(RF_HEADERADDR, tryflags) ? "header" : "envelope",
				bitset(RF_SENDERADDR, tryflags) ? "sender" : "recipient");
			if (parseaddr(p, &a, tryflags, '\0', NULL, e) == NULL)
				printf("Cannot parse\n");
			else if (a.q_host != NULL && a.q_host[0] != '\0')
				printf("mailer %s, host %s, user %s\n",
					a.q_mailer->m_name, a.q_host, a.q_user);
			else
				printf("mailer %s, user %s\n",
					a.q_mailer->m_name, a.q_user);
			e->e_to = NULL;
		}
		else
		{
			printf("Unknown \"/\" command %s\n", line);
		}
		return;
	}

	for (p = line; isascii(*p) && isspace(*p); p++)
		continue;
	q = p;
	while (*p != '\0' && !(isascii(*p) && isspace(*p)))
		p++;
	if (*p == '\0')
	{
		printf("No address!\n");
		return;
	}
	*p = '\0';
	if (invalidaddr(p + 1, NULL))
		return;
	do
	{
		register char **pvp;
		char pvpbuf[PSBUFSIZE];

		pvp = prescan(++p, ',', pvpbuf, sizeof pvpbuf,
			      &delimptr, ConfigLevel >= 9 ? TokTypeNoC : NULL);
		if (pvp == NULL)
			continue;
		p = q;
		while (*p != '\0')
		{
			int status;

			rs = strtorwset(p, NULL, ST_FIND);
			if (rs < 0)
			{
				printf("Undefined ruleset %s\n", p);
				break;
			}
			status = rewrite(pvp, rs, 0, e);
			if (status != EX_OK)
				printf("== Ruleset %s (%d) status %d\n",
					p, rs, status);
			while (*p != '\0' && *p++ != ',')
				continue;
		}
	} while (*(p = delimptr) != '\0');
}

static void
dump_class(s, id)
	register STAB *s;
	int id;
{
	if (s->s_type != ST_CLASS)
		return;
	if (bitnset(bitidx(id), s->s_class))
		printf("%s\n", s->s_name);
}
