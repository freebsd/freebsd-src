#ifndef lint
#ifndef NOID
static char	elsieid[] = "@(#)zdump.c	7.10";
#endif /* !defined NOID */
#endif /* !defined lint */

/*
** This code has been made independent of the rest of the time
** conversion package to increase confidence in the verification it provides.
** You can use this code to help in verifying other implementations.
*/

#include "stdio.h"	/* for stdout, stderr */
#include "string.h"	/* for strcpy */
#include "sys/types.h"	/* for time_t */
#include "time.h"	/* for struct tm */

#ifndef MAX_STRING_LENGTH
#define MAX_STRING_LENGTH	1024
#endif /* !defined MAX_STRING_LENGTH */

#ifndef TRUE
#define TRUE		1
#endif /* !defined TRUE */

#ifndef FALSE
#define FALSE		0
#endif /* !defined FALSE */

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS	0
#endif /* !defined EXIT_SUCCESS */

#ifndef EXIT_FAILURE
#define EXIT_FAILURE	1
#endif /* !defined EXIT_FAILURE */

#ifndef SECSPERMIN
#define SECSPERMIN	60
#endif /* !defined SECSPERMIN */

#ifndef MINSPERHOUR
#define MINSPERHOUR	60
#endif /* !defined MINSPERHOUR */

#ifndef SECSPERHOUR
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#endif /* !defined SECSPERHOUR */

#ifndef HOURSPERDAY
#define HOURSPERDAY	24
#endif /* !defined HOURSPERDAY */

#ifndef EPOCH_YEAR
#define EPOCH_YEAR	1970
#endif /* !defined EPOCH_YEAR */

#ifndef TM_YEAR_BASE
#define TM_YEAR_BASE	1900
#endif /* !defined TM_YEAR_BASE */

#ifndef DAYSPERNYEAR
#define DAYSPERNYEAR	365
#endif /* !defined DAYSPERNYEAR */

#ifndef isleap
#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
#endif /* !defined isleap */

extern char **	environ;
extern int	getopt();
extern char *	optarg;
extern int	optind;
extern time_t	time();
extern char *	tzname[2];
extern void	tzset();

#ifdef USG
extern void	exit();
extern void	perror();
#endif /* defined USG */

static char *	abbr();
static long	delta();
static time_t	hunt();
static int	longest;
static char *	progname;
static void	show();

int
main(argc, argv)
int	argc;
char *	argv[];
{
	register int	i, c;
	register int	vflag;
	register char *	cutoff;
	register int	cutyear;
	register long	cuttime;
	time_t		now;
	time_t		t, newt;
	time_t		hibit;
	struct tm	tm, newtm;

	progname = argv[0];
	vflag = 0;
	cutoff = NULL;
	while ((c = getopt(argc, argv, "c:v")) == 'c' || c == 'v')
		if (c == 'v')
			vflag = 1;
		else	cutoff = optarg;
	if (c != EOF ||
		(optind == argc - 1 && strcmp(argv[optind], "=") == 0)) {
			(void) fprintf(stderr,
"%s: usage is %s [ -v ] [ -c cutoff ] zonename ...\n",
				argv[0], argv[0]);
			(void) exit(EXIT_FAILURE);
	}
	if (cutoff != NULL) {
		int	y;

		cutyear = atoi(cutoff);
		cuttime = 0;
		for (y = EPOCH_YEAR; y < cutyear; ++y)
			cuttime += DAYSPERNYEAR + isleap(y);
		cuttime *= SECSPERHOUR * HOURSPERDAY;
	}
	(void) time(&now);
	longest = 0;
	for (i = optind; i < argc; ++i)
		if (strlen(argv[i]) > longest)
			longest = strlen(argv[i]);
	for (hibit = 1; (hibit << 1) != 0; hibit <<= 1)
		continue;
	for (i = optind; i < argc; ++i) {
		register char **	saveenv;
		static char		buf[MAX_STRING_LENGTH];
		char *			fakeenv[2];

		if (strlen(argv[i]) + 4 > sizeof buf) {
			(void) fflush(stdout);
			(void) fprintf(stderr, "%s: argument too long -- %s\n",
				progname, argv[i]);
			(void) exit(EXIT_FAILURE);
		}
		(void) strcpy(buf, "TZ=");
		(void) strcat(buf, argv[i]);
		fakeenv[0] = buf;
		fakeenv[1] = NULL;
		saveenv = environ;
		environ = fakeenv;
		(void) tzset();
		environ = saveenv;
		show(argv[i], now, FALSE);
		if (!vflag)
			continue;
		/*
		** Get lowest value of t.
		*/
		t = hibit;
		if (t > 0)		/* time_t is unsigned */
			t = 0;
		show(argv[i], t, TRUE);
		t += SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
		tm = *localtime(&t);
		(void) strncpy(buf, abbr(&tm), (sizeof buf) - 1);
		for ( ; ; ) {
			if (cutoff != NULL && t >= cuttime)
				break;
			newt = t + SECSPERHOUR * 12;
			if (cutoff != NULL && newt >= cuttime)
				break;
			if (newt <= t)
				break;
			newtm = *localtime(&newt);
			if (delta(&newtm, &tm) != (newt - t) ||
				newtm.tm_isdst != tm.tm_isdst ||
				strcmp(abbr(&newtm), buf) != 0) {
					newt = hunt(argv[i], t, newt);
					newtm = *localtime(&newt);
					(void) strncpy(buf, abbr(&newtm),
						(sizeof buf) - 1);
			}
			t = newt;
			tm = newtm;
		}
		/*
		** Get highest value of t.
		*/
		t = ~((time_t) 0);
		if (t < 0)		/* time_t is signed */
			t &= ~hibit;
		t -= SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
		t += SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
	}
	if (fflush(stdout) || ferror(stdout)) {
		(void) fprintf(stderr, "%s: Error writing standard output ",
			argv[0]);
		(void) perror("standard output");
		(void) exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);

	/* gcc -Wall pacifier */
	for ( ; ; )
		continue;
}

static time_t
hunt(name, lot, hit)
char *	name;
time_t	lot;
time_t	hit;
{
	time_t		t;
	struct tm	lotm;
	struct tm	tm;
	static char	loab[MAX_STRING_LENGTH];

	lotm = *localtime(&lot);
	(void) strncpy(loab, abbr(&lotm), (sizeof loab) - 1);
	while ((hit - lot) >= 2) {
		t = lot / 2 + hit / 2;
		if (t <= lot)
			++t;
		else if (t >= hit)
			--t;
		tm = *localtime(&t);
		if (delta(&tm, &lotm) == (t - lot) &&
			tm.tm_isdst == lotm.tm_isdst &&
			strcmp(abbr(&tm), loab) == 0) {
				lot = t;
				lotm = tm;
		} else	hit = t;
	}
	show(name, lot, TRUE);
	show(name, hit, TRUE);
	return hit;
}

/*
** Thanks to Paul Eggert (eggert@twinsun.com) for logic used in delta.
*/

static long
delta(newp, oldp)
struct tm *	newp;
struct tm *	oldp;
{
	long	result;
	int	tmy;

	if (newp->tm_year < oldp->tm_year)
		return -delta(oldp, newp);
	result = 0;
	for (tmy = oldp->tm_year; tmy < newp->tm_year; ++tmy)
		result += DAYSPERNYEAR + isleap(tmy + TM_YEAR_BASE);
	result += newp->tm_yday - oldp->tm_yday;
	result *= HOURSPERDAY;
	result += newp->tm_hour - oldp->tm_hour;
	result *= MINSPERHOUR;
	result += newp->tm_min - oldp->tm_min;
	result *= SECSPERMIN;
	result += newp->tm_sec - oldp->tm_sec;
	return result;
}

static void
show(zone, t, v)
char *	zone;
time_t	t;
int	v;
{
	struct tm *		tmp;
	extern struct tm *	localtime();

	(void) printf("%-*s  ", longest, zone);
	if (v)
		(void) printf("%.24s GMT = ", asctime(gmtime(&t)));
	tmp = localtime(&t);
	(void) printf("%.24s", asctime(tmp));
	if (*abbr(tmp) != '\0')
		(void) printf(" %s", abbr(tmp));
	if (v) {
		(void) printf(" isdst=%d", tmp->tm_isdst);
#ifdef TM_GMTOFF
		(void) printf(" gmtoff=%ld", tmp->TM_GMTOFF);
#endif /* defined TM_GMTOFF */
	}
	(void) printf("\n");
}

static char *
abbr(tmp)
struct tm *	tmp;
{
	register char *	result;
	static char	nada[1];

	if (tmp->tm_isdst != 0 && tmp->tm_isdst != 1)
		return nada;
	result = tzname[tmp->tm_isdst];
	return (result == NULL) ? nada : result;
}
