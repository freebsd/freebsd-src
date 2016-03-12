#include "config.h"

#include "ntp_types.h"
#include "ntp_fp.h"
#include "timespecops.h"

#include "unity.h"

#include <math.h>
#include <string.h>


#define TEST_ASSERT_EQUAL_timespec(a, b) {				\
    TEST_ASSERT_EQUAL_MESSAGE(a.tv_sec, b.tv_sec, "Field tv_sec");	\
    TEST_ASSERT_EQUAL_MESSAGE(a.tv_nsec, b.tv_nsec, "Field tv_nsec");	\
}


#define TEST_ASSERT_EQUAL_l_fp(a, b) {					\
    TEST_ASSERT_EQUAL_MESSAGE(a.l_i, b.l_i, "Field l_i");		\
    TEST_ASSERT_EQUAL_UINT_MESSAGE(a.l_uf, b.l_uf, "Field l_uf");	\
}


static u_int32 my_tick_to_tsf(u_int32 ticks);
static u_int32 my_tsf_to_tick(u_int32 tsf);


// that's it...
struct lfpfracdata {
	long	nsec;
	u_int32 frac;
};


void setUp(void);
void test_Helpers1(void);
void test_Normalise(void);
void test_SignNoFrac(void);
void test_SignWithFrac(void);
void test_CmpFracEQ(void);
void test_CmpFracGT(void);
void test_CmpFracLT(void);
void test_AddFullNorm(void);
void test_AddFullOflow1(void);
void test_AddNsecNorm(void);
void test_AddNsecOflow1(void);
void test_SubFullNorm(void);
void test_SubFullOflow(void);
void test_SubNsecNorm(void);
void test_SubNsecOflow(void);
void test_Neg(void);
void test_AbsNoFrac(void);
void test_AbsWithFrac(void);
void test_Helpers2(void);
void test_ToLFPbittest(void);
void test_ToLFPrelPos(void);
void test_ToLFPrelNeg(void);
void test_ToLFPabs(void);
void test_FromLFPbittest(void);
void test_FromLFPrelPos(void);
void test_FromLFPrelNeg(void);
void test_LFProundtrip(void);
void test_ToString(void);

typedef int bool;

const bool	timespec_isValid(struct timespec V);
struct timespec timespec_init(time_t hi, long lo);
l_fp		l_fp_init(int32 i, u_int32 f);
bool		AssertFpClose(const l_fp m, const l_fp n, const l_fp limit);
bool		AssertTimespecClose(const struct timespec m,
				    const struct timespec n,
				    const struct timespec limit);


//***************************MY CUSTOM FUNCTIONS***************************


void
setUp(void)
{
	init_lib();

	return;
}


const bool
timespec_isValid(struct timespec V)
{

	return V.tv_nsec >= 0 && V.tv_nsec < 1000000000;
}


struct timespec
timespec_init(time_t hi, long lo)
{
	struct timespec V;

	V.tv_sec = hi;
	V.tv_nsec = lo;

	return V;
}


l_fp
l_fp_init(int32 i, u_int32 f)
{
	l_fp temp;

	temp.l_i  = i;
	temp.l_uf = f;

	return temp;
}


bool
AssertFpClose(const l_fp m, const l_fp n, const l_fp limit)
{
	l_fp diff;

	if (L_ISGEQ(&m, &n)) {
		diff = m;
		L_SUB(&diff, &n);
	} else {
		diff = n;
		L_SUB(&diff, &m);
	}
	if (L_ISGEQ(&limit, &diff)) {
		return TRUE;
	}
	else {
		printf("m_expr which is %s \nand\nn_expr which is %s\nare not close; diff=%susec\n", lfptoa(&m, 10), lfptoa(&n, 10), lfptoa(&diff, 10)); 
		return FALSE;
	}
}


bool
AssertTimespecClose(const struct timespec m, const struct timespec n,
	const struct timespec limit)
{
	struct timespec diff;

	diff = abs_tspec(sub_tspec(m, n));
	if (cmp_tspec(limit, diff) >= 0)
		return TRUE;
	else
	{
		printf("m_expr which is %ld.%lu \nand\nn_expr which is %ld.%lu\nare not close; diff=%ld.%lunsec\n", m.tv_sec, m.tv_nsec, n.tv_sec, n.tv_nsec, diff.tv_sec, diff.tv_nsec); 
		return FALSE;
	}
}

//-----------------------------------------------

static const struct lfpfracdata fdata[] = {
	{	  0, 0x00000000 }, {   2218896, 0x00916ae6 },
	{  16408100, 0x0433523d }, { 125000000, 0x20000000 },
	{ 250000000, 0x40000000 }, { 287455871, 0x4996b53d },
	{ 375000000, 0x60000000 }, { 500000000, 0x80000000 },
	{ 518978897, 0x84dbcd0e }, { 563730222, 0x90509fb3 },
	{ 563788007, 0x9054692c }, { 583289882, 0x95527c57 },
	{ 607074509, 0x9b693c2a }, { 625000000, 0xa0000000 },
	{ 645184059, 0xa52ac851 }, { 676497788, 0xad2ef583 },
	{ 678910895, 0xadcd1abb }, { 679569625, 0xadf84663 },
	{ 690926741, 0xb0e0932d }, { 705656483, 0xb4a5e73d },
	{ 723553854, 0xb93ad34c }, { 750000000, 0xc0000000 },
	{ 763550253, 0xc3780785 }, { 775284917, 0xc6791284 },
	{ 826190764, 0xd3813ce8 }, { 875000000, 0xe0000000 },
	{ 956805507, 0xf4f134a9 }, { 982570733, 0xfb89c16c }
	};


u_int32
my_tick_to_tsf(u_int32 ticks)
{
	// convert nanoseconds to l_fp fractional units, using double
	// precision float calculations or, if available, 64bit integer
	// arithmetic. This should give the precise fraction, rounded to
	// the nearest representation.

#ifdef HAVE_U_INT64
	return (u_int32)((( ((u_int64)(ticks)) << 32) + 500000000) / 1000000000);
#else
	return (u_int32)((double(ticks)) * 4.294967296 + 0.5);
#endif
	// And before you ask: if ticks >= 1000000000, the result is
	// truncated nonsense, so don't use it out-of-bounds.
}


u_int32
my_tsf_to_tick(u_int32 tsf)
{

	// Inverse operation: converts fraction to microseconds.
#ifdef HAVE_U_INT64
	return (u_int32)(( ((u_int64)(tsf)) * 1000000000 + 0x80000000) >> 32);
#else
	return (u_int32)(double(tsf) / 4.294967296 + 0.5);
#endif
	// Beware: The result might be 10^9 due to rounding!
}



// ---------------------------------------------------------------------
// test support stuff -- part 1
// ---------------------------------------------------------------------

void
test_Helpers1(void)
{
	struct timespec x;

	for (x.tv_sec = -2; x.tv_sec < 3; x.tv_sec++) {
		x.tv_nsec = -1;
		TEST_ASSERT_FALSE(timespec_isValid(x));
		x.tv_nsec = 0;
		TEST_ASSERT_TRUE(timespec_isValid(x));
		x.tv_nsec = 999999999;
		TEST_ASSERT_TRUE(timespec_isValid(x));
		x.tv_nsec = 1000000000;
		TEST_ASSERT_FALSE(timespec_isValid(x));
	}

	return;
}


//----------------------------------------------------------------------
// test normalisation
//----------------------------------------------------------------------

void
test_Normalise(void)
{
	long ns;

	for ( ns = -2000000000; ns <= 2000000000; ns += 10000000) {
		struct timespec x = timespec_init(0, ns);

		x = normalize_tspec(x);
		TEST_ASSERT_TRUE(timespec_isValid(x));
	}

	return;
}

//----------------------------------------------------------------------
// test classification
//----------------------------------------------------------------------

void
test_SignNoFrac(void)
{
	// sign test, no fraction
	int i;

	for (i = -4; i <= 4; ++i) {
		struct timespec a = timespec_init(i, 0);
		int E = (i > 0) - (i < 0);
		int r = test_tspec(a);

		TEST_ASSERT_EQUAL(E, r);
	}

	return;
}


void
test_SignWithFrac(void)
{
	// sign test, with fraction
	int i;

	for (i = -4; i <= 4; ++i) {
		struct timespec a = timespec_init(i, 10);
		int E = (i >= 0) - (i < 0);
		int r = test_tspec(a);

		TEST_ASSERT_EQUAL(E, r);
	}

	return;
}

//----------------------------------------------------------------------
// test compare
//----------------------------------------------------------------------
void
test_CmpFracEQ(void)
{
	// fractions are equal
	int i, j;
	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timespec a = timespec_init( i , 200);
			struct timespec b = timespec_init( j , 200);
			int   E = (i > j) - (i < j);
			int   r = cmp_tspec_denorm(a, b);

			TEST_ASSERT_EQUAL(E, r);
		}

	return;
}


void
test_CmpFracGT(void)
{
	// fraction a bigger fraction b
	int i, j;

	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timespec a = timespec_init(i, 999999800);
			struct timespec b = timespec_init(j, 200);
			int   E = (i >= j) - (i < j);
			int   r = cmp_tspec_denorm(a, b);

			TEST_ASSERT_EQUAL(E, r);
		}

	return;
}


void
test_CmpFracLT(void)
{
	// fraction a less fraction b
	int i, j;

	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timespec a = timespec_init(i, 200);
			struct timespec b = timespec_init(j, 999999800);
			int   E = (i > j) - (i <= j);
			int   r = cmp_tspec_denorm(a, b);

			TEST_ASSERT_EQUAL(E, r);
		}

	return;
}

//----------------------------------------------------------------------
// Test addition (sum)
//----------------------------------------------------------------------

void
test_AddFullNorm(void)
{
	int i, j;

	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timespec a = timespec_init(i, 200);
			struct timespec b = timespec_init(j, 400);
			struct timespec E = timespec_init(i + j, 200 + 400);
			struct timespec c;

			c = add_tspec(a, b);
			TEST_ASSERT_EQUAL_timespec(E, c);
		}

	return;
}


void
test_AddFullOflow1(void)
{
	int i, j;

	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timespec a = timespec_init(i, 200);
			struct timespec b = timespec_init(j, 999999900);
			struct timespec E = timespec_init(i + j + 1, 100);
			struct timespec c;

			c = add_tspec(a, b);
			TEST_ASSERT_EQUAL_timespec(E, c);
		}

	return;
}


void
test_AddNsecNorm(void) {
	int i;

	for (i = -4; i <= 4; ++i) {
		struct timespec a = timespec_init(i, 200);
		struct timespec E = timespec_init(i, 600);
		struct timespec c;

		c = add_tspec_ns(a, 600 - 200);
		TEST_ASSERT_EQUAL_timespec(E, c);
	}

	return;
}


void
test_AddNsecOflow1(void)
{
	int i;

	for (i = -4; i <= 4; ++i) {
		struct timespec a = timespec_init(i, 200);
		struct timespec E = timespec_init(i + 1, 100);
		struct timespec c;

		c = add_tspec_ns(a, NANOSECONDS - 100);
		TEST_ASSERT_EQUAL_timespec(E, c);
	}

	return;
}

//----------------------------------------------------------------------
// test subtraction (difference)
//----------------------------------------------------------------------

void
test_SubFullNorm(void)
{
	int i, j;

	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timespec a = timespec_init( i , 600);
			struct timespec b = timespec_init( j , 400);
			struct timespec E = timespec_init(i-j, 200);
			struct timespec c;

			c = sub_tspec(a, b);
			TEST_ASSERT_EQUAL_timespec(E, c);
		}

	return;
}


void
test_SubFullOflow(void)
{
	int i, j;

	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timespec a = timespec_init(i, 100);
			struct timespec b = timespec_init(j, 999999900);
			struct timespec E = timespec_init(i - j - 1, 200);
			struct timespec c;

			c = sub_tspec(a, b);
			TEST_ASSERT_EQUAL_timespec(E, c);
		}

	return;
}


void
test_SubNsecNorm(void)
{
	int i;

	for (i = -4; i <= 4; ++i) {
		struct timespec a = timespec_init(i, 600);
		struct timespec E = timespec_init(i, 200);
		struct timespec c;

		c = sub_tspec_ns(a, 600 - 200);
		TEST_ASSERT_EQUAL_timespec(E, c);
	}

	return;
}


void
test_SubNsecOflow(void)
{
	int i;

	for (i = -4; i <= 4; ++i) {
		struct timespec a = timespec_init( i , 100);
		struct timespec E = timespec_init(i-1, 200);
		struct timespec c;

		c = sub_tspec_ns(a, NANOSECONDS - 100);
		TEST_ASSERT_EQUAL_timespec(E, c);
	}

	return;
}

//----------------------------------------------------------------------
// test negation
//----------------------------------------------------------------------


void
test_Neg(void)
{
	int i;

	for (i = -4; i <= 4; ++i) {
		struct timespec a = timespec_init(i, 100);
		struct timespec b;
		struct timespec c;

		b = neg_tspec(a);
		c = add_tspec(a, b);
		TEST_ASSERT_EQUAL(0, test_tspec(c));
	}

	return;
}

//----------------------------------------------------------------------
// test abs value
//----------------------------------------------------------------------

void
test_AbsNoFrac(void)
{
	int i;

	for (i = -4; i <= 4; ++i) {
		struct timespec a = timespec_init(i , 0);
		struct timespec b;

		b = abs_tspec(a);
		TEST_ASSERT_EQUAL((i != 0), test_tspec(b));
	}

	return;
}


void
test_AbsWithFrac(void)
{
	int i;

	for (i = -4; i <= 4; ++i) {
		struct timespec a = timespec_init(i, 100);
		struct timespec b;

		b = abs_tspec(a);
		TEST_ASSERT_EQUAL(1, test_tspec(b));
	}

	return;
}

// ---------------------------------------------------------------------
// test support stuff -- part 2
// ---------------------------------------------------------------------

void
test_Helpers2(void)
{
	struct timespec limit = timespec_init(0, 2);
	struct timespec x, y;
	long i;

	for (x.tv_sec = -2; x.tv_sec < 3; x.tv_sec++)
		for (x.tv_nsec = 1;
		     x.tv_nsec < 1000000000;
		     x.tv_nsec += 499999999) {
			for (i = -4; i < 5; ++i) {
				y = x;
				y.tv_nsec += i;
				if (i >= -2 && i <= 2) {
					TEST_ASSERT_TRUE(AssertTimespecClose(x, y, limit));
				}
				else
				{
					TEST_ASSERT_FALSE(AssertTimespecClose(x, y, limit));
				}
			}
		}

	return;
}

//----------------------------------------------------------------------
// conversion to l_fp
//----------------------------------------------------------------------

void
test_ToLFPbittest(void)
{
	l_fp lfpClose =  l_fp_init(0, 1);
	u_int32 i;

	for (i = 0; i < 1000000000; i+=1000) {
		struct timespec a = timespec_init(1, i);
		l_fp E= l_fp_init(1, my_tick_to_tsf(i));
		l_fp r;

		r = tspec_intv_to_lfp(a);
		TEST_ASSERT_TRUE(AssertFpClose(E, r, lfpClose));
	}

	return;
}


void
test_ToLFPrelPos(void)
{
	int i;

	for (i = 0; i < COUNTOF(fdata); ++i) {
		struct timespec a = timespec_init(1, fdata[i].nsec);
		l_fp E = l_fp_init(1, fdata[i].frac);
		l_fp r;

		r = tspec_intv_to_lfp(a);
		TEST_ASSERT_EQUAL_l_fp(E, r);
	}

	return;
}


void
test_ToLFPrelNeg(void)
{
	int i;

	for (i = 0; i < COUNTOF(fdata); ++i) {
		struct timespec a = timespec_init(-1, fdata[i].nsec);
		l_fp E = l_fp_init(~0, fdata[i].frac);
		l_fp r;

		r = tspec_intv_to_lfp(a);
		TEST_ASSERT_EQUAL_l_fp(E, r);
	}

	return;
}


void
test_ToLFPabs(void)
{
	int i;

	for (i = 0; i < COUNTOF(fdata); ++i) {
		struct timespec a = timespec_init(1, fdata[i].nsec);
		l_fp E = l_fp_init(1 + JAN_1970, fdata[i].frac);
		l_fp r;

		r = tspec_stamp_to_lfp(a);
		TEST_ASSERT_EQUAL_l_fp(E, r);
	}

	return;
}

//----------------------------------------------------------------------
// conversion from l_fp
//----------------------------------------------------------------------

void
test_FromLFPbittest(void)
{
	struct timespec limit = timespec_init(0, 2);

	// Not *exactly* a bittest, because 2**32 tests would take a
	// really long time even on very fast machines! So we do test
	// every 1000 fractional units.
	u_int32 tsf;
	for (tsf = 0; tsf < ~((u_int32)(1000)); tsf += 1000) {
		struct timespec E = timespec_init(1, my_tsf_to_tick(tsf));
		l_fp a = l_fp_init(1, tsf);
		struct timespec r;

		r = lfp_intv_to_tspec(a);
		// The conversion might be off by one nanosecond when
		// comparing to calculated value.
		TEST_ASSERT_TRUE(AssertTimespecClose(E, r, limit));
	}

	return;
}


void
test_FromLFPrelPos(void)
{
	struct timespec limit = timespec_init(0, 2);
	int i;

	for (i = 0; i < COUNTOF(fdata); ++i) {
		l_fp a = l_fp_init(1, fdata[i].frac);
		struct timespec E = timespec_init(1, fdata[i].nsec);
		struct timespec r;

		r = lfp_intv_to_tspec(a);
		TEST_ASSERT_TRUE(AssertTimespecClose(E, r, limit));
	}

	return;
}


void
test_FromLFPrelNeg(void)
{
	struct timespec limit = timespec_init(0, 2);
	int i;

	for (i = 0; i < COUNTOF(fdata); ++i) {
		l_fp a = l_fp_init(~0, fdata[i].frac);
		struct timespec E = timespec_init(-1, fdata[i].nsec);
		struct timespec r;

		r = lfp_intv_to_tspec(a);
		TEST_ASSERT_TRUE(AssertTimespecClose(E, r, limit));
	}

	return;
}


// nsec -> frac -> nsec roundtrip, using a prime start and increment
void
test_LFProundtrip(void)
{
	int32_t t;
	u_int32 i;

	for (t = -1; t < 2; ++t)
		for (i = 4999; i < 1000000000; i += 10007) {
			struct timespec E = timespec_init(t, i);
			l_fp a;
			struct timespec r;

			a = tspec_intv_to_lfp(E);
			r = lfp_intv_to_tspec(a);
			TEST_ASSERT_EQUAL_timespec(E, r);
		}

	return;
}

//----------------------------------------------------------------------
// string formatting
//----------------------------------------------------------------------

void
test_ToString(void)
{
	static const struct {
		time_t		sec;
		long		nsec;
		const char *	repr;
	} data [] = {
		{ 0, 0,	 "0.000000000" },
		{ 2, 0,	 "2.000000000" },
		{-2, 0, "-2.000000000" },
		{ 0, 1,	 "0.000000001" },
		{ 0,-1,	"-0.000000001" },
		{ 1,-1,	 "0.999999999" },
		{-1, 1, "-0.999999999" },
		{-1,-1, "-1.000000001" },
	};
	int i;

	for (i = 0; i < COUNTOF(data); ++i) {
		struct timespec a = timespec_init(data[i].sec, data[i].nsec);
		const char * E = data[i].repr;
		const char * r = tspectoa(a);
		TEST_ASSERT_EQUAL_STRING(E, r);
	}

	return;
}

// -*- EOF -*-
