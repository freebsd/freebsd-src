/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Operating system dependent routines.
 *
 * Most of the stuff in here is based on Unix, but an attempt
 * has been made to make things work on other operating systems.
 * This will sometimes result in a loss of functionality, unless
 * someone rewrites code specifically for the new operating system.
 *
 * The makefile provides defines to decide whether various
 * Unix features are present.
 */

#include "less.h"
#include <signal.h>
#include <setjmp.h>
#if MSDOS_COMPILER==WIN32C
#include <windows.h>
#endif
#if HAVE_TIME_H
#include <time.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if MUST_DEFINE_ERRNO
extern int errno;
#endif
#if HAVE_VALUES_H
#include <values.h>
#endif

#if defined(__APPLE__)
#include <sys/utsname.h>
#endif

#if HAVE_POLL && !MSDOS_COMPILER
#define USE_POLL 1
static lbool use_poll = TRUE;
#else
#define USE_POLL 0
#endif
#if USE_POLL
#include <poll.h>
static lbool any_data = FALSE;
#endif

/*
 * BSD setjmp() saves (and longjmp() restores) the signal mask.
 * This costs a system call or two per setjmp(), so if possible we clear the
 * signal mask with sigsetmask(), and use _setjmp()/_longjmp() instead.
 * On other systems, setjmp() doesn't affect the signal mask and so
 * _setjmp() does not exist; we just use setjmp().
 */
#if HAVE_SIGSETJMP
#define SET_JUMP(label)        sigsetjmp(label, 1)
#define LONG_JUMP(label, val)  siglongjmp(label, val)
#define JUMP_BUF               sigjmp_buf
#else
#if HAVE__SETJMP && HAVE_SIGSETMASK
#define SET_JUMP(label)        _setjmp(label)
#define LONG_JUMP(label, val)  _longjmp(label, val)
#define JUMP_BUF               jmp_buf
#else
#define SET_JUMP(label)        setjmp(label)
#define LONG_JUMP(label, val)  longjmp(label, val)
#define JUMP_BUF               jmp_buf
#endif
#endif

static lbool reading;
static lbool opening;
public lbool waiting_for_data;
public int consecutive_nulls = 0;
public lbool getting_one_screen = FALSE;

/* Milliseconds to wait for data before displaying "waiting for data" message. */
static int waiting_for_data_delay = 4000;
/* Max milliseconds expected to "normally" read and display a screen of text. */
public int screenfill_ms = 3000;

static JUMP_BUF read_label;
static JUMP_BUF open_label;

extern int sigs;
extern int ignore_eoi;
extern int exit_F_on_close;
extern int follow_mode;
extern int scanning_eof;
extern char intr_char;
extern int is_tty;
extern int quit_if_one_screen;
extern int one_screen;
#if HAVE_TIME
extern time_type less_start_time;
#endif
#if !MSDOS_COMPILER
extern int tty;
#endif

public void init_poll(void)
{
	constant char *delay = lgetenv("LESS_DATA_DELAY");
	int idelay = (delay == NULL) ? 0 : atoi(delay);
	if (idelay > 0)
		waiting_for_data_delay = idelay;
	delay = lgetenv("LESS_SCREENFILL_TIME");
	idelay = (delay == NULL) ? 0 : atoi(delay);
	if (idelay > 0)
		screenfill_ms = idelay;
#if USE_POLL
#if defined(__APPLE__)
	/* In old versions of MacOS, poll() does not work with /dev/tty. */
	struct utsname uts;
	if (uname(&uts) < 0 || lstrtoi(uts.release, NULL, 10) < 20)
		use_poll = FALSE;
#endif
#endif
}

#if USE_POLL
/*
 * Check whether data is available, either from a file/pipe or from the tty.
 * Return READ_AGAIN if no data currently available, but caller should retry later.
 * Return READ_INTR to abort F command (forw_loop).
 * Return 0 if safe to read from fd.
 */
static int check_poll(int fd, int tty)
{
	struct pollfd poller[2] = { { fd, POLLIN, 0 }, { tty, POLLIN, 0 } };
	int timeout = (waiting_for_data && !(scanning_eof && follow_mode == FOLLOW_NAME)) ? -1 : (ignore_eoi && !waiting_for_data) ? 0 : waiting_for_data_delay;
#if HAVE_TIME
	if (getting_one_screen && get_time() < less_start_time + screenfill_ms/1000)
		return (0);
#endif
	if (!any_data)
	{
		/*
		 * Don't do polling if no data has yet been received,
		 * to allow a program piping data into less to have temporary
		 * access to the tty (like sudo asking for a password).
		 */
		return (0);
	}
	poll(poller, 2, timeout);
#if LESSTEST
	if (!is_lesstest()) /* Check for ^X only on a real tty. */
#endif /*LESSTEST*/
	{
		if (poller[1].revents & POLLIN) 
		{
			int ch = getchr();
			if (ch < 0 || ch == intr_char)
				/* Break out of "waiting for data". */
				return (READ_INTR);
			ungetcc_back((char) ch);
			return (READ_INTR);
		}
	}
	if (ignore_eoi && exit_F_on_close && (poller[0].revents & (POLLHUP|POLLIN)) == POLLHUP)
		/* Break out of F loop on HUP due to --exit-follow-on-close. */
		return (READ_INTR);
	if ((poller[0].revents & (POLLIN|POLLHUP|POLLERR)) == 0)
		/* No data available; let caller take action, then try again. */
		return (READ_AGAIN);
	/* There is data (or HUP/ERR) available. Safe to call read() without blocking. */
	return (0);
}
#endif /* USE_POLL */

public int supports_ctrl_x(void)
{
#if MSDOS_COMPILER==WIN32C
	return (TRUE);
#else
#if USE_POLL
	return (use_poll);
#else
	return (FALSE);
#endif /* USE_POLL */
#endif /* MSDOS_COMPILER==WIN32C */
}

/*
 * Like read() system call, but is deliberately interruptible.
 * A call to intio() from a signal handler will interrupt
 * any pending iread().
 */
public ssize_t iread(int fd, unsigned char *buf, size_t len)
{
	ssize_t n;

start:
#if MSDOS_COMPILER==WIN32C
	if (ABORT_SIGS())
		return (READ_INTR);
#else
#if MSDOS_COMPILER && MSDOS_COMPILER != DJGPPC
	if (kbhit())
	{
		int c;
		
		c = getch();
		if (c == '\003')
			return (READ_INTR);
		ungetch(c);
	}
#endif
#endif
	if (!reading && SET_JUMP(read_label))
	{
		/*
		 * We jumped here from intio.
		 */
		reading = FALSE;
#if HAVE_SIGPROCMASK
		{
		  sigset_t mask;
		  sigemptyset(&mask);
		  sigprocmask(SIG_SETMASK, &mask, NULL);
		}
#else
#if HAVE_SIGSETMASK
		sigsetmask(0);
#else
#ifdef _OSK
		sigmask(~0);
#endif
#endif
#endif
#if !MSDOS_COMPILER
		if (fd != tty && !ABORT_SIGS())
			/* Non-interrupt signal like SIGWINCH. */
			return (READ_AGAIN);
#endif
		return (READ_INTR);
	}

	flush();
	reading = TRUE;
#if MSDOS_COMPILER==DJGPPC
	if (isatty(fd))
	{
		/*
		 * Don't try reading from a TTY until a character is
		 * available, because that makes some background programs
		 * believe DOS is busy in a way that prevents those
		 * programs from working while "less" waits.
		 * {{ This code was added 12 Jan 2007; still needed? }}
		 */
		fd_set readfds;

		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		if (select(fd+1, &readfds, 0, 0, 0) == -1)
		{
			reading = FALSE;
			return (READ_ERR);
		}
	}
#endif
#if USE_POLL
	if (is_tty && fd != tty && use_poll && !(quit_if_one_screen && one_screen))
	{
		int ret = check_poll(fd, tty);
		if (ret != 0)
		{
			if (ret == READ_INTR)
				sigs |= S_SWINTERRUPT;
			reading = FALSE;
			return (ret);
		}
	}
#else
#if MSDOS_COMPILER==WIN32C
	if (win32_kbhit2(TRUE))
	{
		int c;

		c = WIN32getch();
		sigs |= S_SWINTERRUPT;
		reading = FALSE;
		if (c != CONTROL('C') && c != intr_char)
			WIN32ungetch((char) c);
		return (READ_INTR);
	}
#endif
#endif
	n = read(fd, buf, len);
	reading = FALSE;
#if 0
	/*
	 * This is a kludge to workaround a problem on some systems
	 * where terminating a remote tty connection causes read() to
	 * start returning 0 forever, instead of -1.
	 */
	{
		if (!ignore_eoi)
		{
			if (n == 0)
				consecutive_nulls++;
			else
				consecutive_nulls = 0;
			if (consecutive_nulls > 20)
				quit(QUIT_ERROR);
		}
	}
#endif
	if (n < 0)
	{
#if HAVE_ERRNO
		/*
		 * Certain values of errno indicate we should just retry the read.
		 */
#ifdef EINTR
		if (errno == EINTR)
			goto start;
#endif
#ifdef EAGAIN
		if (errno == EAGAIN)
			goto start;
#endif
#endif
		return (READ_ERR);
	}
#if USE_POLL
	if (fd != tty && n > 0)
		any_data = TRUE;
#endif
	return (n);
}

/*
 * Like open() system call, but is interruptible.
 */
public int iopen(constant char *filename, int flags)
{
	int r;
	while (!opening && SET_JUMP(open_label))
	{
		opening = FALSE;
		if (sigs & (S_INTERRUPT|S_SWINTERRUPT))
		{
			sigs = 0;
#if HAVE_SETTABLE_ERRNO
#ifdef EINTR
			errno = EINTR;
#endif
#endif
			return -1;
		}
		psignals(); /* Handle S_STOP or S_WINCH */
	}
	opening = TRUE;
	r = open(filename, flags);
	opening = FALSE;
	return r;
}

/*
 * Interrupt a pending iopen() or iread().
 */
public void intio(void)
{
	if (opening)
	{
		LONG_JUMP(open_label, 1);
	}
	if (reading)
	{
		LONG_JUMP(read_label, 1);
	}
}

/*
 * Return the current time.
 */
#if HAVE_TIME
public time_type get_time(void)
{
	time_type t;

	time(&t);
	return (t);
}
#endif


#if !HAVE_STRERROR
/*
 * Local version of strerror, if not available from the system.
 */
static char * strerror(int err)
{
	static char buf[INT_STRLEN_BOUND(int)+12];
#if HAVE_SYS_ERRLIST
	extern char *sys_errlist[];
	extern int sys_nerr;
  
	if (err < sys_nerr)
		return sys_errlist[err];
#endif
	sprintf(buf, "Error %d", err);
	return buf;
}
#endif

/*
 * errno_message: Return an error message based on the value of "errno".
 */
public char * errno_message(constant char *filename)
{
	char *p;
	char *m;
	size_t len;
#if HAVE_ERRNO
	p = strerror(errno);
#else
	p = "cannot open";
#endif
	len = strlen(filename) + strlen(p) + 3;
	m = (char *) ecalloc(len, sizeof(char));
	SNPRINTF2(m, len, "%s: %s", filename, p);
	return (m);
}

/*
 * Return a description of a signal.
 * The return value is good until the next call to this function.
 */
public constant char * signal_message(int sig)
{
	static char sigbuf[sizeof("Signal ") + INT_STRLEN_BOUND(sig) + 1];
#if HAVE_STRSIGNAL
	constant char *description = strsignal(sig);
	if (description)
		return description;
#endif
	sprintf(sigbuf, "Signal %d", sig);
	return sigbuf;
}

/*
 * Return (VAL * NUM) / DEN, where DEN is positive
 * and min(VAL, NUM) <= DEN so the result cannot overflow.
 * Round to the nearest integer, breaking ties by rounding to even.
 */
public uintmax umuldiv(uintmax val, uintmax num, uintmax den)
{
	/*
	 * Like round(val * (double) num / den), but without rounding error.
	 * Overflow cannot occur, so there is no need for floating point.
	 */
	uintmax q = val / den;
	uintmax r = val % den;
	uintmax qnum = q * num;
	uintmax rnum = r * num;
	uintmax quot = qnum + rnum / den;
	uintmax rem = rnum % den;
	return quot + (den / 2 < rem + (quot & ~den & 1));
}

/*
 * Return the ratio of two POSITIONS, as a percentage.
 * {{ Assumes a POSITION is a long int. }}
 */
public int percentage(POSITION num, POSITION den)
{
	return (int) muldiv(num, 100, den);
}

/*
 * Return the specified percentage of a POSITION.
 * Assume (0 <= POS && 0 <= PERCENT <= 100
 *	   && 0 <= FRACTION < (PERCENT == 100 ? 1 : NUM_FRAC_DENOM)),
 * so the result cannot overflow.  Round to even.
 */
public POSITION percent_pos(POSITION pos, int percent, long fraction)
{
	/*
	 * Change from percent (parts per 100)
	 * to pctden (parts per 100 * NUM_FRAC_DENOM).
	 */
	POSITION pctden = (percent * NUM_FRAC_DENOM) + fraction;

	return (POSITION) muldiv(pos, pctden, 100 * NUM_FRAC_DENOM);
}

#if !HAVE_STRCHR
/*
 * strchr is used by regexp.c.
 */
char * strchr(char *s, char c)
{
	for ( ;  *s != '\0';  s++)
		if (*s == c)
			return (s);
	if (c == '\0')
		return (s);
	return (NULL);
}
#endif

#if !HAVE_MEMCPY
void * memcpy(void *dst, void *src, size_t len)
{
	char *dstp = (char *) dst;
	char *srcp = (char *) src;
	int i;

	for (i = 0;  i < len;  i++)
		dstp[i] = srcp[i];
	return (dst);
}
#endif

#ifdef _OSK_MWC32

/*
 * This implements an ANSI-style intercept setup for Microware C 3.2
 */
public int os9_signal(int type, RETSIGTYPE (*handler)())
{
	intercept(handler);
}

#include <sgstat.h>

int isatty(int f)
{
	struct sgbuf sgbuf;

	if (_gs_opt(f, &sgbuf) < 0)
		return -1;
	return (sgbuf.sg_class == 0);
}
	
#endif

public void sleep_ms(int ms)
{
#if MSDOS_COMPILER==WIN32C
	Sleep(ms);
#else
#if HAVE_NANOSLEEP
	int sec = ms / 1000;
	struct timespec t = { sec, (ms - sec*1000) * 1000000 };
	nanosleep(&t, NULL);
#else
#if HAVE_USLEEP
	usleep(ms * 1000);
#else
	sleep(ms / 1000 + (ms % 1000 != 0));
#endif
#endif
#endif
}
