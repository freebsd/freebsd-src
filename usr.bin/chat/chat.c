/*
 *	Chat -- a program for automatic session establishment (i.e. dial
 *		the phone and log in).
 *
 * Standard termination codes:
 *  0 - successful completion of the script
 *  1 - invalid argument, expect string too large, etc.
 *  2 - error on an I/O operation or fatal error condtion.
 *  3 - timeout waiting for a simple string.
 *  4 - the first string declared as "ABORT"
 *  5 - the second string declared as "ABORT"
 *  6 - ... and so on for successive ABORT strings.
 *
 *	This software is in the public domain.
 *
 *	Please send all bug reports, requests for information, etc. to:
 *
 *		Al Longyear (longyear@netcom.com)
 *		(I was the last person to change this code.)
 *
 *      Added -r "report file" switch & REPORT keyword.
 *              Robert Geer <bgeer@xmission.com>
 *
 *	The original author is:
 *
 *		Karl Fox <karl@MorningStar.Com>
 *		Morning Star Technologies, Inc.
 *		1760 Zollinger Road
 *		Columbus, OH  43221
 *		(614)451-1883
 *
 */

static char rcsid[] = "$Id: chat.c,v 1.1.1.1 1994/11/12 05:25:32 lars Exp $";

#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

#ifndef TERMIO
#undef	TERMIOS
#define TERMIOS
#endif

#ifdef TERMIO
#include <termio.h>
#endif
#ifdef TERMIOS
#include <termios.h>
#endif

#define	STR_LEN	1024

#ifndef SIGTYPE
#define SIGTYPE void
#endif

#ifdef __STDC__
#undef __P
#define __P(x)	x
#else
#define __P(x)	()
#define const
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK	O_NDELAY
#endif

/*************** Micro getopt() *********************************************/
#define	OPTION(c,v)	(_O&2&&**v?*(*v)++:!c||_O&4?0:(!(_O&1)&& \
				(--c,++v),_O=4,c&&**v=='-'&&v[0][1]?*++*v=='-'\
				&&!v[0][1]?(--c,++v,0):(_O=2,*(*v)++):0))
#define	OPTARG(c,v)	(_O&2?**v||(++v,--c)?(_O=1,--c,*v++): \
				(_O=4,(char*)0):(char*)0)
#define	OPTONLYARG(c,v)	(_O&2&&**v?(_O=1,--c,*v++):(char*)0)
#define	ARG(c,v)	(c?(--c,*v++):(char*)0)

static int _O = 0;		/* Internal state */
/*************** Micro getopt() *********************************************/

char *program_name;

#define	MAX_ABORTS		50
#define	MAX_REPORTS		50
#define	DEFAULT_CHAT_TIMEOUT	45

int verbose       = 0;
int quiet         = 0;
int report        = 0;
int exit_code     = 0;
FILE* report_fp   = (FILE *) 0;
char *report_file = (char *) 0;
char *chat_file   = (char *) 0;
int timeout       = DEFAULT_CHAT_TIMEOUT;

int have_tty_parameters = 0;

#ifdef TERMIO
#define term_parms struct termio
#define get_term_param(param) ioctl(0, TCGETA, param)
#define set_term_param(param) ioctl(0, TCSETA, param)
struct termio saved_tty_parameters;
#endif

#ifdef TERMIOS
#define term_parms struct termios
#define get_term_param(param) tcgetattr(0, param)
#define set_term_param(param) tcsetattr(0, TCSANOW, param)
struct termios saved_tty_parameters;
#endif

char *abort_string[MAX_ABORTS], *fail_reason = (char *)0,
	fail_buffer[50];
int n_aborts = 0, abort_next = 0, timeout_next = 0;

char *report_string[MAX_REPORTS] ;
char  report_buffer[50] ;
int n_reports = 0, report_next = 0, report_gathering = 0 ; 

void *dup_mem __P((void *b, size_t c));
void *copy_of __P((char *s));
void usage __P((void));
void logf __P((const char *str));
void logflush __P((void));
void fatal __P((const char *msg));
void sysfatal __P((const char *msg));
SIGTYPE sigalrm __P((int signo));
SIGTYPE sigint __P((int signo));
SIGTYPE sigterm __P((int signo));
SIGTYPE sighup __P((int signo));
void unalarm __P((void));
void init __P((void));
void set_tty_parameters __P((void));
void break_sequence __P((void));
void terminate __P((int status));
void do_file __P((char *chat_file));
int  get_string __P((register char *string));
int  put_string __P((register char *s));
int  write_char __P((int c));
int  put_char __P((int c));
int  get_char __P((void));
void chat_send __P((register char *s));
char *character __P((int c));
void chat_expect __P((register char *s));
char *clean __P((register char *s, int sending));
void break_sequence __P((void));
void terminate __P((int status));
void die __P((void));

void *dup_mem(b, c)
void *b;
size_t c;
    {
    void *ans = malloc (c);
    if (!ans)
        {
	fatal ("memory error!\n");
        }
    memcpy (ans, b, c);
    return ans;
    }

void *copy_of (s)
char *s;
    {
    return dup_mem (s, strlen (s) + 1);
    }

/*
 *	chat [ -v ] [ -t timeout ] [ -f chat-file ] [ -r report-file ] \
 *		[...[[expect[-say[-expect...]] say expect[-say[-expect]] ...]]]
 *
 *	Perform a UUCP-dialer-like chat script on stdin and stdout.
 */
int
main(argc, argv)
int argc;
char **argv;
    {
    int option;
    char *arg;

    program_name = *argv;
    tzset();

    while (option = OPTION(argc, argv))
        {
	switch (option)
	    {
	    case 'v':
		++verbose;
		break;

	    case 'f':
		if (arg = OPTARG(argc, argv))
		    {
		    chat_file = copy_of(arg);
		    }
		else
		    {
		    usage();
		    }
		break;

	    case 't':
		if (arg = OPTARG(argc, argv))
		    {
		    timeout = atoi(arg);
		    }
		else
		    {
		    usage();
		    }
		break;

	    case 'r':
		arg = OPTARG (argc, argv);
		if (arg)
		    {
		    if (report_fp != NULL)
		        {
			fclose (report_fp);
		        }
		    report_file = copy_of (arg);
		    report_fp   = fopen (report_file, "a");
		    if (report_fp != NULL)
		        {
			if (verbose)
			    {
			    fprintf (report_fp, "Opening \"%s\"...\n",
				     report_file);
			    }
			report = 1;
		        }
		    }
		break;

	    default:
		usage();
		break;
	    }
      }
/*
 * Default the report file to the stderr location
 */
    if (report_fp == NULL)
        {
	report_fp = stderr;
        }

#ifdef ultrix
    openlog("chat", LOG_PID);
#else
    openlog("chat", LOG_PID | LOG_NDELAY, LOG_LOCAL2);

    if (verbose)
        {
	setlogmask(LOG_UPTO(LOG_INFO));
        }
    else
        {
	setlogmask(LOG_UPTO(LOG_WARNING));
        }
#endif

    init();
    
    if (chat_file != NULL)
	{
	arg = ARG(argc, argv);
	if (arg != NULL)
	    {
	    usage();
	    }
	else
	    {
	    do_file (chat_file);
	    }
	}
    else
	{
	while (arg = ARG(argc, argv))
	    {
	    chat_expect(arg);

	    if (arg = ARG(argc, argv))
	        {
		chat_send(arg);
	        }
	    }
	}

    terminate(0);
    }

/*
 *  Process a chat script when read from a file.
 */

void do_file (chat_file)
char *chat_file;
    {
    int linect, len, sendflg;
    char *sp, *arg, quote;
    char buf [STR_LEN];
    FILE *cfp;

    cfp = fopen (chat_file, "r");
    if (cfp == NULL)
	{
	syslog (LOG_ERR, "%s -- open failed: %m", chat_file);
	terminate (1);
	}

    linect = 0;
    sendflg = 0;

    while (fgets(buf, STR_LEN, cfp) != NULL)
	{
	sp = strchr (buf, '\n');
	if (sp)
	    {
	    *sp = '\0';
	    }

	linect++;
	sp = buf;
	while (*sp != '\0')
	    {
	    if (*sp == ' ' || *sp == '\t')
		{
		++sp;
		continue;
		}

	    if (*sp == '"' || *sp == '\'')
		{
		quote = *sp++;
		arg = sp;
		while (*sp != quote)
		    {
		    if (*sp == '\0')
			{
			syslog (LOG_ERR, "unterminated quote (line %d)",
				linect);
			terminate (1);
			}
		    
		    if (*sp++ == '\\')
		        {
			if (*sp != '\0')
			    {
			    ++sp;
			    }
		        }
		    }
		}
	    else
		{
		arg = sp;
		while (*sp != '\0' && *sp != ' ' && *sp != '\t')
		    {
		    ++sp;
		    }
		}

	    if (*sp != '\0')
	        {
		*sp++ = '\0';
	        }

	    if (sendflg)
		{
		chat_send (arg);
		}
	    else
		{
		chat_expect (arg);
		}
	    sendflg = !sendflg;
	    }
	}
    fclose (cfp);
    }

/*
 *	We got an error parsing the command line.
 */
void usage()
    {
    fprintf(stderr, "\
Usage: %s [-v] [-t timeout] [-r report-file] {-f chat-file | chat-script}\n",
	    program_name);
    exit(1);
    }

char line[256];
char *p;

void logf (str)
const char *str;
    {
    p = line + strlen(line);
    strcat (p, str);

    if (str[strlen(str)-1] == '\n')
	{
	syslog (LOG_INFO, "%s", line);
	line[0] = 0;
	}
    }

void logflush()
    {
    if (line[0] != 0)
	{
	syslog(LOG_INFO, "%s", line);
	line[0] = 0;
        }
    }

/*
 *	Terminate with an error.
 */
void die()
    {
    terminate(1);
    }

/*
 *	Print an error message and terminate.
 */

void fatal (msg)
const char *msg;
    {
    syslog(LOG_ERR, "%s", msg);
    terminate(2);
    }

/*
 *	Print an error message along with the system error message and
 *	terminate.
 */

void sysfatal (msg)
const char *msg;
    {
    syslog(LOG_ERR, "%s: %m", msg);
    terminate(2);
    }

int alarmed = 0;

SIGTYPE sigalrm(signo)
int signo;
    {
    int flags;

    alarm(1);
    alarmed = 1;		/* Reset alarm to avoid race window */
    signal(SIGALRM, sigalrm);	/* that can cause hanging in read() */

    logflush();
    if ((flags = fcntl(0, F_GETFL, 0)) == -1)
        {
	sysfatal("Can't get file mode flags on stdin");
        }
    else
        {
	if (fcntl(0, F_SETFL, flags | O_NONBLOCK) == -1)
	    {
	    sysfatal("Can't set file mode flags on stdin");
	    }
        }

    if (verbose)
	{
	syslog(LOG_INFO, "alarm");
	}
    }

void unalarm()
    {
    int flags;

    if ((flags = fcntl(0, F_GETFL, 0)) == -1)
        {
	sysfatal("Can't get file mode flags on stdin");
        }
    else
        {
	if (fcntl(0, F_SETFL, flags & ~O_NONBLOCK) == -1)
	    {
	    sysfatal("Can't set file mode flags on stdin");
	    }
        }
    }

SIGTYPE sigint(signo)
int signo;
    {
    fatal("SIGINT");
    }

SIGTYPE sigterm(signo)
int signo;
    {
    fatal("SIGTERM");
    }

SIGTYPE sighup(signo)
int signo;
    {
    fatal("SIGHUP");
    }

void init()
    {
    signal(SIGINT, sigint);
    signal(SIGTERM, sigterm);
    signal(SIGHUP, sighup);

    set_tty_parameters();
    signal(SIGALRM, sigalrm);
    alarm(0);
    alarmed = 0;
    }

void set_tty_parameters()
    {
#if defined(get_term_param)
    term_parms t;

    if (get_term_param (&t) < 0)
        {
	sysfatal("Can't get terminal parameters");
        }

    saved_tty_parameters = t;
    have_tty_parameters  = 1;

    t.c_iflag     |= IGNBRK | ISTRIP | IGNPAR;
    t.c_oflag      = 0;
    t.c_lflag      = 0;
    t.c_cc[VERASE] =
    t.c_cc[VKILL]  = 0;
    t.c_cc[VMIN]   = 1;
    t.c_cc[VTIME]  = 0;

    if (set_term_param (&t) < 0)
        {
	sysfatal("Can't set terminal parameters");
        }
#endif
    }

void break_sequence()
    {
#ifdef TERMIOS
    tcsendbreak (0, 0);
#endif
    }

void terminate(status)
int status;
    {
    if (report_file != (char *) 0 && report_fp != (FILE *) NULL)
        {
	if (verbose)
	    {
	    fprintf (report_fp, "Closing \"%s\".\n", report_file);
	    }
	fclose (report_fp);
	report_fp = (FILE*) NULL;
        }

#if defined(get_term_param)
    if (have_tty_parameters)
        {
	if (set_term_param (&saved_tty_parameters) < 0)
	    {
	    syslog(LOG_ERR, "Can't restore terminal parameters: %m");
	    exit(1);
	    }
        }
#endif

    exit(status);
    }

/*
 *	'Clean up' this string.
 */
char *clean(s, sending)
register char *s;
int sending;
    {
    char temp[STR_LEN], cur_chr;
    register char *s1;
    int add_return = sending;
#define isoctal(chr) (((chr) >= '0') && ((chr) <= '7'))

    s1 = temp;
    while (*s)
	{
	cur_chr = *s++;
	if (cur_chr == '^')
	    {
	    cur_chr = *s++;
	    if (cur_chr == '\0')
		{
		*s1++ = '^';
		break;
		}
	    cur_chr &= 0x1F;
	    if (cur_chr != 0)
	        {
		*s1++ = cur_chr;
	        }
	    continue;
	    }

	if (cur_chr != '\\')
	    {
	    *s1++ = cur_chr;
	    continue;
	    }

	cur_chr = *s++;
	if (cur_chr == '\0')
	    {
	    if (sending)
		{
		*s1++ = '\\';
		*s1++ = '\\';
		}
	    break;
	    }

	switch (cur_chr)
	    {
	case 'b':
	    *s1++ = '\b';
	    break;

	case 'c':
	    if (sending && *s == '\0')
	        {
		add_return = 0;
	        }
	    else
	        {
		*s1++ = cur_chr;
	        }
	    break;

	case '\\':
	case 'K':
	case 'p':
	case 'd':
	    if (sending)
	        {
		*s1++ = '\\';
	        }

	    *s1++ = cur_chr;
	    break;

	case 'q':
	    quiet = ! quiet;
	    break;

	case 'r':
	    *s1++ = '\r';
	    break;

	case 'n':
	    *s1++ = '\n';
	    break;

	case 's':
	    *s1++ = ' ';
	    break;

	case 't':
	    *s1++ = '\t';
	    break;

	case 'N':
	    if (sending)
		{
		*s1++ = '\\';
		*s1++ = '\0';
		}
	    else
	        {
		*s1++ = 'N';
	        }
	    break;
	    
	default:
	    if (isoctal (cur_chr))
		{
		cur_chr &= 0x07;
		if (isoctal (*s))
		    {
		    cur_chr <<= 3;
		    cur_chr |= *s++ - '0';
		    if (isoctal (*s))
			{
			cur_chr <<= 3;
			cur_chr |= *s++ - '0';
			}
		    }

		if (cur_chr != 0 || sending)
		    {
		    if (sending && (cur_chr == '\\' || cur_chr == 0))
		        {
			*s1++ = '\\';
		        }
		    *s1++ = cur_chr;
		    }
		break;
		}

	    if (sending)
	        {
		*s1++ = '\\';
	        }
	    *s1++ = cur_chr;
	    break;
	    }
	}

    if (add_return)
        {
	*s1++ = '\r';
        }

    *s1++ = '\0'; /* guarantee closure */
    *s1++ = '\0'; /* terminate the string */
    return dup_mem (temp, (size_t) (s1 - temp)); /* may have embedded nuls */
    }

/*
 * Process the expect string
 */
void chat_expect(s)
register char *s;
    {
    if (strcmp(s, "ABORT") == 0)
	{
	++abort_next;
	return;
	}

    if (strcmp(s, "REPORT") == 0)
	{
	++report_next;
	return;
	}

    if (strcmp(s, "TIMEOUT") == 0)
	{
	++timeout_next;
	return;
	}

    while (*s)
	{
	register char *hyphen;

	for (hyphen = s; *hyphen; ++hyphen)
	    {
	    if (*hyphen == '-')
	        {
		if (hyphen == s || hyphen[-1] != '\\')
		    {
		    break;
		    }
	        }
	    }
	
	if (*hyphen == '-')
	    {
	    *hyphen = '\0';

	    if (get_string(s))
	        {
		return;
	        }
	    else
		{
		s = hyphen + 1;

		for (hyphen = s; *hyphen; ++hyphen)
		    {
		    if (*hyphen == '-')
		        {
			if (hyphen == s || hyphen[-1] != '\\')
			    {
			    break;
			    }
		        }
		    }

		if (*hyphen == '-')
		    {
		    *hyphen = '\0';

		    chat_send(s);
		    s = hyphen + 1;
		    }
		else
		    {
		    chat_send(s);
		    return;
		    }
		}
	    }
	else
	    {
	    if (get_string(s))
	        {
		return;
	        }
	    else
		{
		if (fail_reason)
		    {
		    syslog(LOG_INFO, "Failed (%s)", fail_reason);
		    }
		else
		    {
		    syslog(LOG_INFO, "Failed");
		    }

		terminate(exit_code);
		}
	    }
	}
    }

char *character(c)
int c;
    {
    static char string[10];
    char *meta;

    meta = (c & 0x80) ? "M-" : "";
    c &= 0x7F;

    if (c < 32)
        {
	sprintf(string, "%s^%c", meta, (int)c + '@');
        }
    else
        {
	if (c == 127)
	    {
	    sprintf(string, "%s^?", meta);
	    }
	else
	    {
	    sprintf(string, "%s%c", meta, c);
	    }
        }

    return (string);
    }

/*
 *  process the reply string
 */
void chat_send (s)
register char *s;
    {
    if (abort_next)
        {
	char *s1;
	
	abort_next = 0;
	
	if (n_aborts >= MAX_ABORTS)
	    {
	    fatal("Too many ABORT strings");
	    }
	
	s1 = clean(s, 0);
	
	if (strlen(s1) > strlen(s)
	    || strlen(s1) + 1 > sizeof(fail_buffer))
	    {
	    syslog(LOG_WARNING, "Illegal or too-long ABORT string ('%s')", s);
	    die();
	    }

	abort_string[n_aborts++] = s1;

	if (verbose)
	    {
	    logf("abort on (");

	    for (s1 = s; *s1; ++s1)
	        {
		logf(character(*s1));
	        }

	    logf(")\n");
	    }
	return;
	}

    if (report_next)
        {
	char *s1;
	
	report_next = 0;
	if (n_reports >= MAX_REPORTS)
	    {
	    fatal("Too many REPORT strings");
	    }
	
	s1 = clean(s, 0);
	
	if (strlen(s1) > strlen(s) || strlen(s1) > sizeof fail_buffer - 1)
	    {
	    syslog(LOG_WARNING, "Illegal or too-long REPORT string ('%s')", s);
	    die();
	    }
	
	report_string[n_reports++] = s1;
	
	if (verbose)
	    {
	    logf("report (");
	    s1 = s;
	    while (*s1)
	        {
		logf(character(*s1));
		++s1;
	        }
	    logf(")\n");
	    }
	return;
        }

    if (timeout_next)
        {
	timeout_next = 0;
	timeout = atoi(s);
	
	if (timeout <= 0)
	    {
	    timeout = DEFAULT_CHAT_TIMEOUT;
	    }

	if (verbose)
	    {
	    syslog(LOG_INFO, "timeout set to %d seconds", timeout);
	    }
	return;
        }

    if (strcmp(s, "EOT") == 0)
        {
	s = "^D\\c";
        }
    else
        {
	if (strcmp(s, "BREAK") == 0)
	    {
	    s = "\\K\\c";
	    }
        }

    if (!put_string(s))
        {
	syslog(LOG_INFO, "Failed");
	terminate(1);
        }
    }

int get_char()
    {
    int status;
    char c;

    status = read(0, &c, 1);

    switch (status)
        {
    case 1:
	return ((int)c & 0x7F);

    default:
	syslog(LOG_WARNING, "warning: read() on stdin returned %d",
	       status);

    case -1:
	if ((status = fcntl(0, F_GETFL, 0)) == -1)
	    {
	    sysfatal("Can't get file mode flags on stdin");
	    }
	else
	    {
	    if (fcntl(0, F_SETFL, status & ~O_NONBLOCK) == -1)
	        {
		sysfatal("Can't set file mode flags on stdin");
	        }
	    }
	
	return (-1);
        }
    }

int put_char(c)
int c;
    {
    int status;
    char ch = c;

    usleep(10000);		/* inter-character typing delay (?) */

    status = write(1, &ch, 1);

    switch (status)
        {
    case 1:
	return (0);
	
    default:
	syslog(LOG_WARNING, "warning: write() on stdout returned %d",
	       status);
	
    case -1:
	if ((status = fcntl(0, F_GETFL, 0)) == -1)
	    {
	    sysfatal("Can't get file mode flags on stdin");
	    }
	else
	    {
	    if (fcntl(0, F_SETFL, status & ~O_NONBLOCK) == -1)
	        {
		sysfatal("Can't set file mode flags on stdin");
	        }
	    }
	
	return (-1);
        }
    }

int write_char (c)
int c;
    {
    if (alarmed || put_char(c) < 0)
	{
	extern int errno;

	alarm(0);
	alarmed = 0;

	if (verbose)
	    {
	    if (errno == EINTR || errno == EWOULDBLOCK)
	        {
		syslog(LOG_INFO, " -- write timed out");
	        }
	    else
	        {
		syslog(LOG_INFO, " -- write failed: %m");
	        }
	    }
	return (0);
	}
    return (1);
    }

int put_string (s)
register char *s;
    {
    s = clean(s, 1);

    if (verbose)
	{
	logf("send (");

	if (quiet)
	    {
	    logf("??????");
	    }
	else
	    {
	    register char *s1 = s;

	    for (s1 = s; *s1; ++s1)
	        {
		logf(character(*s1));
	        }
	    }

	logf(")\n");
	}

    alarm(timeout); alarmed = 0;

    while (*s)
	{
	register char c = *s++;

	if (c != '\\')
	    {
	    if (!write_char (c))
	        {
		return 0;
	        }
	    continue;
	    }

	c = *s++;
	switch (c)
	    {
	case 'd':
	    sleep(1);
	    break;

	case 'K':
	    break_sequence();
	    break;

	case 'p':
	    usleep(10000); 	/* 1/100th of a second (arg is microseconds) */
	    break;

	default:
	    if (!write_char (c))
		return 0;
	    break;
	    }
	}

    alarm(0);
    alarmed = 0;
    return (1);
    }

/*
 *	'Wait for' this string to appear on this file descriptor.
 */
int get_string(string)
register char *string;
    {
    char temp[STR_LEN];
    int c, printed = 0, len, minlen;
    register char *s = temp, *end = s + STR_LEN;

    fail_reason = (char *)0;
    string = clean(string, 0);
    len = strlen(string);
    minlen = (len > sizeof(fail_buffer)? len: sizeof(fail_buffer)) - 1;

    if (verbose)
	{
	register char *s1;

	logf("expect (");

	for (s1 = string; *s1; ++s1)
	    {
	    logf(character(*s1));
	    }

	logf(")\n");
	}

    if (len > STR_LEN)
	{
	syslog(LOG_INFO, "expect string is too long");
	exit_code = 1;
	return 0;
	}

    if (len == 0)
	{
	if (verbose)
	    {
	    syslog(LOG_INFO, "got it");
	    }

	return (1);
	}

    alarm(timeout);
    alarmed = 0;

    while ( ! alarmed && (c = get_char()) >= 0)
	{
	int n, abort_len, report_len;

	if (verbose)
	    {
	    if (c == '\n')
	        {
		logf("\n");
	        }
	    else
	        {
		logf(character(c));
	        }
	    }

	*s++ = c;

	if (s - temp >= len &&
	    c == string[len - 1] &&
	    strncmp(s - len, string, len) == 0)
	    {
	    if (verbose)
		{
		logf(" -- got it\n");
		}

	    alarm(0);
	    alarmed = 0;
	    return (1);
	    }

	for (n = 0; n < n_aborts; ++n)
	    {
	    if (s - temp >= (abort_len = strlen(abort_string[n])) &&
		strncmp(s - abort_len, abort_string[n], abort_len) == 0)
	        {
		if (verbose)
		    {
		    logf(" -- failed\n");
		    }
		
		alarm(0);
		alarmed = 0;
		exit_code = n + 4;
		strcpy(fail_reason = fail_buffer, abort_string[n]);
		return (0);
	        }
	    }

	if (!report_gathering)
	    {
	    for (n = 0; n < n_reports; ++n)
	        {
		if ((report_string[n] != (char*) NULL) &&
		    s - temp >= (report_len = strlen(report_string[n])) &&
		    strncmp(s - report_len, report_string[n], report_len) == 0)
		    {
		    time_t time_now   = time ((time_t*) NULL);
		    struct tm* tm_now = localtime (&time_now);

		    strftime (report_buffer, 20, "%b %d %H:%M:%S ", tm_now);
		    strcat (report_buffer, report_string[n]);

		    report_string[n] = (char *) NULL;
		    report_gathering = 1;
		    break;
		    }
	        }
	    }
	else
	    {
	    if (!iscntrl (c))
	        {
		int rep_len = strlen (report_buffer);
		report_buffer[rep_len]     = c;
		report_buffer[rep_len + 1] = '\0';
	        }
	    else
	        {
		report_gathering = 0;
		fprintf (report_fp, "chat:  %s\n", report_buffer);
	        }
	    }

	if (s >= end)
	    {
	    strncpy (temp, s - minlen, minlen);
	    s = temp + minlen;
	    }

	if (alarmed && verbose)
	    {
	    syslog(LOG_WARNING, "warning: alarm synchronization problem");
	    }
	}

    alarm(0);
    
    if (verbose && printed)
	{
	if (alarmed)
	    {
	    logf(" -- read timed out\n");
	    }
	else
	    {
	    logflush();
	    syslog(LOG_INFO, " -- read failed: %m");
	    }
	}

    exit_code = 3;
    alarmed   = 0;
    return (0);
    }

#ifdef NO_USLEEP
#include <sys/types.h>
#include <sys/time.h>

/*
  usleep -- support routine for 4.2BSD system call emulations
  last edit:  29-Oct-1984     D A Gwyn
  */

extern int	  select();

int
usleep( usec )				  /* returns 0 if ok, else -1 */
    long		usec;		/* delay in microseconds */
{
    static struct			/* `timeval' */
        {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* microsecs */
        } delay;	    /* _select() timeout */

    delay.tv_sec  = usec / 1000000L;
    delay.tv_usec = usec % 1000000L;

    return select( 0, (long *)0, (long *)0, (long *)0, &delay );
}
#endif
