/*
 * ntp_calendar.c - calendar and helper functions
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 *
 * --------------------------------------------------------------------
 * Some notes on the implementation:
 *
 * Calendar algorithms thrive on the division operation, which is one of
 * the slowest numerical operations in any CPU. What saves us here from
 * abysmal performance is the fact that all divisions are divisions by
 * constant numbers, and most compilers can do this by a multiplication
 * operation.  But this might not work when using the div/ldiv/lldiv
 * function family, because many compilers are not able to do inline
 * expansion of the code with following optimisation for the
 * constant-divider case.
 *
 * Also div/ldiv/lldiv are defined in terms of int/long/longlong, which
 * are inherently target dependent. Nothing that could not be cured with
 * autoconf, but still a mess...
 *
 * Furthermore, we need floor division in many places. C either leaves
 * the division behaviour undefined (< C99) or demands truncation to
 * zero (>= C99), so additional steps are required to make sure the
 * algorithms work. The {l,ll}div function family is requested to
 * truncate towards zero, which is also the wrong direction for our
 * purpose.
 *
 * For all this, all divisions by constant are coded manually, even when
 * there is a joined div/mod operation: The optimiser should sort that
 * out, if possible. Most of the calculations are done with unsigned
 * types, explicitely using two's complement arithmetics where
 * necessary. This minimises the dependecies to compiler and target,
 * while still giving reasonable to good performance.
 *
 * The implementation uses a few tricks that exploit properties of the
 * two's complement: Floor division on negative dividents can be
 * executed by using the one's complement of the divident. One's
 * complement can be easily created using XOR and a mask.
 *
 * Finally, check for overflow conditions is minimal. There are only two
 * calculation steps in the whole calendar that potentially suffer from
 * an internal overflow, and these are coded in a way that avoids
 * it. All other functions do not suffer from internal overflow and
 * simply return the result truncated to 32 bits.
 */

#include <config.h>
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"
#include "ntp_fp.h"
#include "ntp_unixtime.h"

#include "ntpd.h"

/* For now, let's take the conservative approach: if the target property
 * macros are not defined, check a few well-known compiler/architecture
 * settings. Default is to assume that the representation of signed
 * integers is unknown and shift-arithmetic-right is not available.
 */
#ifndef TARGET_HAS_2CPL
# if defined(__GNUC__)
#  if defined(__i386__) || defined(__x86_64__) || defined(__arm__)
#   define TARGET_HAS_2CPL 1
#  else
#   define TARGET_HAS_2CPL 0
#  endif
# elif defined(_MSC_VER)
#  if defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM)
#   define TARGET_HAS_2CPL 1
#  else
#   define TARGET_HAS_2CPL 0
#  endif
# else
#  define TARGET_HAS_2CPL 0
# endif
#endif

#ifndef TARGET_HAS_SAR
# define TARGET_HAS_SAR 0
#endif

#if !defined(HAVE_64BITREGS) && defined(UINT64_MAX) && (SIZE_MAX >= UINT64_MAX)
# define HAVE_64BITREGS
#endif

/*
 *---------------------------------------------------------------------
 * replacing the 'time()' function
 *---------------------------------------------------------------------
 */

static systime_func_ptr systime_func = &time;
static inline time_t now(void);


systime_func_ptr
ntpcal_set_timefunc(
	systime_func_ptr nfunc
	)
{
	systime_func_ptr res;

	res = systime_func;
	if (NULL == nfunc)
		nfunc = &time;
	systime_func = nfunc;

	return res;
}


static inline time_t
now(void)
{
	return (*systime_func)(NULL);
}

/*
 *---------------------------------------------------------------------
 * Get sign extension mask and unsigned 2cpl rep for a signed integer
 *---------------------------------------------------------------------
 */

static inline uint32_t
int32_sflag(
	const int32_t v)
{
#   if TARGET_HAS_2CPL && TARGET_HAS_SAR && SIZEOF_INT >= 4

	/* Let's assume that shift is the fastest way to get the sign
	 * extension of of a signed integer. This might not always be
	 * true, though -- On 8bit CPUs or machines without barrel
	 * shifter this will kill the performance. So we make sure
	 * we do this only if 'int' has at least 4 bytes.
	 */
	return (uint32_t)(v >> 31);

#   else

	/* This should be a rather generic approach for getting a sign
	 * extension mask...
	 */
	return UINT32_C(0) - (uint32_t)(v < 0);

#   endif
}

static inline int32_t
uint32_2cpl_to_int32(
	const uint32_t vu)
{
	int32_t v;

#   if TARGET_HAS_2CPL

	/* Just copy through the 32 bits from the unsigned value if
	 * we're on a two's complement target.
	 */
	v = (int32_t)vu;

#   else

	/* Convert to signed integer, making sure signed integer
	 * overflow cannot happen. Again, the optimiser might or might
	 * not find out that this is just a copy of 32 bits on a target
	 * with two's complement representation for signed integers.
	 */
	if (vu > INT32_MAX)
		v = -(int32_t)(~vu) - 1;
	else
		v = (int32_t)vu;

#   endif

	return v;
}

/*
 *---------------------------------------------------------------------
 * Convert between 'time_t' and 'vint64'
 *---------------------------------------------------------------------
 */
vint64
time_to_vint64(
	const time_t * ptt
	)
{
	vint64 res;
	time_t tt;

	tt = *ptt;

#   if SIZEOF_TIME_T <= 4

	res.D_s.hi = 0;
	if (tt < 0) {
		res.D_s.lo = (uint32_t)-tt;
		M_NEG(res.D_s.hi, res.D_s.lo);
	} else {
		res.D_s.lo = (uint32_t)tt;
	}

#   elif defined(HAVE_INT64)

	res.q_s = tt;

#   else
	/*
	 * shifting negative signed quantities is compiler-dependent, so
	 * we better avoid it and do it all manually. And shifting more
	 * than the width of a quantity is undefined. Also a don't do!
	 */
	if (tt < 0) {
		tt = -tt;
		res.D_s.lo = (uint32_t)tt;
		res.D_s.hi = (uint32_t)(tt >> 32);
		M_NEG(res.D_s.hi, res.D_s.lo);
	} else {
		res.D_s.lo = (uint32_t)tt;
		res.D_s.hi = (uint32_t)(tt >> 32);
	}

#   endif

	return res;
}


time_t
vint64_to_time(
	const vint64 *tv
	)
{
	time_t res;

#   if SIZEOF_TIME_T <= 4

	res = (time_t)tv->D_s.lo;

#   elif defined(HAVE_INT64)

	res = (time_t)tv->q_s;

#   else

	res = ((time_t)tv->d_s.hi << 32) | tv->D_s.lo;

#   endif

	return res;
}

/*
 *---------------------------------------------------------------------
 * Get the build date & time
 *---------------------------------------------------------------------
 */
int
ntpcal_get_build_date(
	struct calendar * jd
	)
{
	/* The C standard tells us the format of '__DATE__':
	 *
	 * __DATE__ The date of translation of the preprocessing
	 * translation unit: a character string literal of the form "Mmm
	 * dd yyyy", where the names of the months are the same as those
	 * generated by the asctime function, and the first character of
	 * dd is a space character if the value is less than 10. If the
	 * date of translation is not available, an
	 * implementation-defined valid date shall be supplied.
	 *
	 * __TIME__ The time of translation of the preprocessing
	 * translation unit: a character string literal of the form
	 * "hh:mm:ss" as in the time generated by the asctime
	 * function. If the time of translation is not available, an
	 * implementation-defined valid time shall be supplied.
	 *
	 * Note that MSVC declares DATE and TIME to be in the local time
	 * zone, while neither the C standard nor the GCC docs make any
	 * statement about this. As a result, we may be +/-12hrs off
	 * UTC.	 But for practical purposes, this should not be a
	 * problem.
	 *
	 */
#   ifdef MKREPRO_DATE
	static const char build[] = MKREPRO_TIME "/" MKREPRO_DATE;
#   else
	static const char build[] = __TIME__ "/" __DATE__;
#   endif
	static const char mlist[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

	char		  monstr[4];
	const char *	  cp;
	unsigned short	  hour, minute, second, day, year;
	/* Note: The above quantities are used for sscanf 'hu' format,
	 * so using 'uint16_t' is contra-indicated!
	 */

#   ifdef DEBUG
	static int	  ignore  = 0;
#   endif

	ZERO(*jd);
	jd->year     = 1970;
	jd->month    = 1;
	jd->monthday = 1;

#   ifdef DEBUG
	/* check environment if build date should be ignored */
	if (0 == ignore) {
	    const char * envstr;
	    envstr = getenv("NTPD_IGNORE_BUILD_DATE");
	    ignore = 1 + (envstr && (!*envstr || !strcasecmp(envstr, "yes")));
	}
	if (ignore > 1)
	    return FALSE;
#   endif

	if (6 == sscanf(build, "%hu:%hu:%hu/%3s %hu %hu",
			&hour, &minute, &second, monstr, &day, &year)) {
		cp = strstr(mlist, monstr);
		if (NULL != cp) {
			jd->year     = year;
			jd->month    = (uint8_t)((cp - mlist) / 3 + 1);
			jd->monthday = (uint8_t)day;
			jd->hour     = (uint8_t)hour;
			jd->minute   = (uint8_t)minute;
			jd->second   = (uint8_t)second;

			return TRUE;
		}
	}

	return FALSE;
}


/*
 *---------------------------------------------------------------------
 * basic calendar stuff
 *---------------------------------------------------------------------
 */

/*
 * Some notes on the terminology:
 *
 * We use the proleptic Gregorian calendar, which is the Gregorian
 * calendar extended in both directions ad infinitum. This totally
 * disregards the fact that this calendar was invented in 1582, and
 * was adopted at various dates over the world; sometimes even after
 * the start of the NTP epoch.
 *
 * Normally date parts are given as current cycles, while time parts
 * are given as elapsed cycles:
 *
 * 1970-01-01/03:04:05 means 'IN the 1970st. year, IN the first month,
 * ON the first day, with 3hrs, 4minutes and 5 seconds elapsed.
 *
 * The basic calculations for this calendar implementation deal with
 * ELAPSED date units, which is the number of full years, full months
 * and full days before a date: 1970-01-01 would be (1969, 0, 0) in
 * that notation.
 *
 * To ease the numeric computations, month and day values outside the
 * normal range are acceptable: 2001-03-00 will be treated as the day
 * before 2001-03-01, 2000-13-32 will give the same result as
 * 2001-02-01 and so on.
 *
 * 'rd' or 'RD' is used as an abbreviation for the latin 'rata die'
 * (day number).  This is the number of days elapsed since 0000-12-31
 * in the proleptic Gregorian calendar. The begin of the Christian Era
 * (0001-01-01) is RD(1).
 */

/*
 * ====================================================================
 *
 * General algorithmic stuff
 *
 * ====================================================================
 */

/*
 *---------------------------------------------------------------------
 * fast modulo 7 operations (floor/mathematical convention)
 *---------------------------------------------------------------------
 */
int
u32mod7(
	uint32_t x
	)
{
	/* This is a combination of tricks from "Hacker's Delight" with
	 * some modifications, like a multiplication that rounds up to
	 * drop the final adjustment stage.
	 *
	 * Do a partial reduction by digit sum to keep the value in the
	 * range permitted for the mul/shift stage. There are several
	 * possible and absolutely equivalent shift/mask combinations;
	 * this one is ARM-friendly because of a mask that fits into 16
	 * bit.
	 */
	x = (x >> 15) + (x & UINT32_C(0x7FFF));
	/* Take reminder as (mod 8) by mul/shift. Since the multiplier
	 * was calculated using ceil() instead of floor(), it skips the
	 * value '7' properly.
	 *    M <- ceil(ldexp(8/7, 29))
	 */
	return (int)((x * UINT32_C(0x24924925)) >> 29);
}

int
i32mod7(
	int32_t x
	)
{
	/* We add (2**32 - 2**32 % 7), which is (2**32 - 4), to negative
	 * numbers to map them into the postive range. Only the term '-4'
	 * survives, obviously.
	 */
	uint32_t ux = (uint32_t)x;
	return u32mod7((x < 0) ? (ux - 4u) : ux);
}

uint32_t
i32fmod(
	int32_t	 x,
	uint32_t d
	)
{
	uint32_t ux = (uint32_t)x;
	uint32_t sf = UINT32_C(0) - (x < 0);
	ux = (sf ^ ux ) % d;
	return (d & sf) + (sf ^ ux);
}

/*
 *---------------------------------------------------------------------
 * Do a periodic extension of 'value' around 'pivot' with a period of
 * 'cycle'.
 *
 * The result 'res' is a number that holds to the following properties:
 *
 *   1)	 res MOD cycle == value MOD cycle
 *   2)	 pivot <= res < pivot + cycle
 *	 (replace </<= with >/>= for negative cycles)
 *
 * where 'MOD' denotes the modulo operator for FLOOR DIVISION, which
 * is not the same as the '%' operator in C: C requires division to be
 * a truncated division, where remainder and dividend have the same
 * sign if the remainder is not zero, whereas floor division requires
 * divider and modulus to have the same sign for a non-zero modulus.
 *
 * This function has some useful applications:
 *
 * + let Y be a calendar year and V a truncated 2-digit year: then
 *	periodic_extend(Y-50, V, 100)
 *   is the closest expansion of the truncated year with respect to
 *   the full year, that is a 4-digit year with a difference of less
 *   than 50 years to the year Y. ("century unfolding")
 *
 * + let T be a UN*X time stamp and V be seconds-of-day: then
 *	perodic_extend(T-43200, V, 86400)
 *   is a time stamp that has the same seconds-of-day as the input
 *   value, with an absolute difference to T of <= 12hrs.  ("day
 *   unfolding")
 *
 * + Wherever you have a truncated periodic value and a non-truncated
 *   base value and you want to match them somehow...
 *
 * Basically, the function delivers 'pivot + (value - pivot) % cycle',
 * but the implementation takes some pains to avoid internal signed
 * integer overflows in the '(value - pivot) % cycle' part and adheres
 * to the floor division convention.
 *
 * If 64bit scalars where available on all intended platforms, writing a
 * version that uses 64 bit ops would be easy; writing a general
 * division routine for 64bit ops on a platform that can only do
 * 32/16bit divisions and is still performant is a bit more
 * difficult. Since most usecases can be coded in a way that does only
 * require the 32bit version a 64bit version is NOT provided here.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_periodic_extend(
	int32_t pivot,
	int32_t value,
	int32_t cycle
	)
{
	/* Implement a 4-quadrant modulus calculation by 2 2-quadrant
	 * branches, one for positive and one for negative dividers.
	 * Everything else can be handled by bit level logic and
	 * conditional one's complement arithmetic.  By convention, we
	 * assume
	 *
	 * x % b == 0  if  |b| < 2
	 *
	 * that is, we don't actually divide for cycles of -1,0,1 and
	 * return the pivot value in that case.
	 */
	uint32_t	uv = (uint32_t)value;
	uint32_t	up = (uint32_t)pivot;
	uint32_t	uc, sf;

	if (cycle > 1)
	{
		uc = (uint32_t)cycle;
		sf = UINT32_C(0) - (value < pivot);

		uv = sf ^ (uv - up);
		uv %= uc;
		pivot += (uc & sf) + (sf ^ uv);
	}
	else if (cycle < -1)
	{
		uc = ~(uint32_t)cycle + 1;
		sf = UINT32_C(0) - (value > pivot);

		uv = sf ^ (up - uv);
		uv %= uc;
		pivot -= (uc & sf) + (sf ^ uv);
	}
	return pivot;
}

/*---------------------------------------------------------------------
 * Note to the casual reader
 *
 * In the next two functions you will find (or would have found...)
 * the expression
 *
 *   res.Q_s -= 0x80000000;
 *
 * There was some ruckus about a possible programming error due to
 * integer overflow and sign propagation.
 *
 * This assumption is based on a lack of understanding of the C
 * standard. (Though this is admittedly not one of the most 'natural'
 * aspects of the 'C' language and easily to get wrong.)
 *
 * see
 *	http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
 *	"ISO/IEC 9899:201x Committee Draft — April 12, 2011"
 *	6.4.4.1 Integer constants, clause 5
 *
 * why there is no sign extension/overflow problem here.
 *
 * But to ease the minds of the doubtful, I added back the 'u' qualifiers
 * that somehow got lost over the last years.
 */


/*
 *---------------------------------------------------------------------
 * Convert a timestamp in NTP scale to a 64bit seconds value in the UN*X
 * scale with proper epoch unfolding around a given pivot or the current
 * system time. This function happily accepts negative pivot values as
 * timestamps before 1970-01-01, so be aware of possible trouble on
 * platforms with 32bit 'time_t'!
 *
 * This is also a periodic extension, but since the cycle is 2^32 and
 * the shift is 2^31, we can do some *very* fast math without explicit
 * divisions.
 *---------------------------------------------------------------------
 */
vint64
ntpcal_ntp_to_time(
	uint32_t	ntp,
	const time_t *	pivot
	)
{
	vint64 res;

#   if defined(HAVE_INT64)

	res.q_s = (pivot != NULL)
		      ? *pivot
		      : now();
	res.Q_s -= 0x80000000u;		/* unshift of half range */
	ntp	-= (uint32_t)JAN_1970;	/* warp into UN*X domain */
	ntp	-= res.D_s.lo;		/* cycle difference	 */
	res.Q_s += (uint64_t)ntp;	/* get expanded time	 */

#   else /* no 64bit scalars */

	time_t tmp;

	tmp = (pivot != NULL)
		  ? *pivot
		  : now();
	res = time_to_vint64(&tmp);
	M_SUB(res.D_s.hi, res.D_s.lo, 0, 0x80000000u);
	ntp -= (uint32_t)JAN_1970;	/* warp into UN*X domain */
	ntp -= res.D_s.lo;		/* cycle difference	 */
	M_ADD(res.D_s.hi, res.D_s.lo, 0, ntp);

#   endif /* no 64bit scalars */

	return res;
}

/*
 *---------------------------------------------------------------------
 * Convert a timestamp in NTP scale to a 64bit seconds value in the NTP
 * scale with proper epoch unfolding around a given pivot or the current
 * system time.
 *
 * Note: The pivot must be given in the UN*X time domain!
 *
 * This is also a periodic extension, but since the cycle is 2^32 and
 * the shift is 2^31, we can do some *very* fast math without explicit
 * divisions.
 *---------------------------------------------------------------------
 */
vint64
ntpcal_ntp_to_ntp(
	uint32_t      ntp,
	const time_t *pivot
	)
{
	vint64 res;

#   if defined(HAVE_INT64)

	res.q_s = (pivot)
		      ? *pivot
		      : now();
	res.Q_s -= 0x80000000u;		/* unshift of half range */
	res.Q_s += (uint32_t)JAN_1970;	/* warp into NTP domain	 */
	ntp	-= res.D_s.lo;		/* cycle difference	 */
	res.Q_s += (uint64_t)ntp;	/* get expanded time	 */

#   else /* no 64bit scalars */

	time_t tmp;

	tmp = (pivot)
		  ? *pivot
		  : now();
	res = time_to_vint64(&tmp);
	M_SUB(res.D_s.hi, res.D_s.lo, 0, 0x80000000u);
	M_ADD(res.D_s.hi, res.D_s.lo, 0, (uint32_t)JAN_1970);/*into NTP */
	ntp -= res.D_s.lo;		/* cycle difference	 */
	M_ADD(res.D_s.hi, res.D_s.lo, 0, ntp);

#   endif /* no 64bit scalars */

	return res;
}


/*
 * ====================================================================
 *
 * Splitting values to composite entities
 *
 * ====================================================================
 */

/*
 *---------------------------------------------------------------------
 * Split a 64bit seconds value into elapsed days in 'res.hi' and
 * elapsed seconds since midnight in 'res.lo' using explicit floor
 * division. This function happily accepts negative time values as
 * timestamps before the respective epoch start.
 *---------------------------------------------------------------------
 */
ntpcal_split
ntpcal_daysplit(
	const vint64 *ts
	)
{
	ntpcal_split res;
	uint32_t Q, R;

#   if defined(HAVE_64BITREGS)

	/* Assume we have 64bit registers an can do a divison by
	 * constant reasonably fast using the one's complement trick..
	 */
	uint64_t sf64 = (uint64_t)-(ts->q_s < 0);
	Q = (uint32_t)(sf64 ^ ((sf64 ^ ts->Q_s) / SECSPERDAY));
	R = (uint32_t)(ts->Q_s - Q * SECSPERDAY);

#   elif defined(UINT64_MAX) && !defined(__arm__)

	/* We rely on the compiler to do efficient 64bit divisions as
	 * good as possible. Which might or might not be true. At least
	 * for ARM CPUs, the sum-by-digit code in the next section is
	 * faster for many compilers. (This might change over time, but
	 * the 64bit-by-32bit division will never outperform the exact
	 * division by a substantial factor....)
	 */
	if (ts->q_s < 0)
		Q = ~(uint32_t)(~ts->Q_s / SECSPERDAY);
	else
		Q =  (uint32_t)( ts->Q_s / SECSPERDAY);
	R = ts->D_s.lo - Q * SECSPERDAY;

#   else

	/* We don't have 64bit regs. That hurts a bit.
	 *
	 * Here we use a mean trick to get away with just one explicit
	 * modulo operation and pure 32bit ops.
	 *
	 * Remember: 86400 <--> 128 * 675
	 *
	 * So we discard the lowest 7 bit and do an exact division by
	 * 675, modulo 2**32.
	 *
	 * First we shift out the lower 7 bits.
	 *
	 * Then we use a digit-wise pseudo-reduction, where a 'digit' is
	 * actually a 16-bit group. This is followed by a full reduction
	 * with a 'true' division step. This yields the modulus of the
	 * full 64bit value. The sign bit gets some extra treatment.
	 *
	 * Then we decrement the lower limb by that modulus, so it is
	 * exactly divisible by 675. [*]
	 *
	 * Then we multiply with the modular inverse of 675 (mod 2**32)
	 * and voila, we have the result.
	 *
	 * Special Thanks to Henry S. Warren and his "Hacker's delight"
	 * for giving that idea.
	 *
	 * (Note[*]: that's not the full truth. We would have to
	 * subtract the modulus from the full 64 bit number to get a
	 * number that is divisible by 675. But since we use the
	 * multiplicative inverse (mod 2**32) there's no reason to carry
	 * the subtraction into the upper bits!)
	 */
	uint32_t al = ts->D_s.lo;
	uint32_t ah = ts->D_s.hi;

	/* shift out the lower 7 bits, smash sign bit */
	al = (al >> 7) | (ah << 25);
	ah = (ah >> 7) & 0x00FFFFFFu;

	R  = (ts->d_s.hi < 0) ? 239 : 0;/* sign bit value */
	R += (al & 0xFFFF);
	R += (al >> 16	 ) * 61u;	/* 2**16 % 675 */
	R += (ah & 0xFFFF) * 346u;	/* 2**32 % 675 */
	R += (ah >> 16	 ) * 181u;	/* 2**48 % 675 */
	R %= 675u;			/* final reduction */
	Q  = (al - R) * 0x2D21C10Bu;	/* modinv(675, 2**32) */
	R  = (R << 7) | (ts->d_s.lo & 0x07F);

#   endif

	res.hi = uint32_2cpl_to_int32(Q);
	res.lo = R;

	return res;
}

/*
 *---------------------------------------------------------------------
 * Split a 64bit seconds value into elapsed weeks in 'res.hi' and
 * elapsed seconds since week start in 'res.lo' using explicit floor
 * division. This function happily accepts negative time values as
 * timestamps before the respective epoch start.
 *---------------------------------------------------------------------
 */
ntpcal_split
ntpcal_weeksplit(
	const vint64 *ts
	)
{
	ntpcal_split res;
	uint32_t Q, R;

	/* This is a very close relative to the day split function; for
	 * details, see there!
	 */

#   if defined(HAVE_64BITREGS)

	uint64_t sf64 = (uint64_t)-(ts->q_s < 0);
	Q = (uint32_t)(sf64 ^ ((sf64 ^ ts->Q_s) / SECSPERWEEK));
	R = (uint32_t)(ts->Q_s - Q * SECSPERWEEK);

#   elif defined(UINT64_MAX) && !defined(__arm__)

	if (ts->q_s < 0)
		Q = ~(uint32_t)(~ts->Q_s / SECSPERWEEK);
	else
		Q =  (uint32_t)( ts->Q_s / SECSPERWEEK);
	R = ts->D_s.lo - Q * SECSPERWEEK;

#   else

	/* Remember: 7*86400 <--> 604800 <--> 128 * 4725 */
	uint32_t al = ts->D_s.lo;
	uint32_t ah = ts->D_s.hi;

	al = (al >> 7) | (ah << 25);
	ah = (ah >> 7) & 0x00FFFFFF;

	R  = (ts->d_s.hi < 0) ? 2264 : 0;/* sign bit value */
	R += (al & 0xFFFF);
	R += (al >> 16	 ) * 4111u;	/* 2**16 % 4725 */
	R += (ah & 0xFFFF) * 3721u;	/* 2**32 % 4725 */
	R += (ah >> 16	 ) * 2206u;	/* 2**48 % 4725 */
	R %= 4725u;			/* final reduction */
	Q  = (al - R) * 0x98BBADDDu;	/* modinv(4725, 2**32) */
	R  = (R << 7) | (ts->d_s.lo & 0x07F);

#   endif

	res.hi = uint32_2cpl_to_int32(Q);
	res.lo = R;

	return res;
}

/*
 *---------------------------------------------------------------------
 * Split a 32bit seconds value into h/m/s and excessive days.  This
 * function happily accepts negative time values as timestamps before
 * midnight.
 *---------------------------------------------------------------------
 */
static int32_t
priv_timesplit(
	int32_t split[3],
	int32_t ts
	)
{
	/* Do 3 chained floor divisions by positive constants, using the
	 * one's complement trick and factoring out the intermediate XOR
	 * ops to reduce the number of operations.
	 */
	uint32_t us, um, uh, ud, sf32;

	sf32 = int32_sflag(ts);

	us = (uint32_t)ts;
	um = (sf32 ^ us) / SECSPERMIN;
	uh = um / MINSPERHR;
	ud = uh / HRSPERDAY;

	um ^= sf32;
	uh ^= sf32;
	ud ^= sf32;

	split[0] = (int32_t)(uh - ud * HRSPERDAY );
	split[1] = (int32_t)(um - uh * MINSPERHR );
	split[2] = (int32_t)(us - um * SECSPERMIN);

	return uint32_2cpl_to_int32(ud);
}

/*
 *---------------------------------------------------------------------
 * Given the number of elapsed days in the calendar era, split this
 * number into the number of elapsed years in 'res.hi' and the number
 * of elapsed days of that year in 'res.lo'.
 *
 * if 'isleapyear' is not NULL, it will receive an integer that is 0 for
 * regular years and a non-zero value for leap years.
 *---------------------------------------------------------------------
 */
ntpcal_split
ntpcal_split_eradays(
	int32_t days,
	int  *isleapyear
	)
{
	/* Use the fast cycle split algorithm here, to calculate the
	 * centuries and years in a century with one division each. This
	 * reduces the number of division operations to two, but is
	 * susceptible to internal range overflow. We take some extra
	 * steps to avoid the gap.
	 */
	ntpcal_split res;
	int32_t	 n100, n001; /* calendar year cycles */
	uint32_t uday, Q;

	/* split off centuries first
	 *
	 * We want to execute '(days * 4 + 3) /% 146097' under floor
	 * division rules in the first step. Well, actually we want to
	 * calculate 'floor((days + 0.75) / 36524.25)', but we want to
	 * do it in scaled integer calculation.
	 */
#   if defined(HAVE_64BITREGS)

	/* not too complicated with an intermediate 64bit value */
	uint64_t	ud64, sf64;
	ud64 = ((uint64_t)days << 2) | 3u;
	sf64 = (uint64_t)-(days < 0);
	Q    = (uint32_t)(sf64 ^ ((sf64 ^ ud64) / GREGORIAN_CYCLE_DAYS));
	uday = (uint32_t)(ud64 - Q * GREGORIAN_CYCLE_DAYS);
	n100 = uint32_2cpl_to_int32(Q);

#   else

	/* '4*days+3' suffers from range overflow when going to the
	 * limits. We solve this by doing an exact division (mod 2^32)
	 * after caclulating the remainder first.
	 *
	 * We start with a partial reduction by digit sums, extracting
	 * the upper bits from the original value before they get lost
	 * by scaling, and do one full division step to get the true
	 * remainder.  Then a final multiplication with the
	 * multiplicative inverse of 146097 (mod 2^32) gives us the full
	 * quotient.
	 *
	 * (-2^33) % 146097	--> 130717    : the sign bit value
	 * ( 2^20) % 146097	--> 25897     : the upper digit value
	 * modinv(146097, 2^32) --> 660721233 : the inverse
	 */
	uint32_t ux = ((uint32_t)days << 2) | 3;
	uday  = (days < 0) ? 130717u : 0u;	    /* sign dgt */
	uday += ((days >> 18) & 0x01FFFu) * 25897u; /* hi dgt (src!) */
	uday += (ux & 0xFFFFFu);		    /* lo dgt */
	uday %= GREGORIAN_CYCLE_DAYS;		    /* full reduction */
	Q     = (ux  - uday) * 660721233u;	    /* exact div */
	n100  = uint32_2cpl_to_int32(Q);

#   endif

	/* Split off years in century -- days >= 0 here, and we're far
	 * away from integer overflow trouble now. */
	uday |= 3;
	n001  = uday / GREGORIAN_NORMAL_LEAP_CYCLE_DAYS;
	uday -= n001 * GREGORIAN_NORMAL_LEAP_CYCLE_DAYS;

	/* Assemble the year and day in year */
	res.hi = n100 * 100 + n001;
	res.lo = uday / 4u;

	/* Possibly set the leap year flag */
	if (isleapyear) {
		uint32_t tc = (uint32_t)n100 + 1;
		uint32_t ty = (uint32_t)n001 + 1;
		*isleapyear = !(ty & 3)
		    && ((ty != 100) || !(tc & 3));
	}
	return res;
}

/*
 *---------------------------------------------------------------------
 * Given a number of elapsed days in a year and a leap year indicator,
 * split the number of elapsed days into the number of elapsed months in
 * 'res.hi' and the number of elapsed days of that month in 'res.lo'.
 *
 * This function will fail and return {-1,-1} if the number of elapsed
 * days is not in the valid range!
 *---------------------------------------------------------------------
 */
ntpcal_split
ntpcal_split_yeardays(
	int32_t eyd,
	int	isleap
	)
{
	/* Use the unshifted-year, February-with-30-days approach here.
	 * Fractional interpolations are used in both directions, with
	 * the smallest power-of-two divider to avoid any true division.
	 */
	ntpcal_split	res = {-1, -1};

	/* convert 'isleap' to number of defective days */
	isleap = 1 + !isleap;
	/* adjust for February of 30 nominal days */
	if (eyd >= 61 - isleap)
		eyd += isleap;
	/* if in range, convert to months and days in month */
	if (eyd >= 0 && eyd < 367) {
		res.hi = (eyd * 67 + 32) >> 11;
		res.lo = eyd - ((489 * res.hi + 8) >> 4);
	}

	return res;
}

/*
 *---------------------------------------------------------------------
 * Convert a RD into the date part of a 'struct calendar'.
 *---------------------------------------------------------------------
 */
int
ntpcal_rd_to_date(
	struct calendar *jd,
	int32_t		 rd
	)
{
	ntpcal_split split;
	int	     leapy;
	u_int	     ymask;

	/* Get day-of-week first. It's simply the RD (mod 7)... */
	jd->weekday = i32mod7(rd);

	split = ntpcal_split_eradays(rd - 1, &leapy);
	/* Get year and day-of-year, with overflow check. If any of the
	 * upper 16 bits is set after shifting to unity-based years, we
	 * will have an overflow when converting to an unsigned 16bit
	 * year. Shifting to the right is OK here, since it does not
	 * matter if the shift is logic or arithmetic.
	 */
	split.hi += 1;
	ymask = 0u - ((split.hi >> 16) == 0);
	jd->year = (uint16_t)(split.hi & ymask);
	jd->yearday = (uint16_t)split.lo + 1;

	/* convert to month and mday */
	split = ntpcal_split_yeardays(split.lo, leapy);
	jd->month    = (uint8_t)split.hi + 1;
	jd->monthday = (uint8_t)split.lo + 1;

	return ymask ? leapy : -1;
}

/*
 *---------------------------------------------------------------------
 * Convert a RD into the date part of a 'struct tm'.
 *---------------------------------------------------------------------
 */
int
ntpcal_rd_to_tm(
	struct tm  *utm,
	int32_t	    rd
	)
{
	ntpcal_split split;
	int	     leapy;

	/* get day-of-week first */
	utm->tm_wday = i32mod7(rd);

	/* get year and day-of-year */
	split = ntpcal_split_eradays(rd - 1, &leapy);
	utm->tm_year = split.hi - 1899;
	utm->tm_yday = split.lo;	/* 0-based */

	/* convert to month and mday */
	split = ntpcal_split_yeardays(split.lo, leapy);
	utm->tm_mon  = split.hi;	/* 0-based */
	utm->tm_mday = split.lo + 1;	/* 1-based */

	return leapy;
}

/*
 *---------------------------------------------------------------------
 * Take a value of seconds since midnight and split it into hhmmss in a
 * 'struct calendar'.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_daysec_to_date(
	struct calendar *jd,
	int32_t		sec
	)
{
	int32_t days;
	int   ts[3];

	days = priv_timesplit(ts, sec);
	jd->hour   = (uint8_t)ts[0];
	jd->minute = (uint8_t)ts[1];
	jd->second = (uint8_t)ts[2];

	return days;
}

/*
 *---------------------------------------------------------------------
 * Take a value of seconds since midnight and split it into hhmmss in a
 * 'struct tm'.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_daysec_to_tm(
	struct tm *utm,
	int32_t	   sec
	)
{
	int32_t days;
	int32_t ts[3];

	days = priv_timesplit(ts, sec);
	utm->tm_hour = ts[0];
	utm->tm_min  = ts[1];
	utm->tm_sec  = ts[2];

	return days;
}

/*
 *---------------------------------------------------------------------
 * take a split representation for day/second-of-day and day offset
 * and convert it to a 'struct calendar'. The seconds will be normalised
 * into the range of a day, and the day will be adjusted accordingly.
 *
 * returns >0 if the result is in a leap year, 0 if in a regular
 * year and <0 if the result did not fit into the calendar struct.
 *---------------------------------------------------------------------
 */
int
ntpcal_daysplit_to_date(
	struct calendar	   *jd,
	const ntpcal_split *ds,
	int32_t		    dof
	)
{
	dof += ntpcal_daysec_to_date(jd, ds->lo);
	return ntpcal_rd_to_date(jd, ds->hi + dof);
}

/*
 *---------------------------------------------------------------------
 * take a split representation for day/second-of-day and day offset
 * and convert it to a 'struct tm'. The seconds will be normalised
 * into the range of a day, and the day will be adjusted accordingly.
 *
 * returns 1 if the result is in a leap year and zero if in a regular
 * year.
 *---------------------------------------------------------------------
 */
int
ntpcal_daysplit_to_tm(
	struct tm	   *utm,
	const ntpcal_split *ds ,
	int32_t		    dof
	)
{
	dof += ntpcal_daysec_to_tm(utm, ds->lo);

	return ntpcal_rd_to_tm(utm, ds->hi + dof);
}

/*
 *---------------------------------------------------------------------
 * Take a UN*X time and convert to a calendar structure.
 *---------------------------------------------------------------------
 */
int
ntpcal_time_to_date(
	struct calendar	*jd,
	const vint64	*ts
	)
{
	ntpcal_split ds;

	ds = ntpcal_daysplit(ts);
	ds.hi += ntpcal_daysec_to_date(jd, ds.lo);
	ds.hi += DAY_UNIX_STARTS;

	return ntpcal_rd_to_date(jd, ds.hi);
}


/*
 * ====================================================================
 *
 * merging composite entities
 *
 * ====================================================================
 */

#if !defined(HAVE_INT64)
/* multiplication helper. Seconds in days and weeks are multiples of 128,
 * and without that factor fit well into 16 bit. So a multiplication
 * of 32bit by 16bit and some shifting can be used on pure 32bit machines
 * with compilers that do not support 64bit integers.
 *
 * Calculate ( hi * mul * 128 ) + lo
 */
static vint64
_dwjoin(
	uint16_t	mul,
	int32_t		hi,
	int32_t		lo
	)
{
	vint64		res;
	uint32_t	p1, p2, sf;

	/* get sign flag and absolute value of 'hi' in p1 */
	sf = (uint32_t)-(hi < 0);
	p1 = ((uint32_t)hi + sf) ^ sf;

	/* assemble major units: res <- |hi| * mul */
	res.D_s.lo = (p1 & 0xFFFF) * mul;
	res.D_s.hi = 0;
	p1 = (p1 >> 16) * mul;
	p2 = p1 >> 16;
	p1 = p1 << 16;
	M_ADD(res.D_s.hi, res.D_s.lo, p2, p1);

	/* mul by 128, using shift: res <-- res << 7 */
	res.D_s.hi = (res.D_s.hi << 7) | (res.D_s.lo >> 25);
	res.D_s.lo = (res.D_s.lo << 7);

	/* fix up sign: res <-- (res + [sf|sf]) ^ [sf|sf] */
	M_ADD(res.D_s.hi, res.D_s.lo, sf, sf);
	res.D_s.lo ^= sf;
	res.D_s.hi ^= sf;

	/* properly add seconds: res <-- res + [sx(lo)|lo] */
	p2 = (uint32_t)-(lo < 0);
	p1 = (uint32_t)lo;
	M_ADD(res.D_s.hi, res.D_s.lo, p2, p1);
	return res;
}
#endif

/*
 *---------------------------------------------------------------------
 * Merge a number of days and a number of seconds into seconds,
 * expressed in 64 bits to avoid overflow.
 *---------------------------------------------------------------------
 */
vint64
ntpcal_dayjoin(
	int32_t days,
	int32_t secs
	)
{
	vint64 res;

#   if defined(HAVE_INT64)

	res.q_s	 = days;
	res.q_s *= SECSPERDAY;
	res.q_s += secs;

#   else

	res = _dwjoin(675, days, secs);

#   endif

	return res;
}

/*
 *---------------------------------------------------------------------
 * Merge a number of weeks and a number of seconds into seconds,
 * expressed in 64 bits to avoid overflow.
 *---------------------------------------------------------------------
 */
vint64
ntpcal_weekjoin(
	int32_t week,
	int32_t secs
	)
{
	vint64 res;

#   if defined(HAVE_INT64)

	res.q_s	 = week;
	res.q_s *= SECSPERWEEK;
	res.q_s += secs;

#   else

	res = _dwjoin(4725, week, secs);

#   endif

	return res;
}

/*
 *---------------------------------------------------------------------
 * get leap years since epoch in elapsed years
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_leapyears_in_years(
	int32_t years
	)
{
	/* We use the in-out-in algorithm here, using the one's
	 * complement division trick for negative numbers. The chained
	 * division sequence by 4/25/4 gives the compiler the chance to
	 * get away with only one true division and doing shifts otherwise.
	 */

	uint32_t sf32, sum, uyear;

	sf32  = int32_sflag(years);
	uyear = (uint32_t)years;
	uyear ^= sf32;

	sum  = (uyear /=  4u);	/*   4yr rule --> IN  */
	sum -= (uyear /= 25u);	/* 100yr rule --> OUT */
	sum += (uyear /=  4u);	/* 400yr rule --> IN  */

	/* Thanks to the alternation of IN/OUT/IN we can do the sum
	 * directly and have a single one's complement operation
	 * here. (Only if the years are negative, of course.) Otherwise
	 * the one's complement would have to be done when
	 * adding/subtracting the terms.
	 */
	return uint32_2cpl_to_int32(sf32 ^ sum);
}

/*
 *---------------------------------------------------------------------
 * Convert elapsed years in Era into elapsed days in Era.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_days_in_years(
	int32_t years
	)
{
	return years * DAYSPERYEAR + ntpcal_leapyears_in_years(years);
}

/*
 *---------------------------------------------------------------------
 * Convert a number of elapsed month in a year into elapsed days in year.
 *
 * The month will be normalized, and 'res.hi' will contain the
 * excessive years that must be considered when converting the years,
 * while 'res.lo' will contain the number of elapsed days since start
 * of the year.
 *
 * This code uses the shifted-month-approach to convert month to days,
 * because then there is no need to have explicit leap year
 * information.	 The slight disadvantage is that for most month values
 * the result is a negative value, and the year excess is one; the
 * conversion is then simply based on the start of the following year.
 *---------------------------------------------------------------------
 */
ntpcal_split
ntpcal_days_in_months(
	int32_t m
	)
{
	ntpcal_split res;

	/* Add ten months with proper year adjustment. */
	if (m < 2) {
	    res.lo  = m + 10;
	    res.hi  = 0;
	} else {
	    res.lo  = m - 2;
	    res.hi  = 1;
	}

	/* Possibly normalise by floor division. This does not hapen for
	 * input in normal range. */
	if (res.lo < 0 || res.lo >= 12) {
		uint32_t mu, Q, sf32;
		sf32 = int32_sflag(res.lo);
		mu   = (uint32_t)res.lo;
		Q    = sf32 ^ ((sf32 ^ mu) / 12u);

		res.hi += uint32_2cpl_to_int32(Q);
		res.lo	= mu - Q * 12u;
	}

	/* Get cummulated days in year with unshift. Use the fractional
	 * interpolation with smallest possible power of two in the
	 * divider.
	 */
	res.lo = ((res.lo * 979 + 16) >> 5) - 306;

	return res;
}

/*
 *---------------------------------------------------------------------
 * Convert ELAPSED years/months/days of gregorian calendar to elapsed
 * days in Gregorian epoch.
 *
 * If you want to convert years and days-of-year, just give a month of
 * zero.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_edate_to_eradays(
	int32_t years,
	int32_t mons,
	int32_t mdays
	)
{
	ntpcal_split tmp;
	int32_t	     res;

	if (mons) {
		tmp = ntpcal_days_in_months(mons);
		res = ntpcal_days_in_years(years + tmp.hi) + tmp.lo;
	} else
		res = ntpcal_days_in_years(years);
	res += mdays;

	return res;
}

/*
 *---------------------------------------------------------------------
 * Convert ELAPSED years/months/days of gregorian calendar to elapsed
 * days in year.
 *
 * Note: This will give the true difference to the start of the given
 * year, even if months & days are off-scale.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_edate_to_yeardays(
	int32_t years,
	int32_t mons,
	int32_t mdays
	)
{
	ntpcal_split tmp;

	if (0 <= mons && mons < 12) {
		if (mons >= 2)
			mdays -= 2 - is_leapyear(years+1);
		mdays += (489 * mons + 8) >> 4;
	} else {
		tmp = ntpcal_days_in_months(mons);
		mdays += tmp.lo
		       + ntpcal_days_in_years(years + tmp.hi)
		       - ntpcal_days_in_years(years);
	}

	return mdays;
}

/*
 *---------------------------------------------------------------------
 * Convert elapsed days and the hour/minute/second information into
 * total seconds.
 *
 * If 'isvalid' is not NULL, do a range check on the time specification
 * and tell if the time input is in the normal range, permitting for a
 * single leapsecond.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_etime_to_seconds(
	int32_t hours,
	int32_t minutes,
	int32_t seconds
	)
{
	int32_t res;

	res = (hours * MINSPERHR + minutes) * SECSPERMIN + seconds;

	return res;
}

/*
 *---------------------------------------------------------------------
 * Convert the date part of a 'struct tm' (that is, year, month,
 * day-of-month) into the RD of that day.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_tm_to_rd(
	const struct tm *utm
	)
{
	return ntpcal_edate_to_eradays(utm->tm_year + 1899,
				       utm->tm_mon,
				       utm->tm_mday - 1) + 1;
}

/*
 *---------------------------------------------------------------------
 * Convert the date part of a 'struct calendar' (that is, year, month,
 * day-of-month) into the RD of that day.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_date_to_rd(
	const struct calendar *jd
	)
{
	return ntpcal_edate_to_eradays((int32_t)jd->year - 1,
				       (int32_t)jd->month - 1,
				       (int32_t)jd->monthday - 1) + 1;
}

/*
 *---------------------------------------------------------------------
 * convert a year number to rata die of year start
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_year_to_ystart(
	int32_t year
	)
{
	return ntpcal_days_in_years(year - 1) + 1;
}

/*
 *---------------------------------------------------------------------
 * For a given RD, get the RD of the associated year start,
 * that is, the RD of the last January,1st on or before that day.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_rd_to_ystart(
	int32_t rd
	)
{
	/*
	 * Rather simple exercise: split the day number into elapsed
	 * years and elapsed days, then remove the elapsed days from the
	 * input value. Nice'n sweet...
	 */
	return rd - ntpcal_split_eradays(rd - 1, NULL).lo;
}

/*
 *---------------------------------------------------------------------
 * For a given RD, get the RD of the associated month start.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_rd_to_mstart(
	int32_t rd
	)
{
	ntpcal_split split;
	int	     leaps;

	split = ntpcal_split_eradays(rd - 1, &leaps);
	split = ntpcal_split_yeardays(split.lo, leaps);

	return rd - split.lo;
}

/*
 *---------------------------------------------------------------------
 * take a 'struct calendar' and get the seconds-of-day from it.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_date_to_daysec(
	const struct calendar *jd
	)
{
	return ntpcal_etime_to_seconds(jd->hour, jd->minute,
				       jd->second);
}

/*
 *---------------------------------------------------------------------
 * take a 'struct tm' and get the seconds-of-day from it.
 *---------------------------------------------------------------------
 */
int32_t
ntpcal_tm_to_daysec(
	const struct tm *utm
	)
{
	return ntpcal_etime_to_seconds(utm->tm_hour, utm->tm_min,
				       utm->tm_sec);
}

/*
 *---------------------------------------------------------------------
 * take a 'struct calendar' and convert it to a 'time_t'
 *---------------------------------------------------------------------
 */
time_t
ntpcal_date_to_time(
	const struct calendar *jd
	)
{
	vint64	join;
	int32_t days, secs;

	days = ntpcal_date_to_rd(jd) - DAY_UNIX_STARTS;
	secs = ntpcal_date_to_daysec(jd);
	join = ntpcal_dayjoin(days, secs);

	return vint64_to_time(&join);
}


/*
 * ====================================================================
 *
 * extended and unchecked variants of caljulian/caltontp
 *
 * ====================================================================
 */
int
ntpcal_ntp64_to_date(
	struct calendar *jd,
	const vint64	*ntp
	)
{
	ntpcal_split ds;

	ds = ntpcal_daysplit(ntp);
	ds.hi += ntpcal_daysec_to_date(jd, ds.lo);

	return ntpcal_rd_to_date(jd, ds.hi + DAY_NTP_STARTS);
}

int
ntpcal_ntp_to_date(
	struct calendar *jd,
	uint32_t	 ntp,
	const time_t	*piv
	)
{
	vint64	ntp64;

	/*
	 * Unfold ntp time around current time into NTP domain. Split
	 * into days and seconds, shift days into CE domain and
	 * process the parts.
	 */
	ntp64 = ntpcal_ntp_to_ntp(ntp, piv);
	return ntpcal_ntp64_to_date(jd, &ntp64);
}


vint64
ntpcal_date_to_ntp64(
	const struct calendar *jd
	)
{
	/*
	 * Convert date to NTP. Ignore yearday, use d/m/y only.
	 */
	return ntpcal_dayjoin(ntpcal_date_to_rd(jd) - DAY_NTP_STARTS,
			      ntpcal_date_to_daysec(jd));
}


uint32_t
ntpcal_date_to_ntp(
	const struct calendar *jd
	)
{
	/*
	 * Get lower half of 64bit NTP timestamp from date/time.
	 */
	return ntpcal_date_to_ntp64(jd).d_s.lo;
}



/*
 * ====================================================================
 *
 * day-of-week calculations
 *
 * ====================================================================
 */
/*
 * Given a RataDie and a day-of-week, calculate a RDN that is reater-than,
 * greater-or equal, closest, less-or-equal or less-than the given RDN
 * and denotes the given day-of-week
 */
int32_t
ntpcal_weekday_gt(
	int32_t rdn,
	int32_t dow
	)
{
	return ntpcal_periodic_extend(rdn+1, dow, 7);
}

int32_t
ntpcal_weekday_ge(
	int32_t rdn,
	int32_t dow
	)
{
	return ntpcal_periodic_extend(rdn, dow, 7);
}

int32_t
ntpcal_weekday_close(
	int32_t rdn,
	int32_t dow
	)
{
	return ntpcal_periodic_extend(rdn-3, dow, 7);
}

int32_t
ntpcal_weekday_le(
	int32_t rdn,
	int32_t dow
	)
{
	return ntpcal_periodic_extend(rdn, dow, -7);
}

int32_t
ntpcal_weekday_lt(
	int32_t rdn,
	int32_t dow
	)
{
	return ntpcal_periodic_extend(rdn-1, dow, -7);
}

/*
 * ====================================================================
 *
 * ISO week-calendar conversions
 *
 * The ISO8601 calendar defines a calendar of years, weeks and weekdays.
 * It is related to the Gregorian calendar, and a ISO year starts at the
 * Monday closest to Jan,1st of the corresponding Gregorian year.  A ISO
 * calendar year has always 52 or 53 weeks, and like the Grogrian
 * calendar the ISO8601 calendar repeats itself every 400 years, or
 * 146097 days, or 20871 weeks.
 *
 * While it is possible to write ISO calendar functions based on the
 * Gregorian calendar functions, the following implementation takes a
 * different approach, based directly on years and weeks.
 *
 * Analysis of the tabulated data shows that it is not possible to
 * interpolate from years to weeks over a full 400 year range; cyclic
 * shifts over 400 years do not provide a solution here. But it *is*
 * possible to interpolate over every single century of the 400-year
 * cycle. (The centennial leap year rule seems to be the culprit here.)
 *
 * It can be shown that a conversion from years to weeks can be done
 * using a linear transformation of the form
 *
 *   w = floor( y * a + b )
 *
 * where the slope a must hold to
 *
 *  52.1780821918 <= a < 52.1791044776
 *
 * and b must be chosen according to the selected slope and the number
 * of the century in a 400-year period.
 *
 * The inverse calculation can also be done in this way. Careful scaling
 * provides an unlimited set of integer coefficients a,k,b that enable
 * us to write the calulation in the form
 *
 *   w = (y * a	 + b ) / k
 *   y = (w * a' + b') / k'
 *
 * In this implementation the values of k and k' are chosen to be the
 * smallest possible powers of two, so the division can be implemented
 * as shifts if the optimiser chooses to do so.
 *
 * ====================================================================
 */

/*
 * Given a number of elapsed (ISO-)years since the begin of the
 * christian era, return the number of elapsed weeks corresponding to
 * the number of years.
 */
int32_t
isocal_weeks_in_years(
	int32_t years
	)
{
	/*
	 * use: w = (y * 53431 + b[c]) / 1024 as interpolation
	 */
	static const uint16_t bctab[4] = { 157, 449, 597, 889 };

	int32_t	 cs, cw;
	uint32_t cc, ci, yu, sf32;

	sf32 = int32_sflag(years);
	yu   = (uint32_t)years;

	/* split off centuries, using floor division */
	cc  = sf32 ^ ((sf32 ^ yu) / 100u);
	yu -= cc * 100u;

	/* calculate century cycles shift and cycle index:
	 * Assuming a century is 5217 weeks, we have to add a cycle
	 * shift that is 3 for every 4 centuries, because 3 of the four
	 * centuries have 5218 weeks. So '(cc*3 + 1) / 4' is the actual
	 * correction, and the second century is the defective one.
	 *
	 * Needs floor division by 4, which is done with masking and
	 * shifting.
	 */
	ci = cc * 3u + 1;
	cs = uint32_2cpl_to_int32(sf32 ^ ((sf32 ^ ci) >> 2));
	ci = ci & 3u;

	/* Get weeks in century. Can use plain division here as all ops
	 * are >= 0,  and let the compiler sort out the possible
	 * optimisations.
	 */
	cw = (yu * 53431u + bctab[ci]) / 1024u;

	return uint32_2cpl_to_int32(cc) * 5217 + cs + cw;
}

/*
 * Given a number of elapsed weeks since the begin of the christian
 * era, split this number into the number of elapsed years in res.hi
 * and the excessive number of weeks in res.lo. (That is, res.lo is
 * the number of elapsed weeks in the remaining partial year.)
 */
ntpcal_split
isocal_split_eraweeks(
	int32_t weeks
	)
{
	/*
	 * use: y = (w * 157 + b[c]) / 8192 as interpolation
	 */

	static const uint16_t bctab[4] = { 85, 130, 17, 62 };

	ntpcal_split res;
	int32_t	 cc, ci;
	uint32_t sw, cy, Q;

	/* Use two fast cycle-split divisions again. Herew e want to
	 * execute '(weeks * 4 + 2) /% 20871' under floor division rules
	 * in the first step.
	 *
	 * This is of course (again) susceptible to internal overflow if
	 * coded directly in 32bit. And again we use 64bit division on
	 * a 64bit target and exact division after calculating the
	 * remainder first on a 32bit target. With the smaller divider,
	 * that's even a bit neater.
	 */
#   if defined(HAVE_64BITREGS)

	/* Full floor division with 64bit values. */
	uint64_t sf64, sw64;
	sf64 = (uint64_t)-(weeks < 0);
	sw64 = ((uint64_t)weeks << 2) | 2u;
	Q    = (uint32_t)(sf64 ^ ((sf64 ^ sw64) / GREGORIAN_CYCLE_WEEKS));
	sw   = (uint32_t)(sw64 - Q * GREGORIAN_CYCLE_WEEKS);

#   else

	/* Exact division after calculating the remainder via partial
	 * reduction by digit sum.
	 * (-2^33) % 20871     --> 5491	     : the sign bit value
	 * ( 2^20) % 20871     --> 5026	     : the upper digit value
	 * modinv(20871, 2^32) --> 330081335 : the inverse
	 */
	uint32_t ux = ((uint32_t)weeks << 2) | 2;
	sw  = (weeks < 0) ? 5491u : 0u;		  /* sign dgt */
	sw += ((weeks >> 18) & 0x01FFFu) * 5026u; /* hi dgt (src!) */
	sw += (ux & 0xFFFFFu);			  /* lo dgt */
	sw %= GREGORIAN_CYCLE_WEEKS;		  /* full reduction */
	Q   = (ux  - sw) * 330081335u;		  /* exact div */

#   endif

	ci  = Q & 3u;
	cc  = uint32_2cpl_to_int32(Q);

	/* Split off years; sw >= 0 here! The scaled weeks in the years
	 * are scaled up by 157 afterwards.
	 */
	sw  = (sw / 4u) * 157u + bctab[ci];
	cy  = sw / 8192u;	/* sw >> 13 , let the compiler sort it out */
	sw  = sw % 8192u;	/* sw & 8191, let the compiler sort it out */

	/* assemble elapsed years and downscale the elapsed weeks in
	 * the year.
	 */
	res.hi = 100*cc + cy;
	res.lo = sw / 157u;

	return res;
}

/*
 * Given a second in the NTP time scale and a pivot, expand the NTP
 * time stamp around the pivot and convert into an ISO calendar time
 * stamp.
 */
int
isocal_ntp64_to_date(
	struct isodate *id,
	const vint64   *ntp
	)
{
	ntpcal_split ds;
	int32_t	     ts[3];
	uint32_t     uw, ud, sf32;

	/*
	 * Split NTP time into days and seconds, shift days into CE
	 * domain and process the parts.
	 */
	ds = ntpcal_daysplit(ntp);

	/* split time part */
	ds.hi += priv_timesplit(ts, ds.lo);
	id->hour   = (uint8_t)ts[0];
	id->minute = (uint8_t)ts[1];
	id->second = (uint8_t)ts[2];

	/* split days into days and weeks, using floor division in unsigned */
	ds.hi += DAY_NTP_STARTS - 1; /* shift from NTP to RDN */
	sf32 = int32_sflag(ds.hi);
	ud   = (uint32_t)ds.hi;
	uw   = sf32 ^ ((sf32 ^ ud) / DAYSPERWEEK);
	ud  -= uw * DAYSPERWEEK;

	ds.hi = uint32_2cpl_to_int32(uw);
	ds.lo = ud;

	id->weekday = (uint8_t)ds.lo + 1;	/* weekday result    */

	/* get year and week in year */
	ds = isocal_split_eraweeks(ds.hi);	/* elapsed years&week*/
	id->year = (uint16_t)ds.hi + 1;		/* shift to current  */
	id->week = (uint8_t )ds.lo + 1;

	return (ds.hi >= 0 && ds.hi < 0x0000FFFF);
}

int
isocal_ntp_to_date(
	struct isodate *id,
	uint32_t	ntp,
	const time_t   *piv
	)
{
	vint64	ntp64;

	/*
	 * Unfold ntp time around current time into NTP domain, then
	 * convert the full time stamp.
	 */
	ntp64 = ntpcal_ntp_to_ntp(ntp, piv);
	return isocal_ntp64_to_date(id, &ntp64);
}

/*
 * Convert a ISO date spec into a second in the NTP time scale,
 * properly truncated to 32 bit.
 */
vint64
isocal_date_to_ntp64(
	const struct isodate *id
	)
{
	int32_t weeks, days, secs;

	weeks = isocal_weeks_in_years((int32_t)id->year - 1)
	      + (int32_t)id->week - 1;
	days = weeks * 7 + (int32_t)id->weekday;
	/* days is RDN of ISO date now */
	secs = ntpcal_etime_to_seconds(id->hour, id->minute, id->second);

	return ntpcal_dayjoin(days - DAY_NTP_STARTS, secs);
}

uint32_t
isocal_date_to_ntp(
	const struct isodate *id
	)
{
	/*
	 * Get lower half of 64bit NTP timestamp from date/time.
	 */
	return isocal_date_to_ntp64(id).d_s.lo;
}

/*
 * ====================================================================
 * 'basedate' support functions
 * ====================================================================
 */

static int32_t s_baseday = NTP_TO_UNIX_DAYS;
static int32_t s_gpsweek = 0;

int32_t
basedate_eval_buildstamp(void)
{
	struct calendar jd;
	int32_t		ed;

	if (!ntpcal_get_build_date(&jd))
		return NTP_TO_UNIX_DAYS;

	/* The time zone of the build stamp is unspecified; we remove
	 * one day to provide a certain slack. And in case somebody
	 * fiddled with the system clock, we make sure we do not go
	 * before the UNIX epoch (1970-01-01). It's probably not possible
	 * to do this to the clock on most systems, but there are other
	 * ways to tweak the build stamp.
	 */
	jd.monthday -= 1;
	ed = ntpcal_date_to_rd(&jd) - DAY_NTP_STARTS;
	return (ed < NTP_TO_UNIX_DAYS) ? NTP_TO_UNIX_DAYS : ed;
}

int32_t
basedate_eval_string(
	const char * str
	)
{
	u_short	y,m,d;
	u_long	ned;
	int	rc, nc;
	size_t	sl;

	sl = strlen(str);
	rc = sscanf(str, "%4hu-%2hu-%2hu%n", &y, &m, &d, &nc);
	if (rc == 3 && (size_t)nc == sl) {
		if (m >= 1 && m <= 12 && d >= 1 && d <= 31)
			return ntpcal_edate_to_eradays(y-1, m-1, d)
			    - DAY_NTP_STARTS;
		goto buildstamp;
	}

	rc = sscanf(str, "%lu%n", &ned, &nc);
	if (rc == 1 && (size_t)nc == sl) {
		if (ned <= INT32_MAX)
			return (int32_t)ned;
		goto buildstamp;
	}

  buildstamp:
	msyslog(LOG_WARNING,
		"basedate string \"%s\" invalid, build date substituted!",
		str);
	return basedate_eval_buildstamp();
}

uint32_t
basedate_get_day(void)
{
	return s_baseday;
}

int32_t
basedate_set_day(
	int32_t day
	)
{
	struct calendar	jd;
	int32_t		retv;

	/* set NTP base date for NTP era unfolding */
	if (day < NTP_TO_UNIX_DAYS) {
		msyslog(LOG_WARNING,
			"baseday_set_day: invalid day (%lu), UNIX epoch substituted",
			(unsigned long)day);
		day = NTP_TO_UNIX_DAYS;
	}
	retv = s_baseday;
	s_baseday = day;
	ntpcal_rd_to_date(&jd, day + DAY_NTP_STARTS);
	msyslog(LOG_INFO, "basedate set to %04hu-%02hu-%02hu",
		jd.year, (u_short)jd.month, (u_short)jd.monthday);

	/* set GPS base week for GPS week unfolding */
	day = ntpcal_weekday_ge(day + DAY_NTP_STARTS, CAL_SUNDAY)
	    - DAY_NTP_STARTS;
	if (day < NTP_TO_GPS_DAYS)
	    day = NTP_TO_GPS_DAYS;
	s_gpsweek = (day - NTP_TO_GPS_DAYS) / DAYSPERWEEK;
	ntpcal_rd_to_date(&jd, day + DAY_NTP_STARTS);
	msyslog(LOG_INFO, "gps base set to %04hu-%02hu-%02hu (week %d)",
		jd.year, (u_short)jd.month, (u_short)jd.monthday, s_gpsweek);

	return retv;
}

time_t
basedate_get_eracenter(void)
{
	time_t retv;
	retv  = (time_t)(s_baseday - NTP_TO_UNIX_DAYS);
	retv *= SECSPERDAY;
	retv += (UINT32_C(1) << 31);
	return retv;
}

time_t
basedate_get_erabase(void)
{
	time_t retv;
	retv  = (time_t)(s_baseday - NTP_TO_UNIX_DAYS);
	retv *= SECSPERDAY;
	return retv;
}

uint32_t
basedate_get_gpsweek(void)
{
    return s_gpsweek;
}

uint32_t
basedate_expand_gpsweek(
    unsigned short weekno
    )
{
    /* We do a fast modulus expansion here. Since all quantities are
     * unsigned and we cannot go before the start of the GPS epoch
     * anyway, and since the truncated GPS week number is 10 bit, the
     * expansion becomes a simple sub/and/add sequence.
     */
    #if GPSWEEKS != 1024
    # error GPSWEEKS defined wrong -- should be 1024!
    #endif

    uint32_t diff;
    diff = ((uint32_t)weekno - s_gpsweek) & (GPSWEEKS - 1);
    return s_gpsweek + diff;
}

/*
 * ====================================================================
 * misc. helpers
 * ====================================================================
 */

/* --------------------------------------------------------------------
 * reconstruct the centrury from a truncated date and a day-of-week
 *
 * Given a date with truncated year (2-digit, 0..99) and a day-of-week
 * from 1(Mon) to 7(Sun), recover the full year between 1900AD and 2300AD.
 */
int32_t
ntpcal_expand_century(
	uint32_t y,
	uint32_t m,
	uint32_t d,
	uint32_t wd)
{
	/* This algorithm is short but tricky... It's related to
	 * Zeller's congruence, partially done backwards.
	 *
	 * A few facts to remember:
	 *  1) The Gregorian calendar has a cycle of 400 years.
	 *  2) The weekday of the 1st day of a century shifts by 5 days
	 *     during a great cycle.
	 *  3) For calendar math, a century starts with the 1st year,
	 *     which is year 1, !not! zero.
	 *
	 * So we start with taking the weekday difference (mod 7)
	 * between the truncated date (which is taken as an absolute
	 * date in the 1st century in the proleptic calendar) and the
	 * weekday given.
	 *
	 * When dividing this residual by 5, we obtain the number of
	 * centuries to add to the base. But since the residual is (mod
	 * 7), we have to make this an exact division by multiplication
	 * with the modular inverse of 5 (mod 7), which is 3:
	 *    3*5 === 1 (mod 7).
	 *
	 * If this yields a result of 4/5/6, the given date/day-of-week
	 * combination is impossible, and we return zero as resulting
	 * year to indicate failure.
	 *
	 * Then we remap the century to the range starting with year
	 * 1900.
	 */

	uint32_t c;

	/* check basic constraints */
	if ((y >= 100u) || (--m >= 12u) || (--d >= 31u))
		return 0;

	if ((m += 10u) >= 12u)		/* shift base to prev. March,1st */
		m -= 12u;
	else if (--y >= 100u)
		y += 100u;
	d += y + (y >> 2) + 2u;		/* year share */
	d += (m * 83u + 16u) >> 5;	/* month share */

	/* get (wd - d), shifted to positive value, and multiply with
	 * 3(mod 7). (Exact division, see to comment)
	 * Note: 1) d <= 184 at this point.
	 *	 2) 252 % 7 == 0, but 'wd' is off by one since we did
	 *	    '--d' above, so we add just 251 here!
	 */
	c = u32mod7(3 * (251u + wd - d));
	if (c > 3u)
		return 0;

	if ((m > 9u) && (++y >= 100u)) {/* undo base shift */
		y -= 100u;
		c = (c + 1) & 3u;
	}
	y += (c * 100u);		/* combine into 1st cycle */
	y += (y < 300u) ? 2000 : 1600;	/* map to destination era */
	return (int)y;
}

char *
ntpcal_iso8601std(
	char *		buf,
	size_t		len,
	TcCivilDate *	cdp
	)
{
	if (!buf) {
		LIB_GETBUF(buf);
		len = LIB_BUFLENGTH;
	}
	if (len) {
		len = snprintf(buf, len, "%04u-%02u-%02uT%02u:%02u:%02u",
			       cdp->year, cdp->month, cdp->monthday,
			       cdp->hour, cdp->minute, cdp->second);
		if (len < 0)
			*buf = '\0';
	}
	return buf;
}

/* -*-EOF-*- */
