/* Dump time zone data in a textual format.  */

/*
** This file is in the public domain, so clarified as of
** 2009-05-17 by Arthur David Olson.
*/

#include "version.h"

#ifndef NETBSD_INSPIRED
# define NETBSD_INSPIRED 1
#endif

#include "private.h"
#include <stdio.h>

#ifndef HAVE_SNPRINTF
# define HAVE_SNPRINTF (!PORT_TO_C89 || 199901 <= __STDC_VERSION__)
#endif

#ifndef HAVE_LOCALTIME_R
# define HAVE_LOCALTIME_R 1
#endif

#ifndef HAVE_LOCALTIME_RZ
# ifdef TM_ZONE
#  define HAVE_LOCALTIME_RZ (NETBSD_INSPIRED && USE_LTZ)
# else
#  define HAVE_LOCALTIME_RZ 0
# endif
#endif

#ifndef HAVE_TZSET
# define HAVE_TZSET 1
#endif

#ifndef ZDUMP_LO_YEAR
# define ZDUMP_LO_YEAR (-500)
#endif /* !defined ZDUMP_LO_YEAR */

#ifndef ZDUMP_HI_YEAR
# define ZDUMP_HI_YEAR 2500
#endif /* !defined ZDUMP_HI_YEAR */

#define SECSPERNYEAR	(SECSPERDAY * DAYSPERNYEAR)
#define SECSPERLYEAR	(SECSPERNYEAR + SECSPERDAY)
#define SECSPER400YEARS	(SECSPERNYEAR * (intmax_t) (300 + 3)	\
			 + SECSPERLYEAR * (intmax_t) (100 - 3))

/*
** True if SECSPER400YEARS is known to be representable as an
** intmax_t.  It's OK that SECSPER400YEARS_FITS can in theory be false
** even if SECSPER400YEARS is representable, because when that happens
** the code merely runs a bit more slowly, and this slowness doesn't
** occur on any practical platform.
*/
enum { SECSPER400YEARS_FITS = SECSPERLYEAR <= INTMAX_MAX / 400 };

#if HAVE_GETTEXT
# include <locale.h> /* for setlocale */
#endif /* HAVE_GETTEXT */

#if ! HAVE_LOCALTIME_RZ
# undef  timezone_t
# define timezone_t char **
#endif

#if !HAVE_POSIX_DECLS
extern int	getopt(int argc, char * const argv[],
			const char * options);
extern char *	optarg;
extern int	optind;
#endif

/* The minimum and maximum finite time values.  */
enum { atime_shift = CHAR_BIT * sizeof(time_t) - 2 };
static time_t const absolute_min_time =
  ((time_t) -1 < 0
   ? (- ((time_t) ~ (time_t) 0 < 0)
      - (((time_t) 1 << atime_shift) - 1 + ((time_t) 1 << atime_shift)))
   : 0);
static time_t const absolute_max_time =
  ((time_t) -1 < 0
   ? (((time_t) 1 << atime_shift) - 1 + ((time_t) 1 << atime_shift))
   : -1);
static size_t	longest;
static char const *progname;
static bool	warned;
static bool	errout;

static char const *abbr(struct tm const *);
static intmax_t delta(struct tm *, struct tm *);
static void dumptime(struct tm const *);
static time_t hunt(timezone_t, time_t, time_t, bool);
static void show(timezone_t, char *, time_t, bool);
static void showextrema(timezone_t, char *, time_t, struct tm *, time_t);
static void showtrans(char const *, struct tm const *, time_t, char const *,
		      char const *);
static const char *tformat(void);
ATTRIBUTE_PURE_114833 static time_t yeartot(intmax_t);

/* Is C an ASCII digit?  */
static bool
is_digit(char c)
{
  return '0' <= c && c <= '9';
}

/* Is A an alphabetic character in the C locale?  */
static bool
is_alpha(char a)
{
	switch (a) {
	  default:
		return false;
	  case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
	  case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
	  case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
	  case 'V': case 'W': case 'X': case 'Y': case 'Z':
	  case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
	  case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
	  case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
	  case 'v': case 'w': case 'x': case 'y': case 'z':
		return true;
	}
}

ATTRIBUTE_NORETURN static void
size_overflow(void)
{
  fprintf(stderr, _("%s: size overflow\n"), progname);
  exit(EXIT_FAILURE);
}

/* Return A + B, exiting if the result would overflow either ptrdiff_t
   or size_t.  A and B are both nonnegative.  */
ATTRIBUTE_PURE_114833 static ptrdiff_t
sumsize(ptrdiff_t a, ptrdiff_t b)
{
#ifdef ckd_add
  ptrdiff_t sum;
  if (!ckd_add(&sum, a, b) && sum <= INDEX_MAX)
    return sum;
#else
  if (a <= INDEX_MAX && b <= INDEX_MAX - a)
    return a + b;
#endif
  size_overflow();
}

/* Return the size of of the string STR, including its trailing NUL.
   Report an error and exit if this would exceed INDEX_MAX which means
   pointer subtraction wouldn't work.  */
static ptrdiff_t
xstrsize(char const *str)
{
  size_t len = strlen(str);
  if (len < INDEX_MAX)
    return len + 1;
  size_overflow();
}

/* Return a pointer to a newly allocated buffer of size SIZE, exiting
   on failure.  SIZE should be positive.  */
static void *
xmalloc(ptrdiff_t size)
{
  void *p = malloc(size);
  if (!p) {
    fprintf(stderr, _("%s: Memory exhausted\n"), progname);
    exit(EXIT_FAILURE);
  }
  return p;
}

#if ! HAVE_TZSET
# undef tzset
# define tzset zdump_tzset
static void tzset(void) { }
#endif

/* Assume gmtime_r works if localtime_r does.
   A replacement localtime_r is defined below if needed.  */
#if ! HAVE_LOCALTIME_R

# undef gmtime_r
# define gmtime_r zdump_gmtime_r

static struct tm *
gmtime_r(time_t *tp, struct tm *tmp)
{
  struct tm *r = gmtime(tp);
  if (r) {
    *tmp = *r;
    r = tmp;
  }
  return r;
}

#endif

/* Platforms with TM_ZONE don't need tzname, so they can use the
   faster localtime_rz or localtime_r if available.  */

#if defined TM_ZONE && HAVE_LOCALTIME_RZ
# define USE_LOCALTIME_RZ true
#else
# define USE_LOCALTIME_RZ false
#endif

#if ! USE_LOCALTIME_RZ

# if !defined TM_ZONE || ! HAVE_LOCALTIME_R || ! HAVE_TZSET
#  undef localtime_r
#  define localtime_r zdump_localtime_r
static struct tm *
localtime_r(time_t *tp, struct tm *tmp)
{
  struct tm *r = localtime(tp);
  if (r) {
    *tmp = *r;
    r = tmp;
  }
  return r;
}
# endif

# undef localtime_rz
# define localtime_rz zdump_localtime_rz
static struct tm *
localtime_rz(ATTRIBUTE_MAYBE_UNUSED timezone_t rz, time_t *tp, struct tm *tmp)
{
  return localtime_r(tp, tmp);
}

# ifdef TYPECHECK
#  undef mktime_z
#  define mktime_z zdump_mktime_z
static time_t
mktime_z(timezone_t tz, struct tm *tmp)
{
  return mktime(tmp);
}
# endif

# undef tzalloc
# undef tzfree
# define tzalloc zdump_tzalloc
# define tzfree zdump_tzfree

static timezone_t
tzalloc(char const *val)
{
# if HAVE_SETENV
  if (setenv("TZ", val, 1) != 0) {
    char const *e = strerror(errno);
    fprintf(stderr, _("%s: setenv: %s\n"), progname, e);
    exit(EXIT_FAILURE);
  }
  tzset();
  return &optarg;  /* Any valid non-null char ** will do.  */
# else
  enum { TZeqlen = 3 };
  static char const TZeq[TZeqlen] = "TZ=";
  static char **fakeenv;
  static ptrdiff_t fakeenv0size;
  void *freeable = NULL;
  char **env = fakeenv, **initial_environ;
  ptrdiff_t valsize = xstrsize(val);
  if (fakeenv0size < valsize) {
    char **e = environ, **to;
    ptrdiff_t initial_nenvptrs = 1;  /* Counting the trailing NULL pointer.  */

    while (*e++) {
#  ifdef ckd_add
      if (ckd_add(&initial_nenvptrs, initial_nenvptrs, 1)
	  || INDEX_MAX < initial_nenvptrs)
	size_overflow();
#  else
      if (initial_nenvptrs == INDEX_MAX / sizeof *environ)
	size_overflow();
      initial_nenvptrs++;
#  endif
    }
    fakeenv0size = sumsize(valsize, valsize);
    fakeenv0size = max(fakeenv0size, 64);
    freeable = env;
    fakeenv = env =
      xmalloc(sumsize(sumsize(sizeof *environ,
			      initial_nenvptrs * sizeof *environ),
		      sumsize(TZeqlen, fakeenv0size)));
    to = env + 1;
    for (e = environ; (*to = *e); e++)
      to += strncmp(*e, TZeq, TZeqlen) != 0;
    env[0] = memcpy(to + 1, TZeq, TZeqlen);
  }
  memcpy(env[0] + TZeqlen, val, valsize);
  initial_environ = environ;
  environ = env;
  tzset();
  free(freeable);
  return initial_environ;
# endif
}

static void
tzfree(ATTRIBUTE_MAYBE_UNUSED timezone_t initial_environ)
{
# if !HAVE_SETENV
  environ = initial_environ;
  tzset();
# else
  (void)initial_environ;
# endif
}
#endif /* ! USE_LOCALTIME_RZ */

/* A UT time zone, and its initializer.  */
static timezone_t gmtz;
static void
gmtzinit(void)
{
  if (USE_LOCALTIME_RZ) {
    /* Try "GMT" first to find out whether this is one of the rare
       platforms where time_t counts leap seconds; this works due to
       the "Zone GMT 0 - GMT" line in the "etcetera" file.  If "GMT"
       fails, fall back on "GMT0" which might be similar due to the
       "Link GMT GMT0" line in the "backward" file, and which
       should work on all POSIX platforms.  The rest of zdump does not
       use the "GMT" abbreviation that comes from this setting, so it
       is OK to use "GMT" here rather than the modern "UTC" which
       would not work on platforms that omit the "backward" file.  */
    gmtz = tzalloc("GMT");
    if (!gmtz) {
      static char const gmt0[] = "GMT0";
      gmtz = tzalloc(gmt0);
      if (!gmtz) {
	char const *e = strerror(errno);
	fprintf(stderr, _("%s: unknown timezone '%s': %s\n"),
		progname, gmt0, e);
	exit(EXIT_FAILURE);
      }
    }
  }
}

/* Convert *TP to UT, storing the broken-down time into *TMP.
   Return TMP if successful, NULL otherwise.  This is like gmtime_r(TP, TMP),
   except typically faster if USE_LOCALTIME_RZ.  */
static struct tm *
my_gmtime_r(time_t *tp, struct tm *tmp)
{
  return USE_LOCALTIME_RZ ? localtime_rz(gmtz, tp, tmp) : gmtime_r(tp, tmp);
}

#ifndef TYPECHECK
# define my_localtime_rz localtime_rz
#else /* !defined TYPECHECK */

static struct tm *
my_localtime_rz(timezone_t tz, time_t *tp, struct tm *tmp)
{
	tmp = localtime_rz(tz, tp, tmp);
	if (tmp) {
		struct tm	tm;
		register time_t	t;

		tm = *tmp;
		t = mktime_z(tz, &tm);
		if (t != *tp) {
			fflush(stdout);
			fprintf(stderr, "\n%s: ", progname);
			fprintf(stderr, tformat(), *tp);
			fprintf(stderr, " ->");
			fprintf(stderr, " year=%d", tmp->tm_year);
			fprintf(stderr, " mon=%d", tmp->tm_mon);
			fprintf(stderr, " mday=%d", tmp->tm_mday);
			fprintf(stderr, " hour=%d", tmp->tm_hour);
			fprintf(stderr, " min=%d", tmp->tm_min);
			fprintf(stderr, " sec=%d", tmp->tm_sec);
			fprintf(stderr, " isdst=%d", tmp->tm_isdst);
			fprintf(stderr, " -> ");
			fprintf(stderr, tformat(), t);
			fprintf(stderr, "\n");
			errout = true;
		}
	}
	return tmp;
}
#endif /* !defined TYPECHECK */

static void
abbrok(const char *const abbrp, const char *const zone)
{
	register const char *	cp;
	register const char *	wp;

	if (warned)
		return;
	cp = abbrp;
	while (is_alpha(*cp) || is_digit(*cp) || *cp == '-' || *cp == '+')
		++cp;
	if (*cp)
	  wp = _("has characters other than ASCII alphanumerics, '-' or '+'");
	else if (cp - abbrp < 3)
	  wp = _("has fewer than 3 characters");
	else if (cp - abbrp > 6)
	  wp = _("has more than 6 characters");
	else
	  return;
	fflush(stdout);
	fprintf(stderr,
		_("%s: warning: zone \"%s\" abbreviation \"%s\" %s\n"),
		progname, zone, abbrp, wp);
	warned = errout = true;
}

/* Return a time zone abbreviation.  If the abbreviation needs to be
   saved, use *BUF (of size *BUFALLOC) to save it, and return the
   abbreviation in the possibly reallocated *BUF.  Otherwise, just
   return the abbreviation.  Get the abbreviation from TMP.
   Exit on memory allocation failure.  */
static char const *
saveabbr(char **buf, ptrdiff_t *bufalloc, struct tm const *tmp)
{
  char const *ab = abbr(tmp);
  if (HAVE_LOCALTIME_RZ)
    return ab;
  else {
    ptrdiff_t absize = xstrsize(ab);
    if (*bufalloc < absize) {
      free(*buf);

      /* Make the new buffer at least twice as long as the old,
	 to avoid O(N**2) behavior on repeated calls.  */
      *bufalloc = sumsize(*bufalloc, absize);

      *buf = xmalloc(*bufalloc);
    }
    return strcpy(*buf, ab);
  }
}

static void
close_file(FILE *stream)
{
  char const *e = (ferror(stream) ? _("I/O error")
		   : fclose(stream) != 0 ? strerror(errno) : NULL);
  if (e) {
    fprintf(stderr, "%s: %s\n", progname, e);
    exit(EXIT_FAILURE);
  }
}

static void
usage(FILE * const stream, const int status)
{
	fprintf(stream,
_("%s: usage: %s OPTIONS TIMEZONE ...\n"
  "Options include:\n"
  "  -c [L,]U   Start at year L (default -500), end before year U (default 2500)\n"
  "  -t [L,]U   Start at time L, end before time U (in seconds since 1970)\n"
  "  -i         List transitions briefly (format is experimental)\n" \
  "  -v         List transitions verbosely\n"
  "  -V         List transitions a bit less verbosely\n"
  "  --help     Output this help\n"
  "  --version  Output version info\n"
  "\n"
  "Report bugs to %s.\n"),
		progname, progname, REPORT_BUGS_TO);
	if (status == EXIT_SUCCESS)
	  close_file(stream);
	exit(status);
}

int
main(int argc, char *argv[])
{
	/* These are static so that they're initially zero.  */
	static char *		abbrev;
	static ptrdiff_t	abbrevsize;

	register int		i;
	register bool		vflag;
	register bool		Vflag;
	register char *		cutarg;
	register char *		cuttimes;
	register time_t		cutlotime;
	register time_t		cuthitime;
	time_t			now;
	bool iflag = false;

	cutlotime = absolute_min_time;
	cuthitime = absolute_max_time;
#if HAVE_GETTEXT
	setlocale(LC_ALL, "");
# ifdef TZ_DOMAINDIR
	bindtextdomain(TZ_DOMAIN, TZ_DOMAINDIR);
# endif /* defined TEXTDOMAINDIR */
	textdomain(TZ_DOMAIN);
#endif /* HAVE_GETTEXT */
	progname = argv[0] ? argv[0] : "zdump";
	for (i = 1; i < argc; ++i)
		if (strcmp(argv[i], "--version") == 0) {
			printf("zdump %s%s\n", PKGVERSION, TZVERSION);
			return EXIT_SUCCESS;
		} else if (strcmp(argv[i], "--help") == 0) {
			usage(stdout, EXIT_SUCCESS);
		}
	vflag = Vflag = false;
	cutarg = cuttimes = NULL;
	for (;;)
	  switch (getopt(argc, argv, "c:it:vV")) {
	  case 'c': cutarg = optarg; break;
	  case 't': cuttimes = optarg; break;
	  case 'i': iflag = true; break;
	  case 'v': vflag = true; break;
	  case 'V': Vflag = true; break;
	  case -1:
	    if (! (optind == argc - 1 && strcmp(argv[optind], "=") == 0))
	      goto arg_processing_done;
	    ATTRIBUTE_FALLTHROUGH;
	  default:
	    usage(stderr, EXIT_FAILURE);
	  }
 arg_processing_done:;

	if (iflag | vflag | Vflag) {
		intmax_t	lo;
		intmax_t	hi;
		char *loend, *hiend;
		register intmax_t cutloyear = ZDUMP_LO_YEAR;
		register intmax_t cuthiyear = ZDUMP_HI_YEAR;
		if (cutarg != NULL) {
			lo = strtoimax(cutarg, &loend, 10);
			if (cutarg != loend && !*loend) {
				hi = lo;
				cuthiyear = hi;
			} else if (cutarg != loend && *loend == ','
				   && (hi = strtoimax(loend + 1, &hiend, 10),
				       loend + 1 != hiend && !*hiend)) {
				cutloyear = lo;
				cuthiyear = hi;
			} else {
				fprintf(stderr, _("%s: wild -c argument %s\n"),
					progname, cutarg);
				return EXIT_FAILURE;
			}
		}
		if (cutarg != NULL || cuttimes == NULL) {
			cutlotime = yeartot(cutloyear);
			cuthitime = yeartot(cuthiyear);
		}
		if (cuttimes != NULL) {
			lo = strtoimax(cuttimes, &loend, 10);
			if (cuttimes != loend && !*loend) {
				hi = lo;
				if (hi < cuthitime) {
					if (hi < absolute_min_time + 1)
					  hi = absolute_min_time + 1;
					cuthitime = hi;
				}
			} else if (cuttimes != loend && *loend == ','
				   && (hi = strtoimax(loend + 1, &hiend, 10),
				       loend + 1 != hiend && !*hiend)) {
				if (cutlotime < lo) {
					if (absolute_max_time < lo)
						lo = absolute_max_time;
					cutlotime = lo;
				}
				if (hi < cuthitime) {
					if (hi < absolute_min_time + 1)
					  hi = absolute_min_time + 1;
					cuthitime = hi;
				}
			} else {
				fprintf(stderr,
					_("%s: wild -t argument %s\n"),
					progname, cuttimes);
				return EXIT_FAILURE;
			}
		}
	}
	gmtzinit();
	if (iflag | vflag | Vflag)
	  now = 0;
	else {
	  now = time(NULL);
	  now |= !now;
	}
	longest = 0;
	for (i = optind; i < argc; i++) {
	  size_t arglen = strlen(argv[i]);
	  if (longest < arglen)
	    longest = min(arglen, INT_MAX);
	}

	for (i = optind; i < argc; ++i) {
		timezone_t tz = tzalloc(argv[i]);
		char const *ab;
		time_t t;
		struct tm tm, newtm;
		bool tm_ok;
		if (!tz) {
		  char const *e = strerror(errno);
		  fprintf(stderr, _("%s: unknown timezone '%s': %s\n"),
			  progname, argv[i], e);
		  return EXIT_FAILURE;
		}
		if (now) {
			show(tz, argv[i], now, false);
			tzfree(tz);
			continue;
		}
		warned = false;
		t = absolute_min_time;
		if (! (iflag | Vflag)) {
			show(tz, argv[i], t, true);
			if (my_localtime_rz(tz, &t, &tm) == NULL
			    && t < cutlotime) {
				time_t newt = cutlotime;
				if (my_localtime_rz(tz, &newt, &newtm) != NULL)
				  showextrema(tz, argv[i], t, NULL, newt);
			}
		}
		if (t + 1 < cutlotime)
		  t = cutlotime - 1;
		tm_ok = my_localtime_rz(tz, &t, &tm) != NULL;
		if (tm_ok) {
		  ab = saveabbr(&abbrev, &abbrevsize, &tm);
		  if (iflag) {
		    showtrans("\nTZ=%f", &tm, t, ab, argv[i]);
		    showtrans("-\t-\t%Q", &tm, t, ab, argv[i]);
		  }
		} else
		  ab = NULL;
		while (t < cuthitime - 1) {
		  time_t newt = ((t < absolute_max_time - SECSPERDAY / 2
				  && t + SECSPERDAY / 2 < cuthitime - 1)
				 ? t + SECSPERDAY / 2
				 : cuthitime - 1);
		  struct tm *newtmp = localtime_rz(tz, &newt, &newtm);
		  bool newtm_ok = newtmp != NULL;
		  if (tm_ok != newtm_ok
		      || (ab && (delta(&newtm, &tm) != newt - t
				 || newtm.tm_isdst != tm.tm_isdst
				 || strcmp(abbr(&newtm), ab) != 0))) {
		    newt = hunt(tz, t, newt, false);
		    newtmp = localtime_rz(tz, &newt, &newtm);
		    newtm_ok = newtmp != NULL;
		    if (iflag)
		      showtrans("%Y-%m-%d\t%L\t%Q", newtmp, newt,
				newtm_ok ? abbr(&newtm) : NULL, argv[i]);
		    else {
		      show(tz, argv[i], newt - 1, true);
		      show(tz, argv[i], newt, true);
		    }
		  }
		  t = newt;
		  tm_ok = newtm_ok;
		  if (newtm_ok) {
		    ab = saveabbr(&abbrev, &abbrevsize, &newtm);
		    tm = newtm;
		  }
		}
		if (! (iflag | Vflag)) {
			time_t newt = absolute_max_time;
			t = cuthitime;
			if (t < newt) {
			  struct tm *tmp = my_localtime_rz(tz, &t, &tm);
			  if (tmp != NULL
			      && my_localtime_rz(tz, &newt, &newtm) == NULL)
			    showextrema(tz, argv[i], t, tmp, newt);
			}
			show(tz, argv[i], absolute_max_time, true);
		}
		tzfree(tz);
	}
	close_file(stdout);
	if (errout && (ferror(stderr) || fclose(stderr) != 0))
	  return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

static time_t
yeartot(intmax_t y)
{
	register intmax_t	myy, seconds, years;
	register time_t		t;

	myy = EPOCH_YEAR;
	t = 0;
	while (myy < y) {
		if (SECSPER400YEARS_FITS && 400 <= y - myy) {
			intmax_t diff400 = (y - myy) / 400;
			if (INTMAX_MAX / SECSPER400YEARS < diff400)
				return absolute_max_time;
			seconds = diff400 * SECSPER400YEARS;
			years = diff400 * 400;
                } else {
			seconds = isleap(myy) ? SECSPERLYEAR : SECSPERNYEAR;
			years = 1;
		}
		myy += years;
		if (t > absolute_max_time - seconds)
			return absolute_max_time;
		t += seconds;
	}
	while (y < myy) {
		if (SECSPER400YEARS_FITS && y + 400 <= myy && myy < 0) {
			intmax_t diff400 = (myy - y) / 400;
			if (INTMAX_MAX / SECSPER400YEARS < diff400)
				return absolute_min_time;
			seconds = diff400 * SECSPER400YEARS;
			years = diff400 * 400;
		} else {
			seconds = isleap(myy - 1) ? SECSPERLYEAR : SECSPERNYEAR;
			years = 1;
		}
		myy -= years;
		if (t < absolute_min_time + seconds)
			return absolute_min_time;
		t -= seconds;
	}
	return t;
}

/* Search for a discontinuity in timezone TZ, in the
   timestamps ranging from LOT through HIT.  LOT and HIT disagree
   about some aspect of timezone.  If ONLY_OK, search only for
   definedness changes, i.e., localtime succeeds on one side of the
   transition but fails on the other side.  Return the timestamp just
   before the transition from LOT's settings.  */

static time_t
hunt(timezone_t tz, time_t lot, time_t hit, bool only_ok)
{
	static char *		loab;
	static ptrdiff_t	loabsize;
	struct tm		lotm;
	struct tm		tm;

	/* Convert LOT into a broken-down time here, even though our
	   caller already did that.  On platforms without TM_ZONE,
	   tzname may have been altered since our caller broke down
	   LOT, and tzname needs to be changed back.  */
	bool lotm_ok = my_localtime_rz(tz, &lot, &lotm) != NULL;
	bool tm_ok;
	char const *ab = lotm_ok ? saveabbr(&loab, &loabsize, &lotm) : NULL;

	for ( ; ; ) {
		/* T = average of LOT and HIT, rounding down.
		   Avoid overflow.  */
		int rem_sum = lot % 2 + hit % 2;
		time_t t = (rem_sum == 2) - (rem_sum < 0) + lot / 2 + hit / 2;
		if (t == lot)
			break;
		tm_ok = my_localtime_rz(tz, &t, &tm) != NULL;
		if (lotm_ok == tm_ok
		    && (only_ok
			|| (ab && tm.tm_isdst == lotm.tm_isdst
			    && delta(&tm, &lotm) == t - lot
			    && strcmp(abbr(&tm), ab) == 0))) {
		  lot = t;
		  if (tm_ok)
		    lotm = tm;
		} else	hit = t;
	}
	return hit;
}

/*
** Thanks to Paul Eggert for logic used in delta_nonneg.
*/

static intmax_t
delta_nonneg(struct tm *newp, struct tm *oldp)
{
	intmax_t oldy = oldp->tm_year;
	int cycles = (newp->tm_year - oldy) / YEARSPERREPEAT;
	intmax_t sec = SECSPERREPEAT, result = cycles * sec;
	int tmy = oldp->tm_year + cycles * YEARSPERREPEAT;
	for ( ; tmy < newp->tm_year; ++tmy)
		result += DAYSPERNYEAR + isleap_sum(tmy, TM_YEAR_BASE);
	result += newp->tm_yday - oldp->tm_yday;
	result *= HOURSPERDAY;
	result += newp->tm_hour - oldp->tm_hour;
	result *= MINSPERHOUR;
	result += newp->tm_min - oldp->tm_min;
	result *= SECSPERMIN;
	result += newp->tm_sec - oldp->tm_sec;
	return result;
}

static intmax_t
delta(struct tm *newp, struct tm *oldp)
{
  return (newp->tm_year < oldp->tm_year
	  ? -delta_nonneg(oldp, newp)
	  : delta_nonneg(newp, oldp));
}

#ifndef TM_GMTOFF
/* Return A->tm_yday, adjusted to compare it fairly to B->tm_yday.
   Assume A and B differ by at most one year.  */
static int
adjusted_yday(struct tm const *a, struct tm const *b)
{
  int yday = a->tm_yday;
  if (b->tm_year < a->tm_year)
    yday += 365 + isleap_sum(b->tm_year, TM_YEAR_BASE);
  return yday;
}
#endif

/* If A is the broken-down local time and B the broken-down UT for
   the same instant, return A's UT offset in seconds, where positive
   offsets are east of Greenwich.  On failure, return LONG_MIN.

   If T is nonnull, *T is the timestamp that corresponds to A; call
   my_gmtime_r and use its result instead of B.  Otherwise, B is the
   possibly nonnull result of an earlier call to my_gmtime_r.  */
static long
gmtoff(struct tm const *a, ATTRIBUTE_MAYBE_UNUSED time_t *t,
       ATTRIBUTE_MAYBE_UNUSED struct tm const *b)
{
#ifdef TM_GMTOFF
  return a->TM_GMTOFF;
#else
  struct tm tm;
  if (t)
    b = my_gmtime_r(t, &tm);
  if (! b)
    return LONG_MIN;
  else {
    int ayday = adjusted_yday(a, b);
    int byday = adjusted_yday(b, a);
    int days = ayday - byday;
    long hours = a->tm_hour - b->tm_hour + 24 * days;
    long minutes = a->tm_min - b->tm_min + 60 * hours;
    long seconds = a->tm_sec - b->tm_sec + 60 * minutes;
    return seconds;
  }
#endif
}

static void
show(timezone_t tz, char *zone, time_t t, bool v)
{
	register struct tm *	tmp;
	register struct tm *	gmtmp;
	struct tm tm, gmtm;

	printf("%-*s  ", (int)longest, zone);
	if (v) {
		gmtmp = my_gmtime_r(&t, &gmtm);
		if (gmtmp == NULL) {
			printf(tformat(), t);
			printf(_(" (gmtime failed)"));
		} else {
			dumptime(gmtmp);
			printf(" UT");
		}
		printf(" = ");
	}
	tmp = my_localtime_rz(tz, &t, &tm);
	if (tmp == NULL) {
		printf(tformat(), t);
		printf(_(" (localtime failed)"));
	} else {
		dumptime(tmp);
		if (*abbr(tmp) != '\0')
			printf(" %s", abbr(tmp));
		if (v) {
			long off = gmtoff(tmp, NULL, gmtmp);
			printf(" isdst=%d", tmp->tm_isdst);
			if (off != LONG_MIN)
			  printf(" gmtoff=%ld", off);
		}
	}
	printf("\n");
	if (tmp != NULL && *abbr(tmp) != '\0')
		abbrok(abbr(tmp), zone);
}

/* Show timestamps just before and just after a transition between
   defined and undefined (or vice versa) in either localtime or
   gmtime.  These transitions are for timezone TZ with name ZONE, in
   the range from LO (with broken-down time LOTMP if that is nonnull)
   through HI.  LO and HI disagree on definedness.  */

static void
showextrema(timezone_t tz, char *zone, time_t lo, struct tm *lotmp, time_t hi)
{
  struct tm localtm[2], gmtm[2];
  time_t t, boundary = hunt(tz, lo, hi, true);
  bool old = false;
  hi = (SECSPERDAY < hi - boundary
	? boundary + SECSPERDAY
	: hi + (hi < TIME_T_MAX));
  if (SECSPERDAY < boundary - lo) {
    lo = boundary - SECSPERDAY;
    lotmp = my_localtime_rz(tz, &lo, &localtm[old]);
  }
  if (lotmp)
    localtm[old] = *lotmp;
  else
    localtm[old].tm_sec = -1;
  if (! my_gmtime_r(&lo, &gmtm[old]))
    gmtm[old].tm_sec = -1;

  /* Search sequentially for definedness transitions.  Although this
     could be sped up by refining 'hunt' to search for either
     localtime or gmtime definedness transitions, it hardly seems
     worth the trouble.  */
  for (t = lo + 1; t < hi; t++) {
    bool new = !old;
    if (! my_localtime_rz(tz, &t, &localtm[new]))
      localtm[new].tm_sec = -1;
    if (! my_gmtime_r(&t, &gmtm[new]))
      gmtm[new].tm_sec = -1;
    if (((localtm[old].tm_sec < 0) != (localtm[new].tm_sec < 0))
	| ((gmtm[old].tm_sec < 0) != (gmtm[new].tm_sec < 0))) {
      show(tz, zone, t - 1, true);
      show(tz, zone, t, true);
    }
    old = new;
  }
}

#if HAVE_SNPRINTF
# define my_snprintf snprintf
#else
# include <stdarg.h>

/* A substitute for snprintf that is good enough for zdump.  */
static int
my_snprintf(char *s, size_t size, char const *format, ...)
{
  int n;
  va_list args;
  char const *arg;
  size_t arglen, slen;
  char buf[1024];
  va_start(args, format);
  if (strcmp(format, "%s") == 0) {
    arg = va_arg(args, char const *);
    arglen = strlen(arg);
  } else {
    n = vsprintf(buf, format, args);
    if (n < 0) {
      va_end(args);
      return n;
    }
    arg = buf;
    arglen = n;
  }
  slen = arglen < size ? arglen : size - 1;
  memcpy(s, arg, slen);
  s[slen] = '\0';
  n = arglen <= INT_MAX ? arglen : -1;
  va_end(args);
  return n;
}
#endif

/* Store into BUF, of size SIZE, a formatted local time taken from *TM.
   Use ISO 8601 format +HH:MM:SS.  Omit :SS if SS is zero, and omit
   :MM too if MM is also zero.

   Return the length of the resulting string.  If the string does not
   fit, return the length that the string would have been if it had
   fit; do not overrun the output buffer.  */
static int
format_local_time(char *buf, ptrdiff_t size, struct tm const *tm)
{
  int ss = tm->tm_sec, mm = tm->tm_min, hh = tm->tm_hour;
  return (ss
	  ? my_snprintf(buf, size, "%02d:%02d:%02d", hh, mm, ss)
	  : mm
	  ? my_snprintf(buf, size, "%02d:%02d", hh, mm)
	  : my_snprintf(buf, size, "%02d", hh));
}

/* Store into BUF, of size SIZE, a formatted UT offset for the
   localtime *TM corresponding to time T.  Use ISO 8601 format
   +HHMMSS, or -HHMMSS for timestamps west of Greenwich; use the
   format -00 for unknown UT offsets.  If the hour needs more than
   two digits to represent, extend the length of HH as needed.
   Otherwise, omit SS if SS is zero, and omit MM too if MM is also
   zero.

   Return the length of the resulting string, or -1 if the result is
   not representable as a string.  If the string does not fit, return
   the length that the string would have been if it had fit; do not
   overrun the output buffer.  */
static int
format_utc_offset(char *buf, ptrdiff_t size, struct tm const *tm, time_t t)
{
  long off = gmtoff(tm, &t, NULL);
  char sign = ((off < 0
		|| (off == 0
		    && (*abbr(tm) == '-' || strcmp(abbr(tm), "zzz") == 0)))
	       ? '-' : '+');
  long hh;
  int mm, ss;
  if (off < 0)
    {
      if (off == LONG_MIN)
	return -1;
      off = -off;
    }
  ss = off % 60;
  mm = off / 60 % 60;
  hh = off / 60 / 60;
  return (ss || 100 <= hh
	  ? my_snprintf(buf, size, "%c%02ld%02d%02d", sign, hh, mm, ss)
	  : mm
	  ? my_snprintf(buf, size, "%c%02ld%02d", sign, hh, mm)
	  : my_snprintf(buf, size, "%c%02ld", sign, hh));
}

/* Store into BUF (of size SIZE) a quoted string representation of P.
   If the representation's length is less than SIZE, return the
   length; the representation is not null terminated.  Otherwise
   return SIZE, to indicate that BUF is too small.  */
static ptrdiff_t
format_quoted_string(char *buf, ptrdiff_t size, char const *p)
{
  char *b = buf;
  ptrdiff_t s = size;
  if (!s)
    return size;
  *b++ = '"', s--;
  for (;;) {
    char c = *p++;
    if (s <= 1)
      return size;
    switch (c) {
    default: *b++ = c, s--; continue;
    case '\0': *b++ = '"', s--; return size - s;
    case '"': case '\\': break;
    case ' ': c = 's'; break;
    case '\f': c = 'f'; break;
    case '\n': c = 'n'; break;
    case '\r': c = 'r'; break;
    case '\t': c = 't'; break;
    case '\v': c = 'v'; break;
    }
    *b++ = '\\', *b++ = c, s -= 2;
  }
}

/* Store into BUF (of size SIZE) a timestamp formatted by TIME_FMT.
   TM is the broken-down time, T the seconds count, AB the time zone
   abbreviation, and ZONE_NAME the zone name.  Return true if
   successful, false if the output would require more than SIZE bytes.
   TIME_FMT uses the same format that strftime uses, with these
   additions:

   %f zone name
   %L local time as per format_local_time
   %Q like "U\t%Z\tD" where U is the UT offset as for format_utc_offset
      and D is the isdst flag; except omit D if it is zero, omit %Z if
      it equals U, quote and escape %Z if it contains nonalphabetics,
      and omit any trailing tabs.  */

static bool
istrftime(char *buf, ptrdiff_t size, char const *time_fmt,
	  struct tm const *tm, time_t t, char const *ab, char const *zone_name)
{
  char *b = buf;
  ptrdiff_t s = size;
  char const *f = time_fmt, *p;

  for (p = f; ; p++)
    if (*p == '%' && p[1] == '%')
      p++;
    else if (!*p
	     || (*p == '%'
		 && (p[1] == 'f' || p[1] == 'L' || p[1] == 'Q'))) {
      ptrdiff_t formatted_len;
      ptrdiff_t f_prefix_len = p - f;
      ptrdiff_t f_prefix_copy_size = sumsize(f_prefix_len, 2);
      char fbuf[100];
      bool oversized = sizeof fbuf <= (size_t)f_prefix_copy_size;
      char *f_prefix_copy = oversized ? xmalloc(f_prefix_copy_size) : fbuf;
      memcpy(f_prefix_copy, f, f_prefix_len);
      strcpy(f_prefix_copy + f_prefix_len, "X");
      formatted_len = strftime(b, s, f_prefix_copy, tm);
      if (oversized)
	free(f_prefix_copy);
      if (formatted_len == 0)
	return false;
      formatted_len--;
      b += formatted_len, s -= formatted_len;
      if (!*p++)
	break;
      switch (*p) {
      case 'f':
	formatted_len = format_quoted_string(b, s, zone_name);
	break;
      case 'L':
	formatted_len = format_local_time(b, s, tm);
	break;
      case 'Q':
	{
	  bool show_abbr;
	  int offlen = format_utc_offset(b, s, tm, t);
	  if (! (0 <= offlen && offlen < s))
	    return false;
	  show_abbr = strcmp(b, ab) != 0;
	  b += offlen, s -= offlen;
	  if (show_abbr) {
	    char const *abp;
	    ptrdiff_t len;
	    if (s <= 1)
	      return false;
	    *b++ = '\t', s--;
	    for (abp = ab; is_alpha(*abp); abp++)
	      continue;
	    len = (!*abp && *ab
		   ? my_snprintf(b, s, "%s", ab)
		   : format_quoted_string(b, s, ab));
	    if (s <= len)
	      return false;
	    b += len, s -= len;
	  }
	  formatted_len
	    = (tm->tm_isdst
	       ? my_snprintf(b, s, &"\t\t%d"[show_abbr], tm->tm_isdst)
	       : 0);
	}
	break;
      }
      if (s <= formatted_len)
	return false;
      b += formatted_len, s -= formatted_len;
      f = p + 1;
    }
  *b = '\0';
  return true;
}

/* Show a time transition.  */
static void
showtrans(char const *time_fmt, struct tm const *tm, time_t t, char const *ab,
	  char const *zone_name)
{
  if (!tm) {
    printf(tformat(), t);
    putchar('\n');
  } else {
    char stackbuf[1000];
    ptrdiff_t size = sizeof stackbuf;
    char *buf = stackbuf;
    char *bufalloc = NULL;
    while (! istrftime(buf, size, time_fmt, tm, t, ab, zone_name)) {
      size = sumsize(size, size);
      free(bufalloc);
      buf = bufalloc = xmalloc(size);
    }
    puts(buf);
    free(bufalloc);
  }
}

static char const *
abbr(struct tm const *tmp)
{
#ifdef TM_ZONE
	return tmp->TM_ZONE;
#else
# if HAVE_TZNAME
	if (0 <= tmp->tm_isdst && tzname[0 < tmp->tm_isdst])
	  return tzname[0 < tmp->tm_isdst];
# endif
	return "";
#endif
}

/*
** The code below can fail on certain theoretical systems;
** it works on all known real-world systems as of 2022-01-25.
*/

static const char *
tformat(void)
{
#if HAVE__GENERIC
	/* C11-style _Generic is more likely to return the correct
	   format when distinct types have the same size.  */
	char const *fmt =
	  _Generic(+ (time_t) 0,
		   int: "%d", long: "%ld", long long: "%lld",
		   unsigned: "%u", unsigned long: "%lu",
		   unsigned long long: "%llu",
		   default: NULL);
	if (fmt)
	  return fmt;
	fmt = _Generic((time_t) 0,
		       intmax_t: "%"PRIdMAX, uintmax_t: "%"PRIuMAX,
		       default: NULL);
	if (fmt)
	  return fmt;
#endif
	if (0 > (time_t) -1) {		/* signed */
		if (sizeof(time_t) == sizeof(intmax_t))
			return "%"PRIdMAX;
		if (sizeof(time_t) > sizeof(long))
			return "%lld";
		if (sizeof(time_t) > sizeof(int))
			return "%ld";
		return "%d";
	}
#ifdef PRIuMAX
	if (sizeof(time_t) == sizeof(uintmax_t))
		return "%"PRIuMAX;
#endif
	if (sizeof(time_t) > sizeof(unsigned long))
		return "%llu";
	if (sizeof(time_t) > sizeof(unsigned int))
		return "%lu";
	return "%u";
}

static void
dumptime(register const struct tm *timeptr)
{
	static const char	wday_name[][4] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char	mon_name[][4] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	register int		lead;
	register int		trail;
	int DIVISOR = 10;

	/*
	** The packaged localtime_rz and gmtime_r never put out-of-range
	** values in tm_wday or tm_mon, but since this code might be compiled
	** with other (perhaps experimental) versions, paranoia is in order.
	*/
	printf("%s %s%3d %.2d:%.2d:%.2d ",
		((0 <= timeptr->tm_wday
		  && timeptr->tm_wday < (int)(sizeof wday_name / sizeof wday_name[0]))
		 ? wday_name[timeptr->tm_wday] : "???"),
		((0 <= timeptr->tm_mon
		  && timeptr->tm_mon < (int)(sizeof mon_name / sizeof mon_name[0]))
		 ? mon_name[timeptr->tm_mon] : "???"),
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec);
	trail = timeptr->tm_year % DIVISOR + TM_YEAR_BASE % DIVISOR;
	lead = timeptr->tm_year / DIVISOR + TM_YEAR_BASE / DIVISOR +
		trail / DIVISOR;
	trail %= DIVISOR;
	if (trail < 0 && lead > 0) {
		trail += DIVISOR;
		--lead;
	} else if (lead < 0 && trail > 0) {
		trail -= DIVISOR;
		++lead;
	}
	if (lead == 0)
		printf("%d", trail);
	else	printf("%d%d", lead, ((trail < 0) ? -trail : trail));
}
