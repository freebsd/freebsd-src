/*
 * Copyright (c) 1983 Eric P. Allman
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
static char sccsid[] = "@(#)main.c	8.55 (Berkeley) 4/15/94";
#endif /* not lint */

#define	_DEFINE

#include "sendmail.h"
#if NAMED_BIND
#include <arpa/nameser.h>
#include <resolv.h>
#endif
#include <pwd.h>

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
**		Eric Allman, UCB/INGRES (until 10/81)
**			     Britton-Lee, Inc., purveyors of fine
**				database computers (from 11/81)
**			     Now back at UCB at the Mammoth project.
**		The support of the INGRES Project and Britton-Lee is
**			gratefully acknowledged.  Britton-Lee in
**			particular had absolutely nothing to gain from
**			my involvement in this project.
*/


int		NextMailer;	/* "free" index into Mailer struct */
char		*FullName;	/* sender's full name */
ENVELOPE	BlankEnvelope;	/* a "blank" envelope */
ENVELOPE	MainEnvelope;	/* the envelope around the basic letter */
ADDRESS		NullAddress =	/* a null address */
		{ "", "", NULL, "" };
char		*UserEnviron[MAXUSERENVIRON + 2];
				/* saved user environment */
char		RealUserName[256];	/* the actual user id on this host */
char		*CommandLineArgs;	/* command line args for pid file */
bool		Warn_Q_option = FALSE;	/* warn about Q option use */

/*
**  Pointers for setproctitle.
**	This allows "ps" listings to give more useful information.
*/

# ifdef SETPROCTITLE
char		**Argv = NULL;		/* pointer to argument vector */
char		*LastArgv = NULL;	/* end of argv */
# endif /* SETPROCTITLE */

static void	obsolete();

#ifdef DAEMON
#ifndef SMTP
ERROR %%%%   Cannot have daemon mode without SMTP   %%%% ERROR
#endif /* SMTP */
#endif /* DAEMON */

#define MAXCONFIGLEVEL	5	/* highest config version level known */

main(argc, argv, envp)
	int argc;
	char **argv;
	char **envp;
{
	register char *p;
	char **av;
	extern int finis();
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
	static bool reenter = FALSE;
	char *argv0 = argv[0];
	struct passwd *pw;
	struct stat stb;
	char jbuf[MAXHOSTNAMELEN];	/* holds MyHostName */
	extern int DtableSize;
	extern int optind;
	extern time_t convtime();
	extern putheader(), putbody();
	extern void intsig();
	extern char **myhostname();
	extern char *arpadate();
	extern char *getauthinfo();
	extern char *getcfname();
	extern char *optarg;
	extern char **environ;
	extern void sigusr1();

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

	/* do machine-dependent initializations */
	init_md(argc, argv);

	/* arrange to dump state on signal */
#ifdef SIGUSR1
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

	pw = getpwuid(RealUid);
	if (pw != NULL)
		(void) strcpy(RealUserName, pw->pw_name);
	else
		(void) sprintf(RealUserName, "Unknown UID %d", RealUid);

	/* save command line arguments */
	i = 0;
	for (av = argv; *av != NULL; )
		i += strlen(*av++) + 1;
	CommandLineArgs = xalloc(i);
	p = CommandLineArgs;
	for (av = argv; *av != NULL; )
	{
		if (av != argv)
			*p++ = ' ';
		strcpy(p, *av++);
		p += strlen(p);
	}

	/* Handle any non-getoptable constructions. */
	obsolete(argv);

	/*
	**  Do a quick prescan of the argument list.
	*/

#if defined(__osf__) || defined(_AIX3)
# define OPTIONS	"B:b:C:cd:e:F:f:h:Iimno:p:q:r:sTtvX:x"
#endif
#if defined(ultrix)
# define OPTIONS	"B:b:C:cd:e:F:f:h:IiM:mno:p:q:r:sTtvX:"
#endif
#if defined(NeXT)
# define OPTIONS	"B:b:C:cd:e:F:f:h:IimnOo:p:q:r:sTtvX:"
#endif
#ifndef OPTIONS
# define OPTIONS	"B:b:C:cd:e:F:f:h:Iimno:p:q:r:sTtvX:"
#endif
	while ((j = getopt(argc, argv, OPTIONS)) != EOF)
	{
		switch (j)
		{
		  case 'd':
			tTsetup(tTdvect, sizeof tTdvect, "0-99.1");
			tTflag(optarg);
			setbuf(stdout, (char *) NULL);
			printf("Version %s\n", Version);
			break;
		}
	}

	InChannel = stdin;
	OutChannel = stdout;

	/*
	**  Move the environment so setproctitle can use the space at
	**  the top of memory.
	*/

	for (i = j = 0; j < MAXUSERENVIRON && (p = envp[i]) != NULL; i++)
	{
		if (strncmp(p, "FS=", 3) == 0 || strncmp(p, "LD_", 3) == 0)
			continue;
		UserEnviron[j++] = newstr(p);
	}
	UserEnviron[j] = NULL;
	environ = UserEnviron;

# ifdef SETPROCTITLE
	/*
	**  Save start and extent of argv for setproctitle.
	*/

	Argv = argv;
	if (i > 0)
		LastArgv = envp[i - 1] + strlen(envp[i - 1]);
	else
		LastArgv = argv[argc - 1] + strlen(argv[argc - 1]);
# endif /* SETPROCTITLE */

	if (setsignal(SIGINT, SIG_IGN) != SIG_IGN)
		(void) setsignal(SIGINT, intsig);
	if (setsignal(SIGHUP, SIG_IGN) != SIG_IGN)
		(void) setsignal(SIGHUP, intsig);
	(void) setsignal(SIGTERM, intsig);
	(void) setsignal(SIGPIPE, SIG_IGN);
	OldUmask = umask(022);
	OpMode = MD_DELIVER;
	FullName = getenv("NAME");

#if NAMED_BIND
	if (tTd(8, 8))
		_res.options |= RES_DEBUG;
#endif

	errno = 0;
	from = NULL;

	/* initialize some macros, etc. */
	initmacros(CurEnv);

	/* version */
	define('v', Version, CurEnv);

	/* hostname */
	av = myhostname(jbuf, sizeof jbuf);
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
				setclass('m', &p[1]);
			}
			while (p != NULL && strchr(&p[1], '.') != NULL)
			{
				*p = '\0';
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
			printf("UUCP nodename: %s\n", p);
		p = newstr(p);
		define('k', p, CurEnv);
		setclass('k', p);
		setclass('w', p);
	}
	while (av != NULL && *av != NULL)
	{
		if (tTd(0, 4))
			printf("\ta.k.a.: %s\n", *av);
		setclass('w', *av++);
	}

	/* current time */
	define('b', arpadate((char *) NULL), CurEnv);

	/*
	**  Find our real host name for future logging.
	*/

	p = getauthinfo(STDIN_FILENO);
	define('_', p, CurEnv);

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

	optind = 1;
	while ((j = getopt(argc, argv, OPTIONS)) != EOF)
	{
		switch (j)
		{
		  case 'b':	/* operations mode */
			switch (j = *optarg)
			{
			  case MD_DAEMON:
# ifdef DAEMON
				if (RealUid != 0) {
					usrerr("Permission denied");
					exit (EX_USAGE);
				}
				(void) unsetenv("HOSTALIASES");
# else
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
			  case MD_DELIVER:
			  case MD_VERIFY:
			  case MD_TEST:
			  case MD_INITALIAS:
			  case MD_PRINT:
#ifdef MAYBE_NEXT_RELEASE
			  case MD_ARPAFTP:
#endif
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
			ConfFile = optarg;
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
			from = newstr(optarg);
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
				break;
			}
			break;
		
		  case 'n':	/* don't alias */
			NoAlias = TRUE;
			break;

		  case 'o':	/* set option */
			setoption(*optarg, optarg + 1, FALSE, TRUE, CurEnv);
			break;

		  case 'p':	/* set protocol */
			p = strchr(optarg, ':');
			if (p != NULL)
				*p++ = '\0';
			if (*optarg != '\0')
				define('r', newstr(optarg), CurEnv);
			if (p != NULL && *p != '\0')
				define('s', newstr(p), CurEnv);
			break;

		  case 'q':	/* run queue files at intervals */
# ifdef QUEUE
			(void) unsetenv("HOSTALIASES");
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

		  case 't':	/* read recipients from message */
			GrabTo = TRUE;
			break;

		  case 'X':	/* traffic log file */
			setuid(RealUid);
			TrafficLogFile = fopen(optarg, "a");
			if (TrafficLogFile == NULL)
			{
				syserr("cannot open %s", optarg);
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
# if defined(ultrix)
		  case 'M':	/* define macro */
# endif
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
# if defined(NeXT)
		  case 'O':	/* random flag that NeXT Mail.app passes */
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

#ifdef XDEBUG
	checkfd012("before readcf");
#endif
	readcf(getcfname(), safecf, CurEnv);

	if (tTd(0, 1))
	{
		printf("SYSTEM IDENTITY (after readcf):");
		printf("\n\t    (short domain name) $w = ");
		xputs(macvalue('w', CurEnv));
		printf("\n\t(canonical domain name) $j = ");
		xputs(macvalue('j', CurEnv));
		printf("\n\t       (subdomain name) $m = ");
		xputs(macvalue('m', CurEnv));
		printf("\n\t            (node name) $k = ");
		xputs(macvalue('k', CurEnv));
		printf("\n");
	}

	/*
	**  Process authorization warnings from command line.
	*/

	if (warn_C_flag)
		auth_warning(CurEnv, "Processed by %s with -C %s",
			RealUserName, ConfFile);
/*
	if (warn_f_flag != '\0')
		auth_warning(CurEnv, "%s set sender to %s using -%c",
			RealUserName, from, warn_f_flag);
*/
	if (Warn_Q_option)
		auth_warning(CurEnv, "Processed from queue %s", QueueDir);

	/* Enforce use of local time (null string overrides this) */
	if (TimeZoneSpec == NULL)
		unsetenv("TZ");
	else if (TimeZoneSpec[0] != '\0')
	{
		char **evp = UserEnviron;
		char tzbuf[100];

		strcpy(tzbuf, "TZ=");
		strcpy(&tzbuf[3], TimeZoneSpec);

		while (*evp != NULL && strncmp(*evp, "TZ=", 3) != 0)
			evp++;
		if (*evp == NULL)
		{
			*evp++ = newstr(tzbuf);
			*evp = NULL;
		}
		else
			*evp++ = newstr(tzbuf);
	}

	if (ConfigLevel > MAXCONFIGLEVEL)
	{
		syserr("Warning: .cf version level (%d) exceeds program functionality (%d)",
			ConfigLevel, MAXCONFIGLEVEL);
	}

	if (MeToo)
		BlankEnvelope.e_flags |= EF_METOO;

# ifdef QUEUE
	if (queuemode && RealUid != 0 && bitset(PRIV_RESTRICTQRUN, PrivacyFlags))
	{
		struct stat stbuf;

		/* check to see if we own the queue directory */
		if (stat(QueueDir, &stbuf) < 0)
			syserr("main: cannot stat %s", QueueDir);
		if (stbuf.st_uid != RealUid)
		{
			/* nope, really a botch */
			usrerr("You do not have permission to process the queue");
			exit (EX_NOPERM);
		}
	}
# endif /* QUEUE */

	switch (OpMode)
	{
	  case MD_INITALIAS:
		Verbose = TRUE;
		break;

	  case MD_DAEMON:
		/* remove things that don't make sense in daemon mode */
		FullName = NULL;
		break;
	}

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

	/* our name for SMTP codes */
	expand("\201j", jbuf, &jbuf[sizeof jbuf - 1], CurEnv);
	MyHostName = jbuf;

	/* make certain that this name is part of the $=w class */
	setclass('w', MyHostName);

	/* the indices of built-in mailers */
	st = stab("local", ST_MAILER, ST_FIND);
	if (st == NULL)
		syserr("No local mailer defined");
	else
		LocalMailer = st->s_mailer;

	st = stab("prog", ST_MAILER, ST_FIND);
	if (st == NULL)
		syserr("No prog mailer defined");
	else
		ProgMailer = st->s_mailer;

	st = stab("*file*", ST_MAILER, ST_FIND);
	if (st == NULL)
		syserr("No *file* mailer defined");
	else
		FileMailer = st->s_mailer;

	st = stab("*include*", ST_MAILER, ST_FIND);
	if (st == NULL)
		syserr("No *include* mailer defined");
	else
		InclMailer = st->s_mailer;


	/* operate in queue directory */
	if (OpMode != MD_TEST && chdir(QueueDir) < 0)
	{
		syserr("cannot chdir(%s)", QueueDir);
		ExitStat = EX_SOFTWARE;
	}

	/* if we've had errors so far, exit now */
	if (ExitStat != EX_OK && OpMode != MD_TEST)
	{
		setuid(RealUid);
		exit(ExitStat);
	}

#ifdef XDEBUG
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
		dropenvelope(CurEnv);
		printqueue();
		setuid(RealUid);
		exit(EX_OK);
#else /* QUEUE */
		usrerr("No queue to print");
		finis();
#endif /* QUEUE */

	  case MD_INITALIAS:
		/* initialize alias database */
		initmaps(TRUE, CurEnv);
		setuid(RealUid);
		exit(EX_OK);

	  case MD_DAEMON:
		/* don't open alias database -- done in srvrsmtp */
		break;

	  default:
		/* open the alias database */
		initmaps(FALSE, CurEnv);
		break;
	}

	if (tTd(0, 15))
	{
		/* print configuration table (or at least part of it) */
		printrules();
		for (i = 0; i < MAXMAILERS; i++)
		{
			register struct mailer *m = Mailer[i];
			int j;

			if (m == NULL)
				continue;
			printf("mailer %d (%s): P=%s S=%d/%d R=%d/%d M=%ld F=", i, m->m_name,
				m->m_mailer, m->m_se_rwset, m->m_sh_rwset,
				m->m_re_rwset, m->m_rh_rwset, m->m_maxsize);
			for (j = '\0'; j <= '\177'; j++)
				if (bitnset(j, m->m_flags))
					(void) putchar(j);
			printf(" E=");
			xputs(m->m_eol);
			if (m->m_argv != NULL)
			{
				char **a = m->m_argv;

				printf(" A=");
				while (*a != NULL)
				{
					if (a != m->m_argv)
						printf(" ");
					xputs(*a++);
				}
			}
			printf("\n");
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

		if (isatty(fileno(stdin)))
			Verbose = TRUE;

		if (Verbose)
		{
			printf("ADDRESS TEST MODE (ruleset 3 NOT automatically invoked)\n");
			printf("Enter <ruleset> <address>\n");
		}
		for (;;)
		{
			register char **pvp;
			char *q;
			auto char *delimptr;
			extern bool invalidaddr();
			extern char *crackaddr();

			if (Verbose)
				printf("> ");
			(void) fflush(stdout);
			if (fgets(buf, sizeof buf, stdin) == NULL)
				finis();
			if (!Verbose)
				printf("> %s", buf);
			switch (buf[0])
			{
			  case '#':
				continue;

#ifdef MAYBENEXTRELEASE
			  case 'C':		/* try crackaddr */
			  	q = crackaddr(&buf[1]);
			  	xputs(q);
			  	printf("\n");
			  	continue;
#endif
			}

			for (p = buf; isascii(*p) && isspace(*p); p++)
				continue;
			q = p;
			while (*p != '\0' && !(isascii(*p) && isspace(*p)))
				p++;
			if (*p == '\0')
			{
				printf("No address!\n");
				continue;
			}
			*p = '\0';
			if (invalidaddr(p + 1, NULL))
				continue;
			do
			{
				char pvpbuf[PSBUFSIZE];

				pvp = prescan(++p, ',', pvpbuf, sizeof pvpbuf,
					      &delimptr);
				if (pvp == NULL)
					continue;
				p = q;
				while (*p != '\0')
				{
					int stat;

					stat = rewrite(pvp, atoi(p), 0, CurEnv);
					if (stat != EX_OK)
						printf("== Ruleset %s status %d\n",
							p, stat);
					while (*p != '\0' && *p++ != ',')
						continue;
				}
			} while (*(p = delimptr) != '\0');
		}
	}

# ifdef QUEUE
	/*
	**  If collecting stuff from the queue, go start doing that.
	*/

	if (queuemode && OpMode != MD_DAEMON && QueueIntvl == 0)
	{
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

		if (!tTd(0, 1))
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
		dropenvelope(CurEnv);

#ifdef DAEMON
		getrequests();

		/* at this point we are in a child: reset state */
		(void) newenvelope(CurEnv, CurEnv);

		/*
		**  Get authentication data
		*/

		p = getauthinfo(fileno(InChannel));
		define('_', p, CurEnv);

#endif /* DAEMON */
	}
	
# ifdef SMTP
	/*
	**  If running SMTP protocol, start collecting and executing
	**  commands.  This will never return.
	*/

	if (OpMode == MD_SMTP || OpMode == MD_DAEMON)
		smtp(CurEnv);
# endif /* SMTP */

	if (OpMode == MD_VERIFY)
	{
		CurEnv->e_sendmode = SM_VERIFY;
		CurEnv->e_errormode = EM_QUIET;
	}
	else
	{
		/* interactive -- all errors are global */
		CurEnv->e_flags |= EF_GLOBALERRS;
	}

	/*
	**  Do basic system initialization and set the sender
	*/

	initsys(CurEnv);
	setsender(from, CurEnv, NULL, FALSE);
	if (macvalue('s', CurEnv) == NULL)
		define('s', RealHostName, CurEnv);

	if (*av == NULL && !GrabTo)
	{
		CurEnv->e_flags |= EF_GLOBALERRS;
		usrerr("Recipient names must be specified");

		/* collect body for UUCP return */
		if (OpMode != MD_VERIFY)
			collect(FALSE, FALSE, CurEnv);
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
		collect(FALSE, FALSE, CurEnv);
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

finis()
{
	if (tTd(2, 1))
		printf("\n====finis: stat %d e_flags %o, e_id=%s\n",
			ExitStat, CurEnv->e_flags,
			CurEnv->e_id == NULL ? "NOQUEUE" : CurEnv->e_id);
	if (tTd(2, 9))
		printopenfds(FALSE);

	/* clean up temp files */
	CurEnv->e_to = NULL;
	dropenvelope(CurEnv);

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
	if (ExitStat == EX_TEMPFAIL)
		ExitStat = EX_OK;

	/* reset uid for process accounting */
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
	FileName = NULL;
	unlockqueue(CurEnv);
#ifdef XLA
	xla_all_end();
#endif

	/* reset uid for process accounting */
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
	'*', MATCHZANY,		'+', MATCHANY,		'-', MATCHONE,
	'=', MATCHCLASS,	'~', MATCHNCLASS,

	/* these are RHS metasymbols */
	'#', CANONNET,		'@', CANONHOST,		':', CANONUSER,
	'>', CALLSUBR,

	/* the conditional operations */
	'?', CONDIF,		'|', CONDELSE,		'.', CONDFI,

	/* the hostname lookup characters */
	'[', HOSTBEGIN,		']', HOSTEND,
	'(', LOOKUPBEGIN,	')', LOOKUPEND,

	/* miscellaneous control characters */
	'&', MACRODEXPAND,

	'\0'
};

initmacros(e)
	register ENVELOPE *e;
{
	register struct metamac *m;
	char buf[5];
	register int c;

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
	define('e', "\201j Sendmail \201v ready at \201b", e);
	define('l', "From \201g  \201d", e);
	define('n', "MAILER-DAEMON", e);
	define('o', ".:@[]", e);
	define('q', "<\201g>", e);
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

disconnect(droplev, e)
	int droplev;
	register ENVELOPE *e;
{
	int fd;

	if (tTd(52, 1))
		printf("disconnect: In %d Out %d, e=%x\n",
			fileno(InChannel), fileno(OutChannel), e);
	if (tTd(52, 5))
	{
		printf("don't\n");
		return;
	}

	/* be sure we don't get nasty signals */
	(void) setsignal(SIGHUP, SIG_IGN);
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

#ifdef XDEBUG
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
		    ap[1] != 'd' && argv[1] != NULL && argv[1][0] != '-')
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
		extern char **myhostname();

		if (hostbuf[0] == '\0')
			(void) myhostname(hostbuf, sizeof hostbuf);

		(void) sprintf(buf, "%s: ", hostbuf);
		p = &buf[strlen(buf)];
		VA_START(msg);
		vsprintf(p, msg, ap);
		VA_END;
		addheader("X-Authentication-Warning", buf, e);
	}
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
	register STAB *s;

	syslog(LOG_DEBUG, "--- dumping state on %s: $j = %s ---",
		when,
		j == NULL ? "<NULL>" : j);
	if (j != NULL)
	{
		s = stab(j, ST_CLASS, ST_FIND);
		if (s == NULL || !bitnset('w', s->s_class))
			syslog(LOG_DEBUG, "*** $j not in $=w ***");
	}
	syslog(LOG_DEBUG, "--- open file descriptors: ---");
	printopenfds(TRUE);
	syslog(LOG_DEBUG, "--- connection cache: ---");
	mci_dump_all(TRUE);
	if (RewriteRules[89] != NULL)
	{
		int stat;
		register char **pvp;
		char *pv[MAXATOM + 1];

		pv[0] = NULL;
		stat = rewrite(pv, 89, 0, CurEnv);
		syslog(LOG_DEBUG, "--- ruleset 89 returns stat %d, pv: ---",
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
