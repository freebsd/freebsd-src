#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$Header: /a/cvs/386BSD/src/libexec/crond/misc.c,v 1.1.1.1 1993/06/12 14:55:03 rgrimes Exp $";
#endif

/* vix 26jan87 [RCS has the rest of the log]
 * vix 15jan87 [added TIOCNOTTY, thanks csg@pyramid]
 * vix 30dec86 [written]
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00131
 * --------------------         -----   ----------------------
 *
 * 06 Apr 93	Adam Glass	Fixes so it compiles quitely
 *
 */

/* Copyright 1988,1990 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie, 329 Noe Street, San Francisco, CA, 94114, (415) 864-7013,
 * paul@vixie.sf.ca.us || {hoptoad,pacbell,decwrl,crash}!vixie!paul
 */


#include "cron.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <errno.h>
#if defined(ATT)
# include <fcntl.h>
#endif


void log_it(), be_different(), acquire_daemonlock();


char *
savestr(str)
	char	*str;
{
	extern	int	strlen();
	extern	char	*malloc(), *strcpy();
	/**/	char	*temp;

	temp = malloc((unsigned) (strlen(str) + 1));
	(void) strcpy(temp, str);
	return temp;
}


int
nocase_strcmp(left, right)
	char	*left;
	char	*right;
{
	while (*left && (MkLower(*left) == MkLower(*right)))
	{
		left++;
		right++;
	}
	return MkLower(*left) - MkLower(*right);
}


int
strcmp_until(left, right, until)
	char	*left;
	char	*right;
	char	until;
{
	register int	diff;

	Debug(DPARS|DEXT, ("strcmp_until(%s,%s,%c) ... ", left, right, until))

	while (*left && *left != until && *left == *right)
	{
		left++;
		right++;
	}

	if (	(*left=='\0' || *left == until) 
	    &&	(*right=='\0' || *right == until)
	   )
		diff = 0;
	else
		diff = *left - *right;

	Debug(DPARS|DEXT, ("%d\n", diff))

	return diff;
}


/* strdtb(s) - delete trailing blanks in string 's' and return new length
 */
int
strdtb(s)
	register char	*s;
{
	register char	*x = s;

	/* scan forward to the null
	 */
	while (*x)
		x++;

	/* scan backward to either the first character before the string,
	 * or the last non-blank in the string, whichever comes first.
	 */
	do	{x--;}
	while (x >= s && isspace(*x));

	/* one character beyond where we stopped above is where the null
	 * goes.
	 */
	*++x = '\0';

	/* the difference between the position of the null character and
	 * the position of the first character of the string is the length.
	 */
	return x - s;
}


int
set_debug_flags(flags)
	char	*flags;
{
	/* debug flags are of the form    flag[,flag ...]
	 *
	 * if an error occurs, print a message to stdout and return FALSE.
	 * otherwise return TRUE after setting ERROR_FLAGS.
	 */

#if !DEBUGGING

	printf("this program was compiled without debugging enabled\n");
	return FALSE;

#else /* DEBUGGING */

	char	*pc = flags;

	DebugFlags = 0;

	while (*pc)
	{
		char	**test;
		int	mask;

		/* try to find debug flag name in our list.
		 */
		for (	test = DebugFlagNames, mask = 1;
			*test && strcmp_until(*test, pc, ',');
			test++, mask <<= 1
		    )
			;

		if (!*test)
		{
			fprintf(stderr,
				"unrecognized debug flag <%s> <%s>\n",
				flags, pc);
			return FALSE;
		}

		DebugFlags |= mask;

		/* skip to the next flag
		 */
		while (*pc && *pc != ',')
			pc++;
		if (*pc == ',')
			pc++;
	}

	if (DebugFlags)
	{
		int	flag;

		fprintf(stderr, "debug flags enabled:");

		for (flag = 0;  DebugFlagNames[flag];  flag++)
			if (DebugFlags & (1 << flag))
				fprintf(stderr, " %s", DebugFlagNames[flag]);
		fprintf(stderr, "\n");
	}

	return TRUE;

#endif /* DEBUGGING */
}


#if defined(BSD)
void
set_cron_uid()
{
	int	seteuid();

	if (seteuid(ROOT_UID) < OK)
	{
		perror("seteuid");
		exit(ERROR_EXIT);
	}
}
#endif

#if defined(ATT)
void
set_cron_uid()
{
	int	setuid();

	if (setuid(ROOT_UID) < OK)
	{
		perror("setuid");
		exit(ERROR_EXIT);
	}
}
#endif

void
set_cron_cwd()
{
	extern int	errno;
	struct stat	sb;

	/* first check for CRONDIR ("/var/cron" or some such)
	 */
	if (stat(CRONDIR, &sb) < OK && errno == ENOENT) {
		perror(CRONDIR);
		if (OK == mkdir(CRONDIR, 0700)) {
			fprintf(stderr, "%s: created\n", CRONDIR);
			stat(CRONDIR, &sb);
		} else {
			fprintf(stderr, "%s: ", CRONDIR);
			perror("mkdir");
			exit(ERROR_EXIT);
		}
	}
	if (!(sb.st_mode & S_IFDIR)) {
		fprintf(stderr, "'%s' is not a directory, bailing out.\n",
			CRONDIR);
		exit(ERROR_EXIT);
	}
	if (chdir(CRONDIR) < OK) {
		fprintf(stderr, "cannot chdir(%s), bailing out.\n", CRONDIR);
		perror(CRONDIR);
		exit(ERROR_EXIT);
	}

	/* CRONDIR okay (now==CWD), now look at SPOOL_DIR ("tabs" or some such)
	 */
	if (stat(SPOOL_DIR, &sb) < OK && errno == ENOENT) {
		perror(SPOOL_DIR);
		if (OK == mkdir(SPOOL_DIR, 0700)) {
			fprintf(stderr, "%s: created\n", SPOOL_DIR);
			stat(SPOOL_DIR, &sb);
		} else {
			fprintf(stderr, "%s: ", SPOOL_DIR);
			perror("mkdir");
			exit(ERROR_EXIT);
		}
	}
	if (!(sb.st_mode & S_IFDIR)) {
		fprintf(stderr, "'%s' is not a directory, bailing out.\n",
			SPOOL_DIR);
		exit(ERROR_EXIT);
	}
}


#if defined(BSD)
void
be_different()
{
	/* release the control terminal:
	 *  get new pgrp (name after our PID)
	 *  do an IOCTL to void tty association
	 */

	auto int	fd;

	(void) setpgrp(0, getpid());

	if ((fd = open("/dev/tty", 2)) >= 0)
	{
		(void) ioctl(fd, TIOCNOTTY, (char*)0);
		(void) close(fd);
	}
}
#endif /*BSD*/

#if defined(ATT)
void
be_different()
{
	/* not being a system V wiz, I don't know if this is what you have
	 * to do to release your control terminal.  what I want to accomplish
	 * is to keep this process from getting any signals from the tty.
	 *
	 * some system V person should let me know if this works... (vixie)
	 */
	int	setpgrp(), close(), open();

	(void) setpgrp();

	(void) close(STDIN);	(void) open("/dev/null", 0);
	(void) close(STDOUT);	(void) open("/dev/null", 1);
	(void) close(STDERR);	(void) open("/dev/null", 2);
}
#endif /*ATT*/


/* acquire_daemonlock() - write our PID into /etc/crond.pid, unless
 *	another daemon is already running, which we detect here.
 */
void
acquire_daemonlock()
{
	int	fd = open(PIDFILE, O_RDWR|O_CREAT, 0644);
	FILE	*fp = fdopen(fd, "r+");
	int	pid = getpid(), otherpid;
	char	buf[MAX_TEMPSTR];

	if (fd < 0 || fp == NULL) {
		sprintf(buf, "can't open or create %s, errno %d", PIDFILE, errno);
		log_it("CROND", pid, "DEATH", buf);
		exit(ERROR_EXIT);
	}

	if (flock(fd, LOCK_EX|LOCK_NB) < OK) {
		int save_errno = errno;

		fscanf(fp, "%d", &otherpid);
		sprintf(buf, "can't lock %s, otherpid may be %d, errno %d",
			PIDFILE, otherpid, save_errno);
		log_it("CROND", pid, "DEATH", buf);
		exit(ERROR_EXIT);
	}

	rewind(fp);
	fprintf(fp, "%d\n", pid);
	fflush(fp);
	ftruncate(fd, ftell(fp));

	/* abandon fd and fp even though the file is open. we need to
	 * keep it open and locked, but we don't need the handles elsewhere.
	 */
}

/* get_char(file) : like getc() but increment LineNumber on newlines
 */
int
get_char(file)
	FILE	*file;
{
	int	ch;

	ch = getc(file);
	if (ch == '\n')
		Set_LineNum(LineNumber + 1)
	return ch;
}


/* unget_char(ch, file) : like ungetc but do LineNumber processing
 */
void
unget_char(ch, file)
	int	ch;
	FILE	*file;
{
	ungetc(ch, file);
	if (ch == '\n')
		Set_LineNum(LineNumber - 1)
}


/* get_string(str, max, file, termstr) : like fgets() but
 *		(1) has terminator string which should include \n
 *		(2) will always leave room for the null
 *		(3) uses get_char() so LineNumber will be accurate
 *		(4) returns EOF or terminating character, whichever
 */
int
get_string(string, size, file, terms)
	char	*string;
	int	size;
	FILE	*file;
	char	*terms;
{
	int	ch;
	char	*index();

	while (EOF != (ch = get_char(file)) && !index(terms, ch))
		if (size > 1)
		{
			*string++ = (char) ch;
			size--;
		}

	if (size > 0)
		*string = '\0';

	return ch;
}


/* skip_comments(file) : read past comment (if any)
 */
void
skip_comments(file)
	FILE	*file;
{
	int	ch;

	while (EOF != (ch = get_char(file)))
	{
		/* ch is now the first character of a line.
		 */

		while (ch == ' ' || ch == '\t')
			ch = get_char(file);

		if (ch == EOF)
			break;

		/* ch is now the first non-blank character of a line.
		 */

		if (ch != '\n' && ch != '#')
			break;

		/* ch must be a newline or comment as first non-blank
		 * character on a line.
		 */

		while (ch != '\n' && ch != EOF)
			ch = get_char(file);

		/* ch is now the newline of a line which we're going to
		 * ignore.
		 */
	}
	unget_char(ch, file);
}

/* int in_file(char *string, FILE *file)
 *	return TRUE if one of the lines in file matches string exactly,
 *	FALSE otherwise.
 */
int
in_file(string, file)
	char *string;
	FILE *file;
{
	char line[MAX_TEMPSTR];

	/* let's be persnickety today.
	 */
	if (!file) {
		if (!string)
			string = "0x0";
		fprintf(stderr,
			"in_file(\"%s\", 0x%x): called with NULL file--botch",
			string, file);
		exit(ERROR_EXIT);
	}

	rewind(file);
	while (fgets(line, MAX_TEMPSTR, file)) {
		if (line[0] != '\0')
			line[strlen(line)-1] = '\0';
		if (0 == strcmp(line, string))
			return TRUE;
	}
	return FALSE;
}


/* int allowed(char *username)
 *	returns TRUE if (ALLOW_FILE exists and user is listed)
 *	or (DENY_FILE exists and user is NOT listed)
 *	or (neither file exists but user=="root" so it's okay)
 */
int
allowed(username)
	char *username;
{
	static int	init = FALSE;
	static FILE	*allow, *deny;

	if (!init) {
		init = TRUE;
#if defined(ALLOW_FILE) && defined(DENY_FILE)
		allow = fopen(ALLOW_FILE, "r");
		deny = fopen(DENY_FILE, "r");
		Debug(DMISC, ("allow/deny enabled, %d/%d\n", !!allow, !!deny))
#else
		allow = NULL;
		deny = NULL;
#endif
	}

	if (allow)
		return (in_file(username, allow));
	if (deny)
		return (!in_file(username, deny));

#if defined(ALLOW_ONLY_ROOT)
	return (strcmp(username, ROOT_USER) == 0);
#else
	return TRUE;
#endif
}


#if defined(LOG_FILE) || defined(SYSLOG)
void
log_it(username, pid, event, detail)
	char	*username;
	int	pid;
	char	*event;
	char	*detail;
{
#if defined(LOG_FILE)
	extern struct tm	*localtime();
	extern long		time();
	extern char		*malloc();
	auto char		*msg;
	auto long		now = time((long *) 0);
	register struct tm	*t = localtime(&now);
	static int		log_fd = -1;
#endif /*LOG_FILE*/

#if defined(SYSLOG)
	static int		syslog_open = 0;
#endif


#if defined(LOG_FILE)
	/* we assume that MAX_TEMPSTR will hold the date, time, &punctuation.
	 */
	msg = malloc(strlen(username)
	      + strlen(event)
	      + strlen(detail)
	      + MAX_TEMPSTR);

	if (log_fd < OK) {
		log_fd = open(LOG_FILE, O_WRONLY|O_APPEND|O_CREAT, 0600);
		if (log_fd < OK) {
			fprintf(stderr, "%s: can't open log file\n", ProgramName);
			perror(LOG_FILE);
		}
	}

	/* we have to sprintf() it because fprintf() doesn't always write
	 * everything out in one chunk and this has to be atomically appended
	 * to the log file.
	 */
	sprintf(msg, "%s (%02d/%02d-%02d:%02d:%02d-%d) %s (%s)\n",
		username,
		t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, pid,
		event, detail);

	/* we have to run strlen() because sprintf() returns (char*) on BSD
	 */
	if (log_fd < OK || write(log_fd, msg, strlen(msg)) < OK) {
		fprintf(stderr, "%s: can't write to log file\n", ProgramName);
		if (log_fd >= OK)
			perror(LOG_FILE);
		write(STDERR, msg, strlen(msg));
	}

	/* I suppose we could use alloca()...
	 */
	free(msg);
#endif /*LOG_FILE*/

#if defined(SYSLOG)
	if (!syslog_open) {
		/* we don't use LOG_PID since the pid passed to us by
		 * our client may not be our own.  therefore we want to
		 * print the pid ourselves.
		 */
# ifdef LOG_CRON
		openlog(ProgramName, 0, LOG_CRON);
# else
# ifdef LOG_DAEMON
		openlog(ProgramName, 0, LOG_DAEMON);
# else
		openlog(ProgramName, 0);
# endif /*LOG_DAEMON*/
# endif /*LOG_CRON*/
		syslog_open = TRUE;		/* assume openlog success */
	}

	syslog(LOG_INFO, "(%s %d) %s (%s)\n",
		username, pid, event, detail);

#endif /*SYSLOG*/

	if (DebugFlags) {
		fprintf(stderr, "log_it: (%s %d) %s (%s)",
			username, pid, event, detail);
	}
}
#endif /*LOG_FILE || SYSLOG */


/* two warnings:
 *	(1) this routine is fairly slow
 *	(2) it returns a pointer to static storage
 */
char *
first_word(s, t)
	local char *s;	/* string we want the first word of */
	local char *t;	/* terminators, implicitly including \0 */
{
	static char retbuf[2][MAX_TEMPSTR + 1];	/* sure wish I had GC */
	static int retsel = 0;
	local char *rb, *rp;
	extern char *index();

	/* select a return buffer */
	retsel = 1-retsel;
	rb = &retbuf[retsel][0];
	rp = rb;

	/* skip any leading terminators */
	while (*s && (NULL != index(t, *s))) {s++;}

	/* copy until next terminator or full buffer */
	while (*s && (NULL == index(t, *s)) && (rp < &rb[MAX_TEMPSTR])) {
		*rp++ = *s++;
	}

	/* finish the return-string and return it */
	*rp = '\0';
	return rb;
}


/* warning:
 *	heavily ascii-dependent.
 */

void
mkprint(dst, src, len)
	register char *dst;
	register unsigned char *src;
	register int len;
{
	while (len-- > 0)
	{
		register unsigned char ch = *src++;

		if (ch < ' ') {			/* control character */
			*dst++ = '^';
			*dst++ = ch + '@';
		} else if (ch < 0177) {		/* printable */
			*dst++ = ch;
		} else if (ch == 0177) {	/* delete/rubout */
			*dst++ = '^';
			*dst++ = '?';
		} else {			/* parity character */
			sprintf(dst, "\\%03o", ch);
			dst += 4;
		}
	}
	*dst = NULL;
}


/* warning:
 *	returns a pointer to malloc'd storage, you must call free yourself.
 */

char *
mkprints(src, len)
	register unsigned char *src;
	register unsigned int len;
{
	extern char *malloc();
	register char *dst = malloc(len*4 + 1);

	mkprint(dst, src, len);

	return dst;
}
