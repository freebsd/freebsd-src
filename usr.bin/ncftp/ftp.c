/* ftp.c */

/*  $RCSfile: ftp.c,v $
 *  $Revision: 14020.12 $
 *  $Date: 93/07/09 11:30:28 $
 */

#include "sys.h"

#include <setjmp.h>
#include <sys/stat.h>
#include <sys/file.h>

#ifndef AIX /* AIX-2.2.1 declares utimbuf in unistd.h */
#ifdef NO_UTIMEH
struct	utimbuf {time_t actime; time_t modtime;};
#else
#	include <utime.h>
#endif
#endif /*AIX*/

#ifdef SYSLOG
#	include <syslog.h>
#endif

/* You may need this for declarations of fd_set, etc. */
#ifdef SYSSELECTH
#   include <sys/select.h>
#else
#ifdef STRICT_PROTOS
#ifndef Select
extern int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
#endif
#endif
#endif

#include <netinet/in.h>
#include <arpa/ftp.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <signal.h>
#include <errno.h>
#ifdef NET_ERRNO_H
#       include <net/errno.h>
#endif
#include <netdb.h>
#include <fcntl.h>
#include <pwd.h>
#include <ctype.h>
#include "util.h"
#include "ftp.h"
#include "cmds.h"
#include "main.h"
#include "ftprc.h"
#include "getpass.h"
#include "defaults.h"
#include "copyright.h"

/* ftp.c globals */
struct				sockaddr_in hisctladdr;
struct				sockaddr_in data_addr;
int					data = -1;
int					abrtflag = 0;
struct sockaddr_in	myctladdr;
FILE				*cin = NULL, *cout = NULL;
char				*reply_string = NULL;
static char         pad3a[8] = "Pad 3a";	/* For SunOS :-( */
jmp_buf				sendabort;
static char         pad3b[8] = "Pad 3b";
jmp_buf				recvabort;
static char         pad3c[8] = "Pad 3c";
int					progress_meter = dPROGRESS;
int					cur_progress_meter;
int					sendport = -1;		/* use PORT cmd for each data connection */
int					using_pasv;
int					code;				/* return/reply code for ftp command */
string				indataline;			
int     			cpend;				/* flag: if != 0, then pending server reply */
char				*xferbuf;			/* buffer for local and remote I/O */
size_t				xferbufsize;		/* size in bytes, of the transfer buffer. */
long				next_report;
long				bytes;
long				now_sec;
long				file_size;
struct timeval		start, stop;
int					buffer_only = 0;	/* True if reading into redir line
										 * buffer only (not echoing to
										 * stdout).
										 */

/* ftp.c externs */
extern FILE					*logf;
extern string				anon_password;
extern longstring			cwd, lcwd;
extern Hostname				hostname;
extern int					verbose, debug, macnum, margc;
extern int					curtype, creating, toatty;
extern int					options, activemcmd, paging;
extern int					ansi_escapes, logged_in, macnum;
extern char					*line, *margv[];
extern char					*tcap_normal, *tcap_boldface;
extern char					*tcap_underline, *tcap_reverse;
extern struct userinfo		uinfo;
extern struct macel			macros[];
extern struct lslist		*lshead, *lstail;
extern int					is_ls;
extern int					passivemode;
extern int					restricted_data_ports;

#ifdef GATEWAY
extern string				gateway;
extern string				gate_login;
#endif


#ifdef _POSIX_SOURCE
FILE *safeopen(int s, char *lmode){
  FILE *file;

  setreuid(geteuid(),getuid());
  setregid(getegid(),getgid());
  file=fdopen(s, lmode);
  setreuid(geteuid(),getuid());
  setregid(getegid(),getgid());
  return(file);
}
#else
#define safeopen fdopen
#endif


int hookup(char *host, unsigned int port)
{
	register struct hostent *hp = 0;
	int s, len, hErr = -1;
	string errstr;
	char **curaddr = NULL;

	bzero((char *)&hisctladdr, sizeof (hisctladdr));
#ifdef BAD_INETADDR
	hisctladdr.sin_addr = inet_addr(host);
#else
 	hisctladdr.sin_addr.s_addr = inet_addr(host);
#endif
	if (hisctladdr.sin_addr.s_addr != -1) {
		hisctladdr.sin_family = AF_INET;
		(void) Strncpy(hostname, host);
	} else {
		hp = gethostbyname(host);
		if (hp == NULL) {
#ifdef HERROR
			extern int h_errno;
			if (h_errno == HOST_NOT_FOUND)
				(void) printf("%s: unknown host\n", host);
			else (void) fprintf(stderr, "%s: gethostbyname herror (%d):  ",
				host, h_errno);
			herror(NULL);
#else
			(void) printf("%s: unknown host\n", host);
#endif
			goto done;
		}
		hisctladdr.sin_family = hp->h_addrtype;
		curaddr = hp->h_addr_list;
		bcopy(*curaddr, (caddr_t)&hisctladdr.sin_addr, hp->h_length);
		(void) Strncpy(hostname, hp->h_name);
	}
	s = socket(hisctladdr.sin_family, SOCK_STREAM, 0);
	if (s < 0) {
		PERROR("hookup", "socket");
		goto done;
	}
	hisctladdr.sin_port = port;
#ifdef SOCKS
	while (Rconnect(s, (struct sockaddr *) &hisctladdr, (int) sizeof (hisctladdr)) < 0) {
#else
	while (Connect(s, &hisctladdr, sizeof (hisctladdr)) < 0) {
#endif
		if (curaddr != NULL) {
		curaddr++;
		if (*curaddr != (char *)0) {
			(void) sprintf(errstr, "connect error to address %s",
				inet_ntoa(hisctladdr.sin_addr));
			PERROR("hookup", errstr);
			bcopy(*curaddr, (caddr_t)&hisctladdr.sin_addr, hp->h_length);
			dbprintf("Trying %s...\n", inet_ntoa(hisctladdr.sin_addr));
			(void) close(s);
			s = socket(hisctladdr.sin_family, SOCK_STREAM, 0);
			if (s < 0) {
				PERROR("hookup", "socket");
				goto done;
			}
			continue;
			}
		}
		PERROR("hookup", host);
		switch (errno) {
			case ENETDOWN:
			case ENETUNREACH:
			case ECONNABORTED:
			case ETIMEDOUT:
			case ECONNREFUSED:
			case EHOSTDOWN:
				hErr = -2;	/* we can re-try later. */
		}
		goto bad;
	}
	len = sizeof (myctladdr);
	if (Getsockname(s, (char *)&myctladdr, &len) < 0) {
		PERROR("hookup", "getsockname");
		goto bad;
	}
	cin = safeopen(s, "r");
	cout = safeopen(dup(s), "w");
	if (cin == NULL || cout == NULL) {
		(void) fprintf(stderr, "ftp: safeopen failed.\n");
		close_streams(0);
		goto bad;
	}
	if (IS_VVERBOSE)
		(void) printf("Connected to %s.\n", hostname);
#ifdef IPTOS_LOWDELAY /* control is interactive */
#ifdef IP_TOS
	{
		int nType = IPTOS_LOWDELAY;
		if (setsockopt(s, IPPROTO_IP, IP_TOS,
			       (char *) &nType, sizeof(nType)) < 0) {
			PERROR("hookup", "setsockopt(IP_TOS)");
		}
	}
#endif
#endif
	if (getreply(0) > 2) { 	/* read startup message from server */
		close_streams(0);
		if (code == 421)
			hErr = -2;	/* We can try again later. */
		goto bad;
	}
#ifdef SO_OOBINLINE
	{
	int on = 1;

	if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *) &on, sizeof(on))
		< 0 && debug) {
			PERROR("hookup", "setsockopt(SO_OOBINLINE)");
		}
	}
#endif /* SO_OOBINLINE */

	hErr = 0;
	using_pasv = passivemode;		/* Re-init for each new connection. */
	goto done;

bad:
	(void) close(s);
	if (cin != NULL)
		(void) fclose(cin);
	if (cout != NULL)
		(void) fclose(cout);
	cin = cout = NULL;
done:
	return (hErr);
}	/* hookup */


/* This registers the user's username, password, and account with the remote
 * host which validates it.  If we get on, we also do some other things, like
 * enter a log entry and execute the startup macro.
 */
int Login(char *userNamePtr, char *passWordPtr, char *accountPtr, int doInit)
{
	string userName;
	string str;
	int n;
	int sentAcct = 0;
	int userWasPrompted = 0;
	int result = CMDERR;
	time_t now;

	if (userNamePtr == NULL) {
		/* Prompt for a username. */
		(void) sprintf(str, "Login Name (%s): ", uinfo.username);
		++userWasPrompted;
		if (Gets(str, userName, sizeof(userName)) == NULL)
			goto done;
		else if (userName[0]) {
			/* User didn't just hit return. */
			userNamePtr = userName;
		} else {
			/*
			 * User can hit return if he wants to enter his username
			 * automatically.
			 */
			if (*uinfo.username != '\0')
				userNamePtr = uinfo.username;
			else
				goto done;
		}
	}

#ifdef GATEWAY
	if (*gateway)
		(void) sprintf(str, "USER %s@%s",
				(*gate_login ? gate_login : dGATEWAY_LOGIN),
				hostname);
	else
#endif
		(void) sprintf(str, "USER %s", userNamePtr);

	/* Send the user name. */
	n = command(str);
	if (n == CONTINUE) {
		if (passWordPtr == NULL) {
			if (((strcmp("anonymous", userName) == 0) ||
			     (strcmp("ftp", userName) == 0)) && (*anon_password != '\0'))
				passWordPtr = anon_password;
			else {
				/* Prompt for a password. */
				++userWasPrompted;
				passWordPtr = Getpass("Password:");
			}
		}

		/* The remote site is requesting us to send the password now. */
		(void) sprintf(str, "PASS %s", passWordPtr);
		n = command(str);
		if (n == CONTINUE) {
			/* The remote site is requesting us to send the account now. */
			if (accountPtr == NULL) {
				/* Prompt for a username. */
				(void) sprintf(str, "ACCT %s", Getpass("Account:"));
				++userWasPrompted;
			} else {
				(void) sprintf(str, "ACCT %s", accountPtr);
			}
			++sentAcct;	/* Keep track that we've sent the account already. */
			n = command(str);
		}
	}

	if (n != COMPLETE) {
		(void) printf("Login failed.\n");
		goto done;
	}
	
	/* If you specified an account, and the remote-host didn't request it
	 * (maybe it's optional), we will send the account information.
	 */
	if (!sentAcct && accountPtr != NULL) {
		(void) sprintf(str, "ACCT %s", accountPtr);
		(void) command(str);
	}

	/* See if remote host dropped connection.  Some sites will let you log
	 * in anonymously, only to tell you that they already have too many
	 * anon users, then drop you.  We do a no-op here to see if they've
	 * ditched us.
	 */
	n = quiet_command("NOOP");
	if (n == TRANSIENT)
		goto done;

#ifdef SYSLOG
	syslog(LOG_INFO, "%s connected to %s as %s.",
		   uinfo.username, hostname, userNamePtr);
#endif

	/* Save which sites we opened to the user's logfile. */
	if (logf != NULL) {
		(void) time(&now);
		(void) fprintf(logf, "%s opened at %s",
					   hostname,
					   ctime(&now));
	}

	/* Let the user know we are logged in, unless he was prompted for some
	 * information already.
	 */
	if (!userWasPrompted)
		if (NOT_VQUIET)
			(void) printf("Logged into %s.\n", hostname);

	if ((doInit) && (macnum > 0)) {
		/* Run the startup macro, if any. */
		/* If macnum is non-zero, the init macro was defined from
		 * ruserpass.  It would be the only macro defined at this
		 * point.
		 */
		(void) strcpy(line, "$init");
		makeargv();
		(void) domacro(margc, margv);
	}

	_cd(NULL);	/* Init cwd variable. */

	result = NOERR;
	logged_in = 1;

done:
	return (result);
}									   /* Login */



/*ARGSUSED*/
void cmdabort SIG_PARAMS
{
	(void) printf("\n");
	(void) fflush(stdout);
	abrtflag++;
}	/* cmdabort */




int CommandWithFlags(char *cmd, int flags)
{
	int r;
	Sig_t oldintr;
	string str;

	if (cmd == NULL) {
		/* Should never happen; bug if it does. */
		PERROR("command", "NULL command");
		return (-1);
	}
	abrtflag = 0;
	if (debug) {
		if (strncmp(cmd, "PASS", (size_t)4) == 0)
			dbprintf("cmd: \"PASS ********\"\n");
		else
			dbprintf("cmd: \"%s\" (length %d)\n", cmd, (int) strlen(cmd));
	}
	if (cout == NULL) {
		(void) sprintf(str, "%s: No control connection for command", cmd);
		PERROR("command", str);
		return (0);
	}
	oldintr = Signal(SIGINT, /* cmdabort */ SIG_IGN);

	/* Used to have BROKEN_MEMCPY tested here. */
	if (cout != NULL)
		(void) fprintf(cout, "%s\r\n", cmd);

	(void) fflush(cout);
	cpend = 1;
	r = (flags == WAIT_FOR_REPLY) ?
			(getreply(strcmp(cmd, "QUIT") == 0)) : PRELIM;
	if (abrtflag && oldintr != SIG_IGN && oldintr != NULL)
		(*oldintr)(0);
	(void) Signal(SIGINT, oldintr);
	return(r);
}	/* CommandWithFlags */



/* This stub runs 'CommandWithFlags' above, telling it to wait for
 * reply after the command is sent.
 */
int command(char *cmd)
{
	return (CommandWithFlags(cmd, WAIT_FOR_REPLY));
}	/* command */

/* This stub runs 'CommandWithFlags' above, telling it to NOT wait for
 * reply after the command is sent.
 */
int command_noreply(char *cmd)
{
	return(CommandWithFlags(cmd, DONT_WAIT_FOR_REPLY));
}	/* command */



int quiet_command(char *cmd)
{
	register int oldverbose, result;
	
	oldverbose = verbose;
	verbose = debug ? V_VERBOSE : V_QUIET;
	result = command(cmd);
	verbose = oldverbose;
	return (result);
}	/* quiet_command */




int verbose_command(char *cmd)
{
	register int oldverbose, result;
	
	oldverbose = verbose;
	verbose = V_VERBOSE;
	result = command(cmd);
	verbose = oldverbose;
	return (result);
}	/* quiet_command */




int getreply(int expecteof)
{
	register int c, n = 0;
	int dig;
	char *cp, *end, *dp;
	int thiscode, originalcode = 0, continuation = 0;
	Sig_t oldintr;

	if (cin == NULL)
		return (-1);
	/* oldintr = Signal(SIGINT, SIG_IGN); */
	oldintr = Signal(SIGINT, cmdabort);
	end = reply_string + RECEIVEDLINELEN - 2;
	for (;abrtflag==0;) {
		dig = n = thiscode = code = 0;
		cp = reply_string;
		for (;abrtflag==0;) {
			c = fgetc(cin);
			if (c == IAC) {     /* handle telnet commands */
				switch (c = fgetc(cin)) {
				case WILL:
				case WONT:
					c = fgetc(cin);
					(void) fprintf(cout, "%c%c%c",IAC,DONT,c);
					(void) fflush(cout);
					break;
				case DO:
				case DONT:
					c = fgetc(cin);
					(void) fprintf(cout, "%c%c%c",IAC,WONT,c);
					(void) fflush(cout);
					break;
				default:
					break;
				}
				continue;
			}
			dig++;
			if (c == EOF) {
				if (expecteof) {
					(void) Signal(SIGINT, oldintr);
					code = 221;
					return (0);
				}
				lostpeer(0);
				if (NOT_VQUIET) {
					(void) printf("421 Service not available, remote server has closed connection\n");
					(void) fflush(stdout);
				}
				code = 421;
				return(4);
			}
			if (cp < end && c != '\r')
				*cp++ = c;

			if (c == '\n')
				break;
			if (dig < 4 && isdigit(c))
				code = thiscode = code * 10 + (c - '0');
			else if (dig == 4 && c == '-') {
				if (continuation)
					code = 0;
				continuation++;
			}
			if (n == 0)
				n = c;
		}	/* end for(;;) #2 */
		
		*cp = '\0';
		dbprintf("rsp: %s", reply_string);

		switch (verbose) {
			case V_QUIET:
				/* Don't print anything. */
				break;
			case V_ERRS:
				if (n == '5') {
					dp = reply_string;
					goto stripCode;
				}
				break;	
			case V_IMPLICITCD:
			case V_TERSE:
				dp = NULL;
				if (n == '5' && verbose == V_TERSE)
					dp = reply_string;
				else {
					switch (thiscode) {
						case 230:
						case 214:
						case 331:
						case 332:
						case 421:	/* For ftp.apple.com, etc. */
							dp = reply_string;
							break;
						case 220:
							/*
							 * Skip the foo FTP server ready line.
							 */
							if (strstr(reply_string, "ready.") == NULL)
								dp = reply_string;
							break;
						case 250:
							/*
							 * Print 250 lines if they aren't
							 * "250 CWD command successful."
							 */
							if (strncmp(reply_string + 4, "CWD ", (size_t) 4))
								dp = reply_string;
					}
				}
				if (dp == NULL) break;			
stripCode:
				/* Try to strip out the code numbers, etc. */
				if (isdigit(*dp++) && isdigit(*dp++) && isdigit(*dp++)) {
					if (*dp == ' ' || *dp == '-') {
						dp++;
						if (*dp == ' ') dp++;
					} else dp = reply_string;			
				} else {
					int spaces;
					dp = reply_string;
					for (spaces = 0; spaces < 4; ++spaces)
						if (dp[spaces] != ' ')
							break;
					if (spaces == 4)
						dp += spaces;
				}					
				goto printLine;
			case V_VERBOSE:
				dp = reply_string;
printLine:		(void) fputs(dp, stdout);
		}	/* end switch */

		if (continuation && code != originalcode) {
			if (originalcode == 0)
				originalcode = code;
			continue;
		}
		if (n != '1')
			cpend = 0;
		(void) Signal(SIGINT,oldintr);
		if (code == 421 || originalcode == 421)
			lostpeer(0);
		if (abrtflag && oldintr != cmdabort && oldintr != SIG_IGN && oldintr)
			(*oldintr)(0);
		break;
	}	/* end for(;;) #1 */
	return (n - '0');
}	/* getreply */




static int empty(struct fd_set *mask, int sec)
{
	struct timeval t;

	t.tv_sec = (long) sec;
	t.tv_usec = 0;

	return(Select(32, mask, NULL, NULL, &t));
}	/* empty */




static void tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{
	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}	/* tvsub */


/* Variables private to progress_report code. */
static int barlen;
static long last_dot;
static int dots;

int start_progress(int sending, char *local)
{
	long s;
	char spec[64];

	cur_progress_meter = toatty ? progress_meter : 0;
	if ((cur_progress_meter > pr_last) || (cur_progress_meter < 0))
		cur_progress_meter = dPROGRESS;
	if ((file_size <= 0) && ((cur_progress_meter == pr_percent) || (cur_progress_meter == pr_philbar) || (cur_progress_meter == pr_last)))
		cur_progress_meter = pr_kbytes;
	if (!ansi_escapes && (cur_progress_meter == pr_philbar))
		cur_progress_meter = pr_dots;

	(void) Gettimeofday(&start);
	now_sec = start.tv_sec;

	switch (cur_progress_meter) {
		case pr_none:
			break;
		case pr_percent:
			(void) printf("%s:     ", local);
			goto zz;
		case pr_kbytes:
			(void) printf("%s:       ", local);
			goto zz;
		case pr_philbar:
			(void) printf("%s%s file: %s %s\n", 
				tcap_boldface,
				sending ? "Sending" : "Receiving",
				local,
				tcap_normal
			);
			barlen = 52;
			for (s = file_size; s > 0; s /= 10L) barlen--;
			(void) sprintf(spec, "      0 %%%ds %%ld bytes. ETA: --:--\r",
				barlen);
			(void) printf(spec, " ", file_size);
			goto zz;
		case pr_dots:
			last_dot = (file_size / 10) + 1;
			dots = 0;
			(void) printf("%s: ", local);
		zz:
			(void) fflush(stdout);
			Echo(stdin, 0);
	}	/* end switch */
	return (cur_progress_meter);
}	/* start_progress */




int progress_report(int finish_up)
{
	int size;
	int perc;
	float frac;
	char spec[64];
	float secsElap;
	int secsLeft, minLeft;
	struct timeval td;

	next_report += xferbufsize;
	(void) Gettimeofday(&stop);
	if ((stop.tv_sec > now_sec) || (finish_up && file_size)) {
		switch (cur_progress_meter) {
			case pr_none:
				break;
			case pr_percent:
				perc = (int) (100.0 * (float)bytes / (float)file_size);
				if (perc > 100) perc = 100;
				else if (perc < 0) perc = 0;
				(void) printf("\b\b\b\b%3d%%", perc);
				(void) fflush(stdout);
				break;
			case pr_philbar:
				frac = (float)bytes / (float)file_size;
				if (frac > 1.0)
					frac = 1.0;
				else if (frac < 0.0)
					frac = 0.0;
				size = (int) ((float)barlen * frac);
				(void) sprintf(spec,
					"%%3d%%%%  0 %%s%%%ds%%s%%%ds %%ld bytes. ETA:%%3d:%%02d\r",
					size, barlen - size);
				perc = (long) (100.0 * frac);
				tvsub(&td, &stop, &start);
				secsElap = td.tv_sec + (td.tv_usec / 1000000.0);
				secsLeft = (int) ((float)file_size / ((float)bytes/secsElap) -
					secsElap + 0.5);
				minLeft = secsLeft / 60;
				secsLeft = secsLeft - (minLeft * 60);
				(void) printf(
					spec,
					perc,
					tcap_reverse,
					"",
					tcap_normal,
					"",
					file_size,
					minLeft,
					secsLeft
				);
				(void) fflush(stdout);
				break;
			case pr_kbytes:
				if ((bytes / 1024) > 0) {
					(void) printf("\b\b\b\b\b\b%5ldK", bytes / 1024);
					(void) fflush(stdout);
				}
				break;
			case pr_dots:
				if (bytes > last_dot) {
					(void) fputc('.', stdout);
					(void) fflush(stdout);
					last_dot += (file_size / 10) + 1;
					dots++;
				}	
		}	/* end switch */
		now_sec = stop.tv_sec;
	}	/* end if we updated */
	return (UserLoggedIn());
}	/* progress_report */





void end_progress(char *direction, char *local, char *remote)
{
    struct timeval          td;
    float                   s, bs = 0.0;
    str32                   bsstr;
    int                     doLastReport;
	int						receiving;
    longstring              fullRemote, fullLocal;

    doLastReport = ((UserLoggedIn()) && (cur_progress_meter != pr_none) &&
        (NOT_VQUIET) && (bytes > 0));

	receiving = (direction[0] == 'r');

	switch(FileType(local)) {
		case IS_FILE:
			(void) Strncpy(fullLocal, lcwd);
			(void) Strncat(fullLocal, "/");
			(void) Strncat(fullLocal, local);
			break;
		case IS_PIPE:
			doLastReport = 0;
			local = Strncpy(fullLocal, local);
			break;
		case IS_STREAM:
		default:
			doLastReport = 0;
			local = Strncpy(fullLocal, receiving ? "stdout" : "stdin");
	}

    if (doLastReport)
        (void) progress_report(1);      /* tell progress proc to cleanup. */

    tvsub(&td, &stop, &start);
    s = td.tv_sec + (td.tv_usec / 1000000.0);

    bsstr[0] = '\0';
    if (s != 0.0) {
        bs = (float)bytes / s;
        if (bs > 1024.0)
            sprintf(bsstr, "%.2f K/s", bs / 1024.0);
        else
            sprintf(bsstr, "%.2f Bytes/sec", bs);
    }

    if (doLastReport) switch(cur_progress_meter) {
        case pr_none:
        zz:
            (void) printf("%s: %ld bytes %s in %.2f seconds, %s.\n",
                local, bytes, direction, s, bsstr);
            break;
        case pr_kbytes:
        case pr_percent:
            (void) printf("%s%ld bytes %s in %.2f seconds, %s.\n",
            cur_progress_meter == pr_kbytes ? "\b\b\b\b\b\b" : "\b\b\b\b",
            bytes, direction, s, bsstr);
            Echo(stdin, 1);
            break;
        case pr_philbar:
            (void) printf("\n");
            Echo(stdin, 1);
            goto zz;
        case pr_dots:
            for (; dots < 10; dots++)
                (void) fputc('.', stdout);
            (void) fputc('\n', stdout);
            Echo(stdin, 1);
            goto zz;
    }

    /* Save transfers to the logfile. */
    /* if a simple path is given, try to log the full path */
    if (*remote != '/') {
        (void) Strncpy(fullRemote, cwd);
        (void) Strncat(fullRemote, "/");
        (void) Strncat(fullRemote, remote);
    } else
        (void) Strncpy(fullRemote, remote);

    if (logf != NULL) {
        (void) fprintf(logf, "\t-> \"%s\" %s, %s\n",
            fullRemote, direction, bsstr);
    }
#ifdef SYSLOG
    {
        longstring infoPart1;

        /* Some syslog()'s can't take an unlimited number of arguments,
         * so shorten our call to syslog to 5 arguments total.
         */
        Strncpy(infoPart1, uinfo.username);
        if (receiving) {
            Strncat(infoPart1, " received ");
            Strncat(infoPart1, fullRemote);
            Strncat(infoPart1, " as ");
            Strncat(infoPart1, fullLocal);
            Strncat(infoPart1, " from ");
        } else {
            Strncat(infoPart1, " sent ");
            Strncat(infoPart1, fullLocal);
            Strncat(infoPart1, " as ");
            Strncat(infoPart1, fullRemote);
            Strncat(infoPart1, " to ");
        }
        Strncat(infoPart1, hostname);
        syslog (LOG_INFO, "%s (%ld bytes, %s).", infoPart1, bytes, bsstr);
    }
#endif  /* SYSLOG */
}	/* end_progress */




void close_file(FILE **fin, int filetype)
{
	if (*fin != NULL) {
		if (filetype == IS_FILE) {
			(void) fclose(*fin);
			*fin = NULL;
		} else if (filetype == IS_PIPE) {
			(void) pclose(*fin);
			*fin = NULL;
		}
	}
}	/* close_file */




/*ARGSUSED*/
void abortsend SIG_PARAMS
{
	activemcmd = 0;
	abrtflag = 0;
	(void) fprintf(stderr, "\nSend aborted.\n");
	Echo(stdin, 1);
	longjmp(sendabort, 1);
}	/* abortsend */



int sendrequest(char *cmd, char *local, char *remote)
{
	FILE					*fin, *dout = NULL;
	Sig_t					oldintr, oldintp;
	string					str;
	register int			c, d;
	struct stat				st;
	int						filetype, result = NOERR;
	int						do_reports = 0;
	char					*mode;
	register char			*bufp;

	dbprintf("cmd: %s;  rmt: %s;  loc: %s.\n",
		cmd,
		remote == NULL ? "(null)" : remote,
		local == NULL ? "(null)" : local
	);

	oldintr = NULL;
	oldintp = NULL;
	mode = "w";
	bytes = file_size = 0L;
	if (setjmp(sendabort)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) Signal(SIGINT, oldintr);
		if (oldintp)
			(void) Signal(SIGPIPE, oldintp);
		result = -1;
		goto xx;
	}
	oldintr = Signal(SIGINT, abortsend);
	file_size = -1;
	if (strcmp(local, "-") == 0)  {
		fin = stdin;
		filetype = IS_STREAM;
	} else if (*local == '|') {
		filetype = IS_PIPE;
		oldintp = Signal(SIGPIPE,SIG_IGN);
		fin = popen(local + 1, "r");
		if (fin == NULL) {
			PERROR("sendrequest", local + 1);
			(void) Signal(SIGINT, oldintr);
			(void) Signal(SIGPIPE, oldintp);
			result = -1;
			goto xx;
		}
	} else {
		filetype = IS_FILE;
		fin = fopen(local, "r");
		if (fin == NULL) {
			PERROR("sendrequest", local);
			(void) Signal(SIGINT, oldintr);
			result = -1;
			goto xx;
		}
		if (fstat(fileno(fin), &st) < 0 ||
		    (st.st_mode&S_IFMT) != S_IFREG) {
			(void) fprintf(stdout, "%s: not a plain file.\n", local);
			(void) Signal(SIGINT, oldintr);
			(void) fclose(fin);
			result = -1;
			goto xx;
		}
		file_size = st.st_size;
	}
	if (initconn()) {
		(void) Signal(SIGINT, oldintr);
		if (oldintp)
			(void) Signal(SIGPIPE, oldintp);
		result = -1;
		close_file(&fin, filetype);
		goto xx;
	}
	if (setjmp(sendabort))
		goto Abort;

#ifdef TRY_NOREPLY
	if (remote) {
		(void) sprintf(str, "%s %s", cmd, remote);
		(void) command_noreply(str);
	} else {
		(void) command_noreply(cmd);
	}

	dout = dataconn(mode);
	if (dout == NULL)
		goto Abort;

	if(getreply(0) != PRELIM) {
		(void) Signal(SIGINT, oldintr);
 		if (oldintp)
 			(void) Signal(SIGPIPE, oldintp);
 		close_file(&fin, filetype);
 		return -1;
 	}
#else
	 if (remote) {
		 (void) sprintf(str, "%s %s", cmd, remote);
		 if (command(str) != PRELIM) {
			 (void) Signal(SIGINT, oldintr);
			 if (oldintp)
				 (void) Signal(SIGPIPE, oldintp);
			 close_file(&fin, filetype);
			 goto xx;
		 }
	 } else {
		 if (command(cmd) != PRELIM) {
			 (void) Signal(SIGINT, oldintr);
			 if (oldintp)
				 (void) Signal(SIGPIPE, oldintp);
			 close_file(&fin, filetype);
			 goto xx;
		 }
	 }

	 dout = dataconn(mode);
	 if (dout == NULL)
		 goto Abort;
#endif

	(void) Gettimeofday(&start);
	oldintp = Signal(SIGPIPE, SIG_IGN);
	if ((do_reports = (filetype == IS_FILE && NOT_VQUIET)) != 0)
		do_reports = start_progress(1, local);

	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		errno = d = 0;
		while ((c = read(fileno(fin), xferbuf, (int)xferbufsize)) > 0) {
			bytes += c;
			for (bufp = xferbuf; c > 0; c -= d, bufp += d)
				if ((d = write(fileno(dout), bufp, c)) <= 0)
					break;
			/* Print progress indicator. */
			if (do_reports)
				do_reports = progress_report(0);
		}
		if (c < 0)
			PERROR("sendrequest", local);
		if (d <= 0) {
			if (d == 0 && !creating)
				(void) fprintf(stderr, "netout: write returned 0?\n");
			else if (errno != EPIPE) 
				PERROR("sendrequest", "netout");
			bytes = -1;
		}
		break;

	case TYPE_A:
		next_report = xferbufsize;
		while ((c = getc(fin)) != EOF) {
			if (c == '\n') {
				if (ferror(dout))
					break;
				(void) putc('\r', dout);
				bytes++;
			}
			(void) putc(c, dout);
			bytes++;

			/* Print progress indicator. */
			if (do_reports && bytes > next_report)
				do_reports = progress_report(0);
		}
		if (ferror(fin))
			PERROR("sendrequest", local);
		if (ferror(dout)) {
			if (errno != EPIPE)
				PERROR("sendrequest", "netout");
			bytes = -1;
		}
		break;
	}
Done:
	close_file(&fin, filetype);
	if (dout)
		(void) fclose(dout);
	(void) getreply(0);
	(void) Signal(SIGINT, oldintr);
	if (oldintp)
		(void) Signal(SIGPIPE, oldintp);
	end_progress("sent", local, remote);
xx:
	return (result);
Abort:
	result = -1;
	if (!cpend)
		goto xx;
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
	goto Done;
}	/* sendrequest */




/*ARGSUSED*/
void abortrecv SIG_PARAMS
{
	activemcmd = 0;
	abrtflag = 0;
	(void) fprintf(stderr, 
#ifdef TRY_ABOR
	"(abort)\n");
#else
	"\nAborting, please wait...");
#endif
	(void) fflush(stderr);
	Echo(stdin, 1);
	longjmp(recvabort, 1);
}	/* abortrecv */




void GetLSRemoteDir(char *remote, char *remote_dir)
{
	char *cp;

	/*
	 * The ls() function can specify a directory to list along with ls flags,
	 * if it sends the flags first followed by the directory name.
	 *
	 * So far, we don't care about the remote directory being listed.  I put
	 * it now so I won't forget in case I need to do something with it later.
	 */
	remote_dir[0] = 0;
	if (remote != NULL) {
		cp = index(remote, LS_FLAGS_AND_FILE);
		if (cp == NULL)
			(void) Strncpy(remote_dir, remote);
		else {
			*cp++ = ' ';
			(void) Strncpy(remote_dir, cp);
		}
	}
}	/* GetLSRemoteDir */




int AdjustLocalFileName(char *local)
{
	char *dir;
	
	/* See if the file exists, and if we can overwrite it. */
	if ((access(local, 0) == 0) && (access(local, 2) < 0))
		goto noaccess;

	/*
	 * Make sure we are writing to a valid local path.
	 * First check the local directory, and see if we can write to it.
	 */
	if (access(local, 2) < 0) {
		dir = rindex(local, '/');

		if (errno != ENOENT && errno != EACCES) {
			/* Report an error if it's one we can't handle. */
			PERROR("AdjustLocalFileName", local);
			return -1;
		}
		/* See if we have write permission on this directory. */
		if (dir != NULL) {
			/* Special case: /filename. */
			if (dir != local)
				*dir = 0;
			if (access(dir == local ? "/" : local, 2) < 0) {
				/*
				 *	We have a big long pathname, like /a/b/c/d,
				 *	but see if we can write into the current
				 *	directory and call the file ./d.
				 */
				if (access(".", 2) < 0) {
					(void) strcpy(local, " and .");
					goto noaccess;
				}
				(void) strcpy(local, dir + 1);	/* use simple filename. */
			} else
				*dir = '/';
		} else {
			/* We have a simple path name (file name only). */
			if (access(".", 2) < 0) {
noaccess:		PERROR("AdjustLocalFileName", local);
				return -1;
			}
		}
	}
	return (NOERR);
}	/* AdjustLocalFileName */
	


int SetToAsciiForLS(int is_retr, int currenttype)
{
	int oldt = -1, oldv;

	if (!is_retr) {
		if (currenttype != TYPE_A) {
			oldt = currenttype;
			oldv = verbose;
			if (!debug)
				verbose = V_QUIET;
			(void) setascii(0, NULL);
			verbose = oldv;
		}
	}
	return oldt;
}	/* SetToAsciiForLS */



int IssueCommand(char *ftpcmd, char *remote)
{
	string str;
	int result = NOERR;

	if (remote)
		(void) sprintf(str, "%s %s", ftpcmd, remote);
	else
		(void) Strncpy(str, ftpcmd);
	
#ifdef TRY_NOREPLY
	if (command_noreply(str) != PRELIM)
#else
	if (command(str) != PRELIM)
#endif
		result = -1;
	return (result);
}	/* IssueCommand */



FILE *OpenOutputFile(int filetype, char *local, char *mode, Sig_t *oldintp)
{
	FILE *fout;

	if (filetype == IS_STREAM) {
		fout = stdout;
	} else if (filetype == IS_PIPE) {
		/* If it is a pipe, the pipecmd will have a | as the first char. */
		++local;
		fout = popen(local, "w");
		*oldintp = Signal(SIGPIPE, abortrecv);
	} else {
		fout = fopen(local, mode);
	}
	if (fout == NULL)
		PERROR("OpenOutputFile", local);
	return (fout);
}	/* OpenOutputFile */



void ReceiveBinary(FILE *din, FILE *fout, int *do_reports, char *localfn)
{
	int							c, d, do2;

	errno = 0;			/* Clear any old error left around. */
	do2 = *do_reports;	/* A slight optimization :-) */
	bytes = 0;			/* Init the byte-transfer counter. */

	for (;;) {
		/* Read a block from the input stream. */
		c = read(fileno(din), xferbuf, (int)xferbufsize);

		/* If c is zero, then we've read the whole file. */
		if (c == 0)
			break;

		/* Check for errors that may have occurred while reading. */
		if (c < 0) {
			/* Error occurred while reading. */
			if (errno != EPIPE)
				PERROR("ReceiveBinary", "netin");
			bytes = -1;
			break;
		}

		/* Write out the same block we just read in. */
		d = write(fileno(fout), xferbuf, c);

		/* Check for write errors. */
		if ((d < 0) || (ferror(fout))) {
			/* Error occurred while writing. */
			PERROR("ReceiveBinary", "outfile");
			break;
		}
		if (d < c) {
			(void) fprintf(stderr, "%s: short write\n", localfn);
			break;
		}

		/* Update the byte counter. */
		bytes += (long) c;

		/* Print progress indicator. */
		if (do2 != 0)
			do2 = progress_report(0);
	}

	*do_reports = do2;	/* Update the real do_reports variable. */
}	/* ReceiveBinary */



void AddRedirLine(char *str2)
{
	register struct lslist *new;

	(void) Strncpy(indataline, str2);
	new = (struct lslist *) malloc((size_t) sizeof(struct lslist));
	if (new != NULL) {
		if ((new->string = NewString(str2)) != NULL) {
	   		new->next = NULL;
			if (lshead == NULL)
				lshead = lstail = new;
			else {
				lstail->next = new;
				lstail = new;
			}
		}
	}
}	/* AddRedirLine */



void ReceiveAscii(FILE *din, FILE *fout, int *do_reports, char *localfn, int
lineMode)
{
	string str2;
	int nchars = 0, c;
	char *linePtr;
	int do2 = *do_reports, stripped;

	next_report = xferbufsize;
	bytes = errno = 0;
	if (lineMode) {
		while ((linePtr = FGets(str2, din)) != NULL) {
			bytes += (long) RemoveTrailingNewline(linePtr, &stripped);
			if (is_ls || debug > 0)
				AddRedirLine(linePtr);

			/* Shutup while getting remote size and mod time. */
			if (!buffer_only) {
				c = fputs(linePtr, fout);

				if (c != EOF) {
					if (stripped > 0)
						c = fputc('\n', fout);
				}
				if ((c == EOF) || (ferror(fout))) {
					PERROR("ReceiveAscii", "outfile");
					break;
				}
			}

			/* Print progress indicator. */
			if (do2 && bytes > next_report)
				do2 = progress_report(0);
		}
	} else while ((c = getc(din)) != EOF) {
		linePtr = str2;
		while (c == '\r') {
			bytes++;
			if ((c = getc(din)) != '\n') {
				if (ferror(fout))
					goto break2;
				/* Shutup while getting remote size and mod time. */
				if (!buffer_only)
					(void) putc('\r', fout);
				if (c == '\0') {
					bytes++;
					goto contin2;
				}
				if (c == EOF)
					goto contin2;
			}
		}
		/* Shutup while getting remote size and mod time. */
		if (!buffer_only)
			(void) putc(c, fout);
		bytes++;
		
		/* Print progress indicator. */
		if (do2 && bytes > next_report)
			do2 = progress_report(0);

		/* No seg violations, please */
		if (nchars < sizeof(str2) - 1) {
         	*linePtr++ = c;  /* build redir string */
			nchars++;
		}

   contin2:
		/* Save the input line in the buffer for recall later. */
		if (c == '\n' && is_ls) {
			*--linePtr = 0;
			AddRedirLine(str2);
			nchars = 0;
		}
       
	}	/* while ((c = getc(din)) != EOF) */
break2:
	if (ferror(din)) {
		if (errno != EPIPE)
			PERROR("ReceiveAscii", "netin");
		bytes = -1;
	}
	if (ferror(fout)) {
		if (errno != EPIPE)
			PERROR("ReceiveAscii", localfn);
	}
	*do_reports = do2;
}	/* ReceiveAscii */



void CloseOutputFile(FILE *f, int filetype, char *name, time_t mt)
{
	struct utimbuf				ut;

	if (f != NULL) {
		(void) fflush(f);
		if (filetype == IS_FILE) {
			(void) fclose(f);
#ifndef DONT_TIMESTAMP
			if (mt != (time_t)0) {
				ut.actime = ut.modtime = mt;
				(void) utime(name, &ut);
			}
#endif	/* DONT_TIMESTAMP */
		} else if (filetype == IS_PIPE) {
			(void)pclose(f);
		}
	}
}	/* close_file */



void ResetOldType(int oldtype)
{
	int oldv;

	if (oldtype >= 0) {
		oldv = verbose;
		if (!debug)
			verbose = V_QUIET;
		(void) SetTypeByNumber(oldtype);
		verbose = oldv;
	}
}	/* ResetOldType */



int FileType(char *fname)
{
	int ft = IS_FILE;

	if (strcmp(fname, "-") == 0)
		ft = IS_STREAM;
	else if (*fname == '|')
		ft = IS_PIPE;
	return (ft);
}	/* FileType */




void CloseData(void) {
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
}	/* CloseData */




int recvrequest(char *cmd, char *local, char *remote, char *mode)
{
	FILE						*fout = NULL, *din = NULL;
	Sig_t						oldintr = NULL, oldintp = NULL;
	int							oldtype = -1, is_retr;
	int							nfnd;
	char						msg;
	struct fd_set				mask;
	int							filetype, do_reports = 0;
	string						remote_dir;
	time_t						remfTime = 0;
	int							result = -1;

	dbprintf("---> cmd: %s;  rmt: %s;  loc: %s;  mode: %s.\n",
		cmd,
		remote == NULL ? "(null)" : remote,
		local == NULL ? "(null)" : local,
		mode
	);

	is_retr = strcmp(cmd, "RETR") == 0;

	GetLSRemoteDir(remote, remote_dir);
	if ((filetype = FileType(local)) == IS_FILE) {
		if (AdjustLocalFileName(local))
			goto xx;
	}

	file_size = -1;
	if (filetype == IS_FILE)
		file_size = GetDateAndSize(remote, (unsigned long *) &remfTime);

	if (initconn())
		goto xx;

	oldtype = SetToAsciiForLS(is_retr, curtype);

 	/* Issue the NLST command but don't wait for the reply.  Some FTP 
 	 * servers make the data connection before issuing the 
 	 * "150 Opening ASCII mode data connection for /bin/ls" reply.
 	 */
	if (IssueCommand(cmd, remote))
		goto xx;
	
	if ((fout = OpenOutputFile(filetype, local, mode, &oldintp)) == NULL)
		goto xx;

	if ((din = dataconn("r")) == NULL)
		goto Abort;

#ifdef TRY_NOREPLY
 	/* Now get the reply we skipped above. */
 	(void) getreply(0);
#endif

	do_reports = NOT_VQUIET && is_retr && filetype == IS_FILE;
	if (do_reports)
		do_reports = start_progress(0, local);

	if (setjmp(recvabort)) {
#ifdef TRY_ABOR
		goto Abort;
#else
		/* Just read the rest of the stream without doing anything with
		 * the results.
		 */
		(void) Signal(SIGINT, SIG_IGN);
		(void) Signal(SIGPIPE, SIG_IGN);	/* Don't bug us while aborting. */
		while (read(fileno(din), xferbuf, (int)xferbufsize) > 0)
			;
		(void) fprintf(stderr, "\rAborted.                   \n");
#endif
	} else {
		oldintr = Signal(SIGINT, abortrecv);

		if (curtype == TYPE_A)
			ReceiveAscii(din, fout, &do_reports, local, 1);
		else
			ReceiveBinary(din, fout, &do_reports, local);
		result = NOERR;
		/* Don't interrupt us now, since we finished successfully. */
		(void) Signal(SIGPIPE, SIG_IGN);
		(void) Signal(SIGINT, SIG_IGN);
	}	
	CloseData();
	(void) getreply(0);

	goto xx;

Abort:

/* Abort using RFC959 recommended IP,SYNC sequence  */

	(void) Signal(SIGPIPE, SIG_IGN);	/* Don't bug us while aborting. */
	(void) Signal(SIGINT, SIG_IGN);
	if (!cpend || !cout) goto xx;
	(void) fprintf(cout,"%c%c",IAC,IP);
	(void) fflush(cout); 
	msg = IAC;
/* send IAC in urgent mode instead of DM because UNIX places oob mark */
/* after urgent byte rather than before as now is protocol            */
	if (send(fileno(cout),&msg,1,MSG_OOB) != 1)
		PERROR("recvrequest", "abort");
	(void) fprintf(cout,"%cABOR\r\n",DM);
	(void) fflush(cout);
	FD_ZERO(&mask);
	FD_SET(fileno(cin), &mask);
	if (din)
		FD_SET(fileno(din), &mask);
	if ((nfnd = empty(&mask,10)) <= 0) {
		if (nfnd < 0)
			PERROR("recvrequest", "abort");
		lostpeer(0);
	}
	if (din && FD_ISSET(fileno(din), &mask)) {
		while ((read(fileno(din), xferbuf, xferbufsize)) > 0)
			;
	}
	if ((getreply(0)) == ERROR && code == 552) { /* needed for nic style abort */
		CloseData();
		(void) getreply(0);
	}
	(void) getreply(0);
	result = -1;
	CloseData();

xx:
	CloseOutputFile(fout, filetype, local, remfTime);
	dbprintf("outfile closed.\n");
	if (din)
		(void) fclose(din);
	if (is_retr)
		end_progress("received", local, remote);
	if (oldintr)
		(void) Signal(SIGINT, oldintr);
	if (oldintp)
		(void) Signal(SIGPIPE, oldintp);
	dbprintf("recvrequest result = %d.\n", result);
	if (oldtype >= 0)
		ResetOldType(oldtype);
	bytes = 0L;
	return (result);
}	/* recvrequest */




/*
 * Need to start a listen on the data channel
 * before we send the command, otherwise the
 * server's connect may fail.
 */


int initconn(void)
{
	register char		*p, *a;
	int					result, len, tmpno = 0;
	int					on = 1, rval;
	string				str;
	Sig_t				oldintr;
	char				*cp;
	int					a1, a2, a3, a4, p1, p2;
	unsigned char		n[6];
	int			count;
	static u_short		last_port = FTP_DATA_BOTTOM;
  
  	oldintr = Signal(SIGINT, SIG_IGN);

	if (using_pasv) {
		result = command("PASV");
		if (result != COMPLETE) {
			printf("Passive mode refused.\n");
			using_pasv = 0;
			goto TryPort;
		}

		/*
		 * What we've got here is a string of comma separated one-byte
		 * unsigned integer values.  The first four are the IP address,
		 * the fifth is the MSB of the port address, and the sixth is the
		 * LSB of the port address.  Extract this data and prepare a
		 * 'data_addr' (struct sockaddr_in).
		 */
		for (cp = reply_string + 4; *cp != '\0'; cp++)
			if (isdigit(*cp))
				break;

		if (sscanf(cp, "%d,%d,%d,%d,%d,%d",
				&a1, &a2, &a3, &a4, &p1, &p2) != 6) {
			printf("Cannot parse PASV response: %s\n", reply_string);
			using_pasv = 0;
			goto TryPort;
		}

		data = socket(AF_INET, SOCK_STREAM, 0);
		if (data < 0) {
			PERROR("initconn", "socket");
			rval = 1;
			goto Return;
		}
#ifdef LINGER	/* If puts don't complete, you could try this. */
		{
			struct linger li;
			li.l_onoff = 1;
			li.l_linger = 900;

			if (setsockopt(data, SOL_SOCKET, SO_LINGER,
				(char *)&li, sizeof(struct linger)) < 0)
			{
				PERROR("initconn", "setsockopt(SO_LINGER)");
			}
		}
#endif	/* LINGER */
		if (options & SO_DEBUG &&
			setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on, sizeof(on)) < 0 ) {
				PERROR("initconn", "setscokopt (ignored)");
		}

		n[0] = (unsigned char) a1;
		n[1] = (unsigned char) a2;
		n[2] = (unsigned char) a3;
		n[3] = (unsigned char) a4;
		n[4] = (unsigned char) p1;
		n[5] = (unsigned char) p2;

		data_addr.sin_family = AF_INET;
		bcopy( (void *)&n[0], (void *)&data_addr.sin_addr, 4 );
		bcopy( (void *)&n[4], (void *)&data_addr.sin_port, 2 );

#ifdef SOCKS
		if (Rconnect( data, (struct sockaddr *) &data_addr, (int) sizeof(data_addr) ) < 0 ) {
#else
		if (Connect( data, &data_addr, sizeof(data_addr) ) < 0 ) {
#endif
			if (errno == ECONNREFUSED) {
				dbprintf("Could not connect to port specified by server;\n");
				dbprintf("Falling back to PORT mode.\n");
				close(data);
				data = -1;
				using_pasv = 0;
				goto TryPort;
			}
			PERROR("initconn", "connect");
			rval = 1;
			goto Return;
		}
		rval = 0;
		goto Return;
	}

TryPort:
	rval = 0;

noport:
	if (data != -1)
		(void) close (data);
	data = socket(AF_INET, SOCK_STREAM, 0);
	if (data < 0) {
		PERROR("initconn", "socket");
		if (tmpno)
			sendport = 1;
		rval = 1;  goto Return;
	}

	data_addr = myctladdr;
	if (sendport) {
		if (restricted_data_ports) {
			for (count = 0;
			     count < FTP_DATA_TOP - FTP_DATA_BOTTOM;
			     count++) {
				last_port++;
				if (last_port < FTP_DATA_BOTTOM ||
				    last_port > FTP_DATA_TOP)
					last_port = FTP_DATA_BOTTOM;

				data_addr.sin_port = htons(last_port);
#ifdef SOCKS
				if (Rbind(data,&data_addr,sizeof data_addr,
					  hisctladdr.sin_addr.s_addr) <0) {
#else
				if (Bind(data,&data_addr,sizeof data_addr) <0) {
#endif
					if (errno == EADDRINUSE)
						continue;
					else {
						warn("bind");
						goto bad;
					}
				}
				break;
			}
			if (count >= FTP_DATA_TOP-FTP_DATA_BOTTOM) {
				PERROR("initconn", "bind");
				goto bad;
			}
		} else {
			data_addr.sin_port = 0;	/* use any port */
#ifdef	SOCKS
			if (Rbind(data,&data_addr,sizeof data_addr,
				  hisctladdr.sin_addr.s_addr) <0) {
#else
			if (Bind(data,&data_addr, sizeof data_addr) <0) {
#endif
				PERROR("initconn", "bind");
				goto bad;
			}
		}
	} else {
		if (setsockopt(data, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
			       sizeof (on)) < 0) {
			PERROR("initconn", "setsockopt (reuse address)");
			goto bad;
		}
	}

#ifdef LINGER	/* If puts don't complete, you could try this. */
	{
		struct linger li;
		li.l_onoff = 1;
		li.l_linger = 900;

		if (setsockopt(data, SOL_SOCKET, SO_LINGER,
			(char *)&li, sizeof(struct linger)) < 0)
		{
			PERROR("initconn", "setsockopt(SO_LINGER)");
		}
	}
#endif	/* LINGER */

#ifdef IPTOS_THROUGHPUT /* transfers are background */
#ifdef IP_TOS
	{
		int nType = IPTOS_THROUGHPUT;
		if (setsockopt(data, IPPROTO_IP, IP_TOS,
			       (char *) &nType, sizeof(nType)) < 0) {
			PERROR("initconn", "setsockopt(IP_TOS)");
		}
	}
#endif
#endif

	if (options & SO_DEBUG &&
	    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on, sizeof (on)) < 0)
		PERROR("initconn", "setsockopt (ignored)");
	len = sizeof (data_addr);
	if (Getsockname(data, (char *)&data_addr, &len) < 0) {
		PERROR("initconn", "getsockname");
		goto bad;
	}

#ifdef SOCKS 
	if (Rlisten(data, 1) < 0)
#else
	if (listen(data, 1) < 0)
#endif
		PERROR("initconn", "listen");
	if (sendport) {
		a = (char *)&data_addr.sin_addr;
		p = (char *)&data_addr.sin_port;
#define UC(x) (int) (((int) x) & 0xff)
		(void) sprintf(str, "PORT %d,%d,%d,%d,%d,%d",
			UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
        result = command(str);
		if (result == ERROR && sendport == -1) {
			sendport = 0;
			tmpno = 1;
			goto noport;
		}
		rval = (result != COMPLETE);  goto Return;
	}
	if (tmpno)
		sendport = 1;
	rval = 0;  goto Return;
bad:
	(void) close(data), data = -1;
	if (tmpno)
		sendport = 1;
	rval = 1;
Return:
	(void) Signal(SIGINT, oldintr);
	return (rval);
}	/* initconn */




FILE *
dataconn(char *mode)
{
	struct sockaddr_in from;
	FILE *fp;
	int s, fromlen = sizeof (from);

 	if (using_pasv)
 		return( fdopen( data, mode ));
#ifdef SOCKS
	s = Raccept(data, (struct sockaddr *) &from, &fromlen);
#else
	s = Accept(data, &from, &fromlen);
#endif
	if (s < 0) {
		PERROR("dataconn", "accept");
		(void) close(data), data = -1;
		fp = NULL;
	} else {
		(void) close(data);
		data = s;
		fp = safeopen(data, mode);
	}
	return (fp);
}	/* dataconn */

/* eof ftp.c */
