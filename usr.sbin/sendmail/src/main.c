/*
 * Copyright (c) 1983, 1995, 1996 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	8.215 (Berkeley) 11/16/96";
#endif /* not lint */

#define	_DEFINE

#include "sendmail.h"
#include <arpa/inet.h>
#if NAMED_BIND
#include <resolv.h>
#endif

# ifdef lint
char	edata, end;
# endif /* lint */

/*
**  SENDMAIL -- Post mail to a set of destinations.
**
**	This is the basic mail router.  All user mail programs should
**	call this routine to actually deliver mail.  Sendmail in
**	turn calls a bunch of mail servers that do the real work of
**	delivering the mail.
**
**	Sendmail is driven by tables read in from /usr/lib/sendmail.cf
**	(read by readcf.c).  Some more static configuration info,
**	including some code that you may want to tailor for your
**	installation, is in conf.c.  You may also want to touch
**	daemon.c (if you have some other IPC mechanism), acct.c
**	(to change your accounting), names.c (to adjust the name
**	server mechanism).
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
**			     InReference, Inc. (8/95 - present).
**		The support of the my employers is gratefully acknowledged.
**			Few of them (Britton-Lee in particular) have had
**			anything to gain from my involvement in this project.
*/


int		NextMailer;	/* "free" index into Mailer struct */
char		*FullName;	/* sender's full name */
ENVELOPE	BlankEnvelope;	/* a "blank" envelope */
ENVELOPE	MainEnvelope;	/* the envelope around the basic letter */
ADDRESS		NullAddress =	/* a null address */
		{ "", "", NULL, "" };
char		*CommandLineArgs;	/* command line args for pid file */
bool		Warn_Q_option = FALSE;	/* warn about Q option use */
char		**SaveArgv;	/* argument vector for re-execing */

static void	obsolete();
extern void	printmailer __P((MAILER *));
extern void	tTflag __P((char *));

#ifdef DAEMON
#ifndef SMTP
ERROR %%%%   Cannot have daemon mode without SMTP   %%%% ERROR
#endif /* SMTP */
#endif /* DAEMON */

#define MAXCONFIGLEVEL	7	/* highest config version level known */

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
	typedef int (*fnptr)();
	STAB *st;
	register int i;
	int j;
	bool queuemode = FALSE;		/* process queue requests */
	bool safecf = TRUE;
	bool warn_C_flag = FALSE;
	char warn_f_flag = '\0';
	bool run_in_foreground = FALSE;	/* -bD mode */
	static bool reenter = FALSE;
	struct passwd *pw;
	struct stat stb;
	struct hostent *hp;
	bool nullserver;
	char jbuf[MAXHOSTNAMELEN];	/* holds MyHostName */
	static char rnamebuf[MAXNAME];	/* holds RealUserName */
	char *emptyenviron[1];
	extern int DtableSize;
	extern int optind;
	extern int opterr;
	extern char *optarg;
	extern char **environ;
	extern time_t convtime();
	extern void intsig();
	extern struct hostent *myhostname();
	extern char *getauthinfo();
	extern char *getcfname();
	extern void sigusr1();
	extern void sighup();
	extern void initmacros __P((ENVELOPE *));
	extern void init_md __P((int, char **));
	extern int getdtsize __P((void));
	extern void tTsetup __P((u_char *, int, char *));
	extern void setdefaults __P((ENVELOPE *));
	extern void initsetproctitle __P((int, char **, char **));
	extern void init_vendor_macros __P((ENVELOPE *));
	extern void load_if_names __P((void));
	extern void vendor_pre_defaults __P((ENVELOPE *));
	extern void vendor_post_defaults __P((ENVELOPE *));
	extern void readcf __P((char *, bool, ENVELOPE *));
	extern void printqueue __P((void));
	extern void sendtoargv __P((char **, ENVELOPE *));
	extern void resetlimits __P((void));

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

	/* do machine-dependent initializations */
	init_md(argc, argv);

#ifdef SIGUSR1
	/* arrange to dump state on user-1 signal */
	setsignal(SIGUSR1, sigusr1);
#endif

	/* in 4.4BSD, the table can be huge; impose a reasonable limit */
	DtableSize = getdtsize();
	if (DtableSize > 256)
		DtableSize = 256;

	/*
	**  Be sure we have enough file descriptors.
	**	But also be sure that 0, 1, & 2 are open.
	*/

	i = open("/dev/null", O_RDWR, 0);
	if (fstat(STDIN_FILENO, &stb) < 0 && errno != EOPNOTSUPP)
		(void) dup2(i, STDIN_FILENO);
	if (fstat(STDOUT_FILENO, &stb) < 0 && errno != EOPNOTSUPP)
		(void) dup2(i, STDOUT_FILENO);
	if (fstat(STDERR_FILENO, &stb) < 0 && errno != EOPNOTSUPP)
		(void) dup2(i, STDERR_FILENO);
	if (i != STDIN_FILENO && i != STDOUT_FILENO && i != STDERR_FILENO)
		(void) close(i);

	i = DtableSize;
	while (--i > 0)
	{
		if (i != STDIN_FILENO && i != STDOUT_FILENO && i != STDERR_FILENO)
			(void) close(i);
	}
	errno = 0;

#ifdef LOG
# ifdef LOG_MAIL
	openlog("sendmail", LOG_PID, LOG_MAIL);
# else 
	openlog("sendmail", LOG_PID);
# endif
#endif 

	tTsetup(tTdvect, sizeof tTdvect, "0-99.1");

	/* Handle any non-getoptable constructions. */
	obsolete(argv);

	/*
	**  Do a quick prescan of the argument list.
	*/

#if defined(__osf__) || defined(_AIX3)
# define OPTIONS	"B:b:C:cd:e:F:f:h:IiM:mN:nO:o:p:q:R:r:sTtUV:vX:x"
#endif
#if defined(sony_news)
# define OPTIONS	"B:b:C:cd:E:e:F:f:h:IiJ:M:mN:nO:o:p:q:R:r:sTtUV:vX:"
#endif
#ifndef OPTIONS
# define OPTIONS	"B:b:C:cd:e:F:f:h:IiM:mN:nO:o:p:q:R:r:sTtUV:vX:"
#endif
	opterr = 0;
	while ((j = getopt(argc, argv, OPTIONS)) != EOF)
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
		}
	}
	opterr = 1;

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
		(void) snprintf(rnamebuf, sizeof rnamebuf, "Unknown UID %d", RealUid);
	RealUserName = rnamebuf;

	/* save command line arguments */
	i = 0;
	for (av = argv; *av != NULL; )
		i += strlen(*av++) + 1;
	SaveArgv = (char **) xalloc(sizeof (char *) * (argc + 1));
	CommandLineArgs = xalloc(i);
	p = CommandLineArgs;
	for (av = argv, i = 0; *av != NULL; )
	{
		SaveArgv[i++] = newstr(*av);
		if (av != argv)
			*p++ = ' ';
		strcpy(p, *av++);
		p += strlen(p);
	}
	SaveArgv[i] = NULL;

	if (tTd(0, 1))
	{
		int ll;
		extern char *CompileOptions[];

		printf("Version %s\n Compiled with:", Version);
		av = CompileOptions;
		ll = 7;
		while (*av != NULL)
		{
			if (ll + strlen(*av) > 63)
			{
				putchar('\n');
				ll = 0;
			}
			if (ll == 0)
			{
				putchar('\t');
				putchar('\t');
			}
			else
				putchar(' ');
			printf("%s", *av);
			ll += strlen(*av++) + 1;
		}
		putchar('\n');
	}
	if (tTd(0, 10))
	{
		int ll;
		extern char *OsCompileOptions[];

		printf("    OS Defines:");
		av = OsCompileOptions;
		ll = 7;
		while (*av != NULL)
		{
			if (ll + strlen(*av) > 63)
			{
				putchar('\n');
				ll = 0;
			}
			if (ll == 0)
			{
				putchar('\t');
				putchar('\t');
			}
			else
				putchar(' ');
			printf("%s", *av);
			ll += strlen(*av++) + 1;
		}
		putchar('\n');
#ifdef _PATH_UNIX
		printf("Kernel symbols:\t%s\n", _PATH_UNIX);
#endif
		printf(" Def Conf file:\t%s\n", getcfname());
		printf("      Pid file:\t%s\n", PidFile);
	}

	InChannel = stdin;
	OutChannel = stdout;

	/* initialize for setproctitle */
	initsetproctitle(argc, argv, envp);

	/* clear sendmail's environment */
	ExternalEnviron = environ;
	emptyenviron[0] = NULL;
	environ = emptyenviron;

	/* prime the child environment */
	setuserenv("AGENT", "sendmail");

	if (setsignal(SIGINT, SIG_IGN) != SIG_IGN)
		(void) setsignal(SIGINT, intsig);
	(void) setsignal(SIGTERM, intsig);
	(void) setsignal(SIGPIPE, SIG_IGN);
	OldUmask = umask(022);
	OpMode = MD_DELIVER;
	FullName = getextenv("NAME");

	/*
	**  Initialize name server if it is going to be used.
	*/

#if NAMED_BIND
	if (!bitset(RES_INIT, _res.options))
		res_init();
	if (tTd(8, 8))
		_res.options |= RES_DEBUG;
# ifdef RES_NOALIASES
	_res.options |= RES_NOALIASES;
# endif
#endif

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
			printf("canonical name: %s\n", jbuf);
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
					printf("\ta.k.a.: %s\n", jbuf);
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
				printf("uname failed (%s)\n", errstring(errno));
			makelower(jbuf);
			p = jbuf;
		}
		if (tTd(0, 4))
			printf(" UUCP nodename: %s\n", p);
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
				printf("\ta.k.a.: %s\n", *av);
			setclass('w', *av);
		}
#if NETINET
		if (hp->h_addrtype == AF_INET && hp->h_length == INADDRSZ)
		{
			register int i;

			for (i = 0; hp->h_addr_list[i] != NULL; i++)
			{
				char ipbuf[103];

				snprintf(ipbuf, sizeof ipbuf, "[%.100s]",
					inet_ntoa(*((struct in_addr *) hp->h_addr_list[i])));
				if (tTd(0, 4))
					printf("\ta.k.a.: %s\n", ipbuf);
				setclass('w', ipbuf);
			}
		}
#endif
	}

	/* probe interfaces and locate any additional names */
	load_if_names();

	/* current time */
	define('b', arpadate((char *) NULL), CurEnv);

	/*
	** Crack argv.
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
	while ((j = getopt(argc, argv, OPTIONS)) != EOF)
	{
		switch (j)
		{
		  case 'b':	/* operations mode */
			switch (j = *optarg)
			{
			  case MD_DAEMON:
			  case MD_FGDAEMON:
# ifndef DAEMON
				usrerr("Daemon mode not implemented");
				ExitStat = EX_USAGE;
				break;
# endif /* DAEMON */
			  case MD_SMTP:
# ifndef SMTP
				usrerr("I don't speak SMTP");
				ExitStat = EX_USAGE;
				break;
# endif /* SMTP */

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
			CurEnv->e_bodytype = optarg;
			break;

		  case 'C':	/* select configuration file (already done) */
			if (RealUid != 0)
				warn_C_flag = TRUE;
			ConfFile = optarg;
			endpwent();
			(void) setgid(RealGid);
			(void) setuid(RealUid);
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

		  case 'h':	/* hop count */
			CurEnv->e_hopcount = strtol(optarg, &ep, 10);
			if (*ep)
			{
				usrerr("Bad hop count (%s)", optarg);
				ExitStat = EX_USAGE;
			}
			break;
		
		  case 'n':	/* don't alias */
			NoAlias = TRUE;
			break;

		  case 'N':	/* delivery status notifications */
			DefaultNotify |= QHASNOTIFY;
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
# ifdef QUEUE
			FullName = NULL;
			queuemode = TRUE;
			switch (optarg[0])
			{
			  case 'I':
				QueueLimitId = newstr(&optarg[1]);
				break;

			  case 'R':
				QueueLimitRecipient = newstr(&optarg[1]);
				break;

			  case 'S':
				QueueLimitSender = newstr(&optarg[1]);
				break;

			  default:
				QueueIntvl = convtime(optarg, 'm');
				break;
			}
# else /* QUEUE */
			usrerr("I don't know about queues");
			ExitStat = EX_USAGE;
# endif /* QUEUE */
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
			break;

		  case 't':	/* read recipients from message */
			GrabTo = TRUE;
			break;

		  case 'U':	/* initial (user) submission */
			UserSubmission = TRUE;
			break;

		  case 'V':	/* DSN ENVID: set "original" envelope id */
			if (!xtextok(optarg))
			{
				usrerr("Invalid syntax in -V flag");
				ExitStat = EX_USAGE;
			}
			else
				CurEnv->e_envid = newstr(optarg);
			break;

		  case 'X':	/* traffic log file */
			endpwent();
			setgid(RealGid);
			setuid(RealUid);
			TrafficLogFile = fopen(optarg, "a");
			if (TrafficLogFile == NULL)
			{
				syserr("cannot open %s", optarg);
				ExitStat = EX_CANTCREAT;
				break;
			}
#ifdef HASSETVBUF
			setvbuf(TrafficLogFile, NULL, _IOLBF, 0);
#else
			setlinebuf(TrafficLogFile);
#endif
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

# ifdef DBM
		  case 'I':	/* initialize alias DBM file */
			OpMode = MD_INITALIAS;
			break;
# endif /* DBM */

# if defined(__osf__) || defined(_AIX3)
		  case 'x':	/* random flag that OSF/1 & AIX mailx passes */
			break;
# endif
# if defined(sony_news)
		  case 'E':
		  case 'J':	/* ignore flags for Japanese code conversion
				   impremented on Sony NEWS */
			break;
# endif

		  default:
			ExitStat = EX_USAGE;
			finis();
			break;
		}
	}
	av += optind;

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
#endif
	vendor_pre_defaults(CurEnv);
	readcf(getcfname(), safecf, CurEnv);
	ConfigFileRead = TRUE;
	vendor_post_defaults(CurEnv);

	/* avoid denial-of-service attacks */
	resetlimits();

	if (OpMode != MD_DAEMON && OpMode != MD_FGDAEMON)
	{
		/* drop privileges -- daemon mode done after socket/bind */
		if (RunAsGid != 0)
			(void) setgid(RunAsGid);
		if (RunAsUid != 0)
			(void) setuid(RunAsUid);
	}

	/*
	**  Find our real host name for future logging.
	*/

	p = getauthinfo(STDIN_FILENO);
	define('_', p, CurEnv);

	/* suppress error printing if errors mailed back or whatever */
	if (CurEnv->e_errormode != EM_PRINT)
		HoldErrs = TRUE;

	/* set up the $=m class now, after .cf has a chance to redefine $m */
	expand("\201m", jbuf, sizeof jbuf, CurEnv);
	setclass('m', jbuf);

	if (tTd(0, 1))
	{
		printf("\n============ SYSTEM IDENTITY (after readcf) ============");
		printf("\n      (short domain name) $w = ");
		xputs(macvalue('w', CurEnv));
		printf("\n  (canonical domain name) $j = ");
		xputs(macvalue('j', CurEnv));
		printf("\n         (subdomain name) $m = ");
		xputs(macvalue('m', CurEnv));
		printf("\n              (node name) $k = ");
		xputs(macvalue('k', CurEnv));
		printf("\n========================================================\n\n");
	}

	/*
	**  Do more command line checking -- these are things that
	**  have to modify the results of reading the config file.
	*/

	/* process authorization warnings from command line */
	if (warn_C_flag)
		auth_warning(CurEnv, "Processed by %s with -C %s",
			RealUserName, ConfFile);
	if (Warn_Q_option)
		auth_warning(CurEnv, "Processed from queue %s", QueueDir);

	/* check body type for legality */
	if (CurEnv->e_bodytype == NULL)
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

	/* Enforce use of local time (null string overrides this) */
	if (TimeZoneSpec == NULL)
		unsetenv("TZ");
	else if (TimeZoneSpec[0] != '\0')
		setuserenv("TZ", TimeZoneSpec);
	else
		setuserenv("TZ", NULL);
	tzset();

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

	if (MeToo)
		BlankEnvelope.e_flags |= EF_METOO;

	switch (OpMode)
	{
	  case MD_TEST:
		/* don't have persistent host status in test mode */
		HostStatDir = NULL;
		break;

	  case MD_FGDAEMON:
		run_in_foreground = TRUE;
		OpMode = MD_DAEMON;
		/* fall through ... */

	  case MD_DAEMON:
		/* check for permissions */
		if (RealUid != 0)
		{
#ifdef LOG
			if (LogLevel > 1)
				syslog(LOG_ALERT, "user %d attempted to run daemon",
					RealUid);
#endif
			usrerr("Permission denied");
			exit(EX_USAGE);
		}
		vendor_daemon_setup(CurEnv);

		/* remove things that don't make sense in daemon mode */
		FullName = NULL;
		GrabTo = FALSE;

		/* arrange to restart on hangup signal */
#ifdef LOG
		if (SaveArgv[0] == NULL || SaveArgv[0][0] != '/')
			syslog(LOG_WARNING, "daemon invoked without full pathname; kill -1 won't work");
#endif
		setsignal(SIGHUP, sighup);

		/* workaround: can't seem to release the signal in the parent */
		releasesignal(SIGHUP);
		break;

	  case MD_INITALIAS:
		Verbose = TRUE;
		/* fall through... */

	  default:
		/* arrange to exit cleanly on hangup signal */
		setsignal(SIGHUP, intsig);
		break;
	}

	/* full names can't have newlines */
	if (FullName != NULL && strchr(FullName, '\n') != NULL)
		FullName = newstr(denlstring(FullName, TRUE, TRUE));

	/* do heuristic mode adjustment */
	if (Verbose)
	{
		/* turn off noconnect option */
		setoption('c', "F", TRUE, FALSE, CurEnv);

		/* turn on interactive delivery */
		setoption('d', "", TRUE, FALSE, CurEnv);
	}

	if (ConfigLevel < 3)
	{
		UseErrorsTo = TRUE;
	}

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

	/* our name for SMTP codes */
	expand("\201j", jbuf, sizeof jbuf, CurEnv);
	MyHostName = jbuf;
	if (strchr(jbuf, '.') == NULL)
		message("WARNING: local host name (%s) is not qualified; fix $j in config file",
			jbuf);

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

		/* propogate some envariables into children */
		setuserenv("ISP", NULL);
		setuserenv("SYSTYPE", NULL);
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
#endif

	/* operate in queue directory */
	if (OpMode == MD_TEST)
		/* nothing -- just avoid further if clauses */ ;
	else if (QueueDir == NULL)
	{
		syserr("QueueDirectory (Q) option must be set");
		ExitStat = EX_CONFIG;
	}
	else if (chdir(QueueDir) < 0)
	{
		syserr("cannot chdir(%s)", QueueDir);
		ExitStat = EX_CONFIG;
	}

	/* check host status directory for validity */
	if (HostStatDir != NULL && !path_is_dir(HostStatDir, FALSE))
	{
		/* cannot use this value */
		if (tTd(0, 2))
			printf("Cannot use HostStatusDirectory = %s: %s\n",
				HostStatDir, errstring(errno));
		HostStatDir = NULL;
	}

# ifdef QUEUE
	if (queuemode && RealUid != 0 && bitset(PRIV_RESTRICTQRUN, PrivacyFlags))
	{
		struct stat stbuf;

		/* check to see if we own the queue directory */
		if (stat(".", &stbuf) < 0)
			syserr("main: cannot stat %s", QueueDir);
		if (stbuf.st_uid != RealUid)
		{
			/* nope, really a botch */
			usrerr("You do not have permission to process the queue");
			exit (EX_NOPERM);
		}
	}
# endif /* QUEUE */

	/* if we've had errors so far, exit now */
	if (ExitStat != EX_OK && OpMode != MD_TEST)
	{
		endpwent();
		setuid(RealUid);
		exit(ExitStat);
	}

#if XDEBUG
	checkfd012("before main() initmaps");
#endif

	/*
	**  Do operation-mode-dependent initialization.
	*/

	switch (OpMode)
	{
	  case MD_PRINT:
		/* print the queue */
#ifdef QUEUE
		dropenvelope(CurEnv, TRUE);
		printqueue();
		endpwent();
		setuid(RealUid);
		exit(EX_OK);
#else /* QUEUE */
		usrerr("No queue to print");
		finis();
#endif /* QUEUE */

	  case MD_HOSTSTAT:
		mci_traverse_persistent(mci_print_persistent, NULL);
		exit(EX_OK);
	    	break;

	  case MD_PURGESTAT:
		mci_traverse_persistent(mci_purge_persistent, NULL);
		exit(EX_OK);
	    	break;

	  case MD_INITALIAS:
		/* initialize alias database */
		initmaps(TRUE, CurEnv);
		endpwent();
		setuid(RealUid);
		exit(ExitStat);

	  case MD_SMTP:
		nullserver = FALSE;
		/* fall through... */

	  case MD_DAEMON:
		/* reset DSN parameters */
		DefaultNotify = QPINGONFAILURE|QPINGONDELAY;
		CurEnv->e_envid = NULL;
		CurEnv->e_flags &= ~(EF_RET_PARAM|EF_NO_BODY_RETN);

		/* don't open alias database -- done in srvrsmtp */
		break;

	  default:
		/* open the alias database */
		initmaps(FALSE, CurEnv);
		break;
	}

	if (tTd(0, 15))
	{
		extern void printrules __P((void));

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
		void intindebug();

		if (isatty(fileno(stdin)))
			Verbose = TRUE;

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
			extern void testmodeline __P((char *, ENVELOPE *));

			if (Verbose)
				printf("> ");
			(void) fflush(stdout);
			if (fgets(buf, sizeof buf, stdin) == NULL)
				finis();
			p = strchr(buf, '\n');
			if (p != NULL)
				*p = '\0';
			if (!Verbose)
				printf("> %s\n", buf);
			testmodeline(buf, CurEnv);
		}
	}

# ifdef QUEUE
	/*
	**  If collecting stuff from the queue, go start doing that.
	*/

	if (queuemode && OpMode != MD_DAEMON && QueueIntvl == 0)
	{
		(void) unsetenv("HOSTALIASES");
		runqueue(FALSE);
		finis();
	}
# endif /* QUEUE */

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
		extern bool getrequests __P((ENVELOPE *));

		if (!run_in_foreground && !tTd(99, 100))
		{
			/* put us in background */
			i = fork();
			if (i < 0)
				syserr("daemon: cannot fork");
			if (i != 0)
				exit(0);

			/* disconnect from our controlling tty */
			disconnect(2, CurEnv);
		}

		dtype[0] = '\0';
		if (OpMode == MD_DAEMON)
			strcat(dtype, "+SMTP");
		if (QueueIntvl != 0)
		{
			strcat(dtype, "+queueing@");
			strcat(dtype, pintvl(QueueIntvl, TRUE));
		}
		if (tTd(0, 1))
			strcat(dtype, "+debugging");

#ifdef LOG
		syslog(LOG_INFO, "starting daemon (%s): %s", Version, dtype + 1);
#endif
#ifdef XLA
		xla_create_file();
#endif

# ifdef QUEUE
		if (queuemode)
		{
			runqueue(TRUE);
			if (OpMode != MD_DAEMON)
				for (;;)
					pause();
		}
# endif /* QUEUE */
		dropenvelope(CurEnv, TRUE);

#ifdef DAEMON
		nullserver = getrequests(CurEnv);

		/* drop privileges */
		if (RunAsGid != 0)
			(void) setgid(RunAsGid);
		if (RunAsUid != 0)
			(void) setuid(RunAsUid);

		/* at this point we are in a child: reset state */
		(void) newenvelope(CurEnv, CurEnv);

		/*
		**  Get authentication data
		*/

		p = getauthinfo(fileno(InChannel));
		define('_', p, &BlankEnvelope);
#endif /* DAEMON */
	}

# ifdef SMTP
	/*
	**  If running SMTP protocol, start collecting and executing
	**  commands.  This will never return.
	*/

	if (OpMode == MD_SMTP || OpMode == MD_DAEMON)
	{
		char pbuf[20];
		extern void smtp __P((bool, ENVELOPE *));

		/*
		**  Save some macros for check_* rulesets.
		*/

		define(macid("{client_name}", NULL), RealHostName, &BlankEnvelope);
		define(macid("{client_addr}", NULL),
		       newstr(anynet_ntoa(&RealHostAddr)), &BlankEnvelope);
		if (RealHostAddr.sa.sa_family == AF_INET)
			snprintf(pbuf, sizeof pbuf, "%d", RealHostAddr.sin.sin_port);
		else
			snprintf(pbuf, sizeof pbuf, "0");
		define(macid("{client_port}", NULL), newstr(pbuf), &BlankEnvelope);

		smtp(nullserver, CurEnv);
	}
# endif /* SMTP */

	clearenvelope(CurEnv, FALSE);
	if (OpMode == MD_VERIFY)
	{
		CurEnv->e_sendmode = SM_VERIFY;
		CurEnv->e_errormode = EM_PRINT;
		PostMasterCopy = NULL;
		HoldErrs = FALSE;
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
	if (warn_f_flag != '\0' && !wordinclass(RealUserName, 't'))
		auth_warning(CurEnv, "%s set sender to %s using -%c",
			RealUserName, from, warn_f_flag);
	setsender(from, CurEnv, NULL, FALSE);
	if (macvalue('s', CurEnv) == NULL)
		define('s', RealHostName, CurEnv);

	if (*av == NULL && !GrabTo)
	{
		CurEnv->e_flags |= EF_GLOBALERRS;
		usrerr("Recipient names must be specified");

		/* collect body for UUCP return */
		if (OpMode != MD_VERIFY)
			collect(InChannel, FALSE, FALSE, NULL, CurEnv);
		finis();
	}

	/*
	**  Scan argv and deliver the message to everyone.
	*/

	sendtoargv(av, CurEnv);

	/* if we have had errors sofar, arrange a meaningful exit stat */
	if (Errors > 0 && ExitStat == EX_OK)
		ExitStat = EX_USAGE;

	/*
	**  Read the input mail.
	*/

	CurEnv->e_to = NULL;
	if (OpMode != MD_VERIFY || GrabTo)
	{
		CurEnv->e_flags |= EF_GLOBALERRS;
		collect(InChannel, FALSE, FALSE, NULL, CurEnv);
	}
	errno = 0;

	if (tTd(1, 1))
		printf("From person = \"%s\"\n", CurEnv->e_from.q_paddr);

	/*
	**  Actually send everything.
	**	If verifying, just ack.
	*/

	CurEnv->e_from.q_flags |= QDONTSEND;
	if (tTd(1, 5))
	{
		printf("main: QDONTSEND ");
		printaddr(&CurEnv->e_from, FALSE);
	}
	CurEnv->e_to = NULL;
	sendall(CurEnv, SM_DEFAULT);

	/*
	**  All done.
	**	Don't send return error message if in VERIFY mode.
	*/

	finis();
	/*NOTREACHED*/
	return -1;
}


void
intindebug()
{
	longjmp(TopFrame, 1);
}


/*
**  FINIS -- Clean up and exit.
**
**	Parameters:
**		none
**
**	Returns:
**		never
**
**	Side Effects:
**		exits sendmail
*/

void
finis()
{
	if (tTd(2, 1))
	{
		extern void printenvflags();

		printf("\n====finis: stat %d e_id=%s e_flags=",
			ExitStat,
			CurEnv->e_id == NULL ? "NOQUEUE" : CurEnv->e_id);
		printenvflags(CurEnv);
	}
	if (tTd(2, 9))
		printopenfds(FALSE);

	/* clean up temp files */
	CurEnv->e_to = NULL;
	if (CurEnv->e_id != NULL)
		dropenvelope(CurEnv, TRUE);

	/* flush any cached connections */
	mci_flush(TRUE, NULL);

# ifdef XLA
	/* clean up extended load average stuff */
	xla_all_end();
# endif

	/* and exit */
# ifdef LOG
	if (LogLevel > 78)
		syslog(LOG_DEBUG, "finis, pid=%d", getpid());
# endif /* LOG */
	if (ExitStat == EX_TEMPFAIL || CurEnv->e_errormode == EM_BERKNET)
		ExitStat = EX_OK;

	/* reset uid for process accounting */
	endpwent();
	setuid(RealUid);

	exit(ExitStat);
}
/*
**  INTSIG -- clean up on interrupt
**
**	This just arranges to exit.  It pessimises in that it
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
*/

void
intsig()
{
#ifdef LOG
	if (LogLevel > 79)
		syslog(LOG_DEBUG, "%s: interrupt",
			CurEnv->e_id == NULL ? "[NOQUEUE]" : CurEnv->e_id);
#endif
	FileName = NULL;
	unlockqueue(CurEnv);
#ifdef XLA
	xla_all_end();
#endif

	/* reset uid for process accounting */
	endpwent();
	setuid(RealUid);

	exit(EX_OK);
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

	{ '\0' }
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
	extern char *MacroName[256];

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
**			1 -- also, make stdout go to transcript.
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
		printf("disconnect: In %d Out %d, e=%x\n",
			fileno(InChannel), fileno(OutChannel), e);
	if (tTd(52, 100))
	{
		printf("don't\n");
		return;
	}
#ifdef LOG
	if (LogLevel > 93)
		syslog(LOG_DEBUG, "%s: disconnect level %d",
			e->e_id == NULL ? "[NOQUEUE]" : e->e_id, droplev);
#endif

	/* be sure we don't get nasty signals */
	(void) setsignal(SIGINT, SIG_IGN);
	(void) setsignal(SIGQUIT, SIG_IGN);

	/* we can't communicate with our caller, so.... */
	HoldErrs = TRUE;
	CurEnv->e_errormode = EM_MAIL;
	Verbose = FALSE;
	DisConnected = TRUE;

	/* all input from /dev/null */
	if (InChannel != stdin)
	{
		(void) fclose(InChannel);
		InChannel = stdin;
	}
	(void) freopen("/dev/null", "r", stdin);

	/* output to the transcript */
	if (OutChannel != stdout)
	{
		(void) fclose(OutChannel);
		OutChannel = stdout;
	}
	if (droplev > 0)
	{
		if (e->e_xfp == NULL)
			fd = open("/dev/null", O_WRONLY, 0666);
		else
			fd = fileno(e->e_xfp);
		(void) fflush(stdout);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (e->e_xfp == NULL)
			close(fd);
	}

	/* drop our controlling TTY completely if possible */
	if (droplev > 1)
	{
		(void) setsid();
		errno = 0;
	}

#if XDEBUG
	checkfd012("disconnect");
#endif

# ifdef LOG
	if (LogLevel > 71)
		syslog(LOG_DEBUG, "in background, pid=%d", getpid());
# endif /* LOG */

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
#endif
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
			argv[0][0] = '-';
			argv[0][1] = 'C';
			(void)strcpy(&argv[0][2], __DEFPATH);
		}

		/* If -q doesn't have an argument, run it once. */
		if (ap[1] == 'q' && ap[2] == '\0')
			*argv = "-q0";

		/* if -d doesn't have an argument, use 0-99.1 */
		if (ap[1] == 'd' && ap[2] == '\0')
			*argv = "-d0-99.1";

# if defined(sony_news)
		/* if -E doesn't have an argument, use -EC */
		if (ap[1] == 'E' && ap[2] == '\0')
			*argv = "-EC";

		/* if -J doesn't have an argument, use -JJ */
		if (ap[1] == 'J' && ap[2] == '\0')
			*argv = "-JJ";
# endif
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
#else
auth_warning(e, msg, va_alist)
	register ENVELOPE *e;
	const char *msg;
	va_dcl
#endif
{
	char buf[MAXLINE];
	VA_LOCAL_DECL

	if (bitset(PRIV_AUTHWARNINGS, PrivacyFlags))
	{
		register char *p;
		static char hostbuf[48];
		extern struct hostent *myhostname();

		if (hostbuf[0] == '\0')
			(void) myhostname(hostbuf, sizeof hostbuf);

		(void) snprintf(buf, sizeof buf, "%s: ", hostbuf);
		p = &buf[strlen(buf)];
		VA_START(msg);
		vsnprintf(p, SPACELEFT(buf, p), msg, ap);
		VA_END;
		addheader("X-Authentication-Warning", buf, &e->e_header);
#ifdef LOG
		if (LogLevel > 3)
			syslog(LOG_INFO, "%s: Authentication-Warning: %.400s",
				e->e_id == NULL ? "[NOQUEUE]" : e->e_id, buf);
#endif
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
	int i;
	char **evp = UserEnviron;
	char *p;

	if (value == NULL)
	{
		value = getextenv(envar);
		if (value == NULL)
			return;
	}

	i = strlen(envar);
	p = (char *) xalloc(strlen(value) + i + 2);
	strcpy(p, envar);
	p[i++] = '=';
	strcpy(&p[i], value);

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
#ifdef LOG
	register char *j = macvalue('j', CurEnv);
	int rs;

	syslog(LOG_DEBUG, "--- dumping state on %s: $j = %s ---",
		when,
		j == NULL ? "<NULL>" : j);
	if (j != NULL)
	{
		if (!wordinclass(j, 'w'))
			syslog(LOG_DEBUG, "*** $j not in $=w ***");
	}
	syslog(LOG_DEBUG, "CurChildren = %d", CurChildren);
	syslog(LOG_DEBUG, "--- open file descriptors: ---");
	printopenfds(TRUE);
	syslog(LOG_DEBUG, "--- connection cache: ---");
	mci_dump_all(TRUE);
	rs = strtorwset("debug_dumpstate", NULL, ST_FIND);
	if (rs > 0)
	{
		int stat;
		register char **pvp;
		char *pv[MAXATOM + 1];

		pv[0] = NULL;
		stat = rewrite(pv, rs, 0, CurEnv);
		syslog(LOG_DEBUG,
		       "--- ruleset debug_dumpstate returns stat %d, pv: ---",
		       stat);
		for (pvp = pv; *pvp != NULL; pvp++)
			syslog(LOG_DEBUG, "%s", *pvp);
	}
	syslog(LOG_DEBUG, "--- end of state dump ---");
#endif
}


void
sigusr1()
{
	dumpstate("user signal");
}


void
sighup()
{
#ifdef LOG
	if (LogLevel > 3)
		syslog(LOG_INFO, "restarting %s on signal", SaveArgv[0]);
#endif
	releasesignal(SIGHUP);
	if (setgid(RealGid) < 0 || setuid(RealUid) < 0)
	{
#ifdef LOG
		if (LogLevel > 0)
			syslog(LOG_ALERT, "could not set[ug]id(%d, %d): %m",
				RealUid, RealGid);
#endif
		exit(EX_OSERR);
	}
	execv(SaveArgv[0], (ARGV_T) SaveArgv);
#ifdef LOG
	if (LogLevel > 0)
		syslog(LOG_ALERT, "could not exec %s: %m", SaveArgv[0]);
#endif
	exit(EX_OSFILE);
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

void
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
	extern bool invalidaddr __P((char *, char *));
	extern char *crackaddr __P((char *));
	extern void dump_class __P((STAB *, int));
	extern void translate_dollars __P((char *));
	extern void help __P((char *));

	switch (line[0])
	{
	  case '#':
	  case 0:
		return;

	  case '?':
		help("-bt");
		return;

	  case '.':		/* config-style settings */
		switch (line[1])
		{
		  case 'D':
			mid = macid(&line[2], &delimptr);
			if (mid == '\0')
				return;
			translate_dollars(delimptr);
			define(mid, newstr(delimptr), e);
			break;

		  case 'C':
			if (line[2] == '\0')	/* not to call syserr() */
				return;

			mid = macid(&line[2], &delimptr);
			if (mid == '\0')
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
				putchar('R');
				s = rw->r_lhs;
				while (*s != NULL)
				{
					xputs(*s++);
					putchar(' ');
				}
				putchar('\t');
				putchar('\t');
				s = rw->r_rhs;
				while (*s != NULL)
				{
					xputs(*s++);
					putchar(' ');
				}
				putchar('\n');
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
			if (mid != '\0')
				stabapply(dump_class, mid);
			return;
		}
		mid = macid(&line[1], NULL);
		if (mid == '\0')
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
			nmx = getmxrr(p, mxhosts, FALSE, &rcode);
			printf("getmxrr(%s) returns %d value(s):\n", p, nmx);
			for (i = 0; i < nmx; i++)
				printf("\t%s\n", mxhosts[i]);
#else
			printf("No MX code compiled in\n");
#endif
		}
		else if (strcasecmp(&line[1], "canon") == 0)
		{
			char host[MAXHOSTNAMELEN];

			if (*p == '\0')
			{
				printf("Usage: /canon address\n");
				return;
			}
			else if (strlen(p) >= sizeof host)
			{
				printf("Name too long\n");
				return;
			}
			strcpy(host, p);
			(void) getcanonname(host, sizeof(host), HasWildcardMX);
			printf("getcanonname(%s) returns %s\n", p, host);
		}
		else if (strcasecmp(&line[1], "map") == 0)
		{
			auto int rcode = EX_OK;

			if (*p == '\0')
			{
				printf("Usage: /map mapname key\n");
				return;
			}
			for (q = p; *q != '\0' && !isspace(*q); q++)
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
			printf("map_lookup: %s (%s) ", p, q);
			p = (*map->s_map.map_class->map_lookup)
					(&map->s_map, q, NULL, &rcode);
			if (p == NULL)
				printf("no match (%d)\n", rcode);
			else
				printf("returns %s (%d)\n", p, rcode);
		}
		else if (strcasecmp(&line[1], "try") == 0)
		{
			MAILER *m;
			STAB *s;
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
			s = stab(p, ST_MAILER, ST_FIND);
			if (s == NULL)
			{
				printf("Unknown mailer %s\n", p);
				return;
			}
			m = s->s_mailer;
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
			      &delimptr, NULL);
		if (pvp == NULL)
			continue;
		p = q;
		while (*p != '\0')
		{
			int stat;
			int rs = strtorwset(p, NULL, ST_FIND);

			if (rs < 0)
			{
				printf("Undefined ruleset %s\n", p);
				break;
			}
			stat = rewrite(pvp, rs, 0, e);
			if (stat != EX_OK)
				printf("== Ruleset %s (%d) status %d\n",
					p, rs, stat);
			while (*p != '\0' && *p++ != ',')
				continue;
		}
	} while (*(p = delimptr) != '\0');
}


void
dump_class(s, id)
	register STAB *s;
	int id;
{
	if (s->s_type != ST_CLASS)
		return;
	if (bitnset(id & 0xff, s->s_class))
		printf("%s\n", s->s_name);
}
