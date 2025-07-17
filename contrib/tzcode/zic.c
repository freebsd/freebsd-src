/* Compile .zi time zone data into TZif binary files.  */

/*
** This file is in the public domain, so clarified as of
** 2006-07-17 by Arthur David Olson.
*/

/* Use the system 'time' function, instead of any private replacement.
   This avoids creating an unnecessary dependency on localtime.c.  */
#undef EPOCH_LOCAL
#undef EPOCH_OFFSET
#undef RESERVE_STD_EXT_IDS
#undef time_tz

#include "version.h"
#include "private.h"
#include "tzdir.h"
#include "tzfile.h"

#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

typedef int_fast64_t	zic_t;
static zic_t const
  ZIC_MIN = INT_FAST64_MIN,
  ZIC_MAX = INT_FAST64_MAX,
  ZIC32_MIN = -1 - (zic_t) 0x7fffffff,
  ZIC32_MAX = 0x7fffffff;
#define SCNdZIC SCNdFAST64

#ifndef ZIC_MAX_ABBR_LEN_WO_WARN
# define ZIC_MAX_ABBR_LEN_WO_WARN 6
#endif /* !defined ZIC_MAX_ABBR_LEN_WO_WARN */

/* Minimum and maximum years, assuming signed 32-bit time_t.  */
enum { YEAR_32BIT_MIN = 1901, YEAR_32BIT_MAX = 2038 };

/* An upper bound on how much a format might grow due to concatenation.  */
enum { FORMAT_LEN_GROWTH_BOUND = 5 };

#ifdef HAVE_DIRECT_H
# include <direct.h>
# include <io.h>
# undef mkdir
# define mkdir(name, mode) _mkdir(name)
#endif

#ifndef HAVE_GETRANDOM
# ifdef __has_include
#  if __has_include(<sys/random.h>)
#   include <sys/random.h>
#  endif
# elif 2 < __GLIBC__ + (25 <= __GLIBC_MINOR__)
#  include <sys/random.h>
# endif
# define HAVE_GETRANDOM GRND_RANDOM
#elif HAVE_GETRANDOM
# include <sys/random.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef S_IRUSR
# define MKDIR_UMASK (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)
#else
# define MKDIR_UMASK 0755
#endif

/* The minimum alignment of a type, for pre-C23 platforms.
   The __SUNPRO_C test is because Oracle Developer Studio 12.6 lacks
   <stdalign.h> even though __STDC_VERSION__ == 201112.  */
#if __STDC_VERSION__ < 201112 || defined __SUNPRO_C
# define alignof(type) offsetof(struct { char a; type b; }, b)
#elif __STDC_VERSION__ < 202311
# include <stdalign.h>
#endif

/* The maximum length of a text line, including the trailing newline.  */
#ifndef _POSIX2_LINE_MAX
# define _POSIX2_LINE_MAX 2048
#endif

/* The type for line numbers.  Use PRIdMAX to format them; formerly
   there was also "#define PRIdLINENO PRIdMAX" and formats used
   PRIdLINENO, but xgettext cannot grok that.  */
typedef intmax_t lineno;

struct rule {
	int		r_filenum;
	lineno		r_linenum;
	const char *	r_name;

	zic_t		r_loyear;	/* for example, 1986 */
	zic_t		r_hiyear;	/* for example, 1986 */
	bool		r_hiwasnum;

	int		r_month;	/* 0..11 */

	int		r_dycode;	/* see below */
	int		r_dayofmonth;
	int		r_wday;

	zic_t		r_tod;		/* time from midnight */
	bool		r_todisstd;	/* is r_tod standard time? */
	bool		r_todisut;	/* is r_tod UT? */
	bool		r_isdst;	/* is this daylight saving time? */
	zic_t		r_save;		/* offset from standard time */
	const char *	r_abbrvar;	/* variable part of abbreviation */

	bool		r_todo;		/* a rule to do (used in outzone) */
	zic_t		r_temp;		/* used in outzone */
};

/*
** r_dycode	r_dayofmonth	r_wday
*/
enum {
  DC_DOM,	/* 1..31 */	/* unused */
  DC_DOWGEQ,	/* 1..31 */	/* 0..6 (Sun..Sat) */
  DC_DOWLEQ	/* 1..31 */	/* 0..6 (Sun..Sat) */
};

struct zone {
	int		z_filenum;
	lineno		z_linenum;

	const char *	z_name;
	zic_t		z_stdoff;
	char *		z_rule;
	const char *	z_format;
	char		z_format_specifier;

	bool		z_isdst;
	zic_t		z_save;

	struct rule *	z_rules;
	ptrdiff_t	z_nrules;

	struct rule	z_untilrule;
	zic_t		z_untiltime;
};

#if !HAVE_POSIX_DECLS
extern int	getopt(int argc, char * const argv[],
			const char * options);
extern int	link(const char * target, const char * linkname);
extern char *	optarg;
extern int	optind;
#endif

#if ! HAVE_SYMLINK
static ssize_t
readlink(char const *restrict file, char *restrict buf, size_t size)
{
  errno = ENOTSUP;
  return -1;
}
static int
symlink(char const *target, char const *linkname)
{
  errno = ENOTSUP;
  return -1;
}
#endif
#ifndef AT_SYMLINK_FOLLOW
#  define linkat(targetdir, target, linknamedir, linkname, flag) \
     (errno = ENOTSUP, -1)
#endif

static void	addtt(zic_t starttime, int type);
static int	addtype(zic_t, char const *, bool, bool, bool);
static void	leapadd(zic_t, int, int);
static void	adjleap(void);
static void	associate(void);
static void	dolink(const char *, const char *, bool);
static int	getfields(char *, char **, int);
static zic_t	gethms(const char * string, const char * errstring);
static zic_t	getsave(char *, bool *);
static void	inexpires(char **, int);
static void	infile(int, char const *);
static void	inleap(char ** fields, int nfields);
static void	inlink(char ** fields, int nfields);
static void	inrule(char ** fields, int nfields);
static bool	inzcont(char ** fields, int nfields);
static bool	inzone(char ** fields, int nfields);
static bool	inzsub(char **, int, bool);
static int	itssymlink(char const *, int *);
static bool	is_alpha(char a);
static char	lowerit(char);
static void	mkdirs(char const *, bool);
static void	newabbr(const char * abbr);
static zic_t	oadd(zic_t t1, zic_t t2);
static void	outzone(const struct zone * zp, ptrdiff_t ntzones);
static zic_t	rpytime(const struct rule * rp, zic_t wantedy);
static bool	rulesub(struct rule * rp,
			const char * loyearp, const char * hiyearp,
			const char * typep, const char * monthp,
			const char * dayp, const char * timep);
static void	setgroup(gid_t *flag, const char *name);
static void	setuser(uid_t *flag, const char *name);
static zic_t	tadd(zic_t t1, zic_t t2);

/* Bound on length of what %z can expand to.  */
enum { PERCENT_Z_LEN_BOUND = sizeof "+995959" - 1 };

static int		charcnt;
static bool		errors;
static bool		warnings;
static int		filenum;
static int		leapcnt;
static bool		leapseen;
static zic_t		leapminyear;
static zic_t		leapmaxyear;
static lineno		linenum;
static size_t		max_abbrvar_len = PERCENT_Z_LEN_BOUND;
static int		max_format_len;
static zic_t		max_year;
static zic_t		min_year;
static bool		noise;
static int		rfilenum;
static lineno		rlinenum;
static const char *	progname;
static char const *	leapsec;
static char *const *	main_argv;
static ptrdiff_t	timecnt;
static ptrdiff_t	timecnt_alloc;
static int		typecnt;
static int		unspecifiedtype;

/*
** Line codes.
*/

enum {
  LC_RULE,
  LC_ZONE,
  LC_LINK,
  LC_LEAP,
  LC_EXPIRES
};

/*
** Which fields are which on a Zone line.
*/

enum {
  ZF_NAME = 1,
  ZF_STDOFF,
  ZF_RULE,
  ZF_FORMAT,
  ZF_TILYEAR,
  ZF_TILMONTH,
  ZF_TILDAY,
  ZF_TILTIME,
  ZONE_MAXFIELDS,
  ZONE_MINFIELDS = ZF_TILYEAR
};

/*
** Which fields are which on a Zone continuation line.
*/

enum {
  ZFC_STDOFF,
  ZFC_RULE,
  ZFC_FORMAT,
  ZFC_TILYEAR,
  ZFC_TILMONTH,
  ZFC_TILDAY,
  ZFC_TILTIME,
  ZONEC_MAXFIELDS,
  ZONEC_MINFIELDS = ZFC_TILYEAR
};

/*
** Which files are which on a Rule line.
*/

enum {
  RF_NAME = 1,
  RF_LOYEAR,
  RF_HIYEAR,
  RF_COMMAND,
  RF_MONTH,
  RF_DAY,
  RF_TOD,
  RF_SAVE,
  RF_ABBRVAR,
  RULE_FIELDS
};

/*
** Which fields are which on a Link line.
*/

enum {
  LF_TARGET = 1,
  LF_LINKNAME,
  LINK_FIELDS
};

/*
** Which fields are which on a Leap line.
*/

enum {
  LP_YEAR = 1,
  LP_MONTH,
  LP_DAY,
  LP_TIME,
  LP_CORR,
  LP_ROLL,
  LEAP_FIELDS,

  /* Expires lines are like Leap lines, except without CORR and ROLL fields.  */
  EXPIRES_FIELDS = LP_TIME + 1
};

/* The maximum number of fields on any of the above lines.
   (The "+"s pacify gcc -Wenum-compare.)  */
enum {
  MAX_FIELDS = max(max(+RULE_FIELDS, +LINK_FIELDS),
		   max(+LEAP_FIELDS, +EXPIRES_FIELDS))
};

/*
** Year synonyms.
*/

enum {
  YR_MINIMUM, /* "minimum" is for backward compatibility only */
  YR_MAXIMUM,
  YR_ONLY
};

static struct rule *	rules;
static ptrdiff_t	nrules;	/* number of rules */
static ptrdiff_t	nrules_alloc;

static struct zone *	zones;
static ptrdiff_t	nzones;	/* number of zones */
static ptrdiff_t	nzones_alloc;

struct link {
	int		l_filenum;
	lineno		l_linenum;
	const char *	l_target;
	const char *	l_linkname;
};

static struct link *	links;
static ptrdiff_t	nlinks;
static ptrdiff_t	nlinks_alloc;

struct lookup {
	const char *	l_word;
	const int	l_value;
};

static struct lookup const *	byword(const char * string,
					const struct lookup * lp);

static struct lookup const zi_line_codes[] = {
	{ "Rule",	LC_RULE },
	{ "Zone",	LC_ZONE },
	{ "Link",	LC_LINK },
	{ NULL,		0 }
};
static struct lookup const leap_line_codes[] = {
	{ "Leap",	LC_LEAP },
	{ "Expires",	LC_EXPIRES },
	{ NULL,		0}
};

static struct lookup const	mon_names[] = {
	{ "January",	TM_JANUARY },
	{ "February",	TM_FEBRUARY },
	{ "March",	TM_MARCH },
	{ "April",	TM_APRIL },
	{ "May",	TM_MAY },
	{ "June",	TM_JUNE },
	{ "July",	TM_JULY },
	{ "August",	TM_AUGUST },
	{ "September",	TM_SEPTEMBER },
	{ "October",	TM_OCTOBER },
	{ "November",	TM_NOVEMBER },
	{ "December",	TM_DECEMBER },
	{ NULL,		0 }
};

static struct lookup const	wday_names[] = {
	{ "Sunday",	TM_SUNDAY },
	{ "Monday",	TM_MONDAY },
	{ "Tuesday",	TM_TUESDAY },
	{ "Wednesday",	TM_WEDNESDAY },
	{ "Thursday",	TM_THURSDAY },
	{ "Friday",	TM_FRIDAY },
	{ "Saturday",	TM_SATURDAY },
	{ NULL,		0 }
};

static struct lookup const	lasts[] = {
	{ "last-Sunday",	TM_SUNDAY },
	{ "last-Monday",	TM_MONDAY },
	{ "last-Tuesday",	TM_TUESDAY },
	{ "last-Wednesday",	TM_WEDNESDAY },
	{ "last-Thursday",	TM_THURSDAY },
	{ "last-Friday",	TM_FRIDAY },
	{ "last-Saturday",	TM_SATURDAY },
	{ NULL,			0 }
};

static struct lookup const	begin_years[] = {
	{ "minimum",	YR_MINIMUM },
	{ NULL,		0 }
};

static struct lookup const	end_years[] = {
	{ "maximum",	YR_MAXIMUM },
	{ "only",	YR_ONLY },
	{ NULL,		0 }
};

static struct lookup const	leap_types[] = {
	{ "Rolling",	true },
	{ "Stationary",	false },
	{ NULL,		0 }
};

static const int	len_months[2][MONSPERYEAR] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static const int	len_years[2] = {
	DAYSPERNYEAR, DAYSPERLYEAR
};

static struct attype {
	zic_t		at;
	bool		dontmerge;
	unsigned char	type;
} *			attypes;
static zic_t		utoffs[TZ_MAX_TYPES];
static char		isdsts[TZ_MAX_TYPES];
static unsigned char	desigidx[TZ_MAX_TYPES];
static bool		ttisstds[TZ_MAX_TYPES];
static bool		ttisuts[TZ_MAX_TYPES];
static char		chars[TZ_MAX_CHARS];
static zic_t		trans[TZ_MAX_LEAPS];
static zic_t		corr[TZ_MAX_LEAPS];
static char		roll[TZ_MAX_LEAPS];

/*
** Memory allocation.
*/

ATTRIBUTE_NORETURN static void
memory_exhausted(const char *msg)
{
	fprintf(stderr, _("%s: Memory exhausted: %s\n"), progname, msg);
	exit(EXIT_FAILURE);
}

ATTRIBUTE_NORETURN static void
size_overflow(void)
{
  memory_exhausted(_("size overflow"));
}

ATTRIBUTE_PURE_114833 static ptrdiff_t
size_sum(size_t a, size_t b)
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

ATTRIBUTE_PURE_114833 static ptrdiff_t
size_product(ptrdiff_t nitems, ptrdiff_t itemsize)
{
#ifdef ckd_mul
  ptrdiff_t product;
  if (!ckd_mul(&product, nitems, itemsize) && product <= INDEX_MAX)
    return product;
#else
  ptrdiff_t nitems_max = INDEX_MAX / itemsize;
  if (nitems <= nitems_max)
    return nitems * itemsize;
#endif
  size_overflow();
}

ATTRIBUTE_PURE_114833 static ptrdiff_t
align_to(ptrdiff_t size, ptrdiff_t alignment)
{
  size_t lo_bits = alignment - 1, sum = size_sum(size, lo_bits);
  return sum & ~lo_bits;
}

#if !HAVE_STRDUP
static char *
strdup(char const *str)
{
  char *result = malloc(strlen(str) + 1);
  return result ? strcpy(result, str) : result;
}
#endif

static void *
memcheck(void *ptr)
{
	if (ptr == NULL)
	  memory_exhausted(strerror(HAVE_MALLOC_ERRNO ? errno : ENOMEM));
	return ptr;
}

static void *
emalloc(size_t size)
{
  return memcheck(malloc(size));
}

static void *
erealloc(void *ptr, size_t size)
{
  return memcheck(realloc(ptr, size));
}

static char *
estrdup(char const *str)
{
  return memcheck(strdup(str));
}

static ptrdiff_t
grow_nitems_alloc(ptrdiff_t *nitems_alloc, ptrdiff_t itemsize)
{
  ptrdiff_t addend = (*nitems_alloc >> 1) + 1;
#if defined ckd_add && defined ckd_mul
  ptrdiff_t product;
  if (!ckd_add(nitems_alloc, *nitems_alloc, addend)
      && !ckd_mul(&product, *nitems_alloc, itemsize) && product <= INDEX_MAX)
    return product;
#else
  if (*nitems_alloc <= ((INDEX_MAX - 1) / 3 * 2) / itemsize) {
    *nitems_alloc += addend;
    return *nitems_alloc * itemsize;
  }
#endif
  memory_exhausted(_("integer overflow"));
}

static void *
growalloc(void *ptr, ptrdiff_t itemsize, ptrdiff_t nitems,
	  ptrdiff_t *nitems_alloc)
{
  return (nitems < *nitems_alloc
	  ? ptr
	  : erealloc(ptr, grow_nitems_alloc(nitems_alloc, itemsize)));
}

/*
** Error handling.
*/

/* In most of the code, an input file name is represented by its index
   into the main argument vector, except that LEAPSEC_FILENUM stands
   for leapsec and COMMAND_LINE_FILENUM stands for the command line.  */
enum { LEAPSEC_FILENUM = -2, COMMAND_LINE_FILENUM = -1 };

/* Return the name of the Ith input file, for diagnostics.  */
static char const *
filename(int i)
{
  if (i == COMMAND_LINE_FILENUM)
    return _("command line");
  else {
    char const *fname = i == LEAPSEC_FILENUM ? leapsec : main_argv[i];
    return strcmp(fname, "-") == 0 ? _("standard input") : fname;
  }
}

static void
eats(int fnum, lineno num, int rfnum, lineno rnum)
{
	filenum = fnum;
	linenum = num;
	rfilenum = rfnum;
	rlinenum = rnum;
}

static void
eat(int fnum, lineno num)
{
	eats(fnum, num, 0, -1);
}

ATTRIBUTE_FORMAT((printf, 1, 0)) static void
verror(const char *const string, va_list args)
{
	/*
	** Match the format of "cc" to allow sh users to
	**	zic ... 2>&1 | error -t "*" -v
	** on BSD systems.
	*/
	if (filenum)
	  fprintf(stderr, _("\"%s\", line %"PRIdMAX": "),
		  filename(filenum), linenum);
	vfprintf(stderr, string, args);
	if (rfilenum)
		fprintf(stderr, _(" (rule from \"%s\", line %"PRIdMAX")"),
			filename(rfilenum), rlinenum);
	fprintf(stderr, "\n");
}

ATTRIBUTE_FORMAT((printf, 1, 2)) static void
error(const char *const string, ...)
{
	va_list args;
	va_start(args, string);
	verror(string, args);
	va_end(args);
	errors = true;
}

ATTRIBUTE_FORMAT((printf, 1, 2)) static void
warning(const char *const string, ...)
{
	va_list args;
	fprintf(stderr, _("warning: "));
	va_start(args, string);
	verror(string, args);
	va_end(args);
	warnings = true;
}

/* Close STREAM.  If it had an I/O error, report it against DIR/NAME,
   remove TEMPNAME if nonnull, and then exit.  */
static void
close_file(FILE *stream, char const *dir, char const *name,
	   char const *tempname)
{
  char const *e = (ferror(stream) ? _("I/O error")
		   : fclose(stream) != 0 ? strerror(errno) : NULL);
  if (e) {
    fprintf(stderr, "%s: %s%s%s%s%s\n", progname,
	    dir ? dir : "", dir ? "/" : "",
	    name ? name : "", name ? ": " : "",
	    e);
    if (tempname)
      (void)remove(tempname);
    exit(EXIT_FAILURE);
  }
}

ATTRIBUTE_NORETURN static void
usage(FILE *stream, int status)
{
  fprintf(stream,
	  _("%s: usage is %s [ --version ] [ --help ] [ -v ] \\\n"
	    "\t[ -b {slim|fat} ] [ -d directory ] [ -l localtime ]"
	    " [ -L leapseconds ] \\\n"
	    "\t[ -p posixrules ] [ -r '[@lo][/@hi]' ] [ -R '@hi' ] \\\n"
	    "\t[ -t localtime-link ] [ -D ] [ -g gid ] [ -u uid ] \\\n"
	    "\t[ filename ... ]\n\n"
	    "Report bugs to %s.\n"),
	  progname, progname, REPORT_BUGS_TO);
  if (status == EXIT_SUCCESS)
    close_file(stream, NULL, NULL, NULL);
  exit(status);
}

/* Change the working directory to DIR, possibly creating DIR and its
   ancestors.  After this is done, all files are accessed with names
   relative to DIR.  */
static void
change_directory(char const *dir)
{
  if (chdir(dir) != 0) {
    int chdir_errno = errno;
    if (chdir_errno == ENOENT) {
      mkdirs(dir, false);
      chdir_errno = chdir(dir) == 0 ? 0 : errno;
    }
    if (chdir_errno != 0) {
      fprintf(stderr, _("%s: Can't chdir to %s: %s\n"),
	      progname, dir, strerror(chdir_errno));
      exit(EXIT_FAILURE);
    }
  }
}

/* Compare the two links A and B, for a stable sort by link name.  */
static int
qsort_linkcmp(void const *a, void const *b)
{
  struct link const *l = a;
  struct link const *m = b;
  int cmp = strcmp(l->l_linkname, m->l_linkname);
  if (cmp)
    return cmp;

  /* The link names are the same.  Make the sort stable by comparing
     file numbers (where subtraction cannot overflow) and possibly
     line numbers (where it can).  */
  cmp = l->l_filenum - m->l_filenum;
  if (cmp)
    return cmp;
  return (l->l_linenum > m->l_linenum) - (l->l_linenum < m->l_linenum);
}

/* Compare the string KEY to the link B, for bsearch.  */
static int
bsearch_linkcmp(void const *key, void const *b)
{
  struct link const *m = b;
  return strcmp(key, m->l_linkname);
}

/* Make the links specified by the Link lines.  */
static void
make_links(void)
{
  ptrdiff_t i, j, nalinks, pass_size;
  if (1 < nlinks)
    qsort(links, nlinks, sizeof *links, qsort_linkcmp);

  /* Ignore each link superseded by a later link with the same name.  */
  j = 0;
  for (i = 0; i < nlinks; i++) {
    while (i + 1 < nlinks
	   && strcmp(links[i].l_linkname, links[i + 1].l_linkname) == 0)
      i++;
    links[j++] = links[i];
  }
  nlinks = pass_size = j;

  /* Walk through the link array making links.  However,
     if a link's target has not been made yet, append a copy to the
     end of the array.  The end of the array will gradually fill
     up with a small sorted subsequence of not-yet-made links.
     nalinks counts all the links in the array, including copies.
     When we reach the copied subsequence, it may still contain
     a link to a not-yet-made link, so the process repeats.
     At any given point in time, the link array consists of the
     following subregions, where 0 <= i <= j <= nalinks and
     0 <= nlinks <= nalinks:

       0 .. (i - 1):
	 links that either have been made, or have been copied to a
	 later point point in the array (this later point can be in
	 any of the three subregions)
       i .. (j - 1):
	 not-yet-made links for this pass
       j .. (nalinks - 1):
	 not-yet-made links that this pass has skipped because
	 they were links to not-yet-made links

     The first subregion might not be sorted if nlinks < i;
     the other two subregions are sorted.  This algorithm does
     not alter entries 0 .. (nlinks - 1), which remain sorted.

     If there are L links, this algorithm is O(C*L*log(L)) where
     C is the length of the longest link chain.  Usually C is
     short (e.g., 3) though its worst-case value is L.  */

  j = nalinks = nlinks;

  for (i = 0; i < nalinks; i++) {
    struct link *l;

    eat(links[i].l_filenum, links[i].l_linenum);

    /* If this pass examined all its links, start the next pass.  */
    if (i == j) {
      if (nalinks - i == pass_size) {
	error(_("\"Link %s %s\" is part of a link cycle"),
	      links[i].l_target, links[i].l_linkname);
	break;
      }
      j = nalinks;
      pass_size = nalinks - i;
    }

    /* Diagnose self links, which the cycle detection algorithm would not
       otherwise catch.  */
    if (strcmp(links[i].l_target, links[i].l_linkname) == 0) {
      error(_("link %s targets itself"), links[i].l_target);
      continue;
    }

    /* Make this link unless its target has not been made yet.  */
    l = bsearch(links[i].l_target, &links[i + 1], j - (i + 1),
		sizeof *links, bsearch_linkcmp);
    if (!l)
      l = bsearch(links[i].l_target, &links[j], nalinks - j,
		  sizeof *links, bsearch_linkcmp);
    if (!l)
      dolink(links[i].l_target, links[i].l_linkname, false);
    else {
      /* The link target has not been made yet; copy the link to the end.  */
      links = growalloc(links, sizeof *links, nalinks, &nlinks_alloc);
      links[nalinks++] = links[i];
    }

    if (noise && i < nlinks) {
      if (l)
	warning(_("link %s targeting link %s mishandled by pre-2023 zic"),
		links[i].l_linkname, links[i].l_target);
      else if (bsearch(links[i].l_target, links, nlinks, sizeof *links,
		       bsearch_linkcmp))
	warning(_("link %s targeting link %s"),
		links[i].l_linkname, links[i].l_target);
    }
  }
}

/* Simple signal handling: just set a flag that is checked
   periodically outside critical sections.  To set up the handler,
   prefer sigaction if available to close a signal race.  */

static sig_atomic_t got_signal;

static void
signal_handler(int sig)
{
#ifndef SA_SIGINFO
  signal(sig, signal_handler);
#endif
  got_signal = sig;
}

/* Arrange for SIGINT etc. to be caught by the handler.  */
static void
catch_signals(void)
{
  static int const signals[] = {
#ifdef SIGHUP
    SIGHUP,
#endif
    SIGINT,
#ifdef SIGPIPE
    SIGPIPE,
#endif
    SIGTERM
  };
  size_t i;
  for (i = 0; i < sizeof signals / sizeof signals[0]; i++) {
#ifdef SA_SIGINFO
    struct sigaction act0, act;
    act.sa_handler = signal_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(signals[i], &act, &act0) == 0
	&& ! (act0.sa_flags & SA_SIGINFO) && act0.sa_handler == SIG_IGN) {
      sigaction(signals[i], &act0, NULL);
      got_signal = 0;
    }
#else
    if (signal(signals[i], signal_handler) == SIG_IGN) {
      signal(signals[i], SIG_IGN);
      got_signal = 0;
    }
#endif
  }
}

/* If a signal has arrived, terminate zic with appropriate status.  */
static void
check_for_signal(void)
{
  int sig = got_signal;
  if (sig) {
    signal(sig, SIG_DFL);
    raise(sig);
    abort(); /* A bug in 'raise'.  */
  }
}

enum { TIME_T_BITS_IN_FILE = 64 };

/* The minimum and maximum values representable in a TZif file.  */
static zic_t const min_time = MINVAL(zic_t, TIME_T_BITS_IN_FILE);
static zic_t const max_time = MAXVAL(zic_t, TIME_T_BITS_IN_FILE);

/* The minimum, and one less than the maximum, values specified by
   the -r option.  These default to MIN_TIME and MAX_TIME.  */
static zic_t lo_time = MINVAL(zic_t, TIME_T_BITS_IN_FILE);
static zic_t hi_time = MAXVAL(zic_t, TIME_T_BITS_IN_FILE);

/* The time specified by the -R option, defaulting to MIN_TIME;
   or lo_time, whichever is greater.  */
static zic_t redundant_time = MINVAL(zic_t, TIME_T_BITS_IN_FILE);

/* The time specified by an Expires line, or negative if no such line.  */
static zic_t leapexpires = -1;

/* Set the time range of the output to TIMERANGE.
   Return true if successful.  */
static bool
timerange_option(char *timerange)
{
  intmax_t lo = min_time, hi = max_time;
  char *lo_end = timerange, *hi_end;
  if (*timerange == '@') {
    errno = 0;
    lo = strtoimax(timerange + 1, &lo_end, 10);
    if (lo_end == timerange + 1 || (lo == INTMAX_MAX && errno == ERANGE))
      return false;
  }
  hi_end = lo_end;
  if (lo_end[0] == '/' && lo_end[1] == '@') {
    errno = 0;
    hi = strtoimax(lo_end + 2, &hi_end, 10);
    if (hi_end == lo_end + 2 || hi == INTMAX_MIN)
      return false;
    hi -= ! (hi == INTMAX_MAX && errno == ERANGE);
  }
  if (*hi_end || hi < lo || max_time < lo || hi < min_time)
    return false;
  lo_time = max(lo, min_time);
  hi_time = min(hi, max_time);
  return true;
}

/* Generate redundant time stamps up to OPT.  Return true if successful.  */
static bool
redundant_time_option(char *opt)
{
  if (*opt == '@') {
    intmax_t redundant;
    char *opt_end;
    redundant = strtoimax(opt + 1, &opt_end, 10);
    if (opt_end != opt + 1 && !*opt_end) {
      redundant_time = max(redundant_time, redundant);
      return true;
    }
  }
  return false;
}

static const char *	psxrules;
static const char *	lcltime;
static const char *	directory;
static const char *	leapsec;
static int		Dflag;
static uid_t		uflag = (uid_t)-1;
static gid_t		gflag = (gid_t)-1;
static mode_t		mflag = (S_IRUSR | S_IRGRP | S_IROTH
				 | S_IWUSR);
static const char *	tzdefault;

/* -1 if the TZif output file should be slim, 0 if default, 1 if the
   output should be fat for backward compatibility.  ZIC_BLOAT_DEFAULT
   determines the default.  */
static int bloat;

static bool
want_bloat(void)
{
  return 0 <= bloat;
}

#ifndef ZIC_BLOAT_DEFAULT
# define ZIC_BLOAT_DEFAULT "slim"
#endif

int
main(int argc, char **argv)
{
	register int c, k;
	register ptrdiff_t i, j;
	bool timerange_given = false;

#ifdef S_IWGRP
	umask(umask(S_IWGRP | S_IWOTH) | (S_IWGRP | S_IWOTH));
#endif
#if HAVE_GETTEXT
	setlocale(LC_ALL, "");
# ifdef TZ_DOMAINDIR
	bindtextdomain(TZ_DOMAIN, TZ_DOMAINDIR);
# endif /* defined TEXTDOMAINDIR */
	textdomain(TZ_DOMAIN);
#endif /* HAVE_GETTEXT */
	main_argv = argv;
	progname = /* argv[0] ? argv[0] : */ "zic";
	if (TYPE_BIT(zic_t) < 64) {
		fprintf(stderr, "%s: %s\n", progname,
			_("wild compilation-time specification of zic_t"));
		return EXIT_FAILURE;
	}
	for (k = 1; k < argc; k++)
		if (strcmp(argv[k], "--version") == 0) {
			printf("zic %s%s\n", PKGVERSION, TZVERSION);
			close_file(stdout, NULL, NULL, NULL);
			return EXIT_SUCCESS;
		} else if (strcmp(argv[k], "--help") == 0) {
			usage(stdout, EXIT_SUCCESS);
		}
	while ((c = getopt(argc, argv, "Db:d:g:l:L:m:p:r:R:st:u:vy:")) != EOF
	       && c != -1)
		switch (c) {
			default:
				usage(stderr, EXIT_FAILURE);
			case 'D':
				Dflag = 1;
				break;
			case 'b':
				if (strcmp(optarg, "slim") == 0) {
				  if (0 < bloat)
				    error(_("incompatible -b options"));
				  bloat = -1;
				} else if (strcmp(optarg, "fat") == 0) {
				  if (bloat < 0)
				    error(_("incompatible -b options"));
				  bloat = 1;
				} else
				  error(_("invalid option: -b '%s'"), optarg);
				break;
			case 'd':
				if (directory == NULL)
					directory = optarg;
				else {
					fprintf(stderr,
						_("%s: More than one -d option"
						  " specified\n"),
						progname);
					return EXIT_FAILURE;
				}
				break;
			case 'g':
				setgroup(&gflag, optarg);
				break;
			case 'l':
				if (lcltime == NULL)
					lcltime = optarg;
				else {
					fprintf(stderr,
						_("%s: More than one -l option"
						  " specified\n"),
						progname);
					return EXIT_FAILURE;
				}
				break;
			case 'm':
			{
				void *set = setmode(optarg);
				if (set == NULL) {
					fprintf(stderr,
_("invalid file mode"));
					return EXIT_FAILURE;
				}
				mflag = getmode(set, mflag);
				free(set);
				break;
			}
			case 'p':
				if (psxrules == NULL)
					psxrules = optarg;
				else {
					fprintf(stderr,
						_("%s: More than one -p option"
						  " specified\n"),
						progname);
					return EXIT_FAILURE;
				}
				break;
			case 't':
				if (tzdefault != NULL) {
				  fprintf(stderr,
					  _("%s: More than one -t option"
					    " specified\n"),
					  progname);
				  return EXIT_FAILURE;
				}
				tzdefault = optarg;
				break;
			case 'u':
				setuser(&uflag, optarg);
				break;
			case 'y':
				warning(_("-y ignored"));
				break;
			case 'L':
				if (leapsec == NULL)
					leapsec = optarg;
				else {
					fprintf(stderr,
						_("%s: More than one -L option"
						  " specified\n"),
						progname);
					return EXIT_FAILURE;
				}
				break;
			case 'v':
				noise = true;
				break;
			case 'r':
				if (timerange_given) {
				  fprintf(stderr,
					  _("%s: More than one -r option"
					    " specified\n"),
					  progname);
				  return EXIT_FAILURE;
				}
				if (! timerange_option(optarg)) {
				  fprintf(stderr,
					  _("%s: invalid time range: %s\n"),
					  progname, optarg);
				  return EXIT_FAILURE;
				}
				timerange_given = true;
				break;
			case 'R':
				if (! redundant_time_option(optarg)) {
				  fprintf(stderr, _("%s: invalid time: %s\n"),
					  progname, optarg);
				  return EXIT_FAILURE;
				}
				break;
			case 's':
				warning(_("-s ignored"));
				break;
		}
	if (optind == argc - 1 && strcmp(argv[optind], "=") == 0)
		usage(stderr, EXIT_FAILURE);	/* usage message by request */
	if (hi_time + (hi_time < ZIC_MAX) < redundant_time) {
	  fprintf(stderr, _("%s: -R time exceeds -r cutoff\n"), progname);
	  return EXIT_FAILURE;
	}
	if (redundant_time < lo_time)
	  redundant_time = lo_time;
	if (bloat == 0) {
	  static char const bloat_default[] = ZIC_BLOAT_DEFAULT;
	  if (strcmp(bloat_default, "slim") == 0)
	    bloat = -1;
	  else if (strcmp(bloat_default, "fat") == 0)
	    bloat = 1;
	  else
	    abort(); /* Configuration error.  */
	}
	if (directory == NULL)
		directory = TZDIR;
	if (tzdefault == NULL)
		tzdefault = TZDEFAULT;

	if (optind < argc && leapsec != NULL) {
		infile(LEAPSEC_FILENUM, leapsec);
		adjleap();
	}

	for (k = optind; k < argc; k++)
	  infile(k, argv[k]);
	if (errors)
		return EXIT_FAILURE;
	associate();
	change_directory(directory);
	catch_signals();
	for (i = 0; i < nzones; i = j) {
		/*
		** Find the next non-continuation zone entry.
		*/
		for (j = i + 1; j < nzones && zones[j].z_name == NULL; ++j)
			continue;
		outzone(&zones[i], j - i);
	}
	make_links();
	if (lcltime != NULL) {
		eat(COMMAND_LINE_FILENUM, 1);
		dolink(lcltime, tzdefault, true);
	}
	if (psxrules != NULL) {
		eat(COMMAND_LINE_FILENUM, 1);
		dolink(psxrules, TZDEFRULES, true);
	}
	if (warnings && (ferror(stderr) || fclose(stderr) != 0))
	  return EXIT_FAILURE;
	return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

static bool
componentcheck(char const *name, char const *component,
	       char const *component_end)
{
	enum { component_len_max = 14 };
	ptrdiff_t component_len = component_end - component;
	if (component_len == 0) {
	  if (!*name)
	    error(_("empty file name"));
	  else
	    error(_(component == name
		     ? "file name '%s' begins with '/'"
		     : *component_end
		     ? "file name '%s' contains '//'"
		     : "file name '%s' ends with '/'"),
		   name);
	  return false;
	}
	if (0 < component_len && component_len <= 2
	    && component[0] == '.' && component_end[-1] == '.') {
	  int len = component_len;
	  error(_("file name '%s' contains '%.*s' component"),
		name, len, component);
	  return false;
	}
	if (noise) {
	  if (0 < component_len && component[0] == '-')
	    warning(_("file name '%s' component contains leading '-'"),
		    name);
	  if (component_len_max < component_len)
	    warning(_("file name '%s' contains overlength component"
		      " '%.*s...'"),
		    name, component_len_max, component);
	}
	return true;
}

static bool
namecheck(const char *name)
{
	register char const *cp;

	/* Benign characters in a portable file name.  */
	static char const benign[] =
	  "-/_"
	  "abcdefghijklmnopqrstuvwxyz"
	  "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	/* Non-control chars in the POSIX portable character set,
	   excluding the benign characters.  */
	static char const printable_and_not_benign[] =
	  " !\"#$%&'()*+,.0123456789:;<=>?@[\\]^`{|}~";

	register char const *component = name;
	for (cp = name; *cp; cp++) {
		unsigned char c = *cp;
		if (noise && !strchr(benign, c)) {
			warning((strchr(printable_and_not_benign, c)
				 ? _("file name '%s' contains byte '%c'")
				 : _("file name '%s' contains byte '\\%o'")),
				name, c);
		}
		if (c == '/') {
			if (!componentcheck(name, component, cp))
			  return false;
			component = cp + 1;
		}
	}
	return componentcheck(name, component, cp);
}

/* Return a random uint_fast64_t.  */
static uint_fast64_t
get_rand_u64(void)
{
#if HAVE_GETRANDOM
  static uint_fast64_t entropy_buffer[max(1, 256 / sizeof(uint_fast64_t))];
  static int nwords;
  if (!nwords) {
    ssize_t s;
    do
      s = getrandom(entropy_buffer, sizeof entropy_buffer, 0);
    while (s < 0 && errno == EINTR);

    if (s < 0)
      nwords = -1;
    else
      nwords = s / sizeof *entropy_buffer;
  }
  if (0 < nwords)
    return entropy_buffer[--nwords];
#endif

  /* getrandom didn't work, so fall back on portable code that is
     not the best because the seed isn't cryptographically random and
     'rand' might not be cryptographically secure.  */
  {
    static bool initialized;
    if (!initialized) {
      srand(time(NULL));
      initialized = true;
    }
  }

  /* Return a random number if rand() yields a random number and in
     the typical case where RAND_MAX is one less than a power of two.
     In other cases this code yields a sort-of-random number.  */
  {
    uint_fast64_t rand_max = RAND_MAX,
      nrand = rand_max < UINT_FAST64_MAX ? rand_max + 1 : 0,
      rmod = INT_MAX < UINT_FAST64_MAX ? 0 : UINT_FAST64_MAX / nrand + 1,
      r = 0, rmax = 0;

    do {
      uint_fast64_t rmax1 = rmax;
      if (rmod) {
	/* Avoid signed integer overflow on theoretical platforms
	   where uint_fast64_t promotes to int.  */
	rmax1 %= rmod;
	r %= rmod;
      }
      rmax1 = nrand * rmax1 + rand_max;
      r = nrand * r + rand();
      rmax = rmax < rmax1 ? rmax1 : UINT_FAST64_MAX;
    } while (rmax < UINT_FAST64_MAX);

    return r;
  }
}

/* Generate a randomish name in the same directory as *NAME.  If
   *NAMEALLOC, put the name into *NAMEALLOC which is assumed to be
   that returned by a previous call and is thus already almost set up
   and equal to *NAME; otherwise, allocate a new name and put its
   address into both *NAMEALLOC and *NAME.  */
static void
random_dirent(char const **name, char **namealloc)
{
  char const *src = *name;
  char *dst = *namealloc;
  static char const prefix[] = ".zic";
  static char const alphabet[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";
  enum { prefixlen = sizeof prefix - 1, alphabetlen = sizeof alphabet - 1 };
  int suffixlen = 6;
  char const *lastslash = strrchr(src, '/');
  ptrdiff_t dirlen = lastslash ? lastslash + 1 - src : 0;
  int i;
  uint_fast64_t r;
  uint_fast64_t base = alphabetlen;

  /* BASE**6 */
  uint_fast64_t base__6 = base * base * base * base * base * base;

  /* The largest uintmax_t that is a multiple of BASE**6.  Any random
     uintmax_t value that is this value or greater, yields a biased
     remainder when divided by BASE**6.  UNFAIR_MIN equals the
     mathematical value of ((UINTMAX_MAX + 1) - (UINTMAX_MAX + 1) % BASE**6)
     computed without overflow.  */
  uint_fast64_t unfair_min = - ((UINTMAX_MAX % base__6 + 1) % base__6);

  if (!dst) {
    dst = emalloc(size_sum(dirlen, prefixlen + suffixlen + 1));
    memcpy(dst, src, dirlen);
    memcpy(dst + dirlen, prefix, prefixlen);
    dst[dirlen + prefixlen + suffixlen] = '\0';
    *name = *namealloc = dst;
  }

  do
    r = get_rand_u64();
  while (unfair_min <= r);

  for (i = 0; i < suffixlen; i++) {
    dst[dirlen + prefixlen + i] = alphabet[r % alphabetlen];
    r /= alphabetlen;
  }
}

/* Prepare to write to the file *OUTNAME, using *TEMPNAME to store the
   name of the temporary file that will eventually be renamed to
   *OUTNAME.  Assign the temporary file's name to both *OUTNAME and
   *TEMPNAME.  If *TEMPNAME is null, allocate the name of any such
   temporary file; otherwise, reuse *TEMPNAME's storage, which is
   already set up and only needs its trailing suffix updated.  */
static FILE *
open_outfile(char const **outname, char **tempname)
{
#if __STDC_VERSION__ < 201112
  static char const fopen_mode[] = "wb";
#else
  static char const fopen_mode[] = "wbx";
#endif

  FILE *fp;
  bool dirs_made = false;
  if (!*tempname)
    random_dirent(outname, tempname);

  /*
   * Remove old file, if any, to snap links.
   */
  if (remove(*outname) != 0 && errno != ENOENT && errno != EISDIR) {
    fprintf(stderr, _("can't remove %s"), *outname);
    exit(EXIT_FAILURE);
  }

  while (! (fp = fopen(*outname, fopen_mode))) {
    int fopen_errno = errno;
    if (fopen_errno == ENOENT && !dirs_made) {
      mkdirs(*outname, true);
      dirs_made = true;
    } else if (fopen_errno == EEXIST)
      random_dirent(outname, tempname);
    else {
      fprintf(stderr, _("%s: Can't create %s/%s: %s\n"),
	      progname, directory, *outname, strerror(fopen_errno));
      exit(EXIT_FAILURE);
    }
  }

  return fp;
}

/* If TEMPNAME, the result is in the temporary file TEMPNAME even
   though the user wanted it in NAME, so rename TEMPNAME to NAME.
   Report an error and exit if there is trouble.  Also, free TEMPNAME.  */
static void
rename_dest(char *tempname, char const *name)
{
  if (tempname) {
    if (rename(tempname, name) != 0) {
      int rename_errno = errno;
      (void)remove(tempname);
      fprintf(stderr, _("%s: rename to %s/%s: %s\n"),
	      progname, directory, name, strerror(rename_errno));
      exit(EXIT_FAILURE);
    }
    free(tempname);
  }
}

/* Create symlink contents suitable for symlinking TARGET to LINKNAME, as a
   freshly allocated string.  TARGET should be a relative file name, and
   is relative to the global variable DIRECTORY.  LINKNAME can be either
   relative or absolute.  */
static char *
relname(char const *target, char const *linkname)
{
  size_t i, taillen, dir_len = 0, dotdots = 0;
  ptrdiff_t dotdotetcsize, linksize = INDEX_MAX;
  char const *f = target;
  char *result = NULL;
  if (*linkname == '/') {
    /* Make F absolute too.  */
    size_t len = strlen(directory);
    size_t lenslash = len + (len && directory[len - 1] != '/');
    size_t targetsize = strlen(target) + 1;
    linksize = size_sum(lenslash, targetsize);
    f = result = emalloc(linksize);
    memcpy(result, directory, len);
    result[len] = '/';
    memcpy(result + lenslash, target, targetsize);
  }
  for (i = 0; f[i] && f[i] == linkname[i]; i++)
    if (f[i] == '/')
      dir_len = i + 1;
  for (; linkname[i]; i++)
    dotdots += linkname[i] == '/' && linkname[i - 1] != '/';
  taillen = strlen(f + dir_len);
  dotdotetcsize = size_sum(size_product(dotdots, 3), taillen + 1);
  if (dotdotetcsize <= linksize) {
    if (!result)
      result = emalloc(dotdotetcsize);
    for (i = 0; i < dotdots; i++)
      memcpy(result + 3 * i, "../", 3);
    memmove(result + 3 * dotdots, f + dir_len, taillen + 1);
  }
  return result;
}

/* Return true if A and B must have the same parent dir if A and B exist.
   Return false if this is not necessarily true (though it might be true).
   Keep it simple, and do not inspect the file system.  */
ATTRIBUTE_PURE_114833 static bool
same_parent_dirs(char const *a, char const *b)
{
  for (; *a == *b; a++, b++)
    if (!*a)
      return true;
  return ! (strchr(a, '/') || strchr(b, '/'));
}

static void
dolink(char const *target, char const *linkname, bool staysymlink)
{
	bool linkdirs_made = false;
	int link_errno;
	char *tempname = NULL;
	char const *outname = linkname;
	int targetissym = -2, linknameissym = -2;

	check_for_signal();

	if (strcmp(target, "-") == 0) {
	  if (remove(linkname) == 0 || errno == ENOENT || errno == ENOTDIR)
	    return;
	  else {
	    char const *e = strerror(errno);
	    fprintf(stderr, _("%s: Can't remove %s/%s: %s\n"),
		    progname, directory, linkname, e);
	    exit(EXIT_FAILURE);
	  }
	}

	while (true) {
	  if (linkat(AT_FDCWD, target, AT_FDCWD, outname, AT_SYMLINK_FOLLOW)
	      == 0) {
	    link_errno = 0;
	    break;
	  }
	  link_errno = errno;
	  /* Linux 2.6.16 and 2.6.17 mishandle AT_SYMLINK_FOLLOW.  */
	  if (link_errno == EINVAL)
	    link_errno = ENOTSUP;
#if HAVE_LINK
	  /* If linkat is not supported, fall back on link(A, B).
	     However, skip this if A is a relative symlink
	     and A and B might not have the same parent directory.
	     On some platforms link(A, B) does not follow a symlink A,
	     and if A is relative it might misbehave elsewhere.  */
	  if (link_errno == ENOTSUP
	      && (same_parent_dirs(target, outname)
		  || 0 <= itssymlink(target, &targetissym))) {
	    if (link(target, outname) == 0) {
	      link_errno = 0;
	      break;
	    }
	    link_errno = errno;
	  }
#endif
	  if (link_errno == EXDEV || link_errno == ENOTSUP)
	    break;

	  if (link_errno == EEXIST) {
	    staysymlink &= !tempname;
	    random_dirent(&outname, &tempname);
	    if (staysymlink && itssymlink(linkname, &linknameissym))
	      break;
	  } else if (link_errno == ENOENT && !linkdirs_made) {
	    mkdirs(linkname, true);
	    linkdirs_made = true;
	  } else {
	    fprintf(stderr, _("%s: Can't link %s/%s to %s/%s: %s\n"),
		    progname, directory, target, directory, outname,
		    strerror(link_errno));
	    exit(EXIT_FAILURE);
	  }
	}
	if (link_errno != 0) {
	  bool absolute = *target == '/';
	  char *linkalloc = absolute ? NULL : relname(target, linkname);
	  char const *contents = absolute ? target : linkalloc;
	  int symlink_errno;

	  while (true) {
	    if (symlink(contents, outname) == 0) {
	      symlink_errno = 0;
	      break;
	    }
	    symlink_errno = errno;
	    if (symlink_errno == EEXIST)
	      random_dirent(&outname, &tempname);
	    else if (symlink_errno == ENOENT && !linkdirs_made) {
	      mkdirs(linkname, true);
	      linkdirs_made = true;
	    } else
	      break;
	  }
	  free(linkalloc);
	  if (symlink_errno == 0) {
	    if (link_errno != ENOTSUP && link_errno != EEXIST)
	      warning(_("symbolic link used because hard link failed: %s"),
		      strerror(link_errno));
	  } else {
	    FILE *fp, *tp;
	    int c;
	    fp = fopen(target, "rb");
	    if (!fp) {
	      char const *e = strerror(errno);
	      fprintf(stderr, _("%s: Can't read %s/%s: %s\n"),
		      progname, directory, target, e);
	      exit(EXIT_FAILURE);
	    }
	    tp = open_outfile(&outname, &tempname);
	    while ((c = getc(fp)) != EOF)
	      putc(c, tp);
	    close_file(tp, directory, linkname, tempname);
	    close_file(fp, directory, target, NULL);
	    if (link_errno != ENOTSUP)
	      warning(_("copy used because hard link failed: %s"),
		      strerror(link_errno));
	    else if (symlink_errno != ENOTSUP)
	      warning(_("copy used because symbolic link failed: %s"),
		      strerror(symlink_errno));
	  }
	}
	rename_dest(tempname, linkname);
}

/* Return 1 if NAME is an absolute symbolic link, -1 if it is relative,
   0 if it is not a symbolic link.  If *CACHE is not -2, it is the
   cached result of a previous call to this function with the same NAME.  */
static int
itssymlink(char const *name, int *cache)
{
  if (*cache == -2) {
    char c = '\0';
    *cache = readlink(name, &c, 1) < 0 ? 0 : c == '/' ? 1 : -1;
  }
  return *cache;
}

/*
** Associate sets of rules with zones.
*/

/*
** Sort by rule name.
*/

static int
rcomp(const void *cp1, const void *cp2)
{
  struct rule const *r1 = cp1, *r2 = cp2;
  return strcmp(r1->r_name, r2->r_name);
}

static void
associate(void)
{
	register struct zone *	zp;
	register struct rule *	rp;
	register ptrdiff_t i, j, base, out;

	if (1 < nrules) {
		qsort(rules, nrules, sizeof *rules, rcomp);
		for (i = 0; i < nrules - 1; ++i) {
			if (strcmp(rules[i].r_name,
				rules[i + 1].r_name) != 0)
					continue;
			if (rules[i].r_filenum == rules[i + 1].r_filenum)
					continue;
			eat(rules[i].r_filenum, rules[i].r_linenum);
			warning(_("same rule name in multiple files"));
			eat(rules[i + 1].r_filenum, rules[i + 1].r_linenum);
			warning(_("same rule name in multiple files"));
			for (j = i + 2; j < nrules; ++j) {
				if (strcmp(rules[i].r_name,
					rules[j].r_name) != 0)
						break;
				if (rules[i].r_filenum == rules[j].r_filenum)
						continue;
				if (rules[i + 1].r_filenum
				    == rules[j].r_filenum)
						continue;
				break;
			}
			i = j - 1;
		}
	}
	for (i = 0; i < nzones; ++i) {
		zp = &zones[i];
		zp->z_rules = NULL;
		zp->z_nrules = 0;
	}
	for (base = 0; base < nrules; base = out) {
		rp = &rules[base];
		for (out = base + 1; out < nrules; ++out)
			if (strcmp(rp->r_name, rules[out].r_name) != 0)
				break;
		for (i = 0; i < nzones; ++i) {
			zp = &zones[i];
			if (strcmp(zp->z_rule, rp->r_name) != 0)
				continue;
			zp->z_rules = rp;
			zp->z_nrules = out - base;
		}
	}
	for (i = 0; i < nzones; ++i) {
		zp = &zones[i];
		if (zp->z_nrules == 0) {
			/*
			** Maybe we have a local standard time offset.
			*/
			eat(zp->z_filenum, zp->z_linenum);
			zp->z_save = getsave(zp->z_rule, &zp->z_isdst);
			/*
			** Note, though, that if there's no rule,
			** a '%s' in the format is a bad thing.
			*/
			if (zp->z_format_specifier == 's')
				error("%s", _("%s in ruleless zone"));
		}
	}
	if (errors)
		exit(EXIT_FAILURE);
}

/* Read a text line from FP into BUF, which is of size BUFSIZE.
   Terminate it with a NUL byte instead of a newline.
   Return true if successful, false if EOF.
   On error, report the error and exit.  */
static bool
inputline(FILE *fp, char *buf, ptrdiff_t bufsize)
{
  ptrdiff_t linelen = 0, ch;
  while ((ch = getc(fp)) != '\n') {
    if (ch < 0) {
      if (ferror(fp)) {
	error(_("input error"));
	exit(EXIT_FAILURE);
      }
      if (linelen == 0)
	return false;
      error(_("unterminated line"));
      exit(EXIT_FAILURE);
    }
    if (!ch) {
      error(_("NUL input byte"));
      exit(EXIT_FAILURE);
    }
    buf[linelen++] = ch;
    if (linelen == bufsize) {
      error(_("line too long"));
      exit(EXIT_FAILURE);
    }
  }
  buf[linelen] = '\0';
  return true;
}

static void
infile(int fnum, char const *name)
{
	register FILE *			fp;
	register const struct lookup *	lp;
	register bool			wantcont;
	register lineno			num;

	if (strcmp(name, "-") == 0) {
		fp = stdin;
	} else if ((fp = fopen(name, "r")) == NULL) {
		const char *e = strerror(errno);

		fprintf(stderr, _("%s: Can't open %s: %s\n"),
			progname, name, e);
		exit(EXIT_FAILURE);
	}
	wantcont = false;
	for (num = 1; ; ++num) {
		enum { bufsize_bound
		  = (min(INT_MAX, INDEX_MAX) / FORMAT_LEN_GROWTH_BOUND) };
		char buf[min(_POSIX2_LINE_MAX, bufsize_bound)];
		int nfields;
		char *fields[MAX_FIELDS];
		eat(fnum, num);
		if (!inputline(fp, buf, sizeof buf))
		  break;
		nfields = getfields(buf, fields,
				    sizeof fields / sizeof *fields);
		if (nfields == 0) {
			/* nothing to do */
		} else if (wantcont) {
			wantcont = inzcont(fields, nfields);
		} else {
			struct lookup const *line_codes
			  = fnum < 0 ? leap_line_codes : zi_line_codes;
			lp = byword(fields[0], line_codes);
			if (lp == NULL)
				error(_("input line of unknown type"));
			else switch (lp->l_value) {
				case LC_RULE:
					inrule(fields, nfields);
					wantcont = false;
					break;
				case LC_ZONE:
					wantcont = inzone(fields, nfields);
					break;
				case LC_LINK:
					inlink(fields, nfields);
					wantcont = false;
					break;
				case LC_LEAP:
					inleap(fields, nfields);
					wantcont = false;
					break;
				case LC_EXPIRES:
					inexpires(fields, nfields);
					wantcont = false;
					break;
				default: unreachable();
			}
		}
	}
	close_file(fp, NULL, filename(fnum), NULL);
	if (wantcont)
		error(_("expected continuation line not found"));
}

/*
** Convert a string of one of the forms
**	h	-h	hh:mm	-hh:mm	hh:mm:ss	-hh:mm:ss
** into a number of seconds.
** A null string maps to zero.
** Call error with errstring and return zero on errors.
*/

static zic_t
gethms(char const *string, char const *errstring)
{
	zic_t	hh;
	int sign, mm = 0, ss = 0;
	char hhx, mmx, ssx, xr = '0', xs;
	int tenths = 0;
	bool ok = true;

	if (string == NULL || *string == '\0')
		return 0;
	if (*string == '-') {
		sign = -1;
		++string;
	} else	sign = 1;
	switch (sscanf(string,
		       "%"SCNdZIC"%c%d%c%d%c%1d%*[0]%c%*[0123456789]%c",
		       &hh, &hhx, &mm, &mmx, &ss, &ssx, &tenths, &xr, &xs)) {
	  default: ok = false; break;
	  case 8:
	    ok = '0' <= xr && xr <= '9';
	    ATTRIBUTE_FALLTHROUGH;
	  case 7:
	    ok &= ssx == '.';
	    if (ok && noise)
	      warning(_("fractional seconds rejected by"
			" pre-2018 versions of zic"));
	    ATTRIBUTE_FALLTHROUGH;
	  case 5: ok &= mmx == ':'; ATTRIBUTE_FALLTHROUGH;
	  case 3: ok &= hhx == ':'; ATTRIBUTE_FALLTHROUGH;
	  case 1: break;
	}
	if (!ok) {
			error("%s", errstring);
			return 0;
	}
	if (hh < 0 ||
		mm < 0 || mm >= MINSPERHOUR ||
		ss < 0 || ss > SECSPERMIN) {
			error("%s", errstring);
			return 0;
	}
	if (ZIC_MAX / SECSPERHOUR < hh) {
		error(_("time overflow"));
		return 0;
	}
	ss += 5 + ((ss ^ 1) & (xr == '0')) <= tenths; /* Round to even.  */
	if (noise && (hh > HOURSPERDAY ||
		(hh == HOURSPERDAY && (mm != 0 || ss != 0))))
warning(_("values over 24 hours not handled by pre-2007 versions of zic"));
	return oadd(sign * hh * SECSPERHOUR,
		    sign * (mm * SECSPERMIN + ss));
}

static zic_t
getsave(char *field, bool *isdst)
{
  int dst = -1;
  zic_t save;
  ptrdiff_t fieldlen = strlen(field);
  if (fieldlen != 0) {
    char *ep = field + fieldlen - 1;
    switch (*ep) {
      case 'd': dst = 1; *ep = '\0'; break;
      case 's': dst = 0; *ep = '\0'; break;
    }
  }
  save = gethms(field, _("invalid saved time"));
  *isdst = dst < 0 ? save != 0 : dst;
  return save;
}

static void
inrule(char **fields, int nfields)
{
	struct rule r = { 0 };

	if (nfields != RULE_FIELDS) {
		error(_("wrong number of fields on Rule line"));
		return;
	}
	switch (*fields[RF_NAME]) {
	  case '\0':
	  case ' ': case '\f': case '\n': case '\r': case '\t': case '\v':
	  case '+': case '-':
	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7': case '8': case '9':
		error(_("Invalid rule name \"%s\""), fields[RF_NAME]);
		return;
	}
	r.r_filenum = filenum;
	r.r_linenum = linenum;
	r.r_save = getsave(fields[RF_SAVE], &r.r_isdst);
	if (!rulesub(&r, fields[RF_LOYEAR], fields[RF_HIYEAR],
		     fields[RF_COMMAND], fields[RF_MONTH], fields[RF_DAY],
		     fields[RF_TOD]))
	  return;
	r.r_name = estrdup(fields[RF_NAME]);
	r.r_abbrvar = estrdup(fields[RF_ABBRVAR]);
	if (max_abbrvar_len < strlen(r.r_abbrvar))
		max_abbrvar_len = strlen(r.r_abbrvar);
	rules = growalloc(rules, sizeof *rules, nrules, &nrules_alloc);
	rules[nrules++] = r;
}

static bool
inzone(char **fields, int nfields)
{
	register ptrdiff_t i;

	if (nfields < ZONE_MINFIELDS || nfields > ZONE_MAXFIELDS) {
		error(_("wrong number of fields on Zone line"));
		return false;
	}
	if (lcltime != NULL && strcmp(fields[ZF_NAME], tzdefault) == 0) {
	  error(_("\"Zone %s\" line and -l option are mutually exclusive"),
		tzdefault);
	  return false;
	}
	if (strcmp(fields[ZF_NAME], TZDEFRULES) == 0 && psxrules != NULL) {
	  error(_("\"Zone %s\" line and -p option are mutually exclusive"),
		TZDEFRULES);
	  return false;
	}
	for (i = 0; i < nzones; ++i)
		if (zones[i].z_name != NULL &&
			strcmp(zones[i].z_name, fields[ZF_NAME]) == 0) {
				error(_("duplicate zone name %s"
					" (file \"%s\", line %"PRIdMAX")"),
				      fields[ZF_NAME],
				      filename(zones[i].z_filenum),
				      zones[i].z_linenum);
				return false;
		}
	return inzsub(fields, nfields, false);
}

static bool
inzcont(char **fields, int nfields)
{
	if (nfields < ZONEC_MINFIELDS || nfields > ZONEC_MAXFIELDS) {
		error(_("wrong number of fields on Zone continuation line"));
		return false;
	}
	return inzsub(fields, nfields, true);
}

static bool
inzsub(char **fields, int nfields, bool iscont)
{
	register char *		cp;
	char *			cp1;
	struct zone		z = { 0 };
	int format_len;
	register int		i_stdoff, i_rule, i_format;
	register int		i_untilyear, i_untilmonth;
	register int		i_untilday, i_untiltime;
	register bool		hasuntil;

	if (iscont) {
		i_stdoff = ZFC_STDOFF;
		i_rule = ZFC_RULE;
		i_format = ZFC_FORMAT;
		i_untilyear = ZFC_TILYEAR;
		i_untilmonth = ZFC_TILMONTH;
		i_untilday = ZFC_TILDAY;
		i_untiltime = ZFC_TILTIME;
	} else if (!namecheck(fields[ZF_NAME]))
		return false;
	else {
		i_stdoff = ZF_STDOFF;
		i_rule = ZF_RULE;
		i_format = ZF_FORMAT;
		i_untilyear = ZF_TILYEAR;
		i_untilmonth = ZF_TILMONTH;
		i_untilday = ZF_TILDAY;
		i_untiltime = ZF_TILTIME;
	}
	z.z_filenum = filenum;
	z.z_linenum = linenum;
	z.z_stdoff = gethms(fields[i_stdoff], _("invalid UT offset"));
	if ((cp = strchr(fields[i_format], '%')) != 0) {
		if ((*++cp != 's' && *cp != 'z') || strchr(cp, '%')
		    || strchr(fields[i_format], '/')) {
			error(_("invalid abbreviation format"));
			return false;
		}
	}
	z.z_format_specifier = cp ? *cp : '\0';
	format_len = strlen(fields[i_format]);
	if (max_format_len < format_len)
	  max_format_len = format_len;
	hasuntil = nfields > i_untilyear;
	if (hasuntil) {
		z.z_untilrule.r_filenum = filenum;
		z.z_untilrule.r_linenum = linenum;
		if (!rulesub(
			&z.z_untilrule,
			fields[i_untilyear],
			"only",
			"",
			(nfields > i_untilmonth) ?
			fields[i_untilmonth] : "Jan",
			(nfields > i_untilday) ? fields[i_untilday] : "1",
			(nfields > i_untiltime) ? fields[i_untiltime] : "0"))
		  return false;
		z.z_untiltime = rpytime(&z.z_untilrule,
			z.z_untilrule.r_loyear);
		if (iscont && nzones > 0 &&
			z.z_untiltime > min_time &&
			z.z_untiltime < max_time &&
			zones[nzones - 1].z_untiltime > min_time &&
			zones[nzones - 1].z_untiltime < max_time &&
			zones[nzones - 1].z_untiltime >= z.z_untiltime) {
		  error(_("Zone continuation line end time is"
			  " not after end time of previous line"));
		  return false;
		}
	}
	z.z_name = iscont ? NULL : estrdup(fields[ZF_NAME]);
	z.z_rule = estrdup(fields[i_rule]);
	z.z_format = cp1 = estrdup(fields[i_format]);
	if (z.z_format_specifier == 'z') {
	  cp1[cp - fields[i_format]] = 's';
	  if (noise)
	    warning(_("format '%s' not handled by pre-2015 versions of zic"),
		    fields[i_format]);
	}
	zones = growalloc(zones, sizeof *zones, nzones, &nzones_alloc);
	zones[nzones++] = z;
	/*
	** If there was an UNTIL field on this line,
	** there's more information about the zone on the next line.
	*/
	return hasuntil;
}

static zic_t
getleapdatetime(char **fields, bool expire_line)
{
	register const char *		cp;
	register const struct lookup *	lp;
	register zic_t			i, j;
	zic_t				year;
	int				month, day;
	zic_t				dayoff, tod;
	zic_t				t;
	char xs;

	dayoff = 0;
	cp = fields[LP_YEAR];
	if (sscanf(cp, "%"SCNdZIC"%c", &year, &xs) != 1) {
		/*
		** Leapin' Lizards!
		*/
		error(_("invalid leaping year"));
		return -1;
	}
	if (!expire_line) {
	    if (!leapseen || leapmaxyear < year)
		leapmaxyear = year;
	    if (!leapseen || leapminyear > year)
		leapminyear = year;
	    leapseen = true;
	}
	j = EPOCH_YEAR;
	while (j != year) {
		if (year > j) {
			i = len_years[isleap(j)];
			++j;
		} else {
			--j;
			i = -len_years[isleap(j)];
		}
		dayoff = oadd(dayoff, i);
	}
	if ((lp = byword(fields[LP_MONTH], mon_names)) == NULL) {
		error(_("invalid month name"));
		return -1;
	}
	month = lp->l_value;
	j = TM_JANUARY;
	while (j != month) {
		i = len_months[isleap(year)][j];
		dayoff = oadd(dayoff, i);
		++j;
	}
	cp = fields[LP_DAY];
	if (sscanf(cp, "%d%c", &day, &xs) != 1 ||
		day <= 0 || day > len_months[isleap(year)][month]) {
			error(_("invalid day of month"));
			return -1;
	}
	dayoff = oadd(dayoff, day - 1);
	if (dayoff < min_time / SECSPERDAY) {
		error(_("time too small"));
		return -1;
	}
	if (dayoff > max_time / SECSPERDAY) {
		error(_("time too large"));
		return -1;
	}
	t = dayoff * SECSPERDAY;
	tod = gethms(fields[LP_TIME], _("invalid time of day"));
	t = tadd(t, tod);
	if (t < 0)
	  error(_("leap second precedes Epoch"));
	return t;
}

static void
inleap(char **fields, int nfields)
{
  if (nfields != LEAP_FIELDS)
    error(_("wrong number of fields on Leap line"));
  else {
    zic_t t = getleapdatetime(fields, false);
    if (0 <= t) {
      struct lookup const *lp = byword(fields[LP_ROLL], leap_types);
      if (!lp)
	error(_("invalid Rolling/Stationary field on Leap line"));
      else {
	int correction = 0;
	if (!fields[LP_CORR][0]) /* infile() turns "-" into "".  */
	  correction = -1;
	else if (strcmp(fields[LP_CORR], "+") == 0)
	  correction = 1;
	else
	  error(_("invalid CORRECTION field on Leap line"));
	if (correction)
	  leapadd(t, correction, lp->l_value);
      }
    }
  }
}

static void
inexpires(char **fields, int nfields)
{
  if (nfields != EXPIRES_FIELDS)
    error(_("wrong number of fields on Expires line"));
  else if (0 <= leapexpires)
    error(_("multiple Expires lines"));
  else
    leapexpires = getleapdatetime(fields, true);
}

static void
inlink(char **fields, int nfields)
{
	struct link	l;

	if (nfields != LINK_FIELDS) {
		error(_("wrong number of fields on Link line"));
		return;
	}
	if (*fields[LF_TARGET] == '\0') {
		error(_("blank TARGET field on Link line"));
		return;
	}
	if (! namecheck(fields[LF_LINKNAME]))
	  return;
	l.l_filenum = filenum;
	l.l_linenum = linenum;
	l.l_target = estrdup(fields[LF_TARGET]);
	l.l_linkname = estrdup(fields[LF_LINKNAME]);
	links = growalloc(links, sizeof *links, nlinks, &nlinks_alloc);
	links[nlinks++] = l;
}

static bool
rulesub(struct rule *rp, const char *loyearp, const char *hiyearp,
	const char *typep, const char *monthp, const char *dayp,
	const char *timep)
{
	register const struct lookup *	lp;
	register const char *		cp;
	register char *			dp;
	register char *			ep;
	char xs;

	if ((lp = byword(monthp, mon_names)) == NULL) {
		error(_("invalid month name"));
		return false;
	}
	rp->r_month = lp->l_value;
	rp->r_todisstd = false;
	rp->r_todisut = false;
	dp = estrdup(timep);
	if (*dp != '\0') {
		ep = dp + strlen(dp) - 1;
		switch (lowerit(*ep)) {
			case 's':	/* Standard */
				rp->r_todisstd = true;
				rp->r_todisut = false;
				*ep = '\0';
				break;
			case 'w':	/* Wall */
				rp->r_todisstd = false;
				rp->r_todisut = false;
				*ep = '\0';
				break;
			case 'g':	/* Greenwich */
			case 'u':	/* Universal */
			case 'z':	/* Zulu */
				rp->r_todisstd = true;
				rp->r_todisut = true;
				*ep = '\0';
				break;
		}
	}
	rp->r_tod = gethms(dp, _("invalid time of day"));
	free(dp);
	/*
	** Year work.
	*/
	cp = loyearp;
	lp = byword(cp, begin_years);
	if (lp) switch (lp->l_value) {
		case YR_MINIMUM:
			warning(_("FROM year \"%s\" is obsolete;"
				  " treated as %d"),
				cp, YEAR_32BIT_MIN - 1);
			rp->r_loyear = YEAR_32BIT_MIN - 1;
			break;
		default: unreachable();
	} else if (sscanf(cp, "%"SCNdZIC"%c", &rp->r_loyear, &xs) != 1) {
		error(_("invalid starting year"));
		return false;
	}
	cp = hiyearp;
	lp = byword(cp, end_years);
	rp->r_hiwasnum = lp == NULL;
	if (!rp->r_hiwasnum) switch (lp->l_value) {
		case YR_MAXIMUM:
			rp->r_hiyear = ZIC_MAX;
			break;
		case YR_ONLY:
			rp->r_hiyear = rp->r_loyear;
			break;
		default: unreachable();
	} else if (sscanf(cp, "%"SCNdZIC"%c", &rp->r_hiyear, &xs) != 1) {
		error(_("invalid ending year"));
		return false;
	}
	if (rp->r_loyear > rp->r_hiyear) {
		error(_("starting year greater than ending year"));
		return false;
	}
	if (*typep != '\0') {
		error(_("year type \"%s\" is unsupported; use \"-\" instead"),
			typep);
		return false;
	}
	/*
	** Day work.
	** Accept things such as:
	**	1
	**	lastSunday
	**	last-Sunday (undocumented; warn about this)
	**	Sun<=20
	**	Sun>=7
	*/
	dp = estrdup(dayp);
	if ((lp = byword(dp, lasts)) != NULL) {
		rp->r_dycode = DC_DOWLEQ;
		rp->r_wday = lp->l_value;
		rp->r_dayofmonth = len_months[1][rp->r_month];
	} else {
		if ((ep = strchr(dp, '<')) != 0)
			rp->r_dycode = DC_DOWLEQ;
		else if ((ep = strchr(dp, '>')) != 0)
			rp->r_dycode = DC_DOWGEQ;
		else {
			ep = dp;
			rp->r_dycode = DC_DOM;
		}
		if (rp->r_dycode != DC_DOM) {
			*ep++ = 0;
			if (*ep++ != '=') {
				error(_("invalid day of month"));
				free(dp);
				return false;
			}
			if ((lp = byword(dp, wday_names)) == NULL) {
				error(_("invalid weekday name"));
				free(dp);
				return false;
			}
			rp->r_wday = lp->l_value;
		}
		if (sscanf(ep, "%d%c", &rp->r_dayofmonth, &xs) != 1 ||
			rp->r_dayofmonth <= 0 ||
			(rp->r_dayofmonth > len_months[1][rp->r_month])) {
				error(_("invalid day of month"));
				free(dp);
				return false;
		}
	}
	free(dp);
	return true;
}

static void
convert(uint_fast32_t val, char *buf)
{
	register int	i;
	register int	shift;
	unsigned char *const b = (unsigned char *) buf;

	for (i = 0, shift = 24; i < 4; ++i, shift -= 8)
	  b[i] = (val >> shift) & 0xff;
}

static void
convert64(uint_fast64_t val, char *buf)
{
	register int	i;
	register int	shift;
	unsigned char *const b = (unsigned char *) buf;

	for (i = 0, shift = 56; i < 8; ++i, shift -= 8)
	  b[i] = (val >> shift) & 0xff;
}

static void
puttzcode(zic_t val, FILE *fp)
{
	char	buf[4];

	convert(val, buf);
	fwrite(buf, sizeof buf, 1, fp);
}

static void
puttzcodepass(zic_t val, FILE *fp, int pass)
{
  if (pass == 1)
    puttzcode(val, fp);
  else {
	char	buf[8];

	convert64(val, buf);
	fwrite(buf, sizeof buf, 1, fp);
  }
}

static int
atcomp(const void *avp, const void *bvp)
{
  struct attype const *ap = avp, *bp = bvp;
  zic_t a = ap->at, b = bp->at;
  return a < b ? -1 : a > b;
}

struct timerange {
  int defaulttype;
  ptrdiff_t base, count;
  int leapbase, leapcount;
  bool leapexpiry;
};

static struct timerange
limitrange(struct timerange r, zic_t lo, zic_t hi,
	   zic_t const *ats, unsigned char const *types)
{
  /* Omit ordinary transitions < LO.  */
  while (0 < r.count && ats[r.base] < lo) {
    r.defaulttype = types[r.base];
    r.count--;
    r.base++;
  }

  /* Omit as many initial leap seconds as possible, such that the
     first leap second in the truncated list is <= LO, and is a
     positive leap second if and only if it has a positive correction.
     This supports common TZif readers that assume that the first leap
     second is positive if and only if its correction is positive.  */
  while (1 < r.leapcount && trans[r.leapbase + 1] <= lo) {
    r.leapcount--;
    r.leapbase++;
  }
  while (0 < r.leapbase
	 && ((corr[r.leapbase - 1] < corr[r.leapbase])
	     != (0 < corr[r.leapbase]))) {
    r.leapcount++;
    r.leapbase--;
  }


  /* Omit ordinary and leap second transitions greater than HI + 1.  */
  if (hi < max_time) {
    while (0 < r.count && hi + 1 < ats[r.base + r.count - 1])
      r.count--;
    while (0 < r.leapcount && hi + 1 < trans[r.leapbase + r.leapcount - 1])
      r.leapcount--;
  }

  /* Determine whether to append an expiration to the leap second table.  */
  r.leapexpiry = 0 <= leapexpires && leapexpires - 1 <= hi;

  return r;
}

static void
writezone(const char *const name, const char *const string, char version,
	  int defaulttype)
{
	register FILE *			fp;
	register ptrdiff_t		i, j;
	register size_t			u;
	register int			pass;
	char *tempname = NULL;
	char const *outname = name;

	/* Allocate the ATS and TYPES arrays via a single malloc,
	   as this is a bit faster.  Do not malloc(0) if !timecnt,
	   as that might return NULL even on success.  */
	zic_t *ats = emalloc(align_to(size_product(timecnt + !timecnt,
						   sizeof *ats + 1),
				      alignof(zic_t)));
	void *typesptr = ats + timecnt;
	unsigned char *types = typesptr;
	struct timerange rangeall = {0}, range32, range64;

	/*
	** Sort.
	*/
	if (timecnt > 1)
		qsort(attypes, timecnt, sizeof *attypes, atcomp);
	/*
	** Optimize.
	*/
	{
		ptrdiff_t fromi, toi;

		toi = 0;
		fromi = 0;
		for ( ; fromi < timecnt; ++fromi) {
			if (toi != 0
			    && ((attypes[fromi].at
				 + utoffs[attypes[toi - 1].type])
				<= (attypes[toi - 1].at
				    + utoffs[toi == 1 ? 0
					     : attypes[toi - 2].type]))) {
					attypes[toi - 1].type =
						attypes[fromi].type;
					continue;
			}
			if (toi == 0
			    || attypes[fromi].dontmerge
			    || (utoffs[attypes[toi - 1].type]
				!= utoffs[attypes[fromi].type])
			    || (isdsts[attypes[toi - 1].type]
				!= isdsts[attypes[fromi].type])
			    || (desigidx[attypes[toi - 1].type]
				!= desigidx[attypes[fromi].type]))
					attypes[toi++] = attypes[fromi];
		}
		timecnt = toi;
	}

	if (noise && timecnt > 1200) {
	  if (timecnt > TZ_MAX_TIMES)
		warning(_("reference clients mishandle"
			  " more than %d transition times"),
			TZ_MAX_TIMES);
	  else
		warning(_("pre-2014 clients may mishandle"
			  " more than 1200 transition times"));
	}
	/*
	** Transfer.
	*/
	for (i = 0; i < timecnt; ++i) {
		ats[i] = attypes[i].at;
		types[i] = attypes[i].type;
	}

	/*
	** Correct for leap seconds.
	*/
	for (i = 0; i < timecnt; ++i) {
		j = leapcnt;
		while (--j >= 0)
			if (ats[i] > trans[j] - corr[j]) {
				ats[i] = tadd(ats[i], corr[j]);
				break;
			}
	}

	rangeall.defaulttype = defaulttype;
	rangeall.count = timecnt;
	rangeall.leapcount = leapcnt;
	range64 = limitrange(rangeall, lo_time,
			     max(hi_time,
				 redundant_time - (ZIC_MIN < redundant_time)),
			     ats, types);
	range32 = limitrange(range64, ZIC32_MIN, ZIC32_MAX, ats, types);

	/* TZif version 4 is needed if a no-op transition is appended to
	   indicate the expiration of the leap second table, or if the first
	   leap second transition is not to a +1 or -1 correction.  */
	for (pass = 1; pass <= 2; pass++) {
	  struct timerange const *r = pass == 1 ? &range32 : &range64;
	  if (pass == 1 && !want_bloat())
	    continue;
	  if (r->leapexpiry) {
	    if (noise)
	      warning(_("%s: pre-2021b clients may mishandle"
			" leap second expiry"),
		      name);
	    version = '4';
	  }
	  if (0 < r->leapcount
	      && corr[r->leapbase] != 1 && corr[r->leapbase] != -1) {
	    if (noise)
	      warning(_("%s: pre-2021b clients may mishandle"
			" leap second table truncation"),
		      name);
	    version = '4';
	  }
	  if (version == '4')
	    break;
	}

	fp = open_outfile(&outname, &tempname);

	for (pass = 1; pass <= 2; ++pass) {
		register ptrdiff_t thistimei, thistimecnt, thistimelim;
		register int	thisleapi, thisleapcnt, thisleaplim;
		struct tzhead tzh;
		int pretranstype = -1, thisdefaulttype;
		bool locut, hicut, thisleapexpiry;
		zic_t lo, thismin, thismax;
		int old0;
		char		omittype[TZ_MAX_TYPES];
		int		typemap[TZ_MAX_TYPES];
		int		thistypecnt, stdcnt, utcnt;
		char		thischars[TZ_MAX_CHARS];
		int		thischarcnt;
		bool		toomanytimes;
		int		indmap[TZ_MAX_CHARS];

		if (pass == 1) {
			thisdefaulttype = range32.defaulttype;
			thistimei = range32.base;
			thistimecnt = range32.count;
			toomanytimes = thistimecnt >> 31 >> 1 != 0;
			thisleapi = range32.leapbase;
			thisleapcnt = range32.leapcount;
			thisleapexpiry = range32.leapexpiry;
			thismin = ZIC32_MIN;
			thismax = ZIC32_MAX;
		} else {
			thisdefaulttype = range64.defaulttype;
			thistimei = range64.base;
			thistimecnt = range64.count;
			toomanytimes = thistimecnt >> 31 >> 31 >> 2 != 0;
			thisleapi = range64.leapbase;
			thisleapcnt = range64.leapcount;
			thisleapexpiry = range64.leapexpiry;
			thismin = min_time;
			thismax = max_time;
		}
		if (toomanytimes)
		  error(_("too many transition times"));

		locut = thismin < lo_time && lo_time <= thismax;
		hicut = thismin <= hi_time && hi_time < thismax;
		thistimelim = thistimei + thistimecnt;
		memset(omittype, true, typecnt);

		/* Determine whether to output a transition before the first
		   transition in range.  This is needed when the output is
		   truncated at the start, and is also useful when catering to
		   buggy 32-bit clients that do not use time type 0 for
		   timestamps before the first transition.  */
		if ((locut || (pass == 1 && thistimei))
		    && ! (thistimecnt && ats[thistimei] == lo_time)) {
		  pretranstype = thisdefaulttype;
		  omittype[pretranstype] = false;
		}

		/* Arguably the default time type in the 32-bit data
		   should be range32.defaulttype, which is suited for
		   timestamps just before ZIC32_MIN.  However, zic
		   traditionally used the time type of the indefinite
		   past instead.  Internet RFC 8532 says readers should
		   ignore 32-bit data, so this discrepancy matters only
		   to obsolete readers where the traditional type might
		   be more appropriate even if it's "wrong".  So, use
		   the historical zic value, unless -r specifies a low
		   cutoff that excludes some 32-bit timestamps.  */
		if (pass == 1 && lo_time <= thismin)
		  thisdefaulttype = range64.defaulttype;

		if (locut)
		  thisdefaulttype = unspecifiedtype;
		omittype[thisdefaulttype] = false;
		for (i = thistimei; i < thistimelim; i++)
		  omittype[types[i]] = false;
		if (hicut)
		  omittype[unspecifiedtype] = false;

		/* Reorder types to make THISDEFAULTTYPE type 0.
		   Use TYPEMAP to swap OLD0 and THISDEFAULTTYPE so that
		   THISDEFAULTTYPE appears as type 0 in the output instead
		   of OLD0.  TYPEMAP also omits unused types.  */
		old0 = strlen(omittype);

#ifndef LEAVE_SOME_PRE_2011_SYSTEMS_IN_THE_LURCH
		/*
		** For some pre-2011 systems: if the last-to-be-written
		** standard (or daylight) type has an offset different from the
		** most recently used offset,
		** append an (unused) copy of the most recently used type
		** (to help get global "altzone" and "timezone" variables
		** set correctly).
		*/
		if (want_bloat()) {
			register int	mrudst, mrustd, hidst, histd, type;

			hidst = histd = mrudst = mrustd = -1;
			if (0 <= pretranstype) {
			  if (isdsts[pretranstype])
			    mrudst = pretranstype;
			  else
			    mrustd = pretranstype;
			}
			for (i = thistimei; i < thistimelim; i++)
				if (isdsts[types[i]])
					mrudst = types[i];
				else	mrustd = types[i];
			for (i = old0; i < typecnt; i++) {
			  int h = (i == old0 ? thisdefaulttype
				   : i == thisdefaulttype ? old0 : i);
			  if (!omittype[h]) {
			    if (isdsts[h])
			      hidst = i;
			    else
			      histd = i;
			  }
			}
			if (hidst >= 0 && mrudst >= 0 && hidst != mrudst &&
				utoffs[hidst] != utoffs[mrudst]) {
					isdsts[mrudst] = -1;
					type = addtype(utoffs[mrudst],
						&chars[desigidx[mrudst]],
						true,
						ttisstds[mrudst],
						ttisuts[mrudst]);
					isdsts[mrudst] = 1;
					omittype[type] = false;
			}
			if (histd >= 0 && mrustd >= 0 && histd != mrustd &&
				utoffs[histd] != utoffs[mrustd]) {
					isdsts[mrustd] = -1;
					type = addtype(utoffs[mrustd],
						&chars[desigidx[mrustd]],
						false,
						ttisstds[mrustd],
						ttisuts[mrustd]);
					isdsts[mrustd] = 0;
					omittype[type] = false;
			}
		}
#endif /* !defined LEAVE_SOME_PRE_2011_SYSTEMS_IN_THE_LURCH */
		thistypecnt = 0;
		for (i = old0; i < typecnt; i++)
		  if (!omittype[i])
		    typemap[i == old0 ? thisdefaulttype
			    : i == thisdefaulttype ? old0 : i]
		      = thistypecnt++;

		for (u = 0; u < sizeof indmap / sizeof indmap[0]; ++u)
			indmap[u] = -1;
		thischarcnt = stdcnt = utcnt = 0;
		for (i = old0; i < typecnt; i++) {
			register char *	thisabbr;

			if (omittype[i])
				continue;
			if (ttisstds[i])
			  stdcnt = thistypecnt;
			if (ttisuts[i])
			  utcnt = thistypecnt;
			if (indmap[desigidx[i]] >= 0)
				continue;
			thisabbr = &chars[desigidx[i]];
			for (j = 0; j < thischarcnt; ++j)
				if (strcmp(&thischars[j], thisabbr) == 0)
					break;
			if (j == thischarcnt) {
				strcpy(&thischars[thischarcnt], thisabbr);
				thischarcnt += strlen(thisabbr) + 1;
			}
			indmap[desigidx[i]] = j;
		}
		if (pass == 1 && !want_bloat()) {
		  hicut = thisleapexpiry = false;
		  pretranstype = -1;
		  thistimecnt = thisleapcnt = 0;
		  thistypecnt = thischarcnt = 1;
		}
#define DO(field)	fwrite(tzh.field, sizeof tzh.field, 1, fp)
		memset(&tzh, 0, sizeof tzh);
		memcpy(tzh.tzh_magic, TZ_MAGIC, sizeof tzh.tzh_magic);
		tzh.tzh_version[0] = version;
		convert(utcnt, tzh.tzh_ttisutcnt);
		convert(stdcnt, tzh.tzh_ttisstdcnt);
		convert(thisleapcnt + thisleapexpiry, tzh.tzh_leapcnt);
		convert((0 <= pretranstype) + thistimecnt + hicut,
			tzh.tzh_timecnt);
		convert(thistypecnt, tzh.tzh_typecnt);
		convert(thischarcnt, tzh.tzh_charcnt);
		DO(tzh_magic);
		DO(tzh_version);
		DO(tzh_reserved);
		DO(tzh_ttisutcnt);
		DO(tzh_ttisstdcnt);
		DO(tzh_leapcnt);
		DO(tzh_timecnt);
		DO(tzh_typecnt);
		DO(tzh_charcnt);
#undef DO
		if (pass == 1 && !want_bloat()) {
		  /* Output a minimal data block with just one time type.  */
		  puttzcode(0, fp);	/* utoff */
		  putc(0, fp);		/* dst */
		  putc(0, fp);		/* index of abbreviation */
		  putc(0, fp);		/* empty-string abbreviation */
		  continue;
		}

		/* Output a LO_TIME transition if needed; see limitrange.
		   But do not go below the minimum representable value
		   for this pass.  */
		lo = pass == 1 && lo_time < ZIC32_MIN ? ZIC32_MIN : lo_time;

		if (0 <= pretranstype)
		  puttzcodepass(lo, fp, pass);
		for (i = thistimei; i < thistimelim; ++i) {
		  puttzcodepass(ats[i], fp, pass);
		}
		if (hicut)
		  puttzcodepass(hi_time + 1, fp, pass);
		if (0 <= pretranstype)
		  putc(typemap[pretranstype], fp);
		for (i = thistimei; i < thistimelim; i++)
		  putc(typemap[types[i]], fp);
		if (hicut)
		  putc(typemap[unspecifiedtype], fp);

		for (i = old0; i < typecnt; i++) {
		  int h = (i == old0 ? thisdefaulttype
			   : i == thisdefaulttype ? old0 : i);
		  if (!omittype[h]) {
		    puttzcode(utoffs[h], fp);
		    putc(isdsts[h], fp);
		    putc(indmap[desigidx[h]], fp);
		  }
		}
		if (thischarcnt != 0)
			fwrite(thischars, sizeof thischars[0],
				      thischarcnt, fp);
		thisleaplim = thisleapi + thisleapcnt;
		for (i = thisleapi; i < thisleaplim; ++i) {
			register zic_t	todo;

			if (roll[i]) {
				if (timecnt == 0 || trans[i] < ats[0]) {
					j = 0;
					while (isdsts[j])
						if (++j >= typecnt) {
							j = 0;
							break;
						}
				} else {
					j = 1;
					while (j < timecnt &&
						trans[i] >= ats[j])
							++j;
					j = types[j - 1];
				}
				todo = tadd(trans[i], -utoffs[j]);
			} else	todo = trans[i];
			puttzcodepass(todo, fp, pass);
			puttzcode(corr[i], fp);
		}
		if (thisleapexpiry) {
		  /* Append a no-op leap correction indicating when the leap
		     second table expires.  Although this does not conform to
		     Internet RFC 8536, most clients seem to accept this and
		     the plan is to amend the RFC to allow this in version 4
		     TZif files.  */
		  puttzcodepass(leapexpires, fp, pass);
		  puttzcode(thisleaplim ? corr[thisleaplim - 1] : 0, fp);
		}
		if (stdcnt != 0)
		  for (i = old0; i < typecnt; i++)
			if (!omittype[i])
				putc(ttisstds[i], fp);
		if (utcnt != 0)
		  for (i = old0; i < typecnt; i++)
			if (!omittype[i])
				putc(ttisuts[i], fp);
	}
	fprintf(fp, "\n%s\n", string);
	close_file(fp, directory, name, tempname);
	if (chmod(tempname, mflag) < 0) {
		fprintf(stderr, _("cannot change mode of %s to %03o"),
		    tempname, (unsigned)mflag);
		exit(EXIT_FAILURE);
	}
	if ((uflag != (uid_t)-1 || gflag != (gid_t)-1)
	    && chown(tempname, uflag, gflag) < 0) {
		fprintf(stderr, _("cannot change ownership of %s"), 
		    tempname);
		exit(EXIT_FAILURE);
	}
	rename_dest(tempname, name);
	free(ats);
}

static char const *
abbroffset(char *buf, zic_t offset)
{
  char sign = '+';
  int seconds, minutes;

  if (offset < 0) {
    offset = -offset;
    sign = '-';
  }

  seconds = offset % SECSPERMIN;
  offset /= SECSPERMIN;
  minutes = offset % MINSPERHOUR;
  offset /= MINSPERHOUR;
  if (100 <= offset) {
    error(_("%%z UT offset magnitude exceeds 99:59:59"));
    return "%z";
  } else {
    char *p = buf;
    *p++ = sign;
    *p++ = '0' + offset / 10;
    *p++ = '0' + offset % 10;
    if (minutes | seconds) {
      *p++ = '0' + minutes / 10;
      *p++ = '0' + minutes % 10;
      if (seconds) {
	*p++ = '0' + seconds / 10;
	*p++ = '0' + seconds % 10;
      }
    }
    *p = '\0';
    return buf;
  }
}

static char const disable_percent_s[] = "";

static ptrdiff_t
doabbr(char *abbr, struct zone const *zp, char const *letters,
       bool isdst, zic_t save, bool doquotes)
{
	register char *	cp;
	register char *	slashp;
	ptrdiff_t len;
	char const *format = zp->z_format;

	slashp = strchr(format, '/');
	if (slashp == NULL) {
	  char letterbuf[PERCENT_Z_LEN_BOUND + 1];
	  if (zp->z_format_specifier == 'z')
	    letters = abbroffset(letterbuf, zp->z_stdoff + save);
	  else if (!letters)
	    letters = "%s";
	  else if (letters == disable_percent_s)
	    return 0;
	  sprintf(abbr, format, letters);
	} else if (isdst) {
		strcpy(abbr, slashp + 1);
	} else {
		memcpy(abbr, format, slashp - format);
		abbr[slashp - format] = '\0';
	}
	len = strlen(abbr);
	if (!doquotes)
		return len;
	for (cp = abbr; is_alpha(*cp); cp++)
		continue;
	if (len > 0 && *cp == '\0')
		return len;
	abbr[len + 2] = '\0';
	abbr[len + 1] = '>';
	memmove(abbr + 1, abbr, len);
	abbr[0] = '<';
	return len + 2;
}

static void
updateminmax(const zic_t x)
{
	if (min_year > x)
		min_year = x;
	if (max_year < x)
		max_year = x;
}

static int
stringoffset(char *result, zic_t offset)
{
	register int	hours;
	register int	minutes;
	register int	seconds;
	bool negative = offset < 0;
	int len = negative;

	if (negative) {
		offset = -offset;
		result[0] = '-';
	}
	seconds = offset % SECSPERMIN;
	offset /= SECSPERMIN;
	minutes = offset % MINSPERHOUR;
	offset /= MINSPERHOUR;
	hours = offset;
	if (hours >= HOURSPERDAY * DAYSPERWEEK) {
		result[0] = '\0';
		return 0;
	}
	len += sprintf(result + len, "%d", hours);
	if (minutes != 0 || seconds != 0) {
		len += sprintf(result + len, ":%02d", minutes);
		if (seconds != 0)
			len += sprintf(result + len, ":%02d", seconds);
	}
	return len;
}

static int
stringrule(char *result, struct rule *const rp, zic_t save, zic_t stdoff)
{
	register zic_t	tod = rp->r_tod;
	register int	compat = 0;

	if (rp->r_dycode == DC_DOM) {
		register int	month, total;

		if (rp->r_dayofmonth == 29 && rp->r_month == TM_FEBRUARY)
			return -1;
		total = 0;
		for (month = 0; month < rp->r_month; ++month)
			total += len_months[0][month];
		/* Omit the "J" in Jan and Feb, as that's shorter.  */
		if (rp->r_month <= 1)
		  result += sprintf(result, "%d", total + rp->r_dayofmonth - 1);
		else
		  result += sprintf(result, "J%d", total + rp->r_dayofmonth);
	} else {
		register int	week;
		register int	wday = rp->r_wday;
		register int	wdayoff;

		if (rp->r_dycode == DC_DOWGEQ) {
			wdayoff = (rp->r_dayofmonth - 1) % DAYSPERWEEK;
			if (wdayoff)
				compat = 2013;
			wday -= wdayoff;
			tod += wdayoff * SECSPERDAY;
			week = 1 + (rp->r_dayofmonth - 1) / DAYSPERWEEK;
		} else if (rp->r_dycode == DC_DOWLEQ) {
			if (rp->r_dayofmonth == len_months[1][rp->r_month])
				week = 5;
			else {
				wdayoff = rp->r_dayofmonth % DAYSPERWEEK;
				if (wdayoff)
					compat = 2013;
				wday -= wdayoff;
				tod += wdayoff * SECSPERDAY;
				week = rp->r_dayofmonth / DAYSPERWEEK;
			}
		} else	return -1;	/* "cannot happen" */
		if (wday < 0)
			wday += DAYSPERWEEK;
		result += sprintf(result, "M%d.%d.%d",
				  rp->r_month + 1, week, wday);
	}
	if (rp->r_todisut)
	  tod += stdoff;
	if (rp->r_todisstd && !rp->r_isdst)
	  tod += save;
	if (tod != 2 * SECSPERMIN * MINSPERHOUR) {
		*result++ = '/';
		if (! stringoffset(result, tod))
			return -1;
		if (tod < 0) {
			if (compat < 2013)
				compat = 2013;
		} else if (SECSPERDAY <= tod) {
			if (compat < 1994)
				compat = 1994;
		}
	}
	return compat;
}

static int
rule_cmp(struct rule const *a, struct rule const *b)
{
	if (!a)
		return -!!b;
	if (!b)
		return 1;
	if (a->r_hiyear != b->r_hiyear)
		return a->r_hiyear < b->r_hiyear ? -1 : 1;
	if (a->r_hiyear == ZIC_MAX)
		return 0;
	if (a->r_month - b->r_month != 0)
		return a->r_month - b->r_month;
	return a->r_dayofmonth - b->r_dayofmonth;
}

/* Store into RESULT a proleptic TZ string that represent the future
   predictions for the zone ZPFIRST with ZONECOUNT entries.  Return a
   compatibility indicator (a TZDB release year) if successful, a
   negative integer if no such TZ string exists.  */
static int
stringzone(char *result, struct zone const *zpfirst, ptrdiff_t zonecount)
{
	register const struct zone *	zp;
	register struct rule *		rp;
	register struct rule *		stdrp;
	register struct rule *		dstrp;
	register ptrdiff_t		i;
	register int			compat = 0;
	register int			c;
	int				offsetlen;
	struct rule			stdr, dstr;
	ptrdiff_t len;
	int dstcmp;
	struct rule *lastrp[2] = { NULL, NULL };
	struct zone zstr[2];
	struct zone const *stdzp;
	struct zone const *dstzp;

	result[0] = '\0';

	/* Internet RFC 8536 section 5.1 says to use an empty TZ string if
	   future timestamps are truncated.  */
	if (hi_time < max_time)
	  return -1;

	zp = zpfirst + zonecount - 1;
	for (i = 0; i < zp->z_nrules; ++i) {
		struct rule **last;
		int cmp;
		rp = &zp->z_rules[i];
		last = &lastrp[rp->r_isdst];
		cmp = rule_cmp(*last, rp);
		if (cmp < 0)
		  *last = rp;
		else if (cmp == 0)
		  return -1;
	}
	stdrp = lastrp[false];
	dstrp = lastrp[true];
	dstcmp = zp->z_nrules ? rule_cmp(dstrp, stdrp) : zp->z_isdst ? 1 : -1;
	stdzp = dstzp = zp;

	if (dstcmp < 0) {
	  /* Standard time all year.  */
	  dstrp = NULL;
	} else if (0 < dstcmp) {
	  /* DST all year.  Use an abbreviation like
	     "XXX3EDT4,0/0,J365/23" for EDT (-04) all year.  */
	  zic_t save = dstrp ? dstrp->r_save : zp->z_save;
	  if (0 <= save)
	    {
	      /* Positive DST, the typical case for all-year DST.
		 Fake a timezone with negative DST.  */
	      stdzp = &zstr[0];
	      dstzp = &zstr[1];
	      zstr[0].z_stdoff = zp->z_stdoff + 2 * save;
	      zstr[0].z_format = "XXX";  /* Any 3 letters will do.  */
	      zstr[0].z_format_specifier = 0;
	      zstr[1].z_stdoff = zstr[0].z_stdoff;
	      zstr[1].z_format = zp->z_format;
	      zstr[1].z_format_specifier = zp->z_format_specifier;
	    }
	  dstr.r_month = TM_JANUARY;
	  dstr.r_dycode = DC_DOM;
	  dstr.r_dayofmonth = 1;
	  dstr.r_tod = 0;
	  dstr.r_todisstd = dstr.r_todisut = false;
	  dstr.r_isdst = true;
	  dstr.r_save = save < 0 ? save : -save;
	  dstr.r_abbrvar = dstrp ? dstrp->r_abbrvar : NULL;
	  stdr.r_month = TM_DECEMBER;
	  stdr.r_dycode = DC_DOM;
	  stdr.r_dayofmonth = 31;
	  stdr.r_tod = SECSPERDAY + dstr.r_save;
	  stdr.r_todisstd = stdr.r_todisut = false;
	  stdr.r_isdst = false;
	  stdr.r_save = 0;
	  stdr.r_abbrvar = save < 0 && stdrp ? stdrp->r_abbrvar : NULL;
	  dstrp = &dstr;
	  stdrp = &stdr;
	}
	len = doabbr(result, stdzp, stdrp ? stdrp->r_abbrvar : NULL,
		     false, 0, true);
	offsetlen = stringoffset(result + len, - stdzp->z_stdoff);
	if (! offsetlen) {
		result[0] = '\0';
		return -1;
	}
	len += offsetlen;
	if (dstrp == NULL)
		return compat;
	len += doabbr(result + len, dstzp, dstrp->r_abbrvar,
		      dstrp->r_isdst, dstrp->r_save, true);
	if (dstrp->r_save != SECSPERMIN * MINSPERHOUR) {
	  offsetlen = stringoffset(result + len,
				   - (dstzp->z_stdoff + dstrp->r_save));
	  if (! offsetlen) {
	    result[0] = '\0';
	    return -1;
	  }
	  len += offsetlen;
	}
	result[len++] = ',';
	c = stringrule(result + len, dstrp, dstrp->r_save, stdzp->z_stdoff);
	if (c < 0) {
		result[0] = '\0';
		return -1;
	}
	if (compat < c)
		compat = c;
	len += strlen(result + len);
	result[len++] = ',';
	c = stringrule(result + len, stdrp, dstrp->r_save, stdzp->z_stdoff);
	if (c < 0) {
		result[0] = '\0';
		return -1;
	}
	if (compat < c)
		compat = c;
	return compat;
}

static void
outzone(const struct zone *zpfirst, ptrdiff_t zonecount)
{
	register ptrdiff_t		i, j;
	register zic_t			starttime, untiltime;
	register bool			startttisstd;
	register bool			startttisut;
	register char *			startbuf;
	register char *			ab;
	register char *			envvar;
	register int			max_abbr_len;
	register int			max_envvar_len;
	register int			compat;
	register bool			do_extend;
	register char			version;
	zic_t nonTZlimtime = ZIC_MIN;
	int nonTZlimtype = -1;
	zic_t max_year0;
	int defaulttype = -1;

	check_for_signal();

	/* This cannot overflow; see FORMAT_LEN_GROWTH_BOUND.  */
	max_abbr_len = 2 + max_format_len + max_abbrvar_len;
	max_envvar_len = 2 * max_abbr_len + 5 * 9;

	startbuf = emalloc(max_abbr_len + 1);
	ab = emalloc(max_abbr_len + 1);
	envvar = emalloc(max_envvar_len + 1);
	INITIALIZE(untiltime);
	INITIALIZE(starttime);
	/*
	** Now. . .finally. . .generate some useful data!
	*/
	timecnt = 0;
	typecnt = 0;
	charcnt = 0;
	/*
	** Thanks to Earl Chew
	** for noting the need to unconditionally initialize startttisstd.
	*/
	startttisstd = false;
	startttisut = false;
	min_year = max_year = EPOCH_YEAR;
	if (leapseen) {
		updateminmax(leapminyear);
		updateminmax(leapmaxyear + (leapmaxyear < ZIC_MAX));
	}
	for (i = 0; i < zonecount; ++i) {
		struct zone const *zp = &zpfirst[i];
		if (i < zonecount - 1)
			updateminmax(zp->z_untilrule.r_loyear);
		for (j = 0; j < zp->z_nrules; ++j) {
			struct rule *rp = &zp->z_rules[j];
			updateminmax(rp->r_loyear);
			if (rp->r_hiwasnum)
				updateminmax(rp->r_hiyear);
		}
	}
	/*
	** Generate lots of data if a rule can't cover all future times.
	*/
	compat = stringzone(envvar, zpfirst, zonecount);
	version = compat < 2013 ? '2' : '3';
	do_extend = compat < 0;
	if (noise) {
		if (!*envvar)
			warning("%s %s",
				_("no proleptic TZ string for zone"),
				zpfirst->z_name);
		else if (compat != 0) {
			/* Circa-COMPAT clients, and earlier clients, might
			   not work for this zone when given dates before
			   1970 or after 2038.  */
			warning(_("%s: pre-%d clients may mishandle"
				  " distant timestamps"),
				zpfirst->z_name, compat);
		}
	}
	if (do_extend) {
		if (min_year >= ZIC_MIN + years_of_observations)
			min_year -= years_of_observations;
		else	min_year = ZIC_MIN;
		if (max_year <= ZIC_MAX - years_of_observations)
			max_year += years_of_observations;
		else	max_year = ZIC_MAX;
	}
	max_year = max(max_year, (redundant_time / (SECSPERDAY * DAYSPERNYEAR)
				  + EPOCH_YEAR + 1));
	max_year0 = max_year;
	if (want_bloat()) {
	  /* For the benefit of older systems,
	     generate data from 1900 through 2038.  */
	  if (min_year > YEAR_32BIT_MIN - 1)
		min_year = YEAR_32BIT_MIN - 1;
	  if (max_year < YEAR_32BIT_MAX)
		max_year = YEAR_32BIT_MAX;
	}

	if (min_time < lo_time || hi_time < max_time)
	  unspecifiedtype = addtype(0, "-00", false, false, false);

	for (i = 0; i < zonecount; ++i) {
		/*
		** A guess that may well be corrected later.
		*/
		zic_t save = 0;
		struct zone const *zp = &zpfirst[i];
		bool usestart = i > 0 && (zp - 1)->z_untiltime > min_time;
		bool useuntil = i < (zonecount - 1);
		zic_t stdoff = zp->z_stdoff;
		zic_t startoff = stdoff;
		if (useuntil && zp->z_untiltime <= min_time)
			continue;
		eat(zp->z_filenum, zp->z_linenum);
		*startbuf = '\0';
		if (zp->z_nrules == 0) {
			int type;
			save = zp->z_save;
			doabbr(startbuf, zp, NULL, zp->z_isdst, save, false);
			type = addtype(oadd(zp->z_stdoff, save),
				startbuf, zp->z_isdst, startttisstd,
				startttisut);
			if (usestart) {
				addtt(starttime, type);
				if (useuntil && nonTZlimtime < starttime) {
				  nonTZlimtime = starttime;
				  nonTZlimtype = type;
				}
				usestart = false;
			} else
				defaulttype = type;
		} else {
		  zic_t year;
		  for (year = min_year; year <= max_year; ++year) {
			if (useuntil && year > zp->z_untilrule.r_hiyear)
				break;
			/*
			** Mark which rules to do in the current year.
			** For those to do, calculate rpytime(rp, year);
			** The former TYPE field was also considered here.
			*/
			for (j = 0; j < zp->z_nrules; ++j) {
				zic_t one = 1;
				zic_t y2038_boundary = one << 31;
				struct rule *rp = &zp->z_rules[j];
				eats(zp->z_filenum, zp->z_linenum,
				     rp->r_filenum, rp->r_linenum);
				rp->r_todo = year >= rp->r_loyear &&
						year <= rp->r_hiyear;
				if (rp->r_todo) {
					rp->r_temp = rpytime(rp, year);
					rp->r_todo
					  = (rp->r_temp < y2038_boundary
					     || year <= max_year0);
				}
			}
			for ( ; ; ) {
				register ptrdiff_t k;
				register zic_t	jtime, ktime;
				register zic_t	offset;
				struct rule *rp;
				int type;

				INITIALIZE(ktime);
				if (useuntil) {
					/*
					** Turn untiltime into UT
					** assuming the current stdoff and
					** save values.
					*/
					untiltime = zp->z_untiltime;
					if (!zp->z_untilrule.r_todisut)
						untiltime = tadd(untiltime,
								 -stdoff);
					if (!zp->z_untilrule.r_todisstd)
						untiltime = tadd(untiltime,
								 -save);
				}
				/*
				** Find the rule (of those to do, if any)
				** that takes effect earliest in the year.
				*/
				k = -1;
				for (j = 0; j < zp->z_nrules; ++j) {
					struct rule *r = &zp->z_rules[j];
					if (!r->r_todo)
						continue;
					eats(zp->z_filenum, zp->z_linenum,
					     r->r_filenum, r->r_linenum);
					offset = r->r_todisut ? 0 : stdoff;
					if (!r->r_todisstd)
						offset = oadd(offset, save);
					jtime = r->r_temp;
					if (jtime == min_time ||
						jtime == max_time)
							continue;
					jtime = tadd(jtime, -offset);
					if (k < 0 || jtime < ktime) {
						k = j;
						ktime = jtime;
					} else if (jtime == ktime) {
					  char const *dup_rules_msg =
					    _("two rules for same instant");
					  eats(zp->z_filenum, zp->z_linenum,
					       r->r_filenum, r->r_linenum);
					  warning("%s", dup_rules_msg);
					  r = &zp->z_rules[k];
					  eats(zp->z_filenum, zp->z_linenum,
					       r->r_filenum, r->r_linenum);
					  error("%s", dup_rules_msg);
					}
				}
				if (k < 0)
					break;	/* go on to next year */
				rp = &zp->z_rules[k];
				rp->r_todo = false;
				if (useuntil && ktime >= untiltime) {
					if (!*startbuf
					    && (oadd(zp->z_stdoff, rp->r_save)
						== startoff))
					  doabbr(startbuf, zp, rp->r_abbrvar,
						 rp->r_isdst, rp->r_save,
						 false);
					break;
				}
				save = rp->r_save;
				if (usestart && ktime == starttime)
					usestart = false;
				if (usestart) {
					if (ktime < starttime) {
						startoff = oadd(zp->z_stdoff,
								save);
						doabbr(startbuf, zp,
							rp->r_abbrvar,
							rp->r_isdst,
							rp->r_save,
							false);
						continue;
					}
					if (*startbuf == '\0'
					    && startoff == oadd(zp->z_stdoff,
								save)) {
							doabbr(startbuf,
								zp,
								rp->r_abbrvar,
								rp->r_isdst,
								rp->r_save,
								false);
					}
				}
				eats(zp->z_filenum, zp->z_linenum,
				     rp->r_filenum, rp->r_linenum);
				doabbr(ab, zp, rp->r_abbrvar,
				       rp->r_isdst, rp->r_save, false);
				offset = oadd(zp->z_stdoff, rp->r_save);
				type = addtype(offset, ab, rp->r_isdst,
					rp->r_todisstd, rp->r_todisut);
				if (defaulttype < 0 && !rp->r_isdst)
				  defaulttype = type;
				addtt(ktime, type);
				if (nonTZlimtime < ktime
				    && (useuntil || rp->r_hiyear != ZIC_MAX)) {
				  nonTZlimtime = ktime;
				  nonTZlimtype = type;
				}
			}
		  }
		}
		if (usestart) {
			bool isdst = startoff != zp->z_stdoff;
			if (*startbuf == '\0' && zp->z_format)
			  doabbr(startbuf, zp, disable_percent_s,
				 isdst, save, false);
			eat(zp->z_filenum, zp->z_linenum);
			if (*startbuf == '\0')
			  error(_("can't determine time zone abbreviation"
				  " to use just after until time"));
			else {
			  int type = addtype(startoff, startbuf, isdst,
					     startttisstd, startttisut);
			  if (defaulttype < 0 && !isdst)
			    defaulttype = type;
			  addtt(starttime, type);
			}
		}
		/*
		** Now we may get to set starttime for the next zone line.
		*/
		if (useuntil) {
			startttisstd = zp->z_untilrule.r_todisstd;
			startttisut = zp->z_untilrule.r_todisut;
			starttime = zp->z_untiltime;
			if (!startttisstd)
			  starttime = tadd(starttime, -save);
			if (!startttisut)
			  starttime = tadd(starttime, -stdoff);
		}
	}
	if (defaulttype < 0)
	  defaulttype = 0;
	if (!do_extend && !want_bloat()) {
	  /* Keep trailing transitions that are no greater than this.  */
	  zic_t keep_at_max;

	  /* The earliest transition into a time governed by the TZ string.  */
	  zic_t TZstarttime = ZIC_MAX;
	  for (i = 0; i < timecnt; i++) {
	    zic_t at = attypes[i].at;
	    if (nonTZlimtime < at && at < TZstarttime)
	      TZstarttime = at;
	  }
	  if (TZstarttime == ZIC_MAX)
	    TZstarttime = nonTZlimtime;

	  /* Omit trailing transitions deducible from the TZ string,
	     and not needed for -r or -R.  */
	  keep_at_max = max(TZstarttime, redundant_time);
	  for (i = j = 0; i < timecnt; i++)
	    if (attypes[i].at <= keep_at_max) {
	      attypes[j].at = attypes[i].at;
	      attypes[j].dontmerge = (attypes[i].at == TZstarttime
				      && (nonTZlimtype != attypes[i].type
					  || strchr(envvar, ',')));
	      attypes[j].type = attypes[i].type;
	      j++;
	    }
	  timecnt = j;
	}
	if (do_extend) {
		/*
		** If we're extending the explicitly listed observations for
		** 400 years because we can't fill the proleptic TZ field,
		** check whether we actually ended up explicitly listing
		** observations through that period.  If there aren't any
		** near the end of the 400-year period, add a redundant
		** one at the end of the final year, to make it clear
		** that we are claiming to have definite knowledge of
		** the lack of transitions up to that point.
		*/
		struct rule xr;
		struct attype *lastat;
		xr.r_month = TM_JANUARY;
		xr.r_dycode = DC_DOM;
		xr.r_dayofmonth = 1;
		xr.r_tod = 0;
		for (lastat = attypes, i = 1; i < timecnt; i++)
			if (attypes[i].at > lastat->at)
				lastat = &attypes[i];
		if (!lastat || lastat->at < rpytime(&xr, max_year - 1)) {
			addtt(rpytime(&xr, max_year + 1),
			      lastat ? lastat->type : defaulttype);
			attypes[timecnt - 1].dontmerge = true;
		}
	}
	writezone(zpfirst->z_name, envvar, version, defaulttype);
	free(startbuf);
	free(ab);
	free(envvar);
}

static void
addtt(zic_t starttime, int type)
{
	attypes = growalloc(attypes, sizeof *attypes, timecnt, &timecnt_alloc);
	attypes[timecnt].at = starttime;
	attypes[timecnt].dontmerge = false;
	attypes[timecnt].type = type;
	++timecnt;
}

static int
addtype(zic_t utoff, char const *abbr, bool isdst, bool ttisstd, bool ttisut)
{
	register int	i, j;

	if (! (-1L - 2147483647L <= utoff && utoff <= 2147483647L)) {
		error(_("UT offset out of range"));
		exit(EXIT_FAILURE);
	}
	if (!want_bloat())
	  ttisstd = ttisut = false;

	for (j = 0; j < charcnt; ++j)
		if (strcmp(&chars[j], abbr) == 0)
			break;
	if (j == charcnt)
		newabbr(abbr);
	else {
	  /* If there's already an entry, return its index.  */
	  for (i = 0; i < typecnt; i++)
	    if (utoff == utoffs[i] && isdst == isdsts[i] && j == desigidx[i]
		&& ttisstd == ttisstds[i] && ttisut == ttisuts[i])
	      return i;
	}
	/*
	** There isn't one; add a new one, unless there are already too
	** many.
	*/
	if (typecnt >= TZ_MAX_TYPES) {
		error(_("too many local time types"));
		exit(EXIT_FAILURE);
	}
	i = typecnt++;
	utoffs[i] = utoff;
	isdsts[i] = isdst;
	ttisstds[i] = ttisstd;
	ttisuts[i] = ttisut;
	desigidx[i] = j;
	return i;
}

static void
leapadd(zic_t t, int correction, int rolling)
{
	register int i;

	if (TZ_MAX_LEAPS <= leapcnt) {
		error(_("too many leap seconds"));
		exit(EXIT_FAILURE);
	}
	if (rolling && (lo_time != min_time || hi_time != max_time)) {
	  error(_("Rolling leap seconds not supported with -r"));
	  exit(EXIT_FAILURE);
	}
	for (i = 0; i < leapcnt; ++i)
		if (t <= trans[i])
			break;
	memmove(&trans[i + 1], &trans[i], (leapcnt - i) * sizeof *trans);
	memmove(&corr[i + 1], &corr[i], (leapcnt - i) * sizeof *corr);
	memmove(&roll[i + 1], &roll[i], (leapcnt - i) * sizeof *roll);
	trans[i] = t;
	corr[i] = correction;
	roll[i] = rolling;
	++leapcnt;
}

static void
adjleap(void)
{
	register int	i;
	register zic_t	last = 0;
	register zic_t	prevtrans = 0;

	/*
	** propagate leap seconds forward
	*/
	for (i = 0; i < leapcnt; ++i) {
		if (trans[i] - prevtrans < 28 * SECSPERDAY) {
		  error(_("Leap seconds too close together"));
		  exit(EXIT_FAILURE);
		}
		prevtrans = trans[i];
		trans[i] = tadd(trans[i], last);
		last = corr[i] += last;
	}

	if (0 <= leapexpires) {
	  leapexpires = oadd(leapexpires, last);
	  if (! (leapcnt == 0 || (trans[leapcnt - 1] < leapexpires))) {
	    error(_("last Leap time does not precede Expires time"));
	    exit(EXIT_FAILURE);
	  }
	}
}

/* Is A a space character in the C locale?  */
static bool
is_space(char a)
{
	switch (a) {
	  default:
		return false;
	  case ' ': case '\f': case '\n': case '\r': case '\t': case '\v':
		return true;
	}
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

/* If A is an uppercase character in the C locale, return its lowercase
   counterpart.  Otherwise, return A.  */
static char
lowerit(char a)
{
	switch (a) {
	  default: return a;
	  case 'A': return 'a'; case 'B': return 'b'; case 'C': return 'c';
	  case 'D': return 'd'; case 'E': return 'e'; case 'F': return 'f';
	  case 'G': return 'g'; case 'H': return 'h'; case 'I': return 'i';
	  case 'J': return 'j'; case 'K': return 'k'; case 'L': return 'l';
	  case 'M': return 'm'; case 'N': return 'n'; case 'O': return 'o';
	  case 'P': return 'p'; case 'Q': return 'q'; case 'R': return 'r';
	  case 'S': return 's'; case 'T': return 't'; case 'U': return 'u';
	  case 'V': return 'v'; case 'W': return 'w'; case 'X': return 'x';
	  case 'Y': return 'y'; case 'Z': return 'z';
	}
}

/* case-insensitive equality */
ATTRIBUTE_PURE_114833 static bool
ciequal(register const char *ap, register const char *bp)
{
	while (lowerit(*ap) == lowerit(*bp++))
		if (*ap++ == '\0')
			return true;
	return false;
}

ATTRIBUTE_PURE_114833 static bool
itsabbr(register const char *abbr, register const char *word)
{
	if (lowerit(*abbr) != lowerit(*word))
		return false;
	++word;
	while (*++abbr != '\0')
		do {
			if (*word == '\0')
				return false;
		} while (lowerit(*word++) != lowerit(*abbr));
	return true;
}

/* Return true if ABBR is an initial prefix of WORD, ignoring ASCII case.  */

ATTRIBUTE_PURE_114833 static bool
ciprefix(char const *abbr, char const *word)
{
  do
    if (!*abbr)
      return true;
  while (lowerit(*abbr++) == lowerit(*word++));

  return false;
}

static const struct lookup *
byword(const char *word, const struct lookup *table)
{
	register const struct lookup *	foundlp;
	register const struct lookup *	lp;

	if (word == NULL || table == NULL)
		return NULL;

	/* If TABLE is LASTS and the word starts with "last" followed
	   by a non-'-', skip the "last" and look in WDAY_NAMES instead.
	   Warn about any usage of the undocumented prefix "last-".  */
	if (table == lasts && ciprefix("last", word) && word[4]) {
	  if (word[4] == '-')
	    warning(_("\"%s\" is undocumented; use \"last%s\" instead"),
		    word, word + 5);
	  else {
	    word += 4;
	    table = wday_names;
	  }
	}

	/*
	** Look for exact match.
	*/
	for (lp = table; lp->l_word != NULL; ++lp)
		if (ciequal(word, lp->l_word))
			return lp;
	/*
	** Look for inexact match.
	*/
	foundlp = NULL;
	for (lp = table; lp->l_word != NULL; ++lp)
		if (ciprefix(word, lp->l_word)) {
			if (foundlp == NULL)
				foundlp = lp;
			else	return NULL;	/* multiple inexact matches */
		}

	if (foundlp && noise) {
	  /* Warn about any backward-compatibility issue with pre-2017c zic.  */
	  bool pre_2017c_match = false;
	  for (lp = table; lp->l_word; lp++)
	    if (itsabbr(word, lp->l_word)) {
	      if (pre_2017c_match) {
		warning(_("\"%s\" is ambiguous in pre-2017c zic"), word);
		break;
	      }
	      pre_2017c_match = true;
	    }
	}

	return foundlp;
}

static int
getfields(char *cp, char **array, int arrayelts)
{
	register char *		dp;
	register int		nsubs;

	nsubs = 0;
	for ( ; ; ) {
		char *dstart;
		while (is_space(*cp))
				++cp;
		if (*cp == '\0' || *cp == '#')
			break;
		dstart = dp = cp;
		do {
			if ((*dp = *cp++) != '"')
				++dp;
			else while ((*dp = *cp++) != '"')
				if (*dp != '\0')
					++dp;
				else {
				  error(_("Odd number of quotation marks"));
				  exit(EXIT_FAILURE);
				}
		} while (*cp && *cp != '#' && !is_space(*cp));
		if (is_space(*cp))
			++cp;
		*dp = '\0';
		if (nsubs == arrayelts) {
		  error(_("Too many input fields"));
		  exit(EXIT_FAILURE);
		}
		array[nsubs++] = dstart + (*dstart == '-' && dp == dstart + 1);
	}
	return nsubs;
}

ATTRIBUTE_NORETURN static void
time_overflow(void)
{
  error(_("time overflow"));
  exit(EXIT_FAILURE);
}

ATTRIBUTE_PURE_114833 static zic_t
oadd(zic_t t1, zic_t t2)
{
#ifdef ckd_add
  zic_t sum;
  if (!ckd_add(&sum, t1, t2))
    return sum;
#else
  if (t1 < 0 ? ZIC_MIN - t1 <= t2 : t2 <= ZIC_MAX - t1)
    return t1 + t2;
#endif
  time_overflow();
}

ATTRIBUTE_PURE_114833 static zic_t
tadd(zic_t t1, zic_t t2)
{
#ifdef ckd_add
  zic_t sum;
  if (!ckd_add(&sum, t1, t2) && min_time <= sum && sum <= max_time)
    return sum;
#else
  if (t1 < 0 ? min_time - t1 <= t2 : t2 <= max_time - t1)
    return t1 + t2;
#endif
  if (t1 == min_time || t1 == max_time)
    return t1;
  time_overflow();
}

/*
** Given a rule, and a year, compute the date (in seconds since January 1,
** 1970, 00:00 LOCAL time) in that year that the rule refers to.
*/

static zic_t
rpytime(const struct rule *rp, zic_t wantedy)
{
	register int	m, i;
	register zic_t	dayoff;			/* with a nod to Margaret O. */
	register zic_t	t, y;
	int yrem;

	if (wantedy == ZIC_MIN)
		return min_time;
	if (wantedy == ZIC_MAX)
		return max_time;
	m = TM_JANUARY;
	y = EPOCH_YEAR;

	/* dayoff = floor((wantedy - y) / YEARSPERREPEAT) * DAYSPERREPEAT,
	   sans overflow.  */
	yrem = wantedy % YEARSPERREPEAT - y % YEARSPERREPEAT;
	dayoff = ((wantedy / YEARSPERREPEAT - y / YEARSPERREPEAT
		   + yrem / YEARSPERREPEAT - (yrem % YEARSPERREPEAT < 0))
		  * DAYSPERREPEAT);
	/* wantedy = y + ((wantedy - y) mod YEARSPERREPEAT), sans overflow.  */
	wantedy = y + (yrem + 2 * YEARSPERREPEAT) % YEARSPERREPEAT;

	while (wantedy != y) {
		i = len_years[isleap(y)];
		dayoff = oadd(dayoff, i);
		y++;
	}
	while (m != rp->r_month) {
		i = len_months[isleap(y)][m];
		dayoff = oadd(dayoff, i);
		++m;
	}
	i = rp->r_dayofmonth;
	if (m == TM_FEBRUARY && i == 29 && !isleap(y)) {
		if (rp->r_dycode == DC_DOWLEQ)
			--i;
		else {
			error(_("use of 2/29 in non leap-year"));
			exit(EXIT_FAILURE);
		}
	}
	--i;
	dayoff = oadd(dayoff, i);
	if (rp->r_dycode == DC_DOWGEQ || rp->r_dycode == DC_DOWLEQ) {
		/*
		** Don't trust mod of negative numbers.
		*/
		zic_t wday = ((EPOCH_WDAY + dayoff % DAYSPERWEEK + DAYSPERWEEK)
			      % DAYSPERWEEK);
		while (wday != rp->r_wday)
			if (rp->r_dycode == DC_DOWGEQ) {
				dayoff = oadd(dayoff, 1);
				if (++wday >= DAYSPERWEEK)
					wday = 0;
				++i;
			} else {
				dayoff = oadd(dayoff, -1);
				if (--wday < 0)
					wday = DAYSPERWEEK - 1;
				--i;
			}
		if (i < 0 || i >= len_months[isleap(y)][m]) {
			if (noise)
				warning(_("rule goes past start/end of month; \
will not work with pre-2004 versions of zic"));
		}
	}
	if (dayoff < min_time / SECSPERDAY)
		return min_time;
	if (dayoff > max_time / SECSPERDAY)
		return max_time;
	t = (zic_t) dayoff * SECSPERDAY;
	return tadd(t, rp->r_tod);
}

static void
newabbr(const char *string)
{
	register int	i;

	if (strcmp(string, GRANDPARENTED) != 0) {
		register const char *	cp;
		const char *		mp;

		cp = string;
		mp = NULL;
		while (is_alpha(*cp) || ('0' <= *cp && *cp <= '9')
		       || *cp == '-' || *cp == '+')
				++cp;
		if (noise && cp - string < 3)
		  mp = _("time zone abbreviation has fewer than 3 characters");
		if (cp - string > ZIC_MAX_ABBR_LEN_WO_WARN)
		  mp = _("time zone abbreviation has too many characters");
		if (*cp != '\0')
mp = _("time zone abbreviation differs from POSIX standard");
		if (mp != NULL)
			warning("%s (%s)", mp, string);
	}
	i = strlen(string) + 1;
	if (charcnt + i > TZ_MAX_CHARS) {
		error(_("too many, or too long, time zone abbreviations"));
		exit(EXIT_FAILURE);
	}
	strcpy(&chars[charcnt], string);
	charcnt += i;
}

/* Ensure that the directories of ARGNAME exist, by making any missing
   ones.  If ANCESTORS, do this only for ARGNAME's ancestors; otherwise,
   do it for ARGNAME too.  Exit with failure if there is trouble.
   Do not consider an existing file to be trouble.  */
static void
mkdirs(char const *argname, bool ancestors)
{
	/*
	 * If -D was specified, do not create directories.  A subsequent
	 * file operation will fail and produce an appropriate error
	 * message.
	 */
	if (Dflag)
		return;

	char *name = estrdup(argname);
	char *cp = name;

	/* On MS-Windows systems, do not worry about drive letters or
	   backslashes, as this should suffice in practice.  Time zone
	   names do not use drive letters and backslashes.  If the -d
	   option of zic does not name an already-existing directory,
	   it can use slashes to separate the already-existing
	   ancestor prefix from the to-be-created subdirectories.  */

	/* Do not mkdir a root directory, as it must exist.  */
	while (*cp == '/')
	  cp++;

	while (cp && ((cp = strchr(cp, '/')) || !ancestors)) {
		if (cp)
		  *cp = '\0';
		/*
		** Try to create it.  It's OK if creation fails because
		** the directory already exists, perhaps because some
		** other process just created it.  For simplicity do
		** not check first whether it already exists, as that
		** is checked anyway if the mkdir fails.
		*/
		if (mkdir(name, MKDIR_UMASK) != 0) {
			/* Do not report an error if err == EEXIST, because
			   some other process might have made the directory
			   in the meantime.  Likewise for ENOSYS, because
			   Solaris 10 mkdir fails with ENOSYS if the
			   directory is an automounted mount point.
			   Likewise for EACCES, since mkdir can fail
			   with EACCES merely because the parent directory
			   is unwritable.  Likewise for most other error
			   numbers.  */
			int err = errno;
			if (err == ELOOP || err == ENAMETOOLONG
			    || err == ENOENT || err == ENOTDIR) {
				error(_("%s: Can't create directory %s: %s"),
				      progname, name, strerror(err));
				exit(EXIT_FAILURE);
			}
		}
		if (cp)
		  *cp++ = '/';
	}
	free(name);
}

#include <grp.h>
#include <pwd.h>

static void
setgroup(gid_t *flag, const char *name)
{
	struct group *gr;

	if (*flag != (gid_t)-1) {
		fprintf(stderr, _("multiple -g flags specified"));
		exit(EXIT_FAILURE);
	}

	gr = getgrnam(name);
	if (gr == 0) {
		char *ep;
		unsigned long ul;

		ul = strtoul(name, &ep, 10);
		if (ul == (unsigned long)(gid_t)ul && *ep == '\0') {
			*flag = ul;
			return;
		}
		fprintf(stderr, _("group `%s' not found"), name);
		exit(EXIT_FAILURE);
	}
	*flag = gr->gr_gid;
}

static void
setuser(uid_t *flag, const char *name)
{
	struct passwd *pw;

	if (*flag != (gid_t)-1) {
		fprintf(stderr, _("multiple -u flags specified"));
		exit(EXIT_FAILURE);
	}

	pw = getpwnam(name);
	if (pw == 0) {
		char *ep;
		unsigned long ul;

		ul = strtoul(name, &ep, 10);
		if (ul == (unsigned long)(gid_t)ul && *ep == '\0') {
			*flag = ul;
			return;
		}
		fprintf(stderr, _("user `%s' not found"), name);
		exit(EXIT_FAILURE);
	}
	*flag = pw->pw_uid;
}

/*
** UNIX was a registered trademark of The Open Group in 2003.
*/
