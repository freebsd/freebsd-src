/* main.c */

#define _main_c_

#define FTP_VERSION "1.9.5 (October 29, 1995)"

/* #define BETA 1 */ /* If defined, it prints a little warning message. */

#include "sys.h"

#include <sys/stat.h>
#include <arpa/ftp.h>
#include <setjmp.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <pwd.h>
#ifdef __FreeBSD__
#include <locale.h>
#endif

#ifdef SYSLOG
#	include <syslog.h>
#endif

#if defined(CURSES) && !defined(NO_CURSES_H)
#	undef HZ		/* Collides with HaZeltine ! */
#	include <curses.h>
#	ifdef TERMH
#		include <term.h>
#	endif
#endif	/* CURSES */

#if defined(CURSES) && defined(SGTTYB)
#       include <sgtty.h>
#endif

#include "util.h"
#include "cmds.h"
#include "main.h"
#include "ftp.h"
#include "ftprc.h"
#include "open.h"
#include "set.h"
#include "defaults.h"
#include "copyright.h"

/* main.c globals */
int					slrflag;
int					fromatty;			/* input is from a terminal */
int					toatty;				/* output is to a terminal */
int					doing_script;		/* is a file being <redirected to me? */
char				*altarg;			/* argv[1] with no shell-like preprocessing  */
struct servent		serv;				/* service spec for tcp/ftp */
static char			pad2a[8] = "Pad 2a";	/* SunOS overwrites jmp_bufs... */
jmp_buf				toplevel;			/* non-local goto stuff for cmd scanner */
static char			pad2b[8] = "Pad 2b";
char				*line;				/* input line buffer */
char				*stringbase;		/* current scan point in line buffer */
char				*argbuf;			/* argument storage buffer */
char				*argbase;			/* current storage point in arg buffer */
int					margc;				/* count of arguments on input line */
char				*margv[20];			/* args parsed from input line */
struct userinfo		uinfo;				/* a copy of their pwent really */
int					ansi_escapes;		/* for fancy graphics */
int                             startup_msg = 1;        /* TAR: display message on startup? */
int					ignore_rc;			/* are we supposed to ignore the netrc */
string				progname;			/* simple filename */
string				prompt, prompt2;	/* shell prompt string */
string				anon_password;		/* most likely your email address */
string				pager;				/* program to browse text files */
string				version = FTP_VERSION;
long				eventnumber;		/* number of commands we've done */
FILE				*logf = NULL;		/* log user activity */
longstring			logfname;			/* name of the logfile */
long				logsize = 4096L;	/* max log size. 0 == no limit */
int					percent_flags;		/* "%" in prompt string? */
int					at_flags;			/* "@" in prompt string? */
string 				mail_path;			/* your mailbox */
time_t				mbox_time;			/* last modified time of mbox */
size_t				epromptlen;			/* length of the last line of the
										 * prompt as it will appear on screen,
										 * (i.e. no invis escape codes).
										 */

#ifdef HPUX
char				*tcap_normal = "\033&d@";	/* Default ANSI escapes */
char				*tcap_boldface = "\033&dH";     /* Half Bright */
char				*tcap_underline = "\033&dD";
char				*tcap_reverse = "\033&dB";

#else

#ifdef NO_FORMATTING

char                            *tcap_normal = "";
char                            *tcap_boldface = "";
char                            *tcap_underline = "";
char                            *tcap_reverse = "";

#else

char                            *tcap_normal = "\033[0m";       /* Default ANSI escapes */
char                            *tcap_boldface = "\033[1m";
char                            *tcap_underline = "\033[4m";
char                            *tcap_reverse = "\033[7m";

#endif

#endif

size_t				tcl_normal = 4,		/* lengths of the above strings. */
					tcl_bold = 4,
					tcl_uline = 4,
					tcl_rev = 4;

#ifdef CURSES
static char			tcbuf[2048];
#endif

/* main.c externs */
extern int			debug, verbose, mprompt, passivemode;
extern int			options, cpend, data, connected, logged_in;
extern int			curtype, macnum, remote_is_unix;
extern FILE			*cout;
extern struct cmd	cmdtab[];
extern str32		curtypename;
extern char			*macbuf;
extern char			*reply_string;
extern char			*short_verbose_msgs[4];
extern string		vstr;
extern Hostname		hostname;
extern longstring	cwd, lcwd, recent_file;
extern int			Optind;
extern char			*Optarg;
#ifdef GATEWAY
extern string		gate_login;
#endif

void main(int argc, char **argv)
{
	register char		*cp;
	int					top, opt, openopts = 0;
	string				tmp, oline;
	struct servent		*sptr;

#ifdef __FreeBSD__
	(void) setlocale(LC_TIME, "");
#endif

	if ((cp = rindex(argv[0], '/'))) cp++;
	else cp = argv[0];
	(void) Strncpy(progname, cp);
	
	sptr = getservbyname("ftp", "tcp");
	if (sptr == 0) fatal("ftp/tcp: unknown service");
	serv = *sptr;

	if (init_arrays())			/* Reserve large blocks of memory now */
		fatal("could not reserve large amounts of memory.");

#ifdef GZCAT
	if ((GZCAT == (char *)1) || (GZCAT == (char *)0)) {
		(void) fprintf(stderr,
"You compiled the program with -DGZCAT, but you must specify the path with it!\n\
Re-compile, this time with -DGZCAT=\\\"/path/to/gzcat\\\".\n");
		exit(1);
	}
#endif
#ifdef ZCAT
	if ((ZCAT == (char *)1) || (ZCAT == (char *)0)) {
		(void) fprintf(stderr,
"You compiled the program with -DZCAT, but you must specify the path with it!\n\
Re-compile, this time with -DZCAT=\\\"/path/to/zcat\\\".\n");
		exit(1);
	}
#endif

	/*
	 * Set up defaults for FTP.
	 */
	mprompt = dMPROMPT;
	debug = dDEBUG;
	verbose = dVERBOSE;
	passivemode = dPASSIVE;
	(void) Strncpy(vstr, short_verbose_msgs[verbose+1]);

	(void) Strncpy(curtypename, dTYPESTR);
	curtype = dTYPE;
	(void) Strncpy(prompt, dPROMPT);
#ifdef GATEWAY
	(void) Strncpy(gate_login, dGATEWAY_LOGIN);
#endif

#ifdef SOCKS
	SOCKSinit("ncftp");
#endif
	
	/*	Setup our pager variable, before we run through the rc,
		which may change it. */
	set_pager(getenv("PAGER"), 0);
#ifdef CURSES
	ansi_escapes = 1;
	termcap_init();
#else
	ansi_escapes = 0;
	if ((cp = getenv("TERM")) != NULL) {
		if ((*cp == 'v' && cp[1] == 't')		/* vt100, vt102, ... */
			|| (strcmp(cp, "xterm") == 0))
			ansi_escapes = 1;
	}
#endif
	(void) getuserinfo();

	/* Init the mailbox checking code. */
	(void) time(&mbox_time);

	(void) Strncpy(anon_password, uinfo.username);
	if (getlocalhostname(uinfo.hostname, sizeof(uinfo.hostname)) == 0) {
		(void) Strncat(anon_password, "@");
		(void) Strncat(anon_password, uinfo.hostname);
	}
#if dLOGGING
	(void) Strncpy(logfname, dLOGNAME);
	(void) LocalDotPath(logfname);
#else
	*logfname = 0;
#endif
	(void) Strncpy(recent_file, dRECENTF);
	(void) LocalDotPath(recent_file);

	(void) get_cwd(lcwd, (int) sizeof(lcwd));

#ifdef SYSLOG
#	ifdef LOG_LOCAL3
	openlog ("NcFTP", LOG_PID, LOG_LOCAL3);
#	else
	openlog ("NcFTP", LOG_PID);
#	endif
#endif				/* SYSLOG */


	ignore_rc = 0;
	(void) strcpy(oline, "open ");
	while ((opt = Getopt(argc, argv, "D:V:INPRHaicmup:rd:g:")) >= 0) {
		switch(opt) {
			case 'a':
			case 'c':
			case 'i':
			case 'm':
			case 'u':
			case 'r':
				(void) sprintf(tmp, "-%c ", opt);
				goto cattmp;

			case 'p':
			case 'd':
			case 'g':
				(void) sprintf(tmp, "-%c %s ", opt, Optarg);
			cattmp:
				(void) strcat(oline, tmp);
				openopts++;
				break;

			case 'D':
				debug = atoi(Optarg);
				break;
			
			case 'V':
				set_verbose(Optarg, 0);
				break;

			case 'I':
				mprompt = !mprompt;
				break;

			case 'N':
				++ignore_rc;
				break;

			case 'P':
				passivemode = !passivemode;
				break;

			case 'H':
				(void) show_version(0, NULL);
				exit (0);

			default:
			usage:
				(void) fprintf(stderr, "Usage: %s [program options] [[open options] site.to.open[:path]]\n\
Program Options:\n\
    -D x   : Set debugging level to x (a number).\n\
    -H     : Show version and compilation information.\n\
    -I     : Toggle interactive (mprompt) mode.\n\
    -N     : Toggle reading of the .netrc/.ncftprc.\n\
    -P     : Toggle passive mode ftp (for use behind firewalls).\n\
    -V x   : Set verbosity to level x (-1,0,1,2).\n\
Open Options:\n\
    -a     : Open anonymously (this is the default).\n\
    -u     : Open, specify user/password.\n\
    -i     : Ignore machine entry in your .netrc.\n\
    -p N   : Use port #N for connection.\n\
    -r     : \"Redial\" until connected.\n\
    -d N   : Redial, pausing N seconds between tries.\n\
    -g N   : Redial, giving up after N tries.\n\
    :path  : ``Colon-mode:'' If \"path\" is a file, it opens site, retrieves\n\
             file \"path,\" then exits; if \"path\" is a remote directory,\n\
             it opens site then starts you in that directory..\n\
    -c     : If you're using colon-mode with a file path, this will cat the\n\
             file to stdout instead of storing on disk.\n\
    -m     : Just like -c, only it pipes the file to your $PAGER.\n\
Examples:\n\
    ncftp ftp.unl.edu:/pub/README (just fetches README then quits)\n\
    ncftp  (just enters ncftp command shell)\n\
    ncftp -V -u ftp.unl.edu\n\
    ncftp -c ftp.unl.edu:/pub/README (cats README to stdout then quits)\n\
    ncftp -D -r -d 120 -g 10 ftp.unl.edu\n", progname);
			exit(1);
		}
	}

	cp = argv[Optind];  /* the site to open. */
	if (cp == NULL) {
		if (openopts)
			goto usage;
	} else
		(void) strcat(oline, cp);

	if (ignore_rc <= 0)
		(void) thrash_rc();
	if (ignore_rc <= 1)
		ReadRecentSitesFile();

	(void) fix_options();	/* adjust "options" according to "debug"  */
	
	fromatty = doing_script = isatty(0);
	toatty = isatty(1);
	(void) UserLoggedIn();	/* Init parent-death detection. */
	cpend = 0;  /* no pending replies */
	
	if (*logfname)
		logf = fopen (logfname, "a");


	/* The user specified a host, maybe in 'colon-mode', on the command
	 * line.  Open it now...
	 */
	if (argc > 1 && cp) {
		if (setjmp(toplevel))
			exit(0);
		(void) Signal(SIGINT, intr);
		(void) Signal(SIGPIPE, lostpeer);
		(void) strcpy(line, oline);
		makeargv();
		/* setpeer uses this to tell if it was called from the cmd-line. */
		eventnumber = 0L;
		if (cmdOpen(margc, margv) != NOERR) {
			exit(1);
		}
	}
	eventnumber = 1L;

	(void) init_prompt();

	if (startup_msg) {  /* TAR */
	    if (ansi_escapes) {
#ifdef BETA
#	define BETA_MSG "\n\
For testing purposes only.  Do not re-distribute or subject to novice users."
#else
#	define BETA_MSG ""
#endif

#ifndef CURSES
		(void) printf("%sNcFTP %s by Mike Gleason, NCEMRSoft.%s%s%s%s\n", 
			tcap_boldface,
			FTP_VERSION,
			tcap_normal,
			tcap_reverse,
			BETA_MSG,
			tcap_normal
		);
#else
		char vis[256];
		(void) sprintf(vis, "%sNcFTP %s by Mike Gleason, NCEMRSoft.%s%s%s%s\n", 
			tcap_boldface,
			FTP_VERSION,
			tcap_normal,
			tcap_reverse,
			BETA_MSG,
			tcap_normal
		);
		tcap_put(vis);
#endif /* !CURSES */
	    }
	    else
		(void) printf("%s%s\n", FTP_VERSION, BETA_MSG);
	}  /* TAR */
	if (NOT_VQUIET)
		PrintTip();
	top = setjmp(toplevel) == 0;
	if (top) {
		(void) Signal(SIGINT, intr);
		(void) Signal(SIGPIPE, lostpeer);
	}
	for (;;) {
		if (cmdscanner(top) && !fromatty)
			exit(1);
		top = 1;
	}
}	/* main */



/*ARGSUSED*/
void intr SIG_PARAMS
{
	dbprintf("intr()\n");
	(void) Signal(SIGINT, intr);
	(void) longjmp(toplevel, 1);
}	/* intr */



int getuserinfo(void)
{
	register char			*cp;
	struct passwd			*pw;
	string					str;
	extern char				*home;	/* for glob.c */
	
	home = uinfo.homedir;	/* for glob.c */
	pw = NULL;
#ifdef USE_GETPWUID
	/* Try to use getpwuid(), but if we have to, fall back to getpwnam(). */
	pw = getpwuid(getuid());
	if (pw == NULL) {
		/* Oh well, try getpwnam() then. */
		cp = getlogin();
		if (cp == NULL) {
			cp = getenv("LOGNAME");
			if (cp == NULL)
				cp = getenv("USER");
		}
		if (cp != NULL)
			pw = getpwnam(cp);
	}
#else
	/* Try to use getpwnam(), but if we have to, fall back to getpwuid(). */
	cp = getlogin();
	if (cp == NULL) {
		cp = getenv("LOGNAME");
		if (cp == NULL)
			cp = getenv("USER");
	}
	if (cp != NULL)
		pw = getpwnam(cp);
	if (pw == NULL) {
		/* Oh well, try getpwuid() then. */
		pw = getpwuid(getuid());
	}
#endif
	if (pw != NULL) {
		uinfo.uid = pw->pw_uid;
		(void) Strncpy(uinfo.username, pw->pw_name);
		(void) Strncpy(uinfo.shell, pw->pw_shell);
		if ((cp = getenv("HOME")) != NULL)
			(void) Strncpy(uinfo.homedir, cp);
		else
			(void) Strncpy(uinfo.homedir, pw->pw_dir);
		cp = getenv("MAIL");
		if (cp != NULL) {
			(void) Strncpy(str, cp);
		} else {
			/* Check a few typical mail directories.
			 * If we don't find it, too bad.  Checking for new mail
			 * isn't very important anyway.
			 */
			(void) sprintf(str, "/usr/spool/mail/%s", uinfo.username);
			if (access(str, 0) < 0) {
				(void) sprintf(str, "/var/mail/%s", uinfo.username);
			}
		}
		cp = str;
		if (access(cp, 0) < 0) {
			mail_path[0] = 0;
		} else {
			/*
			 * mbox variable may be like MAIL=(28 /usr/mail/me /usr/mail/you),
			 * so try to find the first mail path.
			 */
			while ((*cp != '/') && (*cp != 0))
				cp++;
			(void) Strncpy(mail_path, cp);
			if ((cp = index(mail_path, ' ')) != NULL)
				*cp = '\0';
		}
		return (0);
	} else {
		PERROR("getuserinfo", "Could not get your passwd entry!");
		(void) Strncpy(uinfo.shell, "/bin/sh");
		(void) Strncpy(uinfo.homedir, ".");	/* current directory */
		uinfo.uid = 999;
		if ((cp = getenv("HOME")) != NULL)
			(void) Strncpy(uinfo.homedir, cp);
		mail_path[0] = 0;
		return (-1);
	}
}	/* getuserinfo */




int init_arrays(void)
{
	if ((macbuf = (char *) malloc((size_t)(MACBUFLEN))) == NULL)
		goto barf;
	if ((line = (char *) malloc((size_t)(CMDLINELEN))) == NULL)
		goto barf;
	if ((argbuf = (char *) malloc((size_t)(CMDLINELEN))) == NULL)
		goto barf;
	if ((reply_string = (char *) malloc((size_t)(RECEIVEDLINELEN))) == NULL)
		goto barf;
	
	*macbuf = '\0';
	init_transfer_buffer();
	return (0);
barf:
	return (-1);
}	/* init_arrays */



#ifndef BUFSIZ
#define BUFSIZ 512
#endif

void init_transfer_buffer(void)
{
	extern char *xferbuf;
	extern size_t xferbufsize;
	
	/* Make sure we use a multiple of BUFSIZ for efficiency. */
	xferbufsize = (MAX_XFER_BUFSIZE / BUFSIZ) * BUFSIZ;
	while (1) {
		xferbuf = (char *) malloc (xferbufsize);
		if (xferbuf != NULL || xferbufsize < 1024)
			break;
		xferbufsize >>= 2;
	}
	
	if (xferbuf != NULL) return;
	fatal("out of memory for transfer buffer.");
}	/* init_transfer_buffer */




void init_prompt(void)
{
	register char *cp;
	
	percent_flags = at_flags = 0;
	for (cp = prompt; *cp; cp++) {
		if (*cp == '%') percent_flags = 1;
		else if (*cp == '@') at_flags = 1;
	}
}	/* init_prompt */



/*ARGSUSED*/
void lostpeer SIG_PARAMS
{
	if (connected) {
		close_streams(1);
		if (data >= 0) {
			(void) shutdown(data, 1+1);
			(void) close(data);
			data = -1;
		}
		connected = 0;
	}
	if (connected) {
		close_streams(1);
		connected = 0;
	}
	hostname[0] = cwd[0] = 0;
	logged_in = macnum = 0;
}	/* lostpeer */


/*
 * Command parser.
 */
int cmdscanner(int top)
{
	register struct cmd *c;
	int cmd_status, rcode = 0;

	if (!top)
		(void) putchar('\n');
	for (;;) {
		if (!doing_script && !UserLoggedIn())
			(void) quit(0, NULL);
		if (Gets(strprompt(), line, (size_t)CMDLINELEN) == NULL) {
			(void) quit(0, NULL);	/* control-d */
		}
		eventnumber++;
		dbprintf("\"%s\"\n", line);
		(void) makeargv();
		if (margc == 0) {
			continue;	/* blank line... */
		}
		c = getcmd(margv[0]);
		if (c == (struct cmd *) -1) {
			(void) printf("?Ambiguous command\n");
			continue;
		}
		if (c == 0) {
			if (!implicit_cd(margv[0]))
				(void) printf("?Invalid command\n");
			continue;
		}
		if (c->c_conn && !connected) {
			(void) printf ("Not connected.\n");
			continue;
		}
		cmd_status = (*c->c_handler)(margc, margv);
	    if (cmd_status == USAGE)
			cmd_usage(c);
	    else if (cmd_status == CMDERR)
			rcode = 1;
		if (c->c_handler != help)
			break;
	}
	(void) Signal(SIGINT, intr);
	(void) Signal(SIGPIPE, lostpeer);
	return rcode;
}	/* cmdscanner */




char *strprompt(void)
{
	time_t					tyme;
	char					eventstr[8];
	char					*dname, *lastlinestart;
	register char			*p, *q;
	string					str;
	int						flag;

	if (at_flags == 0 && percent_flags == 0) {
		epromptlen = strlen(prompt);
		return (prompt);	/* But don't overwrite it! */
	}
	epromptlen = 0;
	lastlinestart = prompt2;
	if (at_flags) {
		for (p = prompt, q = prompt2, *q = 0; (*p); p++) {
			if (*p == '@') switch (flag = *++p) {
				case '\0':
					--p;
					break;
				case 'M':
					if (CheckNewMail() > 0)
						q = Strpcpy(q, "(Mail) ");
					break;
				case 'N':
					q = Strpcpy(q, "\n");
					lastlinestart = q;
					epromptlen = 0;
					break;
				case 'P':	/* reset to no bold, no uline, no inverse, etc. */
					if (ansi_escapes) {
						q = Strpcpy(q, tcap_normal);
						epromptlen += tcl_normal;
					}
					break;
				case 'B':	/* toggle boldface */
					if (ansi_escapes) {
						q = Strpcpy(q, tcap_boldface);
						epromptlen += tcl_bold;
					}
					break;
				case 'U':	/* toggle underline */
					if (ansi_escapes) {
						q = Strpcpy(q, tcap_underline);
						epromptlen += tcl_uline;
					}
					break;
				case 'R':
				case 'I':	/* toggle inverse (reverse) video */
					if (ansi_escapes) {
						q = Strpcpy(q, tcap_reverse);
						epromptlen += tcl_rev;
					}
					break;
				case 'D':	/* insert current directory */
				case 'J':
					if ((flag == 'J') && (remote_is_unix)) {
						/* Not the whole path, just the dir name. */
						dname = rindex(cwd, '/');
						if (dname == NULL)
							dname = cwd;
						else if ((dname != cwd) && (dname[1]))
							++dname;
					} else
						dname = cwd;
					if (dname[0]) {
						q = Strpcpy(q, dname);
						q = Strpcpy(q, " ");
					}
					break;
				case 'H':	/* insert name of connected host */
					if (logged_in) {
						(void) sprintf(str, "%s ", hostname);
						q = Strpcpy(q, str);
					}
					break;
				case 'C':  /* Insert host:path (colon-mode format. */
					if (logged_in) {
						(void) sprintf(str, "%s:%s ", hostname, cwd);
						q = Strpcpy(q, str);
					} else
						q = Strpcpy(q, "(not connected)");
					break;
				case 'c':
					if (logged_in) {
						(void) sprintf(str, "%s:%s\n", hostname, cwd);
						q = Strpcpy(q, str);
						lastlinestart = q;	/* there is a \n at the end. */
						epromptlen = 0;
					}
					break;
				case '!':
				case 'E':	/* insert event number */
					(void) sprintf(eventstr, "%ld", eventnumber);
					q = Strpcpy(q, eventstr);
					break;
				default:
					*q++ = *p;	/* just copy it; unknown switch */
			} else
				*q++ = *p;
		}
		*q = '\0';
	} else 
		(void) strcpy(prompt2, prompt);
	
#ifndef NO_STRFTIME
	if (percent_flags) {
		/*	only strftime if the user requested it (with a %something),
			otherwise don't waste time doing nothing. */
		(void) time(&tyme);
		(void) Strncpy(str, prompt2);
		(void) strftime(prompt2, sizeof(str), str, localtime(&tyme));
	}
#endif
	epromptlen = (size_t) ((long) strlen(lastlinestart) - (long) epromptlen);
	return (prompt2);
}	/* strprompt */


/*
 * Slice a string up into argc/argv.
 */

void makeargv(void)
{
	char **argp;

	margc = 0;
	argp = margv;
	stringbase = line;		/* scan from first of buffer */
	argbase = argbuf;		/* store from first of buffer */
	slrflag = 0;
	while ((*argp++ = slurpstring()) != 0)
		margc++;
}	/* makeargv */




/*
 * Parse string into argbuf;
 * implemented with FSM to
 * handle quoting and strings
 */
char *slurpstring(void)
{
	int got_one = 0;
	register char *sb = stringbase;
	register char *ap = argbase;
	char *tmp = argbase;		/* will return this if token found */

	if (*sb == '!' || *sb == '$') {	/* recognize ! as a token for shell */
		switch (slrflag) {	/* and $ as token for macro invoke */
			case 0:
				slrflag++;
				stringbase++;
				return ((*sb == '!') ? "!" : "$");
				/* NOTREACHED */
			case 1:
				slrflag++;
				altarg = stringbase;
				break;
			default:
				break;
		}
	}

S0:
	switch (*sb) {

	case '\0':
		goto OUT;

	case ' ':
	case '\t':
	case '\n':
	case '=':
		sb++; goto S0;

	default:
		switch (slrflag) {
			case 0:
				slrflag++;
				break;
			case 1:
				slrflag++;
				altarg = sb;
				break;
			default:
				break;
		}
		goto S1;
	}

S1:
	switch (*sb) {

	case ' ':
	case '\t':
	case '\n':
	case '=':
	case '\0':
		goto OUT;	/* end of token */

	case '\\':
		sb++; goto S2;	/* slurp next character */

	case '"':
		sb++; goto S3;	/* slurp quoted string */

	default:
		*ap++ = *sb++;	/* add character to token */
		got_one = 1;
		goto S1;
	}

S2:
	switch (*sb) {

	case '\0':
		goto OUT;

	default:
		*ap++ = *sb++;
		got_one = 1;
		goto S1;
	}

S3:
	switch (*sb) {

	case '\0':
		goto OUT;

	case '"':
		sb++; goto S1;

	default:
		*ap++ = *sb++;
		got_one = 1;
		goto S3;
	}

OUT:
	if (got_one)
		*ap++ = '\0';
	argbase = ap;			/* update storage pointer */
	stringbase = sb;		/* update scan pointer */
	if (got_one) {
		return(tmp);
	}
	switch (slrflag) {
		case 0:
			slrflag++;
			break;
		case 1:
			slrflag++;
			altarg = (char *) 0;
			break;
		default:
			break;
	}
	return((char *)0);
}	/* slurpstring */

/*
 * Help command.
 * Call each command handler with argc == 0 and argv[0] == name.
 */
int
help(int argc, char **argv)
{
	register struct cmd		*c;
	int						showall = 0, helpall = 0;
	char					*arg;
	int						i, j, k;
	int 					nRows, nCols;
	int 					nCmds2Print;
	int 					screenColumns;
	int 					len, widestName;
	char 					*cp, **cmdnames, spec[16];

	if (argc == 2) {
		showall = (strcmp(argv[1], "showall") == 0);
		helpall = (strcmp(argv[1], "helpall") == 0);
	}
	if (argc == 1 || showall)  {
		(void) printf("\
Commands may be abbreviated.  'help showall' shows aliases, invisible and\n\
unsupported commands.  'help <command>' gives a brief description of <command>.\n\n");

		for (c = cmdtab, nCmds2Print=0; c->c_name != NULL; c++) 
			if (!c->c_hidden || showall)
				nCmds2Print++;

		if ((cmdnames = (char **) malloc(sizeof(char *) * nCmds2Print)) == NULL)
			fatal("out of memory!");

		for (c = cmdtab, i=0, widestName=0; c->c_name != NULL; c++) {
			if (!c->c_hidden || showall) {
				cmdnames[i++] = c->c_name;
				len = (int) strlen(c->c_name);
				if (len > widestName)
					widestName = len;
			}
		}

		if ((cp = getenv("COLUMNS")) == NULL)
			screenColumns = 80;
		else
			screenColumns = atoi(cp);

		widestName += 2;	/* leave room for white-space in between cols. */
		nCols = screenColumns / widestName;
		/* if ((screenColumns % widestName) > 0) nCols++; */
		nRows = nCmds2Print / nCols;
		if ((nCmds2Print % nCols) > 0)
			nRows++;

		(void) sprintf(spec, "%%-%ds", widestName);
		for (i=0; i<nRows; i++) {
			for (j=0; j<nCols; j++) {
				k = nRows*j + i;
				if (k < nCmds2Print)
					(void) printf(spec, cmdnames[k]);
			}
			(void) printf("\n");
		}
		Free(cmdnames);
	} else if (helpall) {
		/* Really intended to debug the help strings. */
		for (c = cmdtab; c->c_name != NULL; c++) {
			cmd_help(c);
			cmd_usage(c);
		}
	} else while (--argc > 0) {
		arg = *++argv;
		c = getcmd(arg);
		if (c == (struct cmd *)-1)
			(void) printf("?Ambiguous help command %s\n", arg);
		else if (c == (struct cmd *)0)
			(void) printf("?Invalid help command %s\n", arg);
		else {
			cmd_help(c);
			cmd_usage(c);
		}
	}
	return NOERR;
}	/* help */


/*
 * If the user wants to, s/he can specify the maximum size of the log
 * file, so it doesn't waste too much disk space.  If the log is too
 * fat, trim the older lines (at the top) until we're under the limit.
 */
void trim_log(void)
{
	FILE				*new, *old;
	struct stat			st;
	long				fat;
	string				tmplogname, str;

	if (logsize <= 0 || *logfname == 0 || stat(logfname, &st) ||
		(old = fopen(logfname, "r")) == NULL)
		return;	/* never trim, or no log */
	fat = st.st_size - logsize;
	if (fat <= 0L) return;	/* log too small yet */
	while (fat > 0L) {
		if (FGets(str, old) == NULL) return;
		fat -= (long) strlen(str);
	}
	/* skip lines until a new site was opened */
	while (1) {
		if (FGets(str, old) == NULL) {
			(void) fclose(old);
			(void) unlink(logfname);
			return;	/* nothing left, start anew */
		}
		if (*str != '\t') break;
	}
	
	/* copy the remaining lines in "old" to "new" */
	(void) Strncpy(tmplogname, logfname);
	tmplogname[strlen(tmplogname) - 1] = 'T';
	if ((new = fopen(tmplogname, "w")) == NULL) {
		(void) PERROR("trim_log", tmplogname);
		return;
	}
	(void) fputs(str, new);
	while (FGets(str, old))
		(void) fputs(str, new);
	(void) fclose(old); (void) fclose(new);
	if (unlink(logfname) < 0)
		PERROR("trim_log", logfname);
	if (rename(tmplogname, logfname) < 0)
		PERROR("trim_log", tmplogname);
}	/* trim_log */




int CheckNewMail(void)
{
	struct stat stbuf;

	if (*mail_path == '\0') return 0;
	if (stat(mail_path, &stbuf) < 0) {	/* cant find mail_path so we'll */
		*mail_path = '\0';				/* never check it again */
		return 0;
	}

	/*
	 * Check if the size is non-zero and the access time is less than
	 * the modify time -- this indicates unread mail.
	 */
	if ((stbuf.st_size != 0) && (stbuf.st_atime <= stbuf.st_mtime)) {
		if (stbuf.st_mtime > mbox_time) {
			(void) printf("%s\n", NEWMAILMESSAGE);
			mbox_time = stbuf.st_mtime;
		}
		return 1;
	}

	return 0;
}	/* CheckNewMail */



#ifdef CURSES
int termcap_get(char **dest, char *attr)
{
	static char area[1024];
	static char *s = area;
	char *buf, *cp;
	int i, result = -1;
	int len = 0;

	*dest = NULL;
	while (*attr != '\0') {
		buf = tgetstr(attr, &s);
		if (buf != NULL && buf[0] != '\0') {
			for (i = 0; (buf[i] <= '9') && (buf[i] >= '0'); )
				i++;
			/* Get rid of the terminal delays, like "$<2>". */
			if ((cp = strstr(&(buf[i]), "$<")) != NULL)
				*cp = 0;
			if (*dest == NULL)
				*dest = (char *)malloc(strlen(&(buf[i])) + 1);
			else
				*dest = (char *)realloc(*dest, len + strlen(&(buf[i])) + 1);
			if (*dest == NULL)
				break;
			(void) strcpy(*dest + len, &(buf[i]));
			len += strlen (&(buf[i]));
		}
		attr += 2;
	}
	if (*dest == NULL)
		*dest = "";
	else
		result = 0;
	return (result);
}	/* termcap_get */


  
void termcap_init(void)
{
	char *term;
#ifdef  SGTTYB
	struct sgttyb ttyb;
	extern short ospeed;
#endif

	if ((term = getenv("TERM")) == NULL) {
		term = "dumb";  /* TAR */
		ansi_escapes = 0;
	}
	if (tgetent(tcbuf,term) != 1) {
		(void) fprintf(stderr,"Can't get termcap entry for terminal [%s]\n", term);
	} else {
		(void) termcap_get(&tcap_normal, "meuese");
		if (termcap_get(&tcap_boldface, "md") < 0) {
			/* Dim-mode is better than nothing... */
			(void) termcap_get(&tcap_boldface, "mh");
		}
		(void) termcap_get(&tcap_underline, "us");
		(void) termcap_get(&tcap_reverse, "so");
		tcl_normal = strlen(tcap_normal);
		tcl_bold = strlen(tcap_boldface);
		tcl_uline = strlen(tcap_underline);
		tcl_rev = strlen(tcap_reverse);
#ifdef  SGTTYB
		if (ioctl(fileno(stdout), TIOCGETP, &ttyb) == 0)
			ospeed = ttyb.sg_ospeed;
#endif
	}

}	/* termcap_init */



static int c_output(int c)
{
	return (putchar(c));
}	/* c_output */




void tcap_put(char *cap)
{
	tputs(cap, 0, c_output);
}	/* tcap_put */

#endif /* CURSES */

/* eof main.c */

