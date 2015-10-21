#include "config.h"

//some unused features are still in the wrapper, unconverted

#include "ntp_types.h"
#include "ntp_fp.h"

#include <math.h>
#include "timevalops.h"

#include "unity.h"


//in unity_helper.h :
#define TEST_ASSERT_EQUAL_timeval(a, b) { \
    TEST_ASSERT_EQUAL_MESSAGE(a.tv_sec, b.tv_sec, "Field tv_sec"); \
    TEST_ASSERT_EQUAL_MESSAGE(a.tv_usec, b.tv_usec, "Field tv_usec");	\
}

//timeval has time_t, long, and time_t is basically uint

static u_int32 my_tick_to_tsf(u_int32 ticks);
static u_int32 my_tsf_to_tick(u_int32 tsf);

// that's it...
typedef struct {
	long	usec;
	u_int32	frac;
} lfpfracdata ;


//******************************************MY CUSTOM FUNCTIONS*******************************

typedef int bool; //TRUE and FALSE are already defined somewhere, so I can't do typedef enum { FALSE, TRUE } boolean;

struct timeval timeval_init( time_t hi, long lo){
	struct timeval V; 
	V.tv_sec = hi; 
	V.tv_usec = lo;
	return V;
}

const bool timeval_isValid(struct timeval V)
	{ return V.tv_usec >= 0 && V.tv_usec < 1000000; }

//taken from lfpfunc.c -> maybe remove this from timevalops.c and lfpfunc. and put in c_timstructs.h ????!!!!!
l_fp l_fp_init(int32 i, u_int32 f)
{
	l_fp temp;
	temp.l_i  = i;
	temp.l_uf = f;

	return temp;
}

bool AssertTimevalClose(const struct timeval m, const struct timeval n, const struct timeval limit)
{
	struct timeval diff;

	diff = abs_tval(sub_tval(m, n));
	if (cmp_tval(limit, diff) >= 0)
		return TRUE;
	
	else 
	{
		//printf("");
		//<< m_expr << " which is " << timeval_wrap(m)
		//<< "\nand\n"
		//<< n_expr << " which is " << timeval_wrap(n)
		//<< "\nare not close; diff=" << timeval_wrap(diff);
		return FALSE;
	}
}

bool AssertFpClose(const l_fp m,const l_fp n, const l_fp limit)
{
	l_fp diff;

	if (L_ISGEQ(&m, &n)) {
		diff = m;
		L_SUB(&diff, &n);
	} else {
		diff = n;
		L_SUB(&diff, &m);
	}
	if (L_ISGEQ(&limit, &diff)){
		return TRUE;
	}
	else {
		//<< m_expr << " which is " << l_fp_wrap(m)
		//<< "\nand\n"
		//<< n_expr << " which is " << l_fp_wrap(n)
		//<< "\nare not close; diff=" << l_fp_wrap(diff);
		return FALSE;
	}
}


//---------------------------------------------------

static const lfpfracdata fdata[] = {
	{      0, 0x00000000 }, {   7478, 0x01ea1405 },
	{  22077, 0x05a6d699 }, { 125000, 0x20000000 },
	{ 180326, 0x2e29d841 }, { 207979, 0x353e1c9b },
	{ 250000, 0x40000000 }, { 269509, 0x44fe8ab5 },
	{ 330441, 0x5497c808 }, { 333038, 0x5541fa76 },
	{ 375000, 0x60000000 }, { 394734, 0x650d4995 },
	{ 446327, 0x72427c7c }, { 500000, 0x80000000 },
	{ 517139, 0x846338b4 }, { 571953, 0x926b8306 },
	{ 587353, 0x965cc426 }, { 625000, 0xa0000000 },
	{ 692136, 0xb12fd32c }, { 750000, 0xc0000000 },
	{ 834068, 0xd5857aff }, { 848454, 0xd9344806 },
	{ 854222, 0xdaae4b02 }, { 861465, 0xdc88f862 },
	{ 875000, 0xe0000000 }, { 910661, 0xe921144d },
	{ 922162, 0xec12cf10 }, { 942190, 0xf1335d25 }
};


u_int32 my_tick_to_tsf(u_int32 ticks)
{
	// convert microseconds to l_fp fractional units, using double
	// precision float calculations or, if available, 64bit integer
	// arithmetic. This should give the precise fraction, rounded to
	// the nearest representation.
#ifdef HAVE_U_INT64
	return (u_int32)((( ((u_int64)(ticks)) << 32) + 500000) / 1000000); //I put too much () when casting just to be safe
#else
	return (u_int32)( ((double)(ticks)) * 4294.967296 + 0.5);
#endif
	// And before you ask: if ticks >= 1000000, the result is
	// truncated nonsense, so don't use it out-of-bounds.
}

u_int32 my_tsf_to_tick(u_int32 tsf)
{
	// Inverse operation: converts fraction to microseconds.
#ifdef HAVE_U_INT64
	return (u_int32)( ((u_int64)(tsf) * 1000000 + 0x80000000) >> 32); //CHECK ME!!!
#else
	return (u_int32)(double(tsf) / 4294.967296 + 0.5);
#endif
	// Beware: The result might be 10^6 due to rounding!
}


//***************************************END OF CUSTOM FUNCTIONS*****************************


// ---------------------------------------------------------------------
// test support stuff - part1
// ---------------------------------------------------------------------

void test_Helpers1() {
	struct timeval x;

	for (x.tv_sec = -2; x.tv_sec < 3; x.tv_sec++) {
		x.tv_usec = -1;
		TEST_ASSERT_FALSE(timeval_isValid(x));
		x.tv_usec = 0;
		TEST_ASSERT_TRUE(timeval_isValid(x));
		x.tv_usec = 999999;
		TEST_ASSERT_TRUE(timeval_isValid(x));
		x.tv_usec = 1000000;
		TEST_ASSERT_FALSE(timeval_isValid(x));
	}
}


//----------------------------------------------------------------------
// test normalisation
//----------------------------------------------------------------------

void test_Normalise() {
	long ns;
	for (ns = -2000000000; ns <= 2000000000; ns += 10000000) {
		struct timeval x = timeval_init(0, ns);
		
		x = normalize_tval(x);
		TEST_ASSERT_TRUE(timeval_isValid(x));
	}
}

//----------------------------------------------------------------------
// test classification
//----------------------------------------------------------------------

void test_SignNoFrac() {
	int i;
	// sign test, no fraction
	for (i = -4; i <= 4; ++i) {
		struct timeval a = timeval_init(i, 0);
		int	     E = (i > 0) - (i < 0);
		int	     r = test_tval(a);

		TEST_ASSERT_EQUAL(E, r);
	}
}

void test_SignWithFrac() {
	// sign test, with fraction
	int i;
	for (i = -4; i <= 4; ++i) {
		struct timeval a = timeval_init(i, 10);
		int	     E = (i >= 0) - (i < 0);
		int	     r = test_tval(a);

		TEST_ASSERT_EQUAL(E, r);
	}
}

//----------------------------------------------------------------------
// test compare
//----------------------------------------------------------------------
void test_CmpFracEQ() {
	int i,j;
	// fractions are equal
	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timeval a = timeval_init(i, 200);
			struct timeval b = timeval_init(j, 200);
			int	     E = (i > j) - (i < j);
			int	     r = cmp_tval_denorm(a, b);

			TEST_ASSERT_EQUAL(E, r);
		}
}

void test_CmpFracGT() {
	// fraction a bigger fraction b
	int i,j;
	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timeval a = timeval_init( i , 999800);
			struct timeval b = timeval_init( j , 200);
			int	     E = (i >= j) - (i < j);
			int	     r = cmp_tval_denorm(a, b);

			TEST_ASSERT_EQUAL(E, r);
		}
}

void test_CmpFracLT() {
	// fraction a less fraction b
	int i,j;
	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timeval a = timeval_init(i, 200);
			struct timeval b = timeval_init(j, 999800);
			int	     E = (i > j) - (i <= j);
			int	     r = cmp_tval_denorm(a, b);

			TEST_ASSERT_EQUAL(E, r);
		}
}

//----------------------------------------------------------------------
// Test addition (sum)
//----------------------------------------------------------------------

void test_AddFullNorm() {
	int i,j;
	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timeval a = timeval_init(i, 200);
			struct timeval b = timeval_init(j, 400);
			struct timeval E = timeval_init(i + j, 200 + 400);
			struct timeval c;

			c = add_tval(a, b);
			TEST_ASSERT_EQUAL_timeval(E, c);
		}
}

void test_AddFullOflow1() {
	int i,j;
	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timeval a = timeval_init(i, 200);
			struct timeval b = timeval_init(j, 999900);
			struct timeval E = timeval_init(i + j + 1, 100);
			struct timeval c;

			c = add_tval(a, b);
			TEST_ASSERT_EQUAL_timeval(E, c);
		}
}

void test_AddUsecNorm() {
	int i;
	for (i = -4; i <= 4; ++i) {
		struct timeval a = timeval_init(i, 200);
		struct timeval E = timeval_init(i, 600);
		struct timeval c;

		c = add_tval_us(a, 600 - 200);
		TEST_ASSERT_EQUAL_timeval(E, c);
	}
}

void test_AddUsecOflow1() {
	int i;
	for (i = -4; i <= 4; ++i) {
		struct timeval a = timeval_init(i, 200);
		struct timeval E = timeval_init(i + 1, 100);
		struct timeval c;

		c = add_tval_us(a, MICROSECONDS - 100);
		TEST_ASSERT_EQUAL_timeval(E, c);
	}
}

//----------------------------------------------------------------------
// test subtraction (difference)
//----------------------------------------------------------------------

void test_SubFullNorm() {
	int i,j;
	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timeval a = timeval_init(i, 600);
			struct timeval b = timeval_init(j, 400);
			struct timeval E = timeval_init(i - j, 600 - 400);
			struct timeval c;

			c = sub_tval(a, b);
			TEST_ASSERT_EQUAL_timeval(E, c);
		}
}

void test_SubFullOflow() {
	int i,j;
	for (i = -4; i <= 4; ++i)
		for (j = -4; j <= 4; ++j) {
			struct timeval a = timeval_init(i, 100);
			struct timeval b = timeval_init(j, 999900);
			struct timeval E = timeval_init(i - j - 1, 200);
			struct timeval c;

			c = sub_tval(a, b);
			TEST_ASSERT_EQUAL_timeval(E, c);
		}
}

void test_SubUsecNorm() {
	int i = -4;
	for (i = -4; i <= 4; ++i) {
		struct timeval a = timeval_init(i, 600);
		struct timeval E = timeval_init(i, 200);
		struct timeval c;

		c = sub_tval_us(a, 600 - 200);
		TEST_ASSERT_EQUAL_timeval(E, c);
	}
}

void test_SubUsecOflow() {
	int i = -4;
	for (i = -4; i <= 4; ++i) {
		struct timeval a = timeval_init(i, 100);
		struct timeval E = timeval_init(i - 1, 200);
		struct timeval c;

		c = sub_tval_us(a, MICROSECONDS - 100);
		TEST_ASSERT_EQUAL_timeval(E, c);
	}
}

//----------------------------------------------------------------------
// test negation
//----------------------------------------------------------------------

void test_Neg() {
	int i = -4;
	for (i = -4; i <= 4; ++i) {
		struct timeval a = timeval_init(i, 100);
		struct timeval b;
		struct timeval c;

		b = neg_tval(a);
		c = add_tval(a, b);
		TEST_ASSERT_EQUAL(0, test_tval(c));
	}
}

//----------------------------------------------------------------------
// test abs value
//----------------------------------------------------------------------

void test_AbsNoFrac() {
	int i = -4;
	for (i = -4; i <= 4; ++i) {
		struct timeval a = timeval_init(i, 0);
		struct timeval b;

		b = abs_tval(a);
		TEST_ASSERT_EQUAL((i != 0), test_tval(b));
	}
}

void test_AbsWithFrac() {
	int i = -4;
	for (i = -4; i <= 4; ++i) {
		struct timeval a = timeval_init(i, 100);
		struct timeval b;

		b = abs_tval(a);
		TEST_ASSERT_EQUAL(1, test_tval(b));
	}
}

// ---------------------------------------------------------------------
// test support stuff -- part 2
// ---------------------------------------------------------------------


void test_Helpers2() {
	//struct AssertTimevalClose isClose = AssertTimevalClose_init(0, 2);
	struct timeval limit = timeval_init(0, 2);
	struct timeval x, y;
	long i;	

	for (x.tv_sec = -2; x.tv_sec < 3; x.tv_sec++){
		for (x.tv_usec = 1;
		     x.tv_usec < 1000000;
		     x.tv_usec += 499999) {
			for (i = -4; i < 5; i++) {
				y = x;
				y.tv_usec += i;
				if (i >= -2 && i <= 2){
					TEST_ASSERT_TRUE(AssertTimevalClose(x,y,limit));//ASSERT_PRED_FORMAT2(isClose, x, y);
				}
				else {
					TEST_ASSERT_FALSE(AssertTimevalClose(x,y,limit));//ASSERT_PRED_FORMAT2(!isClose, x, y);
				}
			}
		}
	}
}

// and the global predicate instances we're using here

//static l_fp lfpClose =  l_fp_init(0,1); //static AssertFpClose FpClose(0, 1);
//static struct timeval timevalClose = timeval_init(0,1); //static AssertTimevalClose TimevalClose(0, 1);

//----------------------------------------------------------------------
// conversion to l_fp
//----------------------------------------------------------------------

void test_ToLFPbittest() {
	l_fp lfpClose =  l_fp_init(0,1);	

	u_int32 i = 0;
	for (i = 0; i < 1000000; i++) {
		struct timeval a = timeval_init(1, i);
		l_fp E = l_fp_init(1,my_tick_to_tsf(i));
		l_fp r;

		r = tval_intv_to_lfp(a);
		TEST_ASSERT_TRUE(AssertFpClose(E,r,lfpClose));	//ASSERT_PRED_FORMAT2(FpClose, E, r);
	}
}


void test_ToLFPrelPos() {
	l_fp lfpClose =  l_fp_init(0,1);

	int i = 0;
	for (i = 0; i < COUNTOF(fdata); i++) {
		struct timeval a = timeval_init(1, fdata[i].usec);
		l_fp E = l_fp_init(1, fdata[i].frac);
		l_fp r;

		r = tval_intv_to_lfp(a);
		TEST_ASSERT_TRUE(AssertFpClose(E,r,lfpClose)); //ASSERT_PRED_FORMAT2(FpClose, E, r);
	}
}

void test_ToLFPrelNeg() {
	l_fp lfpClose =  l_fp_init(0,1);
	int i = 0;
	for (i = 0; i < COUNTOF(fdata); i++) {
		struct timeval a = timeval_init(-1, fdata[i].usec);
		l_fp E = l_fp_init(~0, fdata[i].frac);
		l_fp    r;

		r = tval_intv_to_lfp(a);
		TEST_ASSERT_TRUE(AssertFpClose(E,r,lfpClose)); //ASSERT_PRED_FORMAT2(FpClose,E, r);
	}
}

void test_ToLFPabs() {
	l_fp lfpClose =  l_fp_init(0,1);

	int i = 0;
	for (i = 0; i < COUNTOF(fdata); i++) {
		struct timeval a = timeval_init(1, fdata[i].usec);
		l_fp E = l_fp_init(1 + JAN_1970, fdata[i].frac);
		l_fp    r;

		r = tval_stamp_to_lfp(a);
		TEST_ASSERT_TRUE(AssertFpClose(E,r,lfpClose)); //ASSERT_PRED_FORMAT2(FpClose, E, r);
	}
}

//----------------------------------------------------------------------
// conversion from l_fp
//----------------------------------------------------------------------

void test_FromLFPbittest() {
	struct timeval timevalClose = timeval_init(0,1);
	// Not *exactly* a bittest, because 2**32 tests would take a
	// really long time even on very fast machines! So we do test
	// every 1000 fractional units.
	u_int32 tsf = 0;
	for (tsf = 0; tsf < ~((u_int32)(1000)); tsf += 1000) {
		struct timeval E = timeval_init(1, my_tsf_to_tick(tsf));
		l_fp a = l_fp_init(1, tsf);
		struct timeval r;

		r = lfp_intv_to_tval(a);
		// The conversion might be off by one microsecond when
		// comparing to calculated value.
		TEST_ASSERT_TRUE(AssertTimevalClose(E,r,timevalClose)); //ASSERT_PRED_FORMAT2(TimevalClose, E, r);
	}
}

void test_FromLFPrelPos() {
	struct timeval timevalClose = timeval_init(0,1);
	int i = 0;	
	for (i = 0; i < COUNTOF(fdata); i++) {
		l_fp a = l_fp_init(1, fdata[i].frac);
		struct timeval E = timeval_init(1, fdata[i].usec);
		struct timeval r;

		r = lfp_intv_to_tval(a);
		TEST_ASSERT_TRUE(AssertTimevalClose(E,r,timevalClose)); //ASSERT_PRED_FORMAT2(TimevalClose, E, r);
	}
}

void test_FromLFPrelNeg() {
	struct timeval timevalClose = timeval_init(0,1);
	int i = 0;
	for (i = 0; i < COUNTOF(fdata); i++) {
		l_fp a = l_fp_init(~0, fdata[i].frac);
		struct timeval E = timeval_init(-1, fdata[i].usec);
		struct timeval r;

		r = lfp_intv_to_tval(a);
		TEST_ASSERT_TRUE(AssertTimevalClose(E,r,timevalClose)); //ASSERT_PRED_FORMAT2(TimevalClose, E, r);
	}
}

// usec -> frac -> usec roundtrip, using a prime start and increment
void test_LFProundtrip() {
	int32_t t = -1;
	u_int32 i = 5;
	for (t = -1; t < 2; ++t)
		for (i = 5; i < 1000000; i+=11) {
			struct timeval E = timeval_init(t, i);
			l_fp a;
			struct timeval r;

			a = tval_intv_to_lfp(E);
			r = lfp_intv_to_tval(a);
			TEST_ASSERT_EQUAL_timeval(E, r);
		}
}

//----------------------------------------------------------------------
// string formatting
//----------------------------------------------------------------------

void test_ToString() {
	static const struct {
		time_t	     sec;
		long	     usec;
		const char * repr;
	} data [] = {
		{ 0, 0,	 "0.000000" },
		{ 2, 0,	 "2.000000" },
		{-2, 0, "-2.000000" },
		{ 0, 1,	 "0.000001" },
		{ 0,-1,	"-0.000001" },
		{ 1,-1,	 "0.999999" },
		{-1, 1, "-0.999999" },
		{-1,-1, "-1.000001" },
	};
	int i;
	for (i = 0; i < COUNTOF(data); ++i) {
		struct timeval a = timeval_init(data[i].sec, data[i].usec);
		const char *  E = data[i].repr; //??
		const char *  r = tvaltoa(a);

		TEST_ASSERT_EQUAL_STRING(E, r);
	}
}

// -*- EOF -*-
