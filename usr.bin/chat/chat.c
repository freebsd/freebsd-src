/*
 *	Chat -- a program for automatic session establishment (i.e. dial
 *		the phone and log in).
 *
 *	This software is in the public domain.
 *
 *	Please send all bug reports, requests for information, etc. to:
 *
 *		Karl Fox <karl@MorningStar.Com>
 *		Morning Star Technologies, Inc.
 *		1760 Zollinger Road
 *		Columbus, OH  43221
 *		(614)451-1883
 */

static char sccs_id[] = "@(#)chat.c	1.7";

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <varargs.h>
#include <syslog.h>

#ifdef __386BSD__
#define TERMIOS
#define SIGHAND_TYPE	__sighandler_t
#else
#define TERMIO
#endif

#ifdef sun
#define SIGHAND_TYPE	int (*)()
# if defined(SUNOS) && SUNOS >= 41
# ifndef HDB
#  define	HDB
# endif
# endif
#endif

#ifdef TERMIO
#include <termio.h>
#endif
#ifdef TERMIOS
#include <sys/ioctl.h>
#include <termios.h>
#endif

#define	STR_LEN	1024

#if defined(sun) | defined(SYSV) | defined(POSIX_SOURCE)
#define SIGTYPE void
#else 
#define SIGTYPE int
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

extern char *strcpy(), *strcat(), *malloc();
extern int strlen();
#define	copyof(s)	((s) ? strcpy(malloc(strlen(s) + 1), s) : (s))

#ifndef LOCK_DIR
#  ifdef HDB
#   define	LOCK_DIR	"/usr/spool/locks"
#  else /* HDB */
#   define	LOCK_DIR	"/usr/spool/uucp"
#  endif /* HDB */
#endif /* LOCK_DIR */

#define	MAX_ABORTS		50
#define	DEFAULT_CHAT_TIMEOUT	45

int verbose = 0;
int quiet = 0;
char *lock_file = (char *)0;
int timeout = DEFAULT_CHAT_TIMEOUT;

int have_tty_parameters = 0;
#ifdef TERMIO
struct termio saved_tty_parameters;
#endif
#ifdef TERMIOS
struct termios saved_tty_parameters;
#endif

char *abort_string[MAX_ABORTS], *fail_reason = (char *)0,
	fail_buffer[50];
int n_aborts = 0, abort_next = 0, timeout_next = 0;

/*
 *	chat [ -v ] [ -t timeout ] [ -l lock-file ] \
 *		[...[[expect[-say[-expect...]] say expect[-say[-expect]] ...]]]
 *
 *	Perform a UUCP-dialer-like chat script on stdin and stdout.
 */
main(argc, argv)
int argc;
char **argv;
    {
    int option, n;
    char *arg;

    program_name = *argv;

    while (option = OPTION(argc, argv))
	switch (option)
	    {
	    case 'v':
		++verbose;
		break;

	    case 'l':
		if (arg = OPTARG(argc, argv))
		    lock_file = copyof(arg);
		else
		    usage();

		break;

	    case 't':
		if (arg = OPTARG(argc, argv))
		    timeout = atoi(arg);
		else
		    usage();

		break;

	    default:
		usage();
	    }

    openlog("chat", LOG_PID | LOG_NDELAY, LOG_LOCAL2);

    if (verbose) {
	setlogmask(LOG_UPTO(LOG_INFO));
    } else {
	setlogmask(LOG_UPTO(LOG_WARNING));
    }

    init();
    
    while (arg = ARG(argc, argv))
	{
	chat_expect(arg);

	if (arg = ARG(argc, argv))
	    chat_send(arg);
	}

    terminate(0);
    }

/*
 *	We got an error parsing the command line.
 */
usage()
    {
    fprintf(stderr,
	    "Usage: %s [ -v ] [ -l lock-file ] [ -t timeout ] chat-script\n",
	    program_name);
    exit(1);
    }

/*
 *	Print a warning message.
 */
/*VARARGS1*/
warn(format, arg1, arg2, arg3, arg4)
char *format;
int arg1, arg2, arg3, arg4;
    {
    logf("%s: Warning: ", program_name);
    logf(format, arg1, arg2, arg3, arg4);
    logf("\n");
    }

/*
 *	Print an error message and terminate.
 */
/*VARARGS1*/
fatal(format, arg1, arg2, arg3, arg4)
char *format;
int arg1, arg2, arg3, arg4;
    {
    logf("%s: ", program_name);
    logf(format, arg1, arg2, arg3, arg4);
    logf("\n");
    unlock();
    terminate(1);
    }

/*
 *	Print an error message along with the system error message and
 *	terminate.
 */
/*VARARGS1*/
sysfatal(format, arg1, arg2, arg3, arg4)
char *format;
int arg1, arg2, arg3, arg4;
    {
    char message[STR_LEN];

    sprintf(message, "%s: ", program_name);
    sprintf(message + strlen(message), format, arg1, arg2, arg3, arg4);
    perror(message);
    unlock();
    terminate(1);
    }

int alarmed = 0;

SIGTYPE
  sigalrm()
{
    int flags;

    alarm(1); alarmed = 1;		/* Reset alarm to avoid race window */
    signal(SIGALRM, (SIGHAND_TYPE)sigalrm); /* that can cause hanging in read() */

    if ((flags = fcntl(0, F_GETFL, 0)) == -1)
	sysfatal("Can't get file mode flags on stdin");
    else
	if (fcntl(0, F_SETFL, flags | FNDELAY) == -1)
	    sysfatal("Can't set file mode flags on stdin");

    if (verbose)
	{
	logf("alarm\n");
	}
    }

unalarm()
    {
    int flags;

    if ((flags = fcntl(0, F_GETFL, 0)) == -1)
	sysfatal("Can't get file mode flags on stdin");
    else
	if (fcntl(0, F_SETFL, flags & ~FNDELAY) == -1)
	    sysfatal("Can't set file mode flags on stdin");
    }

SIGTYPE
  sigint()
{
  fatal("SIGINT");
}

SIGTYPE
  sigterm()
{
  fatal("SIGTERM");
}

SIGTYPE
  sighup()
{
  fatal("SIGHUP");
}

init()
    {
    signal(SIGINT, (SIGHAND_TYPE)sigint);
    signal(SIGTERM, (SIGHAND_TYPE)sigterm);
    signal(SIGHUP, (SIGHAND_TYPE)sighup);

    if (lock_file)
	lock();

    set_tty_parameters();
    signal(SIGALRM, (SIGHAND_TYPE)sigalrm);
    alarm(0); alarmed = 0;
    }


set_tty_parameters()
    {
#ifdef TERMIO
    struct termio t;

    if (ioctl(0, TCGETA, &t) < 0)
	sysfatal("Can't get terminal parameters");
#endif
#ifdef TERMIOS
    struct termios t;

    if (ioctl(0, TIOCGETA, &t) < 0)
	sysfatal("Can't get terminal parameters");
#endif

    saved_tty_parameters = t;
    have_tty_parameters = 1;

    t.c_iflag = IGNBRK | ISTRIP | IGNPAR;
    t.c_oflag = 0;
    t.c_lflag = 0;
    t.c_cc[VERASE] = t.c_cc[VKILL] = 0;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;

#ifdef TERMIO
    if (ioctl(0, TCSETA, &t) < 0)
	sysfatal("Can't set terminal parameters");
#endif
#ifdef TERMIOS
    if (ioctl(0, TIOCSETA, &t) < 0)
	sysfatal("Can't set terminal parameters");
#endif
    }


terminate(status)
{
  if (have_tty_parameters &&
#ifdef TERMIO
      ioctl(0, TCSETA, &saved_tty_parameters) < 0
#endif
#ifdef TERMIOS
      ioctl(0, TIOCSETA, &saved_tty_parameters) < 0
#endif
      ) {
    perror("Can't restore terminal parameters");
    unlock();
    exit(1);
  }
  exit(status);
}

/*
 *	Create a lock file for the named lock device
 */
lock()
    {
    char hdb_lock_buffer[12];
    int fd, pid;

    lock_file = strcat(strcat(strcpy(malloc(strlen(LOCK_DIR)
				       + 1 + strlen(lock_file) + 1),
				LOCK_DIR), "/"), lock_file);

    if ((fd = open(lock_file, O_EXCL | O_CREAT | O_RDWR, 0644)) < 0)
	{
	char *s = lock_file;

	lock_file = (char *)0;	/* Don't remove someone else's lock file! */
	sysfatal("Can't get lock file '%s'", s);
	}

# ifdef HDB
    sprintf(hdb_lock_buffer, "%10d\n", getpid());
    write(fd, hdb_lock_buffer, 11);
# else /* HDB */
    pid = getpid();
    write(fd, &pid, sizeof pid);
# endif /* HDB */

    close(fd);
    }

/*
 *	Remove our lockfile
 */
unlock()
    {
    if (lock_file)
	{
	unlink(lock_file);
	lock_file = (char *)0;
	}
    }

/*
 *	'Clean up' this string.
 */
char *clean(s, sending)
register char *s;
int sending;
    {
    char temp[STR_LEN];
    register char *s1;
    int add_return = sending;

    for (s1 = temp; *s; ++s)
	 switch (*s)
	     {
	     case '\\':
		 switch (*++s)
		     {
		     case '\\':
		     case 'd':	if (sending)
				    *s1++ = '\\';

				*s1++ = *s;
				break;

		     case 'q':	quiet = ! quiet; break;
		     case 'r':	*s1++ = '\r'; break;
		     case 'n':	*s1++ = '\n'; break;
		     case 's':	*s1++ = ' '; break;

		     case 'c':	if (sending && s[1] == '\0')
				    add_return = 0;
				else
				    *s1++ = *s;

				break;

		     default:	*s1++ = *s;
		     }

		 break;

	     case '^':
		 *s1++ = (int)(*++s) & 0x1F;
		 break;

	     default:
		 *s1++ = *s;
	     }
    
    if (add_return)
	*s1++ = '\r';

    *s1 = '\0';
    return (copyof(temp));
    }

/*
 *
 */
chat_expect(s)
register char *s;
    {
    if (strcmp(s, "ABORT") == 0)
	{
	++abort_next;
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
	    if (*hyphen == '-')
		if (hyphen == s || hyphen[-1] != '\\')
		    break;
	
	if (*hyphen == '-')
	    {
	    *hyphen = '\0';

	    if (get_string(s))
		return;
	    else
		{
		s = hyphen + 1;

		for (hyphen = s; *hyphen; ++hyphen)
		    if (*hyphen == '-')
			if (hyphen == s || hyphen[-1] != '\\')
			    break;

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
	    if (get_string(s))
		return;
	    else
		{
		if (fail_reason)
		    logf("Failed(%s)\n", fail_reason);
		else
		    logf("Failed\n");

		unlock();
		terminate(1);
		}
	}
    }

char *character(c)
char c;
    {
    static char string[10];
    char *meta;

    meta = (c & 0x80) ? "M-" : "";
    c &= 0x7F;

    if (c < 32)
	sprintf(string, "%s^%c", meta, (int)c + '@');
    else
	if (c == 127)
	    sprintf(string, "%s^?", meta);
	else
	    sprintf(string, "%s%c", meta, c);

    return (string);
    }

/*
 *
 */
chat_send(s)
register char *s;
    {
    if (abort_next)
	{
	char *s1;

	abort_next = 0;

	if (n_aborts >= MAX_ABORTS)
	    fatal("Too many ABORT strings");

	s1 = clean(s, 0);

	if (strlen(s1) > strlen(s))
	    fatal("Illegal ABORT string ('%s')\n", s);

	if (strlen(s1) > sizeof fail_buffer - 1)
	    fatal("Too long ABORT string ('%s')\n", s);

	strcpy(s, s1);
	abort_string[n_aborts++] = s;

	if (verbose)
	    {
	    register char *s1 = s;

	    logf("abort on (");

	    for (s1 = s; *s1; ++s1)
		logf("%s", character(*s1));

	    logf(")\n");
	    }
	}
    else
	if (timeout_next)
	    {
	    timeout_next = 0;
	    timeout = atoi(s);

	    if (timeout <= 0)
		timeout = DEFAULT_CHAT_TIMEOUT;

	    if (verbose)
		{
		logf("timeout set to %d seconds\n", timeout);
		}
	    }
	else
	    if ( ! put_string(s))
		{
		logf("Failed\n");
		unlock();
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
	    warn("read() on stdin returned %d", status);

	case -1:
	    if ((status = fcntl(0, F_GETFL, 0)) == -1)
		sysfatal("Can't get file mode flags on stdin");
	    else
		if (fcntl(0, F_SETFL, status & ~FNDELAY) == -1)
		    sysfatal("Can't set file mode flags on stdin");

	    return (-1);
	}
    }

int put_char(c)
char c;
    {
    int status;

    delay();

    status = write(1, &c, 1);

    switch (status)
	{
	case 1:
	    return (0);

	default:
	    warn("write() on stdout returned %d", status);

	case -1:
	    if ((status = fcntl(0, F_GETFL, 0)) == -1)
		sysfatal("Can't get file mode flags on stdin");
	    else
		if (fcntl(0, F_SETFL, status & ~FNDELAY) == -1)
		    sysfatal("Can't set file mode flags on stdin");

	    return (-1);
	}
    }

int put_string(s)
register char *s;
    {
    s = clean(s, 1);

    if (verbose)
	{
	logf("send (");

	if (quiet)
	    logf("??????");
	else
	    {
	    register char *s1 = s;

	    for (s1 = s; *s1; ++s1)
		logf("%s", character(*s1));
	    }

	logf(")\n");
	}

    alarm(timeout); alarmed = 0;

    for ( ; *s; ++s)
	{
	register char c = *s;

	if (c == '\\')
	    if ((c = *++s) == '\0')
		break;
	    else
		if (c == 'd')		/* \d -- Delay */
		    {
		    sleep(2);
		    continue;
		    }

	if (alarmed || put_char(*s) < 0)
	    {
	    extern int errno;

	    alarm(0); alarmed = 0;

	    if (verbose)
		{
		if (errno == EINTR || errno == EWOULDBLOCK)
		    logf(" -- write timed out\n");
		else
		    syslog(LOG_INFO, " -- write failed: %m");
		}

	    return (0);
	    }
	}

    alarm(0); alarmed = 0;
    return (1);
    }

/*
 *	'Wait for' this string to appear on this file descriptor.
 */
int get_string(string)
register char *string;
    {
    char temp[STR_LEN];
    int c, printed = 0, len;
    register char *s = temp, *end = s + STR_LEN;

    fail_reason = (char *)0;
    string = clean(string, 0);
    len = strlen(string);

    if (verbose)
	{
	register char *s1;

	logf("expect (");

	for (s1 = string; *s1; ++s1)
	    logf("%s", character(*s1));

	logf(")\n");
	}

    if (len == 0)
	{
	if (verbose)
	    {
	    logf("got it\n");
	    }

	return (1);
	}

    alarm(timeout); alarmed = 0;

    while ( ! alarmed && (c = get_char()) >= 0)
	{
	int n, abort_len;

	if (verbose)
	    {
	    if (c == '\n')
		logf("\n");
	    else
		logf("%s", character(c));
	    }

	*s++ = c;

	if (s >= end)
	    {
	    if (verbose)
		{
		logf(" -- too much data\n");
		}

	    alarm(0); alarmed = 0;
	    return (0);
	    }

	if (s - temp >= len &&
	    c == string[len - 1] &&
	    strncmp(s - len, string, len) == 0)
	    {
	    if (verbose)
		{
		logf("got it\n");
		}

	    alarm(0); alarmed = 0;
	    return (1);
	    }

	for (n = 0; n < n_aborts; ++n)
	    if (s - temp >= (abort_len = strlen(abort_string[n])) &&
		strncmp(s - abort_len, abort_string[n], abort_len) == 0)
		{
		if (verbose)
		    {
		    logf(" -- failed\n");
		    }

		alarm(0); alarmed = 0;
		strcpy(fail_reason = fail_buffer, abort_string[n]);
		return (0);
		}

	if (alarmed && verbose)
	    warn("Alarm synchronization problem");
	}

    alarm(0);
    
    if (verbose && printed)
	{
	extern int errno;

	if (alarmed)
	    logf(" -- read timed out\n");
	else
	    syslog(LOG_INFO, " -- read failed: %m");
	}

    alarmed = 0;
    return (0);
    }

/*
 *	Delay an amount appropriate for between typed characters.
 */
delay()
    {
    register int i;

# ifdef NO_USLEEP
    for (i = 0; i < 30000; ++i)		/* ... did we just say appropriate? */
	;
# else /* NO_USLEEP */
    usleep(100);
# endif /* NO_USLEEP */
    }

char line[256];
char *p;

logf(fmt, va_alist)
char *fmt;
va_dcl
{
    va_list pvar;
    char buf[256];

    va_start(pvar);
    vsprintf(buf, fmt, pvar);
    va_end(pvar);

    p = line + strlen(line);
    strcat(p, buf);

    if (buf[strlen(buf)-1] == '\n') {
	syslog(LOG_INFO, "%s", line);
	line[0] = 0;
    }
}
