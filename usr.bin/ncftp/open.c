/* open.c */

/*  $RCSfile: open.c,v $
 *  $Revision: 1.1 $
 *  $Date: 93/07/09 11:27:07 $
 */

#include "sys.h"

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/ftp.h>

#include <errno.h>

#include "util.h"
#include "open.h"
#include "cmds.h"
#include "ftp.h"
#include "ftprc.h"
#include "main.h"
#include "defaults.h"
#include "copyright.h"

/* open.c globals */
int					remote_is_unix;		/* TRUE if remote host is unix. */
int					auto_binary = dAUTOBINARY;
int					anon_open = dANONOPEN;
										/* Anonymous logins by default? */
int					connected = 0;		/* TRUE if connected to server */
										/* If TRUE, set binary each connection. */
int					www = 0;		/* TRUE	if use URL */
Hostname			hostname;			/* Name of current host */
RemoteSiteInfo		gRmtInfo;
#ifdef GATEWAY
string				gateway;			/* node name of firewall gateway */
string				gate_login;			/* login at firewall gateway */
#endif

/* open.c externs */
extern char					*reply_string, *line, *Optarg, *margv[];
extern int					Optind, margc, verbose, macnum;
extern long					eventnumber;
extern struct servent		serv;
extern FILE					*cout;
extern string				anon_password;

/* Given a pointer to an OpenOptions (structure containing all variables
 * that can be set from the command line), this routine makes sure all
 * the variables have valid values by setting them to their defaults.
 */
 
void InitOpenOptions(OpenOptions *openopt)
{
	/* How do you want to open a site if neither -a or -u are given?
	 * anon_open is true (default to anonymous login), unless
	 * defaults.h was edited to set dANONOPEN to 0 instead.
	 */
	openopt->openmode = anon_open ? openImplicitAnon : openImplicitUser;

	/* Normally you don't want to ignore the entry in your netrc. */
	openopt->ignore_rc = 0;

	/* Set the default delay if the user specifies redial mode without
	 * specifying the redial delay.
	 */
	openopt->redial_delay = dREDIALDELAY;

	/* Normally, you only want to try once. If you specify redial mode,
	 * this is changed.
	 */
	openopt->max_dials = 1;
	
	/* You don't want to cat the file to stdout by default. */
	openopt->ftpcat = NO_FTPCAT;

	/* Setup the port number to try. */
#ifdef dFTP_PORT
	/* If dFTP_PORT is defined, we use a different port number by default
	 * than the one supplied in the servent structure.
	 */
	openopt->port = dFTP_PORT;
	/* Make sure the correct byte order is supplied! */
	openopt->port = htons(openopt->port);
#else
	/* Use the port number supplied by the operating system's servent
	 * structure.
	 */
	openopt->port = serv.s_port;
#endif

	/* We are not in colon-mode (yet). */
	openopt->colonmodepath[0] = 0;

	/* Set the hostname to a null string, since there is no default host. */
	openopt->hostname[0] = 0;
	
	/* Set the opening directory path to a null string. */
	openopt->cdpath[0] = 0;
}	/* InitOpenOptions */




/* This is responsible for parsing the command line and setting variables
 * in the OpenOptions structure according to the user's flags.
 */

int GetOpenOptions(int argc, char **argv, OpenOptions *openopt)
{
	int                                     opt;
	char				*cp, *hostp, *cpath;

	/* First setup the openopt variables. */
	InitOpenOptions(openopt);

	/* Tell Getopt() that we want to start over with a new command. */
	Getopt_Reset();
	while ((opt = Getopt(argc, argv, "aiup:rd:g:cm")) >= 0) {
		switch (opt) {		
			case 'a':
				/* User wants to open anonymously. */
				openopt->openmode = openExplicitAnon;
				break;
				
			case 'u':
				/* User wants to open with a login and password. */
				openopt->openmode = openExplicitUser;
				break;
				
			case 'i':
				/* User wants to ignore the entry in the netrc. */
				openopt->ignore_rc = 1;
				break;
				
			case 'p':
				/* User supplied a port number different from the default
				 * ftp port.
				 */
				openopt->port = atoi(Optarg);
				if (openopt->port <= 0) {
					/* Probably never happen, but just in case. */
					(void) printf("%s: bad port number (%s).\n", argv[0], Optarg);
					goto usage;
				}
				/* Must ensure that the port is in the correct byte order! */
				openopt->port = htons(openopt->port);
				break;
				
			case 'd':
				/* User supplied a delay (in seconds) that differs from
				 * the default.
				 */
				openopt->redial_delay = atoi(Optarg);
				break;
				
			case 'g':
				/* User supplied an upper-bound on the number of redials
				 * to try.
				 */
				openopt->max_dials = atoi(Optarg);
				break;

			case 'r':
				openopt->max_dials = -1;
				break;

			case 'm':
				/* ftpcat mode is only available from your shell command-line,
				 * not from the ncftp shell.  Do that yourself with 'more zz'.
				 */
				if (eventnumber == 0L) {
					/* If eventnumber is zero, then we were called directly
					 * from main(), and before the ftp shell has started.
					 */
					openopt->ftpcat = FTPMORE;
					/* ftpcat mode is really ftpmore mode. */
					break;
				} else {
					fprintf(stderr,
"You can only use this form of colon-mode (-m) from your shell command line.\n\
Try 'ncftp -m wuarchive.wustl.edu:/README'\n");
					goto usage;
				}
				/* break; */

			case 'c':
				/* ftpcat mode is only available from your shell command-line,
				 * not from the ncftp shell.  Do that yourself with 'get zz -'.
				 */
				if (eventnumber == 0L) {
					/* If eventnumber is zero, then we were called directly
					 * from main(), and before the ftp shell has started.
					 */
					openopt->ftpcat = FTPCAT;
					break;
				} else {
					fprintf(stderr,
"You can only use ftpcat/colon-mode from your shell command line.\n\
Try 'ncftp -c wuarchive.wustl.edu:/README > file.'\n");
					goto usage;
				}
				/* break; */
				
			default:
			usage:
				return USAGE;
		}
	}

	if (argv[Optind] == NULL) {
		/* No host was supplied.  Print out the list of sites we know
		 * about and ask the user for one.
		 */
		PrintSiteList();
		(void) Gets("(site to open) ", openopt->hostname, sizeof(openopt->hostname));
		/* Make sure the user just didn't hit return, in which case we
		 * just give up and go home.
		 */
		if (openopt->hostname[0] == 0)
			goto usage;
	} else {
		/* The user gave us a host to open.
		 *
		 * First, check to see if they gave us a colon-mode path
		 * along with the hostname.  We also understand a WWW path,
		 * like "ftp://bang.nta.no/pub/fm2html.v.0.8.4.tar.Z".
		 */
		hostp = argv[Optind];
		cpath = NULL;
		if ((cp = index(hostp, ':')) != NULL) {
			*cp++ = '\0';
			cpath = cp;
			www = 0;	/* Is 0 or 1, depending on the type of path. */
			if ((*cp == '/') && (cp[1] == '/')) {
				/* First make sure the path was intended to be used
				 * with ftp and not one of the other URLs.
				 */
				if (strcmp(argv[Optind], "ftp")) {
					fprintf(
						stderr,
						"Bad URL '%s' -- WWW paths must be prefixed by 'ftp://'.\n",
						argv[Optind]
					);
					goto usage;
				}

				cp += 2;
				hostp = cp;
				cpath = NULL;	/* It could have been ftp://hostname only. */

				if ((cp = index(hostp, '/')) != NULL) {
					*cp++ = '\0';
					cpath = cp;
				}
				www = 1;
			}
			if (cpath != NULL) {
				(void) Strncpy(openopt->colonmodepath, www ? "/" : "");
				(void) Strncat(openopt->colonmodepath, cpath);
				dbprintf("Colon-Mode Path = '%s'\n", openopt->colonmodepath);
			}
		}	
		(void) Strncpy(openopt->hostname, hostp);
		dbprintf("Host = '%s'\n", hostp);
	}
	return NOERR;
}	/* GetOpenOptions */




/* This examines the format of the string stored in the hostname
 * field of the OpenOptions, and sees if has to strip out a colon-mode
 * pathname (to store in the colonmodepath field).  Since colon-mode
 * is run quietly (without any output being generated), we init the
 * login_verbosity variable here to quiet if we are running colon-mode.
 */
int CheckForColonMode(OpenOptions *openopt, int *login_verbosity)
{
	/* Usually the user doesn't supply hostname in colon-mode format,
	 * and wants to interactively browse the remote host, so set the
	 * login_verbosity to whatever it is set to now.
	 */
	*login_verbosity = verbose;

	if (openopt->colonmodepath[0] != 0) {
		/* But if the user does use colon-mode, we want to do our business
		 * and leave, without all the login messages, etc., so set
		 * login_verbosity to quiet so we won't print anything until
		 * we finish.  Colon-mode can be specified from the shell command
		 * line, so we would like to be able to execute ncftp as a one
		 * line command from the shell without spewing gobs of output.
		 */
		*login_verbosity = V_QUIET;
	} else if (openopt->ftpcat != 0) {
		/* User specified ftpcat mode, but didn't supply the host:file. */
		(void) fprintf(stderr, "You didn't use colon mode correctly.\n\
If you use -c or -m, you need to do something like this:\n\
	ncftp -c wuarchive.wustl.edu:/pub/README (to cat this file to stdout).\n");
		return USAGE;
	}
	return NOERR;
}	/* CheckForColonMode */




/* All this short routine does is to hookup a socket to either the
 * remote host or the firewall gateway host.
 */
int HookupToRemote(OpenOptions *openopt)
{
	int hErr;

#ifdef GATEWAY
	/* Try connecting to the gateway host. */
	if (*gateway) {
		hErr = hookup(gateway, openopt->port);
		(void) Strncpy(hostname, openopt->hostname);
	} else
#endif
		hErr = hookup(openopt->hostname, openopt->port);
	
	return hErr;
}	/* HookupToRemote */




void CheckRemoteSystemType(int force_binary)
{
	int tmpverbose;
	char *cp, c;

	/* As of this writing, UNIX is pretty much standard. */
	remote_is_unix = 1;

	/* Do a SYSTem command quietly. */
	tmpverbose = verbose;
	verbose = V_QUIET;
	if (command("SYST") == COMPLETE) {
		if (tmpverbose == V_VERBOSE) {		
			/* Find the system type embedded in the reply_string,
			 * and separate it from the rest of the junk.
			 */
			cp = index(reply_string+4, ' ');
			if (cp == NULL)
				cp = index(reply_string+4, '\r');
			if (cp) {
				if (cp[-1] == '.')
					cp--;
				c = *cp;
				*cp = '\0';
			}

			(void) printf("Remote system type is %s.\n",
				reply_string+4);
			if (cp)
				*cp = c;
		}
		remote_is_unix = !strncmp(reply_string + 4, "UNIX", (size_t) 4);
	}

	/* Set to binary mode if any of the following are true:
	 * (a) The user has auto-binary set;
	 * (b) The user is using colon-mode (force_binary);
	 * (c) The reply-string from SYST said it was UNIX with 8-bit chars.
	 */
	if (auto_binary || force_binary
		|| !strncmp(reply_string, "215 UNIX Type: L8", (size_t) 17)) {
		(void) _settype("binary");
		if (tmpverbose > V_TERSE)
		    (void) printf("Using binary mode to transfer files.\n");
	}

	/* Print a warning for that (extremely) rare Tenex machine. */
	if (tmpverbose >= V_ERRS && 
	    !strncmp(reply_string, "215 TOPS20", (size_t) 10)) {
		(void) _settype("tenex");
		(void) printf("Using tenex mode to transfer files.\n");
	}
	verbose = tmpverbose;
}	/* CheckRemoteSystemType */



/* This is called if the user opened the host with a file appended to
 * the host's name, like "wuarchive.wustl.edu:/pub/readme," or
 * "wuarchive.wustl.edu:/pub."  In the former case, we open wuarchive,
 * and fetch "readme."  In the latter case, we open wuarchive, then set
 * the current remote directory to "/pub."  If we are fetching a file,
 * we can do some other tricks if "ftpcat mode" is enabled.  This mode
 * must be selected from your shell's command line, and this allows you
 * to use the program as a one-liner to pipe a remote file into something,
 * like "ncftp -c wu:/pub/README | wc."  If the user uses ftpcat mode,
 * the program immediately quits instead of going into it's own command
 * shell.
 */
void ColonMode(OpenOptions *openopt)
{
	int tmpverbose;
	int cmdstatus;

	/* How do we tell if colonmodepath is a file or a directory?
	 * We first try cd'ing to the path first.  If we can, then it
	 * was a directory.  If we could not, we'll assume it was a file.
	 */

	/* Shut up, so cd won't print 'foobar: Not a directory.' */
	tmpverbose = verbose;
	verbose = V_QUIET;

	/* If we are using ftpcat|more mode, or we couldn't cd to the
	 * colon-mode path (then it must be a file to fetch), then
	 * we need to fetch a file.
	 */
	if (openopt->ftpcat || ! _cd(openopt->colonmodepath)) {
		/* We call the appropriate fetching routine, so we have to
		 * have the argc and argv set up correctly.  To do this,
		 * we just make an entire command line, then let makeargv()
		 * convert it to argv/argc.
		 */
		if (openopt->ftpcat == FTPCAT)
			(void) sprintf(line, "get %s -", openopt->colonmodepath);
		else if (openopt->ftpcat == FTPMORE)
			(void) sprintf(line, "more %s", openopt->colonmodepath);
		else {
			/* Regular colon-mode, where we fetch the file, putting the
			 * copy in the current local directory.
			 */
			(void) sprintf(line, "mget %s", openopt->colonmodepath);
		}
		makeargv();

		/* Turn on messaging if we aren't catting. */
		if (openopt->ftpcat == 0)
			verbose = tmpverbose;
		
		/* get() also handles 'more'. */
		if (openopt->ftpcat)
			cmdstatus = get(margc, margv);
		else
			cmdstatus = mget(margc, margv);

		/* If we were invoked from the command line, quit
		 * after we got this file.
		 */
		if (eventnumber == 0L) {
			(void) quit(cmdstatus == CMDERR ? -1 : 0, NULL);
		}
	}
	verbose = tmpverbose;
}	/* ColonMode */




/* Given a properly set up OpenOptions, we try connecting to the site,
 * redialing if necessary, and do some initialization steps so the user
 * can send commands.
 */
int Open(OpenOptions *openopt)
{
	int					hErr;
	int					dials;
	char				*ruser, *rpass, *racct;
	int					siteInRC;
	char				*user, *pass, *acct;	
	int					login_verbosity, oldv;
	int				result = CMDERR;

	macnum = 0;	 /* Reset macros. */

	/* If the hostname supplied is in the form host.name.str:/path/file,
	 * then colon mode was used, and we need to fix the hostname to be
	 * just the hostname, copy the /path/file to colonmode path, and init
	 * the login_verbosity variable.
	 */
	if (CheckForColonMode(openopt, &login_verbosity) == USAGE)
		return USAGE;

	/* If the hostname supplied was an abbreviation, such as just
	 * "wu" (wuarchive.wustl.edu), look through the list of sites
	 * we know about and get the whole name.  We also would like
	 * the path we want to start out in, if it is available.
	 */
	GetFullSiteName(openopt->hostname, openopt->cdpath);

#ifdef GATEWAY
	/* Make sure the gateway host name is a full name and not an
	 * abbreviation.
	 */
	if (*gateway)
		GetFullSiteName(gateway, NULL);
#endif

	ruser = rpass = racct = NULL;
	/* This also loads the init macro. */
	siteInRC = ruserpass2(openopt->hostname, &ruser, &rpass, &racct);
	if (ISANONOPEN(openopt->openmode)) {
		user = "anonymous";
		pass = anon_password;
	} else {
		user = NULL;
		pass = NULL;
	}
	acct = NULL;
	
	if (siteInRC && !openopt->ignore_rc) {
		acct = racct;
		if (ruser != NULL) {
			/* We were given a username.  If we were given explicit
			 * instructions from the command line, follow those and
			 * ignore what the RC had.  Otherwise if no -a or -u
			 * was specified, we use whatever was in the RC.
			 */
			if (ISIMPLICITOPEN(openopt->openmode)) {
				user = ruser;
				pass = rpass;
			}
		}		
	}

	for (
			dials = 0;
			openopt->max_dials < 0 || dials < openopt->max_dials;
			dials++)
	{
		if (dials > 0) {
			/* If this is the second dial or higher, sleep a bit. */
			(void) sleep(openopt->redial_delay);
			(void) fprintf(stderr, "Retry Number: %d\n", dials + 1);
		}

		if ((hErr = HookupToRemote(openopt)) == -2)	
			/* Recoverable, so we can try re-dialing. */
			continue;
		else if (hErr == NOERR) {
			/* We were hookup'd successfully. */
			connected = 1;

		oldv = verbose;  verbose = login_verbosity;
		
#ifdef GATEWAY
			if (*gateway) {
				if ((Login(
					user,
					pass,
					acct,
					(!openopt->ignore_rc && !openopt->colonmodepath[0])
				) != NOERR) || cout == NULL)
					goto nextdial;		/* error! */
			}
#endif

#ifdef GATEWAY
			if (!*gateway) {
#endif
				/* We don't want to run the init macro for colon-mode. */
				if ((Login(
						user,
						pass,
						acct,
						(!openopt->ignore_rc && !openopt->colonmodepath[0])
					) != NOERR) || cout == NULL)
				{
					goto nextdial;		/* error! */
				}
#ifdef GATEWAY
			}
#endif

			verbose = oldv;

			/* We need to check for unix and see if we should set binary
			 * mode automatically.
			 */
			CheckRemoteSystemType(openopt->colonmodepath[0] != (char)0);

			if (openopt->colonmodepath[0]) {
				ColonMode(openopt);
			} else if (openopt->cdpath[0]) {
				/* If we didn't have a colon-mode path, we try setting
				 * the current remote directory to cdpath.  cdpath is
				 * usually the last directory we were in the previous
				 * time we called this site.
				 */
				(void) _cd(openopt->cdpath);
			} else {
				/* Freshen 'cwd' variable for the prompt. 
				 * We have to do atleast one 'cd' so our variable
				 * cwd (which is saved by _cd()) is set to something
				 * valid.
				 */
				(void) _cd(NULL);
			}
			result = NOERR;
			break;	/* we are connected, so break the redial loop. */
			/* end if we are connected */
		} else {
			/* Irrecoverable error, so don't bother redialing. */
			/* The error message should have already been printed
			 * from Hookup().
			 */
			break;
		}
nextdial:
		disconnect(0, NULL);
		continue;	/* Try re-dialing. */
	}
	return (result);
}	/* Open */



/* This stub is called by our command parser. */
int cmdOpen(int argc, char **argv)
{
	OpenOptions			openopt;
	int				result = NOERR;

	/* If there is already a site open, close that one so we can
	 * open a new one.
	 */
	if (connected && NOT_VQUIET && hostname[0]) {
		(void) printf("Closing %s...\n", hostname);
		(void) disconnect(0, NULL);
	}

	/* Reset the remote info structure for the new site we want to open.
	 * Assume we have these properties until we discover otherwise.
	 */
	gRmtInfo.hasSIZE = 1;
	gRmtInfo.hasMDTM = 1;

	if ((GetOpenOptions(argc, argv, &openopt) == USAGE) ||
		((result = Open(&openopt)) == USAGE))
		return USAGE;
	/* Return an error if colon-mode/URL didn't work. */
	return (openopt.colonmodepath[0] != '\0' ? result : NOERR);
}	/* cmdOpen */

/* eof open.c */
