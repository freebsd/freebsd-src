#ifdef WIN32
#define _POSIX_
#endif

#define PERL_NO_GET_CONTEXT

#include "EXTERN.h"
#define PERLIO_NOT_STDIO 1
#include "perl.h"
#include "XSUB.h"
#if defined(PERL_OBJECT) || defined(PERL_CAPI) || defined(PERL_IMPLICIT_SYS)
#  undef signal
#  undef open
#  undef setmode
#  define open PerlLIO_open3
#endif
#include <ctype.h>
#ifdef I_DIRENT    /* XXX maybe better to just rely on perl.h? */
#include <dirent.h>
#endif
#include <errno.h>
#ifdef I_FLOAT
#include <float.h>
#endif
#ifdef I_LIMITS
#include <limits.h>
#endif
#include <locale.h>
#include <math.h>
#ifdef I_PWD
#include <pwd.h>
#endif
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>

#ifdef I_STDDEF
#include <stddef.h>
#endif

/* XXX This comment is just to make I_TERMIO and I_SGTTY visible to 
   metaconfig for future extension writers.  We don't use them in POSIX.
   (This is really sneaky :-)  --AD
*/
#if defined(I_TERMIOS)
#include <termios.h>
#endif
#ifdef I_STDLIB
#include <stdlib.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#ifdef MACOS_TRADITIONAL
#undef fdopen
#endif
#include <fcntl.h>

#if defined(__VMS) && !defined(__POSIX_SOURCE)
#  include <libdef.h>       /* LIB$_INVARG constant */
#  include <lib$routines.h> /* prototype for lib$ediv() */
#  include <starlet.h>      /* prototype for sys$gettim() */
#  if DECC_VERSION < 50000000
#    define pid_t int       /* old versions of DECC miss this in types.h */
#  endif

#  undef mkfifo
#  define mkfifo(a,b) (not_here("mkfifo"),-1)
#  define tzset() not_here("tzset")

#if ((__VMS_VER >= 70000000) && (__DECC_VER >= 50200000)) || (__CRTL_VER >= 70000000)
#    define HAS_TZNAME  /* shows up in VMS 7.0 or Dec C 5.6 */
#    include <utsname.h>
#  endif /* __VMS_VER >= 70000000 or Dec C 5.6 */

   /* The POSIX notion of ttyname() is better served by getname() under VMS */
   static char ttnambuf[64];
#  define ttyname(fd) (isatty(fd) > 0 ? getname(fd,ttnambuf,0) : NULL)

   /* The non-POSIX CRTL times() has void return type, so we just get the
      current time directly */
   clock_t vms_times(struct tms *bufptr) {
	dTHX;
	clock_t retval;
	/* Get wall time and convert to 10 ms intervals to
	 * produce the return value that the POSIX standard expects */
#  if defined(__DECC) && defined (__ALPHA)
#    include <ints.h>
	uint64 vmstime;
	_ckvmssts(sys$gettim(&vmstime));
	vmstime /= 100000;
	retval = vmstime & 0x7fffffff;
#  else
	/* (Older hw or ccs don't have an atomic 64-bit type, so we
	 * juggle 32-bit ints (and a float) to produce a time_t result
	 * with minimal loss of information.) */
	long int vmstime[2],remainder,divisor = 100000;
	_ckvmssts(sys$gettim((unsigned long int *)vmstime));
	vmstime[1] &= 0x7fff;  /* prevent overflow in EDIV */
	_ckvmssts(lib$ediv(&divisor,vmstime,(long int *)&retval,&remainder));
#  endif
	/* Fill in the struct tms using the CRTL routine . . .*/
	times((tbuffer_t *)bufptr);
	return (clock_t) retval;
   }
#  define times(t) vms_times(t)
#else
#if defined (__CYGWIN__)
#    define tzname _tzname
#endif
#if defined (WIN32)
#  undef mkfifo
#  define mkfifo(a,b) not_here("mkfifo")
#  define ttyname(a) (char*)not_here("ttyname")
#  define sigset_t long
#  define pid_t long
#  ifdef __BORLANDC__
#    define tzname _tzname
#  endif
#  ifdef _MSC_VER
#    define mode_t short
#  endif
#  ifdef __MINGW32__
#    define mode_t short
#    ifndef tzset
#      define tzset()		not_here("tzset")
#    endif
#    ifndef _POSIX_OPEN_MAX
#      define _POSIX_OPEN_MAX	FOPEN_MAX	/* XXX bogus ? */
#    endif
#  endif
#  define sigaction(a,b,c)	not_here("sigaction")
#  define sigpending(a)		not_here("sigpending")
#  define sigprocmask(a,b,c)	not_here("sigprocmask")
#  define sigsuspend(a)		not_here("sigsuspend")
#  define sigemptyset(a)	not_here("sigemptyset")
#  define sigaddset(a,b)	not_here("sigaddset")
#  define sigdelset(a,b)	not_here("sigdelset")
#  define sigfillset(a)		not_here("sigfillset")
#  define sigismember(a,b)	not_here("sigismember")
#  define setuid(a)		not_here("setuid")
#  define setgid(a)		not_here("setgid")
#else

#  ifndef HAS_MKFIFO
#    if defined(OS2) || defined(MACOS_TRADITIONAL)
#      define mkfifo(a,b) not_here("mkfifo")
#    else	/* !( defined OS2 ) */ 
#      ifndef mkfifo
#        define mkfifo(path, mode) (mknod((path), (mode) | S_IFIFO, 0))
#      endif
#    endif
#  endif /* !HAS_MKFIFO */

#  ifdef MACOS_TRADITIONAL
#    define ttyname(a) (char*)not_here("ttyname")
#    define tzset() not_here("tzset")
#  else
#    include <grp.h>
#    include <sys/times.h>
#    ifdef HAS_UNAME
#      include <sys/utsname.h>
#    endif
#    include <sys/wait.h>
#  endif
#  ifdef I_UTIME
#    include <utime.h>
#  endif
#endif /* WIN32 */
#endif /* __VMS */

typedef int SysRet;
typedef long SysRetLong;
typedef sigset_t* POSIX__SigSet;
typedef HV* POSIX__SigAction;
#ifdef I_TERMIOS
typedef struct termios* POSIX__Termios;
#else /* Define termios types to int, and call not_here for the functions.*/
#define POSIX__Termios int
#define speed_t int
#define tcflag_t int
#define cc_t int
#define cfgetispeed(x) not_here("cfgetispeed")
#define cfgetospeed(x) not_here("cfgetospeed")
#define tcdrain(x) not_here("tcdrain")
#define tcflush(x,y) not_here("tcflush")
#define tcsendbreak(x,y) not_here("tcsendbreak")
#define cfsetispeed(x,y) not_here("cfsetispeed")
#define cfsetospeed(x,y) not_here("cfsetospeed")
#define ctermid(x) (char *) not_here("ctermid")
#define tcflow(x,y) not_here("tcflow")
#define tcgetattr(x,y) not_here("tcgetattr")
#define tcsetattr(x,y,z) not_here("tcsetattr")
#endif

/* Possibly needed prototypes */
char *cuserid (char *);
double strtod (const char *, char **);
long strtol (const char *, char **, int);
unsigned long strtoul (const char *, char **, int);

#ifndef HAS_CUSERID
#define cuserid(a) (char *) not_here("cuserid")
#endif
#ifndef HAS_DIFFTIME
#ifndef difftime
#define difftime(a,b) not_here("difftime")
#endif
#endif
#ifndef HAS_FPATHCONF
#define fpathconf(f,n) 	(SysRetLong) not_here("fpathconf")
#endif
#ifndef HAS_MKTIME
#define mktime(a) not_here("mktime")
#endif
#ifndef HAS_NICE
#define nice(a) not_here("nice")
#endif
#ifndef HAS_PATHCONF
#define pathconf(f,n) 	(SysRetLong) not_here("pathconf")
#endif
#ifndef HAS_SYSCONF
#define sysconf(n) 	(SysRetLong) not_here("sysconf")
#endif
#ifndef HAS_READLINK
#define readlink(a,b,c) not_here("readlink")
#endif
#ifndef HAS_SETPGID
#define setpgid(a,b) not_here("setpgid")
#endif
#ifndef HAS_SETSID
#define setsid() not_here("setsid")
#endif
#ifndef HAS_STRCOLL
#define strcoll(s1,s2) not_here("strcoll")
#endif
#ifndef HAS_STRTOD
#define strtod(s1,s2) not_here("strtod")
#endif
#ifndef HAS_STRTOL
#define strtol(s1,s2,b) not_here("strtol")
#endif
#ifndef HAS_STRTOUL
#define strtoul(s1,s2,b) not_here("strtoul")
#endif
#ifndef HAS_STRXFRM
#define strxfrm(s1,s2,n) not_here("strxfrm")
#endif
#ifndef HAS_TCGETPGRP
#define tcgetpgrp(a) not_here("tcgetpgrp")
#endif
#ifndef HAS_TCSETPGRP
#define tcsetpgrp(a,b) not_here("tcsetpgrp")
#endif
#ifndef HAS_TIMES
#define times(a) not_here("times")
#endif
#ifndef HAS_UNAME
#define uname(a) not_here("uname")
#endif
#ifndef HAS_WAITPID
#define waitpid(a,b,c) not_here("waitpid")
#endif

#ifndef HAS_MBLEN
#ifndef mblen
#define mblen(a,b) not_here("mblen")
#endif
#endif
#ifndef HAS_MBSTOWCS
#define mbstowcs(s, pwcs, n) not_here("mbstowcs")
#endif
#ifndef HAS_MBTOWC
#define mbtowc(pwc, s, n) not_here("mbtowc")
#endif
#ifndef HAS_WCSTOMBS
#define wcstombs(s, pwcs, n) not_here("wcstombs")
#endif
#ifndef HAS_WCTOMB
#define wctomb(s, wchar) not_here("wcstombs")
#endif
#if !defined(HAS_MBLEN) && !defined(HAS_MBSTOWCS) && !defined(HAS_MBTOWC) && !defined(HAS_WCSTOMBS) && !defined(HAS_WCTOMB)
/* If we don't have these functions, then we wouldn't have gotten a typedef
   for wchar_t, the wide character type.  Defining wchar_t allows the
   functions referencing it to compile.  Its actual type is then meaningless,
   since without the above functions, all sections using it end up calling
   not_here() and croak.  --Kaveh Ghazi (ghazi@noc.rutgers.edu) 9/18/94. */
#ifndef wchar_t
#define wchar_t char
#endif
#endif

#ifndef HAS_LOCALECONV
#define localeconv() not_here("localeconv")
#endif

#ifdef HAS_TZNAME
#  if !defined(WIN32) && !defined(__CYGWIN__)
extern char *tzname[];
#  endif
#else
#if !defined(WIN32) || (defined(__MINGW32__) && !defined(tzname))
char *tzname[] = { "" , "" };
#endif
#endif

/* XXX struct tm on some systems (SunOS4/BSD) contains extra (non POSIX)
 * fields for which we don't have Configure support yet:
 *   char *tm_zone;   -- abbreviation of timezone name
 *   long tm_gmtoff;  -- offset from GMT in seconds
 * To workaround core dumps from the uninitialised tm_zone we get the
 * system to give us a reasonable struct to copy.  This fix means that
 * strftime uses the tm_zone and tm_gmtoff values returned by
 * localtime(time()). That should give the desired result most of the
 * time. But probably not always!
 *
 * This is a temporary workaround to be removed once Configure
 * support is added and NETaa14816 is considered in full.
 * It does not address tzname aspects of NETaa14816.
 */
#ifdef HAS_GNULIBC
# ifndef STRUCT_TM_HASZONE
#    define STRUCT_TM_HASZONE
# endif
#endif

#ifdef STRUCT_TM_HASZONE
static void
init_tm(struct tm *ptm)		/* see mktime, strftime and asctime	*/
{
    Time_t now;
    (void)time(&now);
    Copy(localtime(&now), ptm, 1, struct tm);
}

#else
# define init_tm(ptm)
#endif

/*
 * mini_mktime - normalise struct tm values without the localtime()
 * semantics (and overhead) of mktime().
 */
static void
mini_mktime(struct tm *ptm)
{
    int yearday;
    int secs;
    int month, mday, year, jday;
    int odd_cent, odd_year;

#define	DAYS_PER_YEAR	365
#define	DAYS_PER_QYEAR	(4*DAYS_PER_YEAR+1)
#define	DAYS_PER_CENT	(25*DAYS_PER_QYEAR-1)
#define	DAYS_PER_QCENT	(4*DAYS_PER_CENT+1)
#define	SECS_PER_HOUR	(60*60)
#define	SECS_PER_DAY	(24*SECS_PER_HOUR)
/* parentheses deliberately absent on these two, otherwise they don't work */
#define	MONTH_TO_DAYS	153/5
#define	DAYS_TO_MONTH	5/153
/* offset to bias by March (month 4) 1st between month/mday & year finding */
#define	YEAR_ADJUST	(4*MONTH_TO_DAYS+1)
/* as used here, the algorithm leaves Sunday as day 1 unless we adjust it */
#define	WEEKDAY_BIAS	6	/* (1+6)%7 makes Sunday 0 again */

/*
 * Year/day algorithm notes:
 *
 * With a suitable offset for numeric value of the month, one can find
 * an offset into the year by considering months to have 30.6 (153/5) days,
 * using integer arithmetic (i.e., with truncation).  To avoid too much
 * messing about with leap days, we consider January and February to be
 * the 13th and 14th month of the previous year.  After that transformation,
 * we need the month index we use to be high by 1 from 'normal human' usage,
 * so the month index values we use run from 4 through 15.
 *
 * Given that, and the rules for the Gregorian calendar (leap years are those
 * divisible by 4 unless also divisible by 100, when they must be divisible
 * by 400 instead), we can simply calculate the number of days since some
 * arbitrary 'beginning of time' by futzing with the (adjusted) year number,
 * the days we derive from our month index, and adding in the day of the
 * month.  The value used here is not adjusted for the actual origin which
 * it normally would use (1 January A.D. 1), since we're not exposing it.
 * We're only building the value so we can turn around and get the
 * normalised values for the year, month, day-of-month, and day-of-year.
 *
 * For going backward, we need to bias the value we're using so that we find
 * the right year value.  (Basically, we don't want the contribution of
 * March 1st to the number to apply while deriving the year).  Having done
 * that, we 'count up' the contribution to the year number by accounting for
 * full quadracenturies (400-year periods) with their extra leap days, plus
 * the contribution from full centuries (to avoid counting in the lost leap
 * days), plus the contribution from full quad-years (to count in the normal
 * leap days), plus the leftover contribution from any non-leap years.
 * At this point, if we were working with an actual leap day, we'll have 0
 * days left over.  This is also true for March 1st, however.  So, we have
 * to special-case that result, and (earlier) keep track of the 'odd'
 * century and year contributions.  If we got 4 extra centuries in a qcent,
 * or 4 extra years in a qyear, then it's a leap day and we call it 29 Feb.
 * Otherwise, we add back in the earlier bias we removed (the 123 from
 * figuring in March 1st), find the month index (integer division by 30.6),
 * and the remainder is the day-of-month.  We then have to convert back to
 * 'real' months (including fixing January and February from being 14/15 in
 * the previous year to being in the proper year).  After that, to get
 * tm_yday, we work with the normalised year and get a new yearday value for
 * January 1st, which we subtract from the yearday value we had earlier,
 * representing the date we've re-built.  This is done from January 1
 * because tm_yday is 0-origin.
 *
 * Since POSIX time routines are only guaranteed to work for times since the
 * UNIX epoch (00:00:00 1 Jan 1970 UTC), the fact that this algorithm
 * applies Gregorian calendar rules even to dates before the 16th century
 * doesn't bother me.  Besides, you'd need cultural context for a given
 * date to know whether it was Julian or Gregorian calendar, and that's
 * outside the scope for this routine.  Since we convert back based on the
 * same rules we used to build the yearday, you'll only get strange results
 * for input which needed normalising, or for the 'odd' century years which
 * were leap years in the Julian calander but not in the Gregorian one.
 * I can live with that.
 *
 * This algorithm also fails to handle years before A.D. 1 gracefully, but
 * that's still outside the scope for POSIX time manipulation, so I don't
 * care.
 */

    year = 1900 + ptm->tm_year;
    month = ptm->tm_mon;
    mday = ptm->tm_mday;
    /* allow given yday with no month & mday to dominate the result */
    if (ptm->tm_yday >= 0 && mday <= 0 && month <= 0) {
	month = 0;
	mday = 0;
	jday = 1 + ptm->tm_yday;
    }
    else {
	jday = 0;
    }
    if (month >= 2)
	month+=2;
    else
	month+=14, year--;
    yearday = DAYS_PER_YEAR * year + year/4 - year/100 + year/400;
    yearday += month*MONTH_TO_DAYS + mday + jday;
    /*
     * Note that we don't know when leap-seconds were or will be,
     * so we have to trust the user if we get something which looks
     * like a sensible leap-second.  Wild values for seconds will
     * be rationalised, however.
     */
    if ((unsigned) ptm->tm_sec <= 60) {
	secs = 0;
    }
    else {
	secs = ptm->tm_sec;
	ptm->tm_sec = 0;
    }
    secs += 60 * ptm->tm_min;
    secs += SECS_PER_HOUR * ptm->tm_hour;
    if (secs < 0) {
	if (secs-(secs/SECS_PER_DAY*SECS_PER_DAY) < 0) {
	    /* got negative remainder, but need positive time */
	    /* back off an extra day to compensate */
	    yearday += (secs/SECS_PER_DAY)-1;
	    secs -= SECS_PER_DAY * (secs/SECS_PER_DAY - 1);
	}
	else {
	    yearday += (secs/SECS_PER_DAY);
	    secs -= SECS_PER_DAY * (secs/SECS_PER_DAY);
	}
    }
    else if (secs >= SECS_PER_DAY) {
	yearday += (secs/SECS_PER_DAY);
	secs %= SECS_PER_DAY;
    }
    ptm->tm_hour = secs/SECS_PER_HOUR;
    secs %= SECS_PER_HOUR;
    ptm->tm_min = secs/60;
    secs %= 60;
    ptm->tm_sec += secs;
    /* done with time of day effects */
    /*
     * The algorithm for yearday has (so far) left it high by 428.
     * To avoid mistaking a legitimate Feb 29 as Mar 1, we need to
     * bias it by 123 while trying to figure out what year it
     * really represents.  Even with this tweak, the reverse
     * translation fails for years before A.D. 0001.
     * It would still fail for Feb 29, but we catch that one below.
     */
    jday = yearday;	/* save for later fixup vis-a-vis Jan 1 */
    yearday -= YEAR_ADJUST;
    year = (yearday / DAYS_PER_QCENT) * 400;
    yearday %= DAYS_PER_QCENT;
    odd_cent = yearday / DAYS_PER_CENT;
    year += odd_cent * 100;
    yearday %= DAYS_PER_CENT;
    year += (yearday / DAYS_PER_QYEAR) * 4;
    yearday %= DAYS_PER_QYEAR;
    odd_year = yearday / DAYS_PER_YEAR;
    year += odd_year;
    yearday %= DAYS_PER_YEAR;
    if (!yearday && (odd_cent==4 || odd_year==4)) { /* catch Feb 29 */
	month = 1;
	yearday = 29;
    }
    else {
	yearday += YEAR_ADJUST;	/* recover March 1st crock */
	month = yearday*DAYS_TO_MONTH;
	yearday -= month*MONTH_TO_DAYS;
	/* recover other leap-year adjustment */
	if (month > 13) {
	    month-=14;
	    year++;
	}
	else {
	    month-=2;
	}
    }
    ptm->tm_year = year - 1900;
    if (yearday) {
      ptm->tm_mday = yearday;
      ptm->tm_mon = month;
    }
    else {
      ptm->tm_mday = 31;
      ptm->tm_mon = month - 1;
    }
    /* re-build yearday based on Jan 1 to get tm_yday */
    year--;
    yearday = year*DAYS_PER_YEAR + year/4 - year/100 + year/400;
    yearday += 14*MONTH_TO_DAYS + 1;
    ptm->tm_yday = jday - yearday;
    /* fix tm_wday if not overridden by caller */
    if ((unsigned)ptm->tm_wday > 6)
	ptm->tm_wday = (jday + WEEKDAY_BIAS) % 7;
}

#ifdef HAS_LONG_DOUBLE
#  if LONG_DOUBLESIZE > NVSIZE
#    undef HAS_LONG_DOUBLE  /* XXX until we figure out how to use them */
#  endif
#endif

#ifndef HAS_LONG_DOUBLE
#ifdef LDBL_MAX
#undef LDBL_MAX
#endif
#ifdef LDBL_MIN
#undef LDBL_MIN
#endif
#ifdef LDBL_EPSILON
#undef LDBL_EPSILON
#endif
#endif

static int
not_here(char *s)
{
    croak("POSIX::%s not implemented on this architecture", s);
    return -1;
}

static
NV
constant(char *name, int arg)
{
    errno = 0;
    switch (*name) {
    case 'A':
	if (strEQ(name, "ARG_MAX"))
#ifdef ARG_MAX
	    return ARG_MAX;
#else
	    goto not_there;
#endif
	break;
    case 'B':
	if (strEQ(name, "BUFSIZ"))
#ifdef BUFSIZ
	    return BUFSIZ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "BRKINT"))
#ifdef BRKINT
	    return BRKINT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B9600"))
#ifdef B9600
	    return B9600;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B19200"))
#ifdef B19200
	    return B19200;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B38400"))
#ifdef B38400
	    return B38400;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B0"))
#ifdef B0
	    return B0;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B110"))
#ifdef B110
	    return B110;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B1200"))
#ifdef B1200
	    return B1200;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B134"))
#ifdef B134
	    return B134;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B150"))
#ifdef B150
	    return B150;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B1800"))
#ifdef B1800
	    return B1800;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B200"))
#ifdef B200
	    return B200;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B2400"))
#ifdef B2400
	    return B2400;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B300"))
#ifdef B300
	    return B300;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B4800"))
#ifdef B4800
	    return B4800;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B50"))
#ifdef B50
	    return B50;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B600"))
#ifdef B600
	    return B600;
#else
	    goto not_there;
#endif
	if (strEQ(name, "B75"))
#ifdef B75
	    return B75;
#else
	    goto not_there;
#endif
	break;
    case 'C':
	if (strEQ(name, "CHAR_BIT"))
#ifdef CHAR_BIT
	    return CHAR_BIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CHAR_MAX"))
#ifdef CHAR_MAX
	    return CHAR_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CHAR_MIN"))
#ifdef CHAR_MIN
	    return CHAR_MIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CHILD_MAX"))
#ifdef CHILD_MAX
	    return CHILD_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLK_TCK"))
#ifdef CLK_TCK
	    return CLK_TCK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLOCAL"))
#ifdef CLOCAL
	    return CLOCAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CLOCKS_PER_SEC"))
#ifdef CLOCKS_PER_SEC
	    return CLOCKS_PER_SEC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CREAD"))
#ifdef CREAD
	    return CREAD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CS5"))
#ifdef CS5
	    return CS5;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CS6"))
#ifdef CS6
	    return CS6;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CS7"))
#ifdef CS7
	    return CS7;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CS8"))
#ifdef CS8
	    return CS8;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CSIZE"))
#ifdef CSIZE
	    return CSIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CSTOPB"))
#ifdef CSTOPB
	    return CSTOPB;
#else
	    goto not_there;
#endif
	break;
    case 'D':
	if (strEQ(name, "DBL_MAX"))
#ifdef DBL_MAX
	    return DBL_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "DBL_MIN"))
#ifdef DBL_MIN
	    return DBL_MIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "DBL_DIG"))
#ifdef DBL_DIG
	    return DBL_DIG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "DBL_EPSILON"))
#ifdef DBL_EPSILON
	    return DBL_EPSILON;
#else
	    goto not_there;
#endif
	if (strEQ(name, "DBL_MANT_DIG"))
#ifdef DBL_MANT_DIG
	    return DBL_MANT_DIG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "DBL_MAX_10_EXP"))
#ifdef DBL_MAX_10_EXP
	    return DBL_MAX_10_EXP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "DBL_MAX_EXP"))
#ifdef DBL_MAX_EXP
	    return DBL_MAX_EXP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "DBL_MIN_10_EXP"))
#ifdef DBL_MIN_10_EXP
	    return DBL_MIN_10_EXP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "DBL_MIN_EXP"))
#ifdef DBL_MIN_EXP
	    return DBL_MIN_EXP;
#else
	    goto not_there;
#endif
	break;
    case 'E':
	switch (name[1]) {
	case 'A':
	    if (strEQ(name, "EACCES"))
#ifdef EACCES
		return EACCES;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EADDRINUSE"))
#ifdef EADDRINUSE
		return EADDRINUSE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EADDRNOTAVAIL"))
#ifdef EADDRNOTAVAIL
		return EADDRNOTAVAIL;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EAFNOSUPPORT"))
#ifdef EAFNOSUPPORT
		return EAFNOSUPPORT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EAGAIN"))
#ifdef EAGAIN
		return EAGAIN;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EALREADY"))
#ifdef EALREADY
		return EALREADY;
#else
		goto not_there;
#endif
	    break;
	case 'B':
	    if (strEQ(name, "EBADF"))
#ifdef EBADF
		return EBADF;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EBUSY"))
#ifdef EBUSY
		return EBUSY;
#else
		goto not_there;
#endif
	    break;
	case 'C':
	    if (strEQ(name, "ECHILD"))
#ifdef ECHILD
		return ECHILD;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ECHO"))
#ifdef ECHO
		return ECHO;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ECHOE"))
#ifdef ECHOE
		return ECHOE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ECHOK"))
#ifdef ECHOK
		return ECHOK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ECHONL"))
#ifdef ECHONL
		return ECHONL;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ECONNABORTED"))
#ifdef ECONNABORTED
		return ECONNABORTED;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ECONNREFUSED"))
#ifdef ECONNREFUSED
		return ECONNREFUSED;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ECONNRESET"))
#ifdef ECONNRESET
		return ECONNRESET;
#else
		goto not_there;
#endif
	    break;
	case 'D':
	    if (strEQ(name, "EDEADLK"))
#ifdef EDEADLK
		return EDEADLK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EDESTADDRREQ"))
#ifdef EDESTADDRREQ
		return EDESTADDRREQ;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EDOM"))
#ifdef EDOM
		return EDOM;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EDQUOT"))
#ifdef EDQUOT
		return EDQUOT;
#else
		goto not_there;
#endif
	    break;
	case 'E':
	    if (strEQ(name, "EEXIST"))
#ifdef EEXIST
		return EEXIST;
#else
		goto not_there;
#endif
	    break;
	case 'F':
	    if (strEQ(name, "EFAULT"))
#ifdef EFAULT
		return EFAULT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EFBIG"))
#ifdef EFBIG
		return EFBIG;
#else
		goto not_there;
#endif
	    break;
	case 'H':
	    if (strEQ(name, "EHOSTDOWN"))
#ifdef EHOSTDOWN
		return EHOSTDOWN;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EHOSTUNREACH"))
#ifdef EHOSTUNREACH
		return EHOSTUNREACH;
#else
		goto not_there;
#endif
    	    break;
	case 'I':
	    if (strEQ(name, "EINPROGRESS"))
#ifdef EINPROGRESS
		return EINPROGRESS;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EINTR"))
#ifdef EINTR
		return EINTR;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EINVAL"))
#ifdef EINVAL
		return EINVAL;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EIO"))
#ifdef EIO
		return EIO;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EISCONN"))
#ifdef EISCONN
		return EISCONN;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EISDIR"))
#ifdef EISDIR
		return EISDIR;
#else
		goto not_there;
#endif
	    break;
	case 'L':
	    if (strEQ(name, "ELOOP"))
#ifdef ELOOP
		return ELOOP;
#else
		goto not_there;
#endif
	    break;
	case 'M':
	    if (strEQ(name, "EMFILE"))
#ifdef EMFILE
		return EMFILE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EMLINK"))
#ifdef EMLINK
		return EMLINK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EMSGSIZE"))
#ifdef EMSGSIZE
		return EMSGSIZE;
#else
		goto not_there;
#endif
	    break;
	case 'N':
	    if (strEQ(name, "ENETDOWN"))
#ifdef ENETDOWN
		return ENETDOWN;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENETRESET"))
#ifdef ENETRESET
		return ENETRESET;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENETUNREACH"))
#ifdef ENETUNREACH
		return ENETUNREACH;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOBUFS"))
#ifdef ENOBUFS
		return ENOBUFS;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOEXEC"))
#ifdef ENOEXEC
		return ENOEXEC;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOMEM"))
#ifdef ENOMEM
		return ENOMEM;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOPROTOOPT"))
#ifdef ENOPROTOOPT
		return ENOPROTOOPT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOSPC"))
#ifdef ENOSPC
		return ENOSPC;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOTBLK"))
#ifdef ENOTBLK
		return ENOTBLK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOTCONN"))
#ifdef ENOTCONN
		return ENOTCONN;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOTDIR"))
#ifdef ENOTDIR
		return ENOTDIR;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOTEMPTY"))
#ifdef ENOTEMPTY
		return ENOTEMPTY;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOTSOCK"))
#ifdef ENOTSOCK
		return ENOTSOCK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOTTY"))
#ifdef ENOTTY
		return ENOTTY;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENFILE"))
#ifdef ENFILE
		return ENFILE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENODEV"))
#ifdef ENODEV
		return ENODEV;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOENT"))
#ifdef ENOENT
		return ENOENT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOLCK"))
#ifdef ENOLCK
		return ENOLCK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENOSYS"))
#ifdef ENOSYS
		return ENOSYS;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENXIO"))
#ifdef ENXIO
		return ENXIO;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ENAMETOOLONG"))
#ifdef ENAMETOOLONG
		return ENAMETOOLONG;
#else
		goto not_there;
#endif
	    break;
	case 'O':
	    if (strEQ(name, "EOF"))
#ifdef EOF
		return EOF;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EOPNOTSUPP"))
#ifdef EOPNOTSUPP
		return EOPNOTSUPP;
#else
		goto not_there;
#endif
	    break;
	case 'P':
	    if (strEQ(name, "EPERM"))
#ifdef EPERM
		return EPERM;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EPFNOSUPPORT"))
#ifdef EPFNOSUPPORT
		return EPFNOSUPPORT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EPIPE"))
#ifdef EPIPE
		return EPIPE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EPROCLIM"))
#ifdef EPROCLIM
		return EPROCLIM;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EPROTONOSUPPORT"))
#ifdef EPROTONOSUPPORT
		return EPROTONOSUPPORT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EPROTOTYPE"))
#ifdef EPROTOTYPE
		return EPROTOTYPE;
#else
		goto not_there;
#endif
	    break;
	case 'R':
	    if (strEQ(name, "ERANGE"))
#ifdef ERANGE
		return ERANGE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EREMOTE"))
#ifdef EREMOTE
		return EREMOTE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ERESTART"))
#ifdef ERESTART
		return ERESTART;
#else
		goto not_there;
#endif
	    if (strEQ(name, "EROFS"))
#ifdef EROFS
		return EROFS;
#else
		goto not_there;
#endif
	    break;
	case 'S':
	    if (strEQ(name, "ESHUTDOWN"))
#ifdef ESHUTDOWN
		return ESHUTDOWN;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ESOCKTNOSUPPORT"))
#ifdef ESOCKTNOSUPPORT
		return ESOCKTNOSUPPORT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ESPIPE"))
#ifdef ESPIPE
		return ESPIPE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ESRCH"))
#ifdef ESRCH
		return ESRCH;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ESTALE"))
#ifdef ESTALE
		return ESTALE;
#else
		goto not_there;
#endif
	    break;
	case 'T':
	    if (strEQ(name, "ETIMEDOUT"))
#ifdef ETIMEDOUT
		return ETIMEDOUT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ETOOMANYREFS"))
#ifdef ETOOMANYREFS
		return ETOOMANYREFS;
#else
		goto not_there;
#endif
	    if (strEQ(name, "ETXTBSY"))
#ifdef ETXTBSY
		return ETXTBSY;
#else
		goto not_there;
#endif
    	    break;
	case 'U':
	    if (strEQ(name, "EUSERS"))
#ifdef EUSERS
		return EUSERS;
#else
		goto not_there;
#endif
    	    break;
    	case 'W':
	    if (strEQ(name, "EWOULDBLOCK"))
#ifdef EWOULDBLOCK
		return EWOULDBLOCK;
#else
		goto not_there;
#endif
    	    break;
	case 'X':
	    if (strEQ(name, "EXIT_FAILURE"))
#ifdef EXIT_FAILURE
		return EXIT_FAILURE;
#else
		return 1;
#endif
	    if (strEQ(name, "EXIT_SUCCESS"))
#ifdef EXIT_SUCCESS
		return EXIT_SUCCESS;
#else
		return 0;
#endif
	    if (strEQ(name, "EXDEV"))
#ifdef EXDEV
		return EXDEV;
#else
		goto not_there;
#endif
	    break;
	}
	if (strEQ(name, "E2BIG"))
#ifdef E2BIG
	    return E2BIG;
#else
	    goto not_there;
#endif
	break;
    case 'F':
	if (strnEQ(name, "FLT_", 4)) {
	    if (strEQ(name, "FLT_MAX"))
#ifdef FLT_MAX
		return FLT_MAX;
#else
		goto not_there;
#endif
	    if (strEQ(name, "FLT_MIN"))
#ifdef FLT_MIN
		return FLT_MIN;
#else
		goto not_there;
#endif
	    if (strEQ(name, "FLT_ROUNDS"))
#ifdef FLT_ROUNDS
		return FLT_ROUNDS;
#else
		goto not_there;
#endif
	    if (strEQ(name, "FLT_DIG"))
#ifdef FLT_DIG
		return FLT_DIG;
#else
		goto not_there;
#endif
	    if (strEQ(name, "FLT_EPSILON"))
#ifdef FLT_EPSILON
		return FLT_EPSILON;
#else
		goto not_there;
#endif
	    if (strEQ(name, "FLT_MANT_DIG"))
#ifdef FLT_MANT_DIG
		return FLT_MANT_DIG;
#else
		goto not_there;
#endif
	    if (strEQ(name, "FLT_MAX_10_EXP"))
#ifdef FLT_MAX_10_EXP
		return FLT_MAX_10_EXP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "FLT_MAX_EXP"))
#ifdef FLT_MAX_EXP
		return FLT_MAX_EXP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "FLT_MIN_10_EXP"))
#ifdef FLT_MIN_10_EXP
		return FLT_MIN_10_EXP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "FLT_MIN_EXP"))
#ifdef FLT_MIN_EXP
		return FLT_MIN_EXP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "FLT_RADIX"))
#ifdef FLT_RADIX
		return FLT_RADIX;
#else
		goto not_there;
#endif
	    break;
	}
	if (strnEQ(name, "F_", 2)) {
	    if (strEQ(name, "F_DUPFD"))
#ifdef F_DUPFD
		return F_DUPFD;
#else
		goto not_there;
#endif
	    if (strEQ(name, "F_GETFD"))
#ifdef F_GETFD
		return F_GETFD;
#else
		goto not_there;
#endif
	    if (strEQ(name, "F_GETFL"))
#ifdef F_GETFL
		return F_GETFL;
#else
		goto not_there;
#endif
	    if (strEQ(name, "F_GETLK"))
#ifdef F_GETLK
		return F_GETLK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "F_OK"))
#ifdef F_OK
		return F_OK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "F_RDLCK"))
#ifdef F_RDLCK
		return F_RDLCK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "F_SETFD"))
#ifdef F_SETFD
		return F_SETFD;
#else
		goto not_there;
#endif
	    if (strEQ(name, "F_SETFL"))
#ifdef F_SETFL
		return F_SETFL;
#else
		goto not_there;
#endif
	    if (strEQ(name, "F_SETLK"))
#ifdef F_SETLK
		return F_SETLK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "F_SETLKW"))
#ifdef F_SETLKW
		return F_SETLKW;
#else
		goto not_there;
#endif
	    if (strEQ(name, "F_UNLCK"))
#ifdef F_UNLCK
		return F_UNLCK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "F_WRLCK"))
#ifdef F_WRLCK
		return F_WRLCK;
#else
		goto not_there;
#endif
	    break;
	}
	if (strEQ(name, "FD_CLOEXEC"))
#ifdef FD_CLOEXEC
	    return FD_CLOEXEC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "FILENAME_MAX"))
#ifdef FILENAME_MAX
	    return FILENAME_MAX;
#else
	    goto not_there;
#endif
	break;
    case 'H':
	if (strEQ(name, "HUGE_VAL"))
#if defined(USE_LONG_DOUBLE) && defined(HUGE_VALL)
	  /* HUGE_VALL is admittedly non-POSIX but if we are using long doubles
	   * we might as well use long doubles. --jhi */
	    return HUGE_VALL;
#endif
#ifdef HUGE_VAL
	    return HUGE_VAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "HUPCL"))
#ifdef HUPCL
	    return HUPCL;
#else
	    goto not_there;
#endif
	break;
    case 'I':
	if (strEQ(name, "INT_MAX"))
#ifdef INT_MAX
	    return INT_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "INT_MIN"))
#ifdef INT_MIN
	    return INT_MIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ICANON"))
#ifdef ICANON
	    return ICANON;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ICRNL"))
#ifdef ICRNL
	    return ICRNL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "IEXTEN"))
#ifdef IEXTEN
	    return IEXTEN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "IGNBRK"))
#ifdef IGNBRK
	    return IGNBRK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "IGNCR"))
#ifdef IGNCR
	    return IGNCR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "IGNPAR"))
#ifdef IGNPAR
	    return IGNPAR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "INLCR"))
#ifdef INLCR
	    return INLCR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "INPCK"))
#ifdef INPCK
	    return INPCK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ISIG"))
#ifdef ISIG
	    return ISIG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ISTRIP"))
#ifdef ISTRIP
	    return ISTRIP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "IXOFF"))
#ifdef IXOFF
	    return IXOFF;
#else
	    goto not_there;
#endif
	if (strEQ(name, "IXON"))
#ifdef IXON
	    return IXON;
#else
	    goto not_there;
#endif
	break;
    case 'L':
	if (strnEQ(name, "LC_", 3)) {
	    if (strEQ(name, "LC_ALL"))
#ifdef LC_ALL
		return LC_ALL;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LC_COLLATE"))
#ifdef LC_COLLATE
		return LC_COLLATE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LC_CTYPE"))
#ifdef LC_CTYPE
		return LC_CTYPE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LC_MONETARY"))
#ifdef LC_MONETARY
		return LC_MONETARY;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LC_NUMERIC"))
#ifdef LC_NUMERIC
		return LC_NUMERIC;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LC_TIME"))
#ifdef LC_TIME
		return LC_TIME;
#else
		goto not_there;
#endif
	    break;
	}
	if (strnEQ(name, "LDBL_", 5)) {
	    if (strEQ(name, "LDBL_MAX"))
#ifdef LDBL_MAX
		return LDBL_MAX;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LDBL_MIN"))
#ifdef LDBL_MIN
		return LDBL_MIN;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LDBL_DIG"))
#ifdef LDBL_DIG
		return LDBL_DIG;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LDBL_EPSILON"))
#ifdef LDBL_EPSILON
		return LDBL_EPSILON;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LDBL_MANT_DIG"))
#ifdef LDBL_MANT_DIG
		return LDBL_MANT_DIG;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LDBL_MAX_10_EXP"))
#ifdef LDBL_MAX_10_EXP
		return LDBL_MAX_10_EXP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LDBL_MAX_EXP"))
#ifdef LDBL_MAX_EXP
		return LDBL_MAX_EXP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LDBL_MIN_10_EXP"))
#ifdef LDBL_MIN_10_EXP
		return LDBL_MIN_10_EXP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "LDBL_MIN_EXP"))
#ifdef LDBL_MIN_EXP
		return LDBL_MIN_EXP;
#else
		goto not_there;
#endif
	    break;
	}
	if (strnEQ(name, "L_", 2)) {
	    if (strEQ(name, "L_ctermid"))
#ifdef L_ctermid
		return L_ctermid;
#else
		goto not_there;
#endif
	    if (strEQ(name, "L_cuserid"))
#ifdef L_cuserid
		return L_cuserid;
#else
		goto not_there;
#endif
	    /* L_tmpnam[e] was a typo--retained for compatibility */
	    if (strEQ(name, "L_tmpname") || strEQ(name, "L_tmpnam"))
#ifdef L_tmpnam
		return L_tmpnam;
#else
		goto not_there;
#endif
	    break;
	}
	if (strEQ(name, "LONG_MAX"))
#ifdef LONG_MAX
	    return LONG_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LONG_MIN"))
#ifdef LONG_MIN
	    return LONG_MIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LINK_MAX"))
#ifdef LINK_MAX
	    return LINK_MAX;
#else
	    goto not_there;
#endif
	break;
    case 'M':
	if (strEQ(name, "MAX_CANON"))
#ifdef MAX_CANON
	    return MAX_CANON;
#else
	    goto not_there;
#endif
	if (strEQ(name, "MAX_INPUT"))
#ifdef MAX_INPUT
	    return MAX_INPUT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "MB_CUR_MAX"))
#ifdef MB_CUR_MAX
	    return MB_CUR_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "MB_LEN_MAX"))
#ifdef MB_LEN_MAX
	    return MB_LEN_MAX;
#else
	    goto not_there;
#endif
	break;
    case 'N':
	if (strEQ(name, "NULL")) return 0;
	if (strEQ(name, "NAME_MAX"))
#ifdef NAME_MAX
	    return NAME_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "NCCS"))
#ifdef NCCS
	    return NCCS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "NGROUPS_MAX"))
#ifdef NGROUPS_MAX
	    return NGROUPS_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "NOFLSH"))
#ifdef NOFLSH
	    return NOFLSH;
#else
	    goto not_there;
#endif
	break;
    case 'O':
	if (strnEQ(name, "O_", 2)) {
	    if (strEQ(name, "O_APPEND"))
#ifdef O_APPEND
		return O_APPEND;
#else
		goto not_there;
#endif
	    if (strEQ(name, "O_CREAT"))
#ifdef O_CREAT
		return O_CREAT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "O_TRUNC"))
#ifdef O_TRUNC
		return O_TRUNC;
#else
		goto not_there;
#endif
	    if (strEQ(name, "O_RDONLY"))
#ifdef O_RDONLY
		return O_RDONLY;
#else
		goto not_there;
#endif
	    if (strEQ(name, "O_RDWR"))
#ifdef O_RDWR
		return O_RDWR;
#else
		goto not_there;
#endif
	    if (strEQ(name, "O_WRONLY"))
#ifdef O_WRONLY
		return O_WRONLY;
#else
		goto not_there;
#endif
	    if (strEQ(name, "O_EXCL"))
#ifdef O_EXCL
		return O_EXCL;
#else
		goto not_there;
#endif
	    if (strEQ(name, "O_NOCTTY"))
#ifdef O_NOCTTY
		return O_NOCTTY;
#else
		goto not_there;
#endif
	    if (strEQ(name, "O_NONBLOCK"))
#ifdef O_NONBLOCK
		return O_NONBLOCK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "O_ACCMODE"))
#ifdef O_ACCMODE
		return O_ACCMODE;
#else
		goto not_there;
#endif
	    break;
	}
	if (strEQ(name, "OPEN_MAX"))
#ifdef OPEN_MAX
	    return OPEN_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "OPOST"))
#ifdef OPOST
	    return OPOST;
#else
	    goto not_there;
#endif
	break;
    case 'P':
	if (strEQ(name, "PATH_MAX"))
#ifdef PATH_MAX
	    return PATH_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "PARENB"))
#ifdef PARENB
	    return PARENB;
#else
	    goto not_there;
#endif
	if (strEQ(name, "PARMRK"))
#ifdef PARMRK
	    return PARMRK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "PARODD"))
#ifdef PARODD
	    return PARODD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "PIPE_BUF"))
#ifdef PIPE_BUF
	    return PIPE_BUF;
#else
	    goto not_there;
#endif
	break;
    case 'R':
	if (strEQ(name, "RAND_MAX"))
#ifdef RAND_MAX
	    return RAND_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_OK"))
#ifdef R_OK
	    return R_OK;
#else
	    goto not_there;
#endif
	break;
    case 'S':
	if (strnEQ(name, "SIG", 3)) {
	    if (name[3] == '_') {
		if (strEQ(name, "SIG_BLOCK"))
#ifdef SIG_BLOCK
		    return SIG_BLOCK;
#else
		    goto not_there;
#endif
#ifdef SIG_DFL
		if (strEQ(name, "SIG_DFL")) return (IV)SIG_DFL;
#endif
#ifdef SIG_ERR
		if (strEQ(name, "SIG_ERR")) return (IV)SIG_ERR;
#endif
#ifdef SIG_IGN
		if (strEQ(name, "SIG_IGN")) return (IV)SIG_IGN;
#endif
		if (strEQ(name, "SIG_SETMASK"))
#ifdef SIG_SETMASK
		    return SIG_SETMASK;
#else
		    goto not_there;
#endif
		if (strEQ(name, "SIG_UNBLOCK"))
#ifdef SIG_UNBLOCK
		    return SIG_UNBLOCK;
#else
		    goto not_there;
#endif
		break;
	    }
	    if (strEQ(name, "SIGABRT"))
#ifdef SIGABRT
		return SIGABRT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGALRM"))
#ifdef SIGALRM
		return SIGALRM;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGCHLD"))
#ifdef SIGCHLD
		return SIGCHLD;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGCONT"))
#ifdef SIGCONT
		return SIGCONT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGFPE"))
#ifdef SIGFPE
		return SIGFPE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGHUP"))
#ifdef SIGHUP
		return SIGHUP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGILL"))
#ifdef SIGILL
		return SIGILL;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGINT"))
#ifdef SIGINT
		return SIGINT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGKILL"))
#ifdef SIGKILL
		return SIGKILL;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGPIPE"))
#ifdef SIGPIPE
		return SIGPIPE;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGQUIT"))
#ifdef SIGQUIT
		return SIGQUIT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGSEGV"))
#ifdef SIGSEGV
		return SIGSEGV;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGSTOP"))
#ifdef SIGSTOP
		return SIGSTOP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGTERM"))
#ifdef SIGTERM
		return SIGTERM;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGTSTP"))
#ifdef SIGTSTP
		return SIGTSTP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGTTIN"))
#ifdef SIGTTIN
		return SIGTTIN;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGTTOU"))
#ifdef SIGTTOU
		return SIGTTOU;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGUSR1"))
#ifdef SIGUSR1
		return SIGUSR1;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SIGUSR2"))
#ifdef SIGUSR2
		return SIGUSR2;
#else
		goto not_there;
#endif
	    break;
	}
	if (name[1] == '_') {
	    if (strEQ(name, "S_ISGID"))
#ifdef S_ISGID
		return S_ISGID;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_ISUID"))
#ifdef S_ISUID
		return S_ISUID;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IRGRP"))
#ifdef S_IRGRP
		return S_IRGRP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IROTH"))
#ifdef S_IROTH
		return S_IROTH;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IRUSR"))
#ifdef S_IRUSR
		return S_IRUSR;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IRWXG"))
#ifdef S_IRWXG
		return S_IRWXG;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IRWXO"))
#ifdef S_IRWXO
		return S_IRWXO;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IRWXU"))
#ifdef S_IRWXU
		return S_IRWXU;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IWGRP"))
#ifdef S_IWGRP
		return S_IWGRP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IWOTH"))
#ifdef S_IWOTH
		return S_IWOTH;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IWUSR"))
#ifdef S_IWUSR
		return S_IWUSR;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IXGRP"))
#ifdef S_IXGRP
		return S_IXGRP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IXOTH"))
#ifdef S_IXOTH
		return S_IXOTH;
#else
		goto not_there;
#endif
	    if (strEQ(name, "S_IXUSR"))
#ifdef S_IXUSR
		return S_IXUSR;
#else
		goto not_there;
#endif
	    errno = EAGAIN;		/* the following aren't constants */
#ifdef S_ISBLK
	    if (strEQ(name, "S_ISBLK")) return S_ISBLK(arg);
#endif
#ifdef S_ISCHR
	    if (strEQ(name, "S_ISCHR")) return S_ISCHR(arg);
#endif
#ifdef S_ISDIR
	    if (strEQ(name, "S_ISDIR")) return S_ISDIR(arg);
#endif
#ifdef S_ISFIFO
	    if (strEQ(name, "S_ISFIFO")) return S_ISFIFO(arg);
#endif
#ifdef S_ISREG
	    if (strEQ(name, "S_ISREG")) return S_ISREG(arg);
#endif
	    break;
	}
	if (strEQ(name, "SEEK_CUR"))
#ifdef SEEK_CUR
	    return SEEK_CUR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "SEEK_END"))
#ifdef SEEK_END
	    return SEEK_END;
#else
	    goto not_there;
#endif
	if (strEQ(name, "SEEK_SET"))
#ifdef SEEK_SET
	    return SEEK_SET;
#else
	    goto not_there;
#endif
	if (strEQ(name, "STREAM_MAX"))
#ifdef STREAM_MAX
	    return STREAM_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "SHRT_MAX"))
#ifdef SHRT_MAX
	    return SHRT_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "SHRT_MIN"))
#ifdef SHRT_MIN
	    return SHRT_MIN;
#else
	    goto not_there;
#endif
	if (strnEQ(name, "SA_", 3)) {
	    if (strEQ(name, "SA_NOCLDSTOP"))
#ifdef SA_NOCLDSTOP
		return SA_NOCLDSTOP;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SA_NOCLDWAIT"))
#ifdef SA_NOCLDWAIT
		return SA_NOCLDWAIT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SA_NODEFER"))
#ifdef SA_NODEFER
		return SA_NODEFER;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SA_ONSTACK"))
#ifdef SA_ONSTACK
		return SA_ONSTACK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SA_RESETHAND"))
#ifdef SA_RESETHAND
		return SA_RESETHAND;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SA_RESTART"))
#ifdef SA_RESTART
		return SA_RESTART;
#else
		goto not_there;
#endif
	    if (strEQ(name, "SA_SIGINFO"))
#ifdef SA_SIGINFO
		return SA_SIGINFO;
#else
		goto not_there;
#endif
	    break;
	}
	if (strEQ(name, "SCHAR_MAX"))
#ifdef SCHAR_MAX
	    return SCHAR_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "SCHAR_MIN"))
#ifdef SCHAR_MIN
	    return SCHAR_MIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "SSIZE_MAX"))
#ifdef SSIZE_MAX
	    return SSIZE_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "STDIN_FILENO"))
#ifdef STDIN_FILENO
	    return STDIN_FILENO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "STDOUT_FILENO"))
#ifdef STDOUT_FILENO
	    return STDOUT_FILENO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "STDERR_FILENO"))
#ifdef STDERR_FILENO
	    return STDERR_FILENO;
#else
	    goto not_there;
#endif
	break;
    case 'T':
	if (strEQ(name, "TCIFLUSH"))
#ifdef TCIFLUSH
	    return TCIFLUSH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TCIOFF"))
#ifdef TCIOFF
	    return TCIOFF;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TCIOFLUSH"))
#ifdef TCIOFLUSH
	    return TCIOFLUSH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TCION"))
#ifdef TCION
	    return TCION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TCOFLUSH"))
#ifdef TCOFLUSH
	    return TCOFLUSH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TCOOFF"))
#ifdef TCOOFF
	    return TCOOFF;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TCOON"))
#ifdef TCOON
	    return TCOON;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TCSADRAIN"))
#ifdef TCSADRAIN
	    return TCSADRAIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TCSAFLUSH"))
#ifdef TCSAFLUSH
	    return TCSAFLUSH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TCSANOW"))
#ifdef TCSANOW
	    return TCSANOW;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TMP_MAX"))
#ifdef TMP_MAX
	    return TMP_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TOSTOP"))
#ifdef TOSTOP
	    return TOSTOP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TZNAME_MAX"))
#ifdef TZNAME_MAX
	    return TZNAME_MAX;
#else
	    goto not_there;
#endif
	break;
    case 'U':
	if (strEQ(name, "UCHAR_MAX"))
#ifdef UCHAR_MAX
	    return UCHAR_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "UINT_MAX"))
#ifdef UINT_MAX
	    return UINT_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ULONG_MAX"))
#ifdef ULONG_MAX
	    return ULONG_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "USHRT_MAX"))
#ifdef USHRT_MAX
	    return USHRT_MAX;
#else
	    goto not_there;
#endif
	break;
    case 'V':
	if (strEQ(name, "VEOF"))
#ifdef VEOF
	    return VEOF;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VEOL"))
#ifdef VEOL
	    return VEOL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VERASE"))
#ifdef VERASE
	    return VERASE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VINTR"))
#ifdef VINTR
	    return VINTR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VKILL"))
#ifdef VKILL
	    return VKILL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VMIN"))
#ifdef VMIN
	    return VMIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VQUIT"))
#ifdef VQUIT
	    return VQUIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VSTART"))
#ifdef VSTART
	    return VSTART;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VSTOP"))
#ifdef VSTOP
	    return VSTOP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VSUSP"))
#ifdef VSUSP
	    return VSUSP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VTIME"))
#ifdef VTIME
	    return VTIME;
#else
	    goto not_there;
#endif
	break;
    case 'W':
	if (strEQ(name, "W_OK"))
#ifdef W_OK
	    return W_OK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "WNOHANG"))
#ifdef WNOHANG
	    return WNOHANG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "WUNTRACED"))
#ifdef WUNTRACED
	    return WUNTRACED;
#else
	    goto not_there;
#endif
	errno = EAGAIN;		/* the following aren't constants */
#ifdef WEXITSTATUS
	if (strEQ(name, "WEXITSTATUS")) return WEXITSTATUS(arg);
#endif
#ifdef WIFEXITED
	if (strEQ(name, "WIFEXITED")) return WIFEXITED(arg);
#endif
#ifdef WIFSIGNALED
	if (strEQ(name, "WIFSIGNALED")) return WIFSIGNALED(arg);
#endif
#ifdef WIFSTOPPED
	if (strEQ(name, "WIFSTOPPED")) return WIFSTOPPED(arg);
#endif
#ifdef WSTOPSIG
	if (strEQ(name, "WSTOPSIG")) return WSTOPSIG(arg);
#endif
#ifdef WTERMSIG
	if (strEQ(name, "WTERMSIG")) return WTERMSIG(arg);
#endif
	break;
    case 'X':
	if (strEQ(name, "X_OK"))
#ifdef X_OK
	    return X_OK;
#else
	    goto not_there;
#endif
	break;
    case '_':
	if (strnEQ(name, "_PC_", 4)) {
	    if (strEQ(name, "_PC_CHOWN_RESTRICTED"))
#if defined(_PC_CHOWN_RESTRICTED) || HINT_SC_EXIST
		return _PC_CHOWN_RESTRICTED;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_PC_LINK_MAX"))
#if defined(_PC_LINK_MAX) || HINT_SC_EXIST
		return _PC_LINK_MAX;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_PC_MAX_CANON"))
#if defined(_PC_MAX_CANON) || HINT_SC_EXIST
		return _PC_MAX_CANON;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_PC_MAX_INPUT"))
#if defined(_PC_MAX_INPUT) || HINT_SC_EXIST
		return _PC_MAX_INPUT;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_PC_NAME_MAX"))
#if defined(_PC_NAME_MAX) || HINT_SC_EXIST
		return _PC_NAME_MAX;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_PC_NO_TRUNC"))
#if defined(_PC_NO_TRUNC) || HINT_SC_EXIST
		return _PC_NO_TRUNC;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_PC_PATH_MAX"))
#if defined(_PC_PATH_MAX) || HINT_SC_EXIST
		return _PC_PATH_MAX;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_PC_PIPE_BUF"))
#if defined(_PC_PIPE_BUF) || HINT_SC_EXIST
		return _PC_PIPE_BUF;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_PC_VDISABLE"))
#if defined(_PC_VDISABLE) || HINT_SC_EXIST
		return _PC_VDISABLE;
#else
		goto not_there;
#endif
	    break;
	}
	if (strnEQ(name, "_POSIX_", 7)) {
	    if (strEQ(name, "_POSIX_ARG_MAX"))
#ifdef _POSIX_ARG_MAX
		return _POSIX_ARG_MAX;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_CHILD_MAX"))
#ifdef _POSIX_CHILD_MAX
		return _POSIX_CHILD_MAX;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_CHOWN_RESTRICTED"))
#ifdef _POSIX_CHOWN_RESTRICTED
		return _POSIX_CHOWN_RESTRICTED;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_JOB_CONTROL"))
#ifdef _POSIX_JOB_CONTROL
		return _POSIX_JOB_CONTROL;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_LINK_MAX"))
#ifdef _POSIX_LINK_MAX
		return _POSIX_LINK_MAX;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_MAX_CANON"))
#ifdef _POSIX_MAX_CANON
		return _POSIX_MAX_CANON;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_MAX_INPUT"))
#ifdef _POSIX_MAX_INPUT
		return _POSIX_MAX_INPUT;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_NAME_MAX"))
#ifdef _POSIX_NAME_MAX
		return _POSIX_NAME_MAX;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_NGROUPS_MAX"))
#ifdef _POSIX_NGROUPS_MAX
		return _POSIX_NGROUPS_MAX;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_NO_TRUNC"))
#ifdef _POSIX_NO_TRUNC
		return _POSIX_NO_TRUNC;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_OPEN_MAX"))
#ifdef _POSIX_OPEN_MAX
		return _POSIX_OPEN_MAX;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_PATH_MAX"))
#ifdef _POSIX_PATH_MAX
		return _POSIX_PATH_MAX;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_PIPE_BUF"))
#ifdef _POSIX_PIPE_BUF
		return _POSIX_PIPE_BUF;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_SAVED_IDS"))
#ifdef _POSIX_SAVED_IDS
		return _POSIX_SAVED_IDS;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_SSIZE_MAX"))
#ifdef _POSIX_SSIZE_MAX
		return _POSIX_SSIZE_MAX;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_STREAM_MAX"))
#ifdef _POSIX_STREAM_MAX
		return _POSIX_STREAM_MAX;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_TZNAME_MAX"))
#ifdef _POSIX_TZNAME_MAX
		return _POSIX_TZNAME_MAX;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_VDISABLE"))
#ifdef _POSIX_VDISABLE
		return _POSIX_VDISABLE;
#else
		return 0;
#endif
	    if (strEQ(name, "_POSIX_VERSION"))
#ifdef _POSIX_VERSION
		return _POSIX_VERSION;
#else
		return 0;
#endif
	    break;
	}
	if (strnEQ(name, "_SC_", 4)) {
	    if (strEQ(name, "_SC_ARG_MAX"))
#if defined(_SC_ARG_MAX) || HINT_SC_EXIST
		return _SC_ARG_MAX;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_SC_CHILD_MAX"))
#if defined(_SC_CHILD_MAX) || HINT_SC_EXIST
		return _SC_CHILD_MAX;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_SC_CLK_TCK"))
#if defined(_SC_CLK_TCK) || HINT_SC_EXIST
		return _SC_CLK_TCK;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_SC_JOB_CONTROL"))
#if defined(_SC_JOB_CONTROL) || HINT_SC_EXIST
		return _SC_JOB_CONTROL;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_SC_NGROUPS_MAX"))
#if defined(_SC_NGROUPS_MAX) || HINT_SC_EXIST
		return _SC_NGROUPS_MAX;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_SC_OPEN_MAX"))
#if defined(_SC_OPEN_MAX) || HINT_SC_EXIST
		return _SC_OPEN_MAX;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_SC_SAVED_IDS"))
#if defined(_SC_SAVED_IDS) || HINT_SC_EXIST
		return _SC_SAVED_IDS;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_SC_STREAM_MAX"))
#if defined(_SC_STREAM_MAX) || HINT_SC_EXIST
		return _SC_STREAM_MAX;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_SC_TZNAME_MAX"))
#if defined(_SC_TZNAME_MAX) || HINT_SC_EXIST
		return _SC_TZNAME_MAX;
#else
		goto not_there;
#endif
	    if (strEQ(name, "_SC_VERSION"))
#if defined(_SC_VERSION) || HINT_SC_EXIST
		return _SC_VERSION;
#else
		goto not_there;
#endif
	    break;
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

MODULE = SigSet		PACKAGE = POSIX::SigSet		PREFIX = sig

POSIX::SigSet
new(packname = "POSIX::SigSet", ...)
    char *		packname
    CODE:
	{
	    int i;
	    New(0, RETVAL, 1, sigset_t);
	    sigemptyset(RETVAL);
	    for (i = 1; i < items; i++)
		sigaddset(RETVAL, SvIV(ST(i)));
	}
    OUTPUT:
	RETVAL

void
DESTROY(sigset)
	POSIX::SigSet	sigset
    CODE:
	Safefree(sigset);

SysRet
sigaddset(sigset, sig)
	POSIX::SigSet	sigset
	int		sig

SysRet
sigdelset(sigset, sig)
	POSIX::SigSet	sigset
	int		sig

SysRet
sigemptyset(sigset)
	POSIX::SigSet	sigset

SysRet
sigfillset(sigset)
	POSIX::SigSet	sigset

int
sigismember(sigset, sig)
	POSIX::SigSet	sigset
	int		sig


MODULE = Termios	PACKAGE = POSIX::Termios	PREFIX = cf

POSIX::Termios
new(packname = "POSIX::Termios", ...)
    char *		packname
    CODE:
	{
#ifdef I_TERMIOS
	    New(0, RETVAL, 1, struct termios);
#else
	    not_here("termios");
        RETVAL = 0;
#endif
	}
    OUTPUT:
	RETVAL

void
DESTROY(termios_ref)
	POSIX::Termios	termios_ref
    CODE:
#ifdef I_TERMIOS
	Safefree(termios_ref);
#else
	    not_here("termios");
#endif

SysRet
getattr(termios_ref, fd = 0)
	POSIX::Termios	termios_ref
	int		fd
    CODE:
	RETVAL = tcgetattr(fd, termios_ref);
    OUTPUT:
	RETVAL

SysRet
setattr(termios_ref, fd = 0, optional_actions = 0)
	POSIX::Termios	termios_ref
	int		fd
	int		optional_actions
    CODE:
	RETVAL = tcsetattr(fd, optional_actions, termios_ref);
    OUTPUT:
	RETVAL

speed_t
cfgetispeed(termios_ref)
	POSIX::Termios	termios_ref

speed_t
cfgetospeed(termios_ref)
	POSIX::Termios	termios_ref

tcflag_t
getiflag(termios_ref)
	POSIX::Termios	termios_ref
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	RETVAL = termios_ref->c_iflag;
#else
     not_here("getiflag");
     RETVAL = 0;
#endif
    OUTPUT:
	RETVAL

tcflag_t
getoflag(termios_ref)
	POSIX::Termios	termios_ref
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	RETVAL = termios_ref->c_oflag;
#else
     not_here("getoflag");
     RETVAL = 0;
#endif
    OUTPUT:
	RETVAL

tcflag_t
getcflag(termios_ref)
	POSIX::Termios	termios_ref
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	RETVAL = termios_ref->c_cflag;
#else
     not_here("getcflag");
     RETVAL = 0;
#endif
    OUTPUT:
	RETVAL

tcflag_t
getlflag(termios_ref)
	POSIX::Termios	termios_ref
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	RETVAL = termios_ref->c_lflag;
#else
     not_here("getlflag");
     RETVAL = 0;
#endif
    OUTPUT:
	RETVAL

cc_t
getcc(termios_ref, ccix)
	POSIX::Termios	termios_ref
	int		ccix
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	if (ccix >= NCCS)
	    croak("Bad getcc subscript");
	RETVAL = termios_ref->c_cc[ccix];
#else
     not_here("getcc");
     RETVAL = 0;
#endif
    OUTPUT:
	RETVAL

SysRet
cfsetispeed(termios_ref, speed)
	POSIX::Termios	termios_ref
	speed_t		speed

SysRet
cfsetospeed(termios_ref, speed)
	POSIX::Termios	termios_ref
	speed_t		speed

void
setiflag(termios_ref, iflag)
	POSIX::Termios	termios_ref
	tcflag_t	iflag
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	termios_ref->c_iflag = iflag;
#else
	    not_here("setiflag");
#endif

void
setoflag(termios_ref, oflag)
	POSIX::Termios	termios_ref
	tcflag_t	oflag
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	termios_ref->c_oflag = oflag;
#else
	    not_here("setoflag");
#endif

void
setcflag(termios_ref, cflag)
	POSIX::Termios	termios_ref
	tcflag_t	cflag
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	termios_ref->c_cflag = cflag;
#else
	    not_here("setcflag");
#endif

void
setlflag(termios_ref, lflag)
	POSIX::Termios	termios_ref
	tcflag_t	lflag
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	termios_ref->c_lflag = lflag;
#else
	    not_here("setlflag");
#endif

void
setcc(termios_ref, ccix, cc)
	POSIX::Termios	termios_ref
	int		ccix
	cc_t		cc
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	if (ccix >= NCCS)
	    croak("Bad setcc subscript");
	termios_ref->c_cc[ccix] = cc;
#else
	    not_here("setcc");
#endif


MODULE = POSIX		PACKAGE = POSIX

NV
constant(name,arg)
	char *		name
	int		arg

int
isalnum(charstring)
	unsigned char *	charstring
    CODE:
	unsigned char *s = charstring;
	unsigned char *e = s + PL_na;	/* "PL_na" set by typemap side effect */
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isalnum(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isalpha(charstring)
	unsigned char *	charstring
    CODE:
	unsigned char *s = charstring;
	unsigned char *e = s + PL_na;	/* "PL_na" set by typemap side effect */
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isalpha(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
iscntrl(charstring)
	unsigned char *	charstring
    CODE:
	unsigned char *s = charstring;
	unsigned char *e = s + PL_na;	/* "PL_na" set by typemap side effect */
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!iscntrl(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isdigit(charstring)
	unsigned char *	charstring
    CODE:
	unsigned char *s = charstring;
	unsigned char *e = s + PL_na;	/* "PL_na" set by typemap side effect */
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isdigit(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isgraph(charstring)
	unsigned char *	charstring
    CODE:
	unsigned char *s = charstring;
	unsigned char *e = s + PL_na;	/* "PL_na" set by typemap side effect */
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isgraph(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
islower(charstring)
	unsigned char *	charstring
    CODE:
	unsigned char *s = charstring;
	unsigned char *e = s + PL_na;	/* "PL_na" set by typemap side effect */
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!islower(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isprint(charstring)
	unsigned char *	charstring
    CODE:
	unsigned char *s = charstring;
	unsigned char *e = s + PL_na;	/* "PL_na" set by typemap side effect */
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isprint(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
ispunct(charstring)
	unsigned char *	charstring
    CODE:
	unsigned char *s = charstring;
	unsigned char *e = s + PL_na;	/* "PL_na" set by typemap side effect */
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!ispunct(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isspace(charstring)
	unsigned char *	charstring
    CODE:
	unsigned char *s = charstring;
	unsigned char *e = s + PL_na;	/* "PL_na" set by typemap side effect */
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isspace(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isupper(charstring)
	unsigned char *	charstring
    CODE:
	unsigned char *s = charstring;
	unsigned char *e = s + PL_na;	/* "PL_na" set by typemap side effect */
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isupper(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isxdigit(charstring)
	unsigned char *	charstring
    CODE:
	unsigned char *s = charstring;
	unsigned char *e = s + PL_na;	/* "PL_na" set by typemap side effect */
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isxdigit(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

SysRet
open(filename, flags = O_RDONLY, mode = 0666)
	char *		filename
	int		flags
	Mode_t		mode
    CODE:
	if (flags & (O_APPEND|O_CREAT|O_TRUNC|O_RDWR|O_WRONLY|O_EXCL))
	    TAINT_PROPER("open");
	RETVAL = open(filename, flags, mode);
    OUTPUT:
	RETVAL


HV *
localeconv()
    CODE:
#ifdef HAS_LOCALECONV
	struct lconv *lcbuf;
	RETVAL = newHV();
	if ((lcbuf = localeconv())) {
	    /* the strings */
	    if (lcbuf->decimal_point && *lcbuf->decimal_point)
		hv_store(RETVAL, "decimal_point", 13,
		    newSVpv(lcbuf->decimal_point, 0), 0);
	    if (lcbuf->thousands_sep && *lcbuf->thousands_sep)
		hv_store(RETVAL, "thousands_sep", 13,
		    newSVpv(lcbuf->thousands_sep, 0), 0);
#ifndef NO_LOCALECONV_GROUPING
	    if (lcbuf->grouping && *lcbuf->grouping)
		hv_store(RETVAL, "grouping", 8,
		    newSVpv(lcbuf->grouping, 0), 0);
#endif
	    if (lcbuf->int_curr_symbol && *lcbuf->int_curr_symbol)
		hv_store(RETVAL, "int_curr_symbol", 15,
		    newSVpv(lcbuf->int_curr_symbol, 0), 0);
	    if (lcbuf->currency_symbol && *lcbuf->currency_symbol)
		hv_store(RETVAL, "currency_symbol", 15,
		    newSVpv(lcbuf->currency_symbol, 0), 0);
	    if (lcbuf->mon_decimal_point && *lcbuf->mon_decimal_point)
		hv_store(RETVAL, "mon_decimal_point", 17,
		    newSVpv(lcbuf->mon_decimal_point, 0), 0);
#ifndef NO_LOCALECONV_MON_THOUSANDS_SEP
	    if (lcbuf->mon_thousands_sep && *lcbuf->mon_thousands_sep)
		hv_store(RETVAL, "mon_thousands_sep", 17,
		    newSVpv(lcbuf->mon_thousands_sep, 0), 0);
#endif                    
#ifndef NO_LOCALECONV_MON_GROUPING
	    if (lcbuf->mon_grouping && *lcbuf->mon_grouping)
		hv_store(RETVAL, "mon_grouping", 12,
		    newSVpv(lcbuf->mon_grouping, 0), 0);
#endif
	    if (lcbuf->positive_sign && *lcbuf->positive_sign)
		hv_store(RETVAL, "positive_sign", 13,
		    newSVpv(lcbuf->positive_sign, 0), 0);
	    if (lcbuf->negative_sign && *lcbuf->negative_sign)
		hv_store(RETVAL, "negative_sign", 13,
		    newSVpv(lcbuf->negative_sign, 0), 0);
	    /* the integers */
	    if (lcbuf->int_frac_digits != CHAR_MAX)
		hv_store(RETVAL, "int_frac_digits", 15,
		    newSViv(lcbuf->int_frac_digits), 0);
	    if (lcbuf->frac_digits != CHAR_MAX)
		hv_store(RETVAL, "frac_digits", 11,
		    newSViv(lcbuf->frac_digits), 0);
	    if (lcbuf->p_cs_precedes != CHAR_MAX)
		hv_store(RETVAL, "p_cs_precedes", 13,
		    newSViv(lcbuf->p_cs_precedes), 0);
	    if (lcbuf->p_sep_by_space != CHAR_MAX)
		hv_store(RETVAL, "p_sep_by_space", 14,
		    newSViv(lcbuf->p_sep_by_space), 0);
	    if (lcbuf->n_cs_precedes != CHAR_MAX)
		hv_store(RETVAL, "n_cs_precedes", 13,
		    newSViv(lcbuf->n_cs_precedes), 0);
	    if (lcbuf->n_sep_by_space != CHAR_MAX)
		hv_store(RETVAL, "n_sep_by_space", 14,
		    newSViv(lcbuf->n_sep_by_space), 0);
	    if (lcbuf->p_sign_posn != CHAR_MAX)
		hv_store(RETVAL, "p_sign_posn", 11,
		    newSViv(lcbuf->p_sign_posn), 0);
	    if (lcbuf->n_sign_posn != CHAR_MAX)
		hv_store(RETVAL, "n_sign_posn", 11,
		    newSViv(lcbuf->n_sign_posn), 0);
	}
#else
	localeconv(); /* A stub to call not_here(). */
#endif
    OUTPUT:
	RETVAL

char *
setlocale(category, locale = 0)
	int		category
	char *		locale
    CODE:
	RETVAL = setlocale(category, locale);
	if (RETVAL) {
#ifdef USE_LOCALE_CTYPE
	    if (category == LC_CTYPE
#ifdef LC_ALL
		|| category == LC_ALL
#endif
		)
	    {
		char *newctype;
#ifdef LC_ALL
		if (category == LC_ALL)
		    newctype = setlocale(LC_CTYPE, NULL);
		else
#endif
		    newctype = RETVAL;
		new_ctype(newctype);
	    }
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	    if (category == LC_COLLATE
#ifdef LC_ALL
		|| category == LC_ALL
#endif
		)
	    {
		char *newcoll;
#ifdef LC_ALL
		if (category == LC_ALL)
		    newcoll = setlocale(LC_COLLATE, NULL);
		else
#endif
		    newcoll = RETVAL;
		new_collate(newcoll);
	    }
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	    if (category == LC_NUMERIC
#ifdef LC_ALL
		|| category == LC_ALL
#endif
		)
	    {
		char *newnum;
#ifdef LC_ALL
		if (category == LC_ALL)
		    newnum = setlocale(LC_NUMERIC, NULL);
		else
#endif
		    newnum = RETVAL;
		new_numeric(newnum);
	    }
#endif /* USE_LOCALE_NUMERIC */
	}
    OUTPUT:
	RETVAL


NV
acos(x)
	NV		x

NV
asin(x)
	NV		x

NV
atan(x)
	NV		x

NV
ceil(x)
	NV		x

NV
cosh(x)
	NV		x

NV
floor(x)
	NV		x

NV
fmod(x,y)
	NV		x
	NV		y

void
frexp(x)
	NV		x
    PPCODE:
	int expvar;
	/* (We already know stack is long enough.) */
	PUSHs(sv_2mortal(newSVnv(frexp(x,&expvar))));
	PUSHs(sv_2mortal(newSViv(expvar)));

NV
ldexp(x,exp)
	NV		x
	int		exp

NV
log10(x)
	NV		x

void
modf(x)
	NV		x
    PPCODE:
	NV intvar;
	/* (We already know stack is long enough.) */
	PUSHs(sv_2mortal(newSVnv(Perl_modf(x,&intvar))));
	PUSHs(sv_2mortal(newSVnv(intvar)));

NV
sinh(x)
	NV		x

NV
tan(x)
	NV		x

NV
tanh(x)
	NV		x

SysRet
sigaction(sig, action, oldaction = 0)
	int			sig
	POSIX::SigAction	action
	POSIX::SigAction	oldaction
    CODE:
#ifdef WIN32
	RETVAL = not_here("sigaction");
#else
# This code is really grody because we're trying to make the signal
# interface look beautiful, which is hard.

	{
	    GV *siggv = gv_fetchpv("SIG", TRUE, SVt_PVHV);
	    struct sigaction act;
	    struct sigaction oact;
	    POSIX__SigSet sigset;
	    SV** svp;
	    SV** sigsvp = hv_fetch(GvHVn(siggv),
				 PL_sig_name[sig],
				 strlen(PL_sig_name[sig]),
				 TRUE);
	    STRLEN n_a;

	    /* Remember old handler name if desired. */
	    if (oldaction) {
		char *hand = SvPVx(*sigsvp, n_a);
		svp = hv_fetch(oldaction, "HANDLER", 7, TRUE);
		sv_setpv(*svp, *hand ? hand : "DEFAULT");
	    }

	    if (action) {
		/* Vector new handler through %SIG.  (We always use sighandler
		   for the C signal handler, which reads %SIG to dispatch.) */
		svp = hv_fetch(action, "HANDLER", 7, FALSE);
		if (!svp)
		    croak("Can't supply an action without a HANDLER");
		sv_setpv(*sigsvp, SvPV(*svp, n_a));
		mg_set(*sigsvp);	/* handles DEFAULT and IGNORE */
		act.sa_handler = PL_sighandlerp;

		/* Set up any desired mask. */
		svp = hv_fetch(action, "MASK", 4, FALSE);
		if (svp && sv_isa(*svp, "POSIX::SigSet")) {
		    IV tmp = SvIV((SV*)SvRV(*svp));
		    sigset =  INT2PTR(sigset_t*, tmp);
		    act.sa_mask = *sigset;
		}
		else
		    sigemptyset(& act.sa_mask);

		/* Set up any desired flags. */
		svp = hv_fetch(action, "FLAGS", 5, FALSE);
		act.sa_flags = svp ? SvIV(*svp) : 0;
	    }

	    /* Now work around sigaction oddities */
	    if (action && oldaction)
		RETVAL = sigaction(sig, & act, & oact);
	    else if (action)
		RETVAL = sigaction(sig, & act, (struct sigaction *)0);
	    else if (oldaction)
		RETVAL = sigaction(sig, (struct sigaction *)0, & oact);
	    else
		RETVAL = -1;

	    if (oldaction) {
		/* Get back the mask. */
		svp = hv_fetch(oldaction, "MASK", 4, TRUE);
		if (sv_isa(*svp, "POSIX::SigSet")) {
		    IV tmp = SvIV((SV*)SvRV(*svp));
		    sigset = INT2PTR(sigset_t*, tmp);
		}
		else {
		    New(0, sigset, 1, sigset_t);
		    sv_setptrobj(*svp, sigset, "POSIX::SigSet");
		}
		*sigset = oact.sa_mask;

		/* Get back the flags. */
		svp = hv_fetch(oldaction, "FLAGS", 5, TRUE);
		sv_setiv(*svp, oact.sa_flags);
	    }
	}
#endif
    OUTPUT:
	RETVAL

SysRet
sigpending(sigset)
	POSIX::SigSet		sigset

SysRet
sigprocmask(how, sigset, oldsigset = 0)
	int			how
	POSIX::SigSet		sigset
	POSIX::SigSet		oldsigset = NO_INIT
INIT:
	if ( items < 3 ) {
	    oldsigset = 0;
	}
	else if (sv_derived_from(ST(2), "POSIX::SigSet")) {
	    IV tmp = SvIV((SV*)SvRV(ST(2)));
	    oldsigset = INT2PTR(POSIX__SigSet,tmp);
	}
	else {
	    New(0, oldsigset, 1, sigset_t);
	    sigemptyset(oldsigset);
	    sv_setref_pv(ST(2), "POSIX::SigSet", (void*)oldsigset);
	}

SysRet
sigsuspend(signal_mask)
	POSIX::SigSet		signal_mask

void
_exit(status)
	int		status

SysRet
close(fd)
	int		fd

SysRet
dup(fd)
	int		fd

SysRet
dup2(fd1, fd2)
	int		fd1
	int		fd2

SysRetLong
lseek(fd, offset, whence)
	int		fd
	Off_t		offset
	int		whence

SysRet
nice(incr)
	int		incr

void
pipe()
    PPCODE:
	int fds[2];
	if (pipe(fds) != -1) {
	    EXTEND(SP,2);
	    PUSHs(sv_2mortal(newSViv(fds[0])));
	    PUSHs(sv_2mortal(newSViv(fds[1])));
	}

SysRet
read(fd, buffer, nbytes)
    PREINIT:
        SV *sv_buffer = SvROK(ST(1)) ? SvRV(ST(1)) : ST(1);
    INPUT:
        int             fd
        size_t          nbytes
        char *          buffer = sv_grow( sv_buffer, nbytes+1 );
    CLEANUP:
        if (RETVAL >= 0) {
            SvCUR(sv_buffer) = RETVAL;
            SvPOK_only(sv_buffer);
            *SvEND(sv_buffer) = '\0';
            SvTAINTED_on(sv_buffer);
        }

SysRet
setpgid(pid, pgid)
	pid_t		pid
	pid_t		pgid

pid_t
setsid()

pid_t
tcgetpgrp(fd)
	int		fd

SysRet
tcsetpgrp(fd, pgrp_id)
	int		fd
	pid_t		pgrp_id

void
uname()
    PPCODE:
#ifdef HAS_UNAME
	struct utsname buf;
	if (uname(&buf) >= 0) {
	    EXTEND(SP, 5);
	    PUSHs(sv_2mortal(newSVpv(buf.sysname, 0)));
	    PUSHs(sv_2mortal(newSVpv(buf.nodename, 0)));
	    PUSHs(sv_2mortal(newSVpv(buf.release, 0)));
	    PUSHs(sv_2mortal(newSVpv(buf.version, 0)));
	    PUSHs(sv_2mortal(newSVpv(buf.machine, 0)));
	}
#else
	uname((char *) 0); /* A stub to call not_here(). */
#endif

SysRet
write(fd, buffer, nbytes)
	int		fd
	char *		buffer
	size_t		nbytes

SV *
tmpnam()
    PREINIT:
	STRLEN i;
	int len;
    CODE:
	RETVAL = newSVpvn("", 0);
	SvGROW(RETVAL, L_tmpnam);
	len = strlen(tmpnam(SvPV(RETVAL, i)));
	SvCUR_set(RETVAL, len);
    OUTPUT:
	RETVAL

void
abort()

int
mblen(s, n)
	char *		s
	size_t		n

size_t
mbstowcs(s, pwcs, n)
	wchar_t *	s
	char *		pwcs
	size_t		n

int
mbtowc(pwc, s, n)
	wchar_t *	pwc
	char *		s
	size_t		n

int
wcstombs(s, pwcs, n)
	char *		s
	wchar_t *	pwcs
	size_t		n

int
wctomb(s, wchar)
	char *		s
	wchar_t		wchar

int
strcoll(s1, s2)
	char *		s1
	char *		s2

void
strtod(str)
	char *		str
    PREINIT:
	double num;
	char *unparsed;
    PPCODE:
	SET_NUMERIC_LOCAL();
	num = strtod(str, &unparsed);
	PUSHs(sv_2mortal(newSVnv(num)));
	if (GIMME == G_ARRAY) {
	    EXTEND(SP, 1);
	    if (unparsed)
		PUSHs(sv_2mortal(newSViv(strlen(unparsed))));
	    else
		PUSHs(&PL_sv_undef);
	}

void
strtol(str, base = 0)
	char *		str
	int		base
    PREINIT:
	long num;
	char *unparsed;
    PPCODE:
	num = strtol(str, &unparsed, base);
#if IVSIZE <= LONGSIZE
	if (num < IV_MIN || num > IV_MAX)
	    PUSHs(sv_2mortal(newSVnv((double)num)));
	else
#endif
	    PUSHs(sv_2mortal(newSViv((IV)num)));
	if (GIMME == G_ARRAY) {
	    EXTEND(SP, 1);
	    if (unparsed)
		PUSHs(sv_2mortal(newSViv(strlen(unparsed))));
	    else
		PUSHs(&PL_sv_undef);
	}

void
strtoul(str, base = 0)
	char *		str
	int		base
    PREINIT:
	unsigned long num;
	char *unparsed;
    PPCODE:
	num = strtoul(str, &unparsed, base);
	if (num <= IV_MAX)
	    PUSHs(sv_2mortal(newSViv((IV)num)));
	else
	    PUSHs(sv_2mortal(newSVnv((double)num)));
	if (GIMME == G_ARRAY) {
	    EXTEND(SP, 1);
	    if (unparsed)
		PUSHs(sv_2mortal(newSViv(strlen(unparsed))));
	    else
		PUSHs(&PL_sv_undef);
	}

void
strxfrm(src)
	SV *		src
    CODE:
	{
          STRLEN srclen;
          STRLEN dstlen;
          char *p = SvPV(src,srclen);
          srclen++;
          ST(0) = sv_2mortal(NEWSV(800,srclen));
          dstlen = strxfrm(SvPVX(ST(0)), p, (size_t)srclen);
          if (dstlen > srclen) {
              dstlen++;
              SvGROW(ST(0), dstlen);
              strxfrm(SvPVX(ST(0)), p, (size_t)dstlen);
              dstlen--;
          }
          SvCUR(ST(0)) = dstlen;
	    SvPOK_only(ST(0));
	}

SysRet
mkfifo(filename, mode)
	char *		filename
	Mode_t		mode
    CODE:
	TAINT_PROPER("mkfifo");
	RETVAL = mkfifo(filename, mode);
    OUTPUT:
	RETVAL

SysRet
tcdrain(fd)
	int		fd


SysRet
tcflow(fd, action)
	int		fd
	int		action


SysRet
tcflush(fd, queue_selector)
	int		fd
	int		queue_selector

SysRet
tcsendbreak(fd, duration)
	int		fd
	int		duration

char *
asctime(sec, min, hour, mday, mon, year, wday = 0, yday = 0, isdst = 0)
	int		sec
	int		min
	int		hour
	int		mday
	int		mon
	int		year
	int		wday
	int		yday
	int		isdst
    CODE:
	{
	    struct tm mytm;
	    init_tm(&mytm);	/* XXX workaround - see init_tm() above */
	    mytm.tm_sec = sec;
	    mytm.tm_min = min;
	    mytm.tm_hour = hour;
	    mytm.tm_mday = mday;
	    mytm.tm_mon = mon;
	    mytm.tm_year = year;
	    mytm.tm_wday = wday;
	    mytm.tm_yday = yday;
	    mytm.tm_isdst = isdst;
	    RETVAL = asctime(&mytm);
	}
    OUTPUT:
	RETVAL

long
clock()

char *
ctime(time)
	Time_t		&time

void
times()
	PPCODE:
	struct tms tms;
	clock_t realtime;
	realtime = times( &tms );
	EXTEND(SP,5);
	PUSHs( sv_2mortal( newSViv( (IV) realtime ) ) );
	PUSHs( sv_2mortal( newSViv( (IV) tms.tms_utime ) ) );
	PUSHs( sv_2mortal( newSViv( (IV) tms.tms_stime ) ) );
	PUSHs( sv_2mortal( newSViv( (IV) tms.tms_cutime ) ) );
	PUSHs( sv_2mortal( newSViv( (IV) tms.tms_cstime ) ) );

double
difftime(time1, time2)
	Time_t		time1
	Time_t		time2

SysRetLong
mktime(sec, min, hour, mday, mon, year, wday = 0, yday = 0, isdst = 0)
	int		sec
	int		min
	int		hour
	int		mday
	int		mon
	int		year
	int		wday
	int		yday
	int		isdst
    CODE:
	{
	    struct tm mytm;
	    init_tm(&mytm);	/* XXX workaround - see init_tm() above */
	    mytm.tm_sec = sec;
	    mytm.tm_min = min;
	    mytm.tm_hour = hour;
	    mytm.tm_mday = mday;
	    mytm.tm_mon = mon;
	    mytm.tm_year = year;
	    mytm.tm_wday = wday;
	    mytm.tm_yday = yday;
	    mytm.tm_isdst = isdst;
	    RETVAL = mktime(&mytm);
	}
    OUTPUT:
	RETVAL

#XXX: if $xsubpp::WantOptimize is always the default
#     sv_setpv(TARG, ...) could be used rather than
#     ST(0) = sv_2mortal(newSVpv(...))
void
strftime(fmt, sec, min, hour, mday, mon, year, wday = -1, yday = -1, isdst = -1)
	char *		fmt
	int		sec
	int		min
	int		hour
	int		mday
	int		mon
	int		year
	int		wday
	int		yday
	int		isdst
    CODE:
	{
	    char tmpbuf[128];
	    struct tm mytm;
	    int len;
	    init_tm(&mytm);	/* XXX workaround - see init_tm() above */
	    mytm.tm_sec = sec;
	    mytm.tm_min = min;
	    mytm.tm_hour = hour;
	    mytm.tm_mday = mday;
	    mytm.tm_mon = mon;
	    mytm.tm_year = year;
	    mytm.tm_wday = wday;
	    mytm.tm_yday = yday;
	    mytm.tm_isdst = isdst;
	    mini_mktime(&mytm);
	    len = strftime(tmpbuf, sizeof tmpbuf, fmt, &mytm);
	    /*
	    ** The following is needed to handle to the situation where 
	    ** tmpbuf overflows.  Basically we want to allocate a buffer
	    ** and try repeatedly.  The reason why it is so complicated
	    ** is that getting a return value of 0 from strftime can indicate
	    ** one of the following:
	    ** 1. buffer overflowed,
	    ** 2. illegal conversion specifier, or
	    ** 3. the format string specifies nothing to be returned(not
	    **	  an error).  This could be because format is an empty string
	    **    or it specifies %p that yields an empty string in some locale.
	    ** If there is a better way to make it portable, go ahead by
	    ** all means.
	    */
	    if ((len > 0 && len < sizeof(tmpbuf)) || (len == 0 && *fmt == '\0'))
		ST(0) = sv_2mortal(newSVpv(tmpbuf, len));
            else {
		/* Possibly buf overflowed - try again with a bigger buf */
                int     fmtlen = strlen(fmt);
		int	bufsize = fmtlen + sizeof(tmpbuf);
		char* 	buf;
		int	buflen;

		New(0, buf, bufsize, char);
		while (buf) {
		    buflen = strftime(buf, bufsize, fmt, &mytm);
		    if (buflen > 0 && buflen < bufsize)
                        break;
                    /* heuristic to prevent out-of-memory errors */
                    if (bufsize > 100*fmtlen) {
                        Safefree(buf);
                        buf = NULL;
                        break;
                    }
		    bufsize *= 2;
		    Renew(buf, bufsize, char);
		}
		if (buf) {
		    ST(0) = sv_2mortal(newSVpvn(buf, buflen));
		    Safefree(buf);
		}
                else
		    ST(0) = sv_2mortal(newSVpvn(tmpbuf, len));
	    }
	}

void
tzset()

void
tzname()
    PPCODE:
	EXTEND(SP,2);
	PUSHs(sv_2mortal(newSVpvn(tzname[0],strlen(tzname[0]))));
	PUSHs(sv_2mortal(newSVpvn(tzname[1],strlen(tzname[1]))));

SysRet
access(filename, mode)
	char *		filename
	Mode_t		mode

char *
ctermid(s = 0)
	char *		s = 0;

char *
cuserid(s = 0)
	char *		s = 0;

SysRetLong
fpathconf(fd, name)
	int		fd
	int		name

SysRetLong
pathconf(filename, name)
	char *		filename
	int		name

SysRet
pause()

SysRetLong
sysconf(name)
	int		name

char *
ttyname(fd)
	int		fd
