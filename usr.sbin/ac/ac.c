/*
 *      Copyright (c) 1994 Christopher G. Demetriou.
 *      @(#)Copyright (c) 1994, Simon J. Gerraty.
 *
 *      This is free software.  It comes with NO WARRANTY.
 *      Permission to use, modify and distribute this source code
 *      is granted subject to the following conditions.
 *      1/ that the above copyright notice and this notice
 *      are preserved in all copies and that due credit be given
 *      to the author.
 *      2/ that any changes to this code are clearly commented
 *      as such so that the author does not get blamed for bugs
 *      other than his own.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/time.h>
#include <err.h>
#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <timeconv.h>
#include <unistd.h>
#include <utmpx.h>

/*
 * this is for our list of currently logged in sessions
 */
struct utmp_list {
	struct utmp_list *next;
	struct utmpx usr;
};

/*
 * this is for our list of users that are accumulating time.
 */
struct user_list {
	struct user_list *next;
	char	name[sizeof(((struct utmpx *)0)->ut_user)];
	time_t	secs;
};

/*
 * this is for chosing whether to ignore a login
 */
struct tty_list {
	struct tty_list *next;
	char	name[sizeof(((struct utmpx *)0)->ut_host) + 2];
	size_t	len;
	int	ret;
};

/*
 * globals - yes yuk
 */
#ifdef CONSOLE_TTY
static char 	*Console = CONSOLE_TTY;
#endif
static time_t	Total = 0;
static time_t	FirstTime = 0;
static int	Flags = 0;
static struct user_list *Users = NULL;
static struct tty_list *Ttys = NULL;

#define NEW(type) (type *)malloc(sizeof (type))

#define	AC_W	1				/* not _PATH_WTMP */
#define	AC_D	2				/* daily totals (ignore -p) */
#define	AC_P	4				/* per-user totals */
#define	AC_U	8				/* specified users only */
#define	AC_T	16				/* specified ttys only */

#ifdef DEBUG
static int Debug = 0;
#endif

int			main(int, char **);
int			ac(const char *);
struct tty_list		*add_tty(char *);
#ifdef DEBUG
const char		*debug_pfx(const struct utmpx *, const struct utmpx *);
#endif
int			do_tty(char *);
struct utmp_list	*log_in(struct utmp_list *, struct utmpx *);
struct utmp_list	*log_out(struct utmp_list *, struct utmpx *);
int			on_console(struct utmp_list *);
void			show(const char *, time_t);
void			show_today(struct user_list *, struct utmp_list *,
			    time_t);
void			show_users(struct user_list *);
struct user_list	*update_user(struct user_list *, char *, time_t);
void			usage(void);

struct tty_list *
add_tty(char *name)
{
	struct tty_list *tp;
	char *rcp;

	Flags |= AC_T;

	if ((tp = NEW(struct tty_list)) == NULL)
		errx(1, "malloc failed");
	tp->len = 0;				/* full match */
	tp->ret = 1;				/* do if match */
	if (*name == '!') {			/* don't do if match */
		tp->ret = 0;
		name++;
	}
	strlcpy(tp->name, name, sizeof (tp->name));
	if ((rcp = strchr(tp->name, '*')) != NULL) {	/* wild card */
		*rcp = '\0';
		tp->len = strlen(tp->name);	/* match len bytes only */
	}
	tp->next = Ttys;
	Ttys = tp;
	return Ttys;
}

/*
 * should we process the named tty?
 */
int
do_tty(char *name)
{
	struct tty_list *tp;
	int def_ret = 0;

	for (tp = Ttys; tp != NULL; tp = tp->next) {
		if (tp->ret == 0)		/* specific don't */
			def_ret = 1;		/* default do */
		if (tp->len != 0) {
			if (strncmp(name, tp->name, tp->len) == 0)
				return tp->ret;
		} else {
			if (strncmp(name, tp->name, sizeof (tp->name)) == 0)
				return tp->ret;
		}
	}
	return def_ret;
}

#ifdef CONSOLE_TTY
/*
 * is someone logged in on Console?
 */
int
on_console(struct utmp_list *head)
{
	struct utmp_list *up;

	for (up = head; up; up = up->next) {
		if (strcmp(up->usr.ut_line, Console) == 0)
			return 1;
	}
	return 0;
}
#endif

/*
 * update user's login time
 */
struct user_list *
update_user(struct user_list *head, char *name, time_t secs)
{
	struct user_list *up;

	for (up = head; up != NULL; up = up->next) {
		if (strcmp(up->name, name) == 0) {
			up->secs += secs;
			Total += secs;
			return head;
		}
	}
	/*
	 * not found so add new user unless specified users only
	 */
	if (Flags & AC_U)
		return head;

	if ((up = NEW(struct user_list)) == NULL)
		errx(1, "malloc failed");
	up->next = head;
	strlcpy(up->name, name, sizeof (up->name));
	up->secs = secs;
	Total += secs;
	return up;
}

#ifdef DEBUG
/*
 * Create a string which is the standard prefix for a debug line.  It
 * includes a timestamp (perhaps with year), device-name, and user-name.
 */
const char *
debug_pfx(const struct utmpx *event_up, const struct utmpx *userinf_up)
{
	static char str_result[40 + sizeof(userinf_up->ut_line) +
	    sizeof(userinf_up->ut_user)];
	static char thisyear[5];
	size_t maxcopy;
	time_t ut_timecopy;

	if (thisyear[0] == '\0') {
		/* Figure out what "this year" is. */
		time(&ut_timecopy);
		strlcpy(str_result, ctime(&ut_timecopy), sizeof(str_result));
		strlcpy(thisyear, &str_result[20], sizeof(thisyear));
	}

	if (event_up->ut_tv.tv_sec == 0)
		strlcpy(str_result, "*ZeroTime* --:--:-- ", sizeof(str_result));
	else {
		ut_timecopy = event_up->ut_tv.tv_sec;
		strlcpy(str_result, ctime(&ut_timecopy), sizeof(str_result));
		/*
		 * Include the year, if it is not the same year as "now".
		 */
		if (strncmp(&str_result[20], thisyear, 4) == 0)
			str_result[20] = '\0';
		else {
			str_result[24] = ' ';		/* Replace a '\n' */
			str_result[25] = '\0';
		}
	}

	if (userinf_up->ut_line[0] == '\0')
		strlcat(str_result, "NoDev", sizeof(str_result));
	else {
		maxcopy = strlen(str_result) + sizeof(userinf_up->ut_line);
		if (maxcopy > sizeof(str_result))
			maxcopy = sizeof(str_result);
		strlcat(str_result, userinf_up->ut_line, maxcopy);
	}
	strlcat(str_result, ": ", sizeof(str_result));

	if (userinf_up->ut_user[0] == '\0')
		strlcat(str_result, "LogOff", sizeof(str_result));
	else {
		maxcopy = strlen(str_result) + sizeof(userinf_up->ut_user);
		if (maxcopy > sizeof(str_result))
			maxcopy = sizeof(str_result);
		strlcat(str_result, userinf_up->ut_user, maxcopy);
	}

	return (str_result);
}
#endif

int
main(int argc, char *argv[])
{
	const char *wtmpf = NULL;
	int c;

	(void) setlocale(LC_TIME, "");

	while ((c = getopt(argc, argv, "Dc:dpt:w:")) != -1) {
		switch (c) {
#ifdef DEBUG
		case 'D':
			Debug++;
			break;
#endif
		case 'c':
#ifdef CONSOLE_TTY
			Console = optarg;
#else
			usage();		/* XXX */
#endif
			break;
		case 'd':
			Flags |= AC_D;
			break;
		case 'p':
			Flags |= AC_P;
			break;
		case 't':			/* only do specified ttys */
			add_tty(optarg);
			break;
		case 'w':
			Flags |= AC_W;
			wtmpf = optarg;
			break;
		case '?':
		default:
			usage();
			break;
		}
	}
	if (optind < argc) {
		/*
		 * initialize user list
		 */
		for (; optind < argc; optind++) {
			Users = update_user(Users, argv[optind], (time_t)0);
		}
		Flags |= AC_U;			/* freeze user list */
	}
	if (Flags & AC_D)
		Flags &= ~AC_P;
	ac(wtmpf);

	return 0;
}

/*
 * print login time in decimal hours
 */
void
show(const char *name, time_t secs)
{
	(void)printf("\t%-*s %8.2f\n",
	    (int)sizeof(((struct utmpx *)0)->ut_user), name,
	    ((double)secs / 3600));
}

void
show_users(struct user_list *list)
{
	struct user_list *lp;

	for (lp = list; lp; lp = lp->next)
		show(lp->name, lp->secs);
}

/*
 * print total login time for 24hr period in decimal hours
 */
void
show_today(struct user_list *users, struct utmp_list *logins, time_t secs)
{
	struct user_list *up;
	struct utmp_list *lp;
	char date[64];
	time_t yesterday = secs - 1;
	static int d_first = -1;

	if (d_first < 0)
		d_first = (*nl_langinfo(D_MD_ORDER) == 'd');
	(void)strftime(date, sizeof (date),
		       d_first ? "%e %b  total" : "%b %e  total",
		       localtime(&yesterday));

	/* restore the missing second */
	yesterday++;

	for (lp = logins; lp != NULL; lp = lp->next) {
		secs = yesterday - lp->usr.ut_tv.tv_sec;
		Users = update_user(Users, lp->usr.ut_user, secs);
		lp->usr.ut_tv.tv_sec = yesterday; /* as if they just logged in */
	}
	secs = 0;
	for (up = users; up != NULL; up = up->next) {
		secs += up->secs;
		up->secs = 0;			/* for next day */
	}
	if (secs)
		(void)printf("%s %11.2f\n", date, ((double)secs / 3600));
}

/*
 * log a user out and update their times.
 * if ut_line is "~", we log all users out as the system has
 * been shut down.
 */
struct utmp_list *
log_out(struct utmp_list *head, struct utmpx *up)
{
	struct utmp_list *lp, *lp2, *tlp;
	time_t secs;

	for (lp = head, lp2 = NULL; lp != NULL; )
		if (up->ut_type == BOOT_TIME || up->ut_type == SHUTDOWN_TIME ||
		    (up->ut_type == DEAD_PROCESS &&
		    memcmp(lp->usr.ut_id, up->ut_id, sizeof up->ut_id) == 0)) {
			secs = up->ut_tv.tv_sec - lp->usr.ut_tv.tv_sec;
			Users = update_user(Users, lp->usr.ut_user, secs);
#ifdef DEBUG
			if (Debug)
				printf("%s logged out (%2d:%02d:%02d)\n",
				    debug_pfx(up, &lp->usr), (int)(secs / 3600),
				    (int)((secs % 3600) / 60),
				    (int)(secs % 60));
#endif
			/*
			 * now lose it
			 */
			tlp = lp;
			lp = lp->next;
			if (tlp == head)
				head = lp;
			else if (lp2 != NULL)
				lp2->next = lp;
			free(tlp);
		} else {
			lp2 = lp;
			lp = lp->next;
		}
	return head;
}


/*
 * if do_tty says ok, login a user
 */
struct utmp_list *
log_in(struct utmp_list *head, struct utmpx *up)
{
	struct utmp_list *lp;

	/*
	 * this could be a login. if we're not dealing with
	 * the console name, say it is.
	 *
	 * If we are, and if ut_host==":0.0" we know that it
	 * isn't a real login. _But_ if we have not yet recorded
	 * someone being logged in on Console - due to the wtmp
	 * file starting after they logged in, we'll pretend they
	 * logged in, at the start of the wtmp file.
	 */

#ifdef CONSOLE_TTY
	if (up->ut_host[0] == ':') {
		/*
		 * SunOS 4.0.2 does not treat ":0.0" as special but we
		 * do.
		 */
		if (on_console(head))
			return head;
		/*
		 * ok, no recorded login, so they were here when wtmp
		 * started!  Adjust ut_time!
		 */
		up->ut_time = FirstTime;
		/*
		 * this allows us to pick the right logout
		 */
		strlcpy(up->ut_line, Console, sizeof (up->ut_line));
	}
#endif
	/*
	 * If we are doing specified ttys only, we ignore
	 * anything else.
	 */
	if (Flags & AC_T)
		if (!do_tty(up->ut_line))
			return head;

	/*
	 * go ahead and log them in
	 */
	if ((lp = NEW(struct utmp_list)) == NULL)
		errx(1, "malloc failed");
	lp->next = head;
	head = lp;
	memmove(&lp->usr, up, sizeof *up);
#ifdef DEBUG
	if (Debug) {
		printf("%s logged in", debug_pfx(&lp->usr, up));
		if (*up->ut_host)
			printf(" (%-.*s)", (int)sizeof(up->ut_host),
			    up->ut_host);
		putchar('\n');
	}
#endif
	return head;
}

int
ac(const char *file)
{
	struct utmp_list *lp, *head = NULL;
	struct utmpx *usr, usht;
	struct tm *ltm;
	time_t prev_secs, secs, ut_timecopy;
	int day, rfound, tchanged, tskipped;

	day = -1;
	prev_secs = 1;			/* Minimum acceptable date == 1970 */
	rfound = tchanged = tskipped = 0;
	secs = 0;
	if (setutxdb(UTXDB_LOG, file) != 0)
		err(1, "%s", file);
	while ((usr = getutxent()) != NULL) {
		rfound++;
		ut_timecopy = usr->ut_tv.tv_sec;
		/*
		 * With sparc64 using 64-bit time_t's, there is some system
		 * routine which sets ut_time==0 (the high-order word of a
		 * 64-bit time) instead of a 32-bit time value.  For those
		 * wtmp files, it is "more-accurate" to substitute the most-
		 * recent time found, instead of throwing away the entire
		 * record.  While it is still just a guess, it is a better
		 * guess than throwing away a log-off record and therefore
		 * counting a session as if it continued to the end of the
		 * month, or the next system-reboot.
		 */
		if (ut_timecopy == 0 && prev_secs > 1) {
#ifdef DEBUG
			if (Debug)
				printf("%s - date changed to: %s",
				    debug_pfx(usr, usr), ctime(&prev_secs));
#endif
			tchanged++;
			usr->ut_tv.tv_sec = ut_timecopy = prev_secs;
		}
		/*
		 * Skip records where the time goes backwards.
		 */
		if (ut_timecopy < prev_secs) {
#ifdef DEBUG
			if (Debug)
				printf("%s - bad date, record skipped\n",
				    debug_pfx(usr, usr));
#endif
			tskipped++;
			continue;	/* Skip this invalid record. */
		}
		prev_secs = ut_timecopy;

		if (!FirstTime)
			FirstTime = ut_timecopy;
		if (Flags & AC_D) {
			ltm = localtime(&ut_timecopy);
			if (day >= 0 && day != ltm->tm_yday) {
				day = ltm->tm_yday;
				/*
				 * print yesterday's total
				 */
				secs = ut_timecopy;
				secs -= ltm->tm_sec;
				secs -= 60 * ltm->tm_min;
				secs -= 3600 * ltm->tm_hour;
				show_today(Users, head, secs);
			} else
				day = ltm->tm_yday;
		}
		switch(usr->ut_type) {
		case OLD_TIME:
			secs = ut_timecopy;
			break;
		case NEW_TIME:
			secs -= ut_timecopy;
			/*
			 * adjust time for those logged in
			 */
			for (lp = head; lp != NULL; lp = lp->next)
				lp->usr.ut_tv.tv_sec -= secs;
			break;
		case BOOT_TIME:
		case SHUTDOWN_TIME:
			head = log_out(head, usr);
			FirstTime = ut_timecopy; /* shouldn't be needed */
			break;
		case USER_PROCESS:
			/*
			 * if they came in on tty[p-sP-S]*, then it is only
			 * a login session if the ut_host field is non-empty
			 */
			if (strncmp(usr->ut_line, "tty", 3) != 0 ||
			    strchr("pqrsPQRS", usr->ut_line[3]) == NULL ||
			    *usr->ut_host != '\0')
				head = log_in(head, usr);
#ifdef DEBUG
			else if (Debug > 1)
				/* Things such as 'screen' sessions. */
				printf("%s - record ignored\n",
				    debug_pfx(usr, usr));
#endif
			break;
		case DEAD_PROCESS:
			head = log_out(head, usr);
			break;
		}
	}
	endutxent();
	if (!(Flags & AC_W))
		usht.ut_tv.tv_sec = time(NULL);
	usht.ut_type = SHUTDOWN_TIME;

	if (Flags & AC_D) {
		ut_timecopy = usht.ut_tv.tv_sec;
		ltm = localtime(&ut_timecopy);
		if (day >= 0 && day != ltm->tm_yday) {
			/*
			 * print yesterday's total
			 */
			secs = ut_timecopy;
			secs -= ltm->tm_sec;
			secs -= 60 * ltm->tm_min;
			secs -= 3600 * ltm->tm_hour;
			show_today(Users, head, secs);
		}
	}
	/*
	 * anyone still logged in gets time up to now
	 */
	head = log_out(head, &usht);

	if (Flags & AC_D)
		show_today(Users, head, time((time_t *)0));
	else {
		if (Flags & AC_P)
			show_users(Users);
		show("total", Total);
	}

	if (tskipped > 0)
		printf("(Skipped %d of %d records due to invalid time values)\n",
		    tskipped, rfound);
	if (tchanged > 0)
		printf("(Changed %d of %d records to have a more likely time value)\n",
		    tchanged, rfound);

	return 0;
}

void
usage(void)
{
	(void)fprintf(stderr,
#ifdef CONSOLE_TTY
	    "ac [-dp] [-c console] [-t tty] [-w wtmp] [users ...]\n");
#else
	    "ac [-dp] [-t tty] [-w wtmp] [users ...]\n");
#endif
	exit(1);
}
