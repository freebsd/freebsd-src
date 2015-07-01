#include "ntpdtest.h"

extern "C" {
#include "ntp.h"
#include "ntp_calendar.h"
#include "ntp_leapsec.h"
}

#include <string>
#include <sstream>

static const char leap1 [] =
    "#\n"
    "#@ 	3610569600\n"
    "#\n"
    "2272060800 10	# 1 Jan 1972\n"
    "2287785600	11	# 1 Jul 1972\n"
    "2303683200	12	# 1 Jan 1973\n"
    "2335219200	13	# 1 Jan 1974\n"
    "2366755200	14	# 1 Jan 1975\n"
    "2398291200	15	# 1 Jan 1976\n"
    "2429913600	16	# 1 Jan 1977\n"
    "2461449600	17	# 1 Jan 1978\n"
    "2492985600	18	# 1 Jan 1979\n"
    "2524521600	19	# 1 Jan 1980\n"
    "   \t  \n"
    "2571782400	20	# 1 Jul 1981\n"
    "2603318400	21	# 1 Jul 1982\n"
    "2634854400	22	# 1 Jul 1983\n"
    "2698012800	23	# 1 Jul 1985\n"
    "2776982400	24	# 1 Jan 1988\n"
    "2840140800	25	# 1 Jan 1990\n"
    "2871676800	26	# 1 Jan 1991\n"
    "2918937600	27	# 1 Jul 1992\n"
    "2950473600	28	# 1 Jul 1993\n"
    "2982009600	29	# 1 Jul 1994\n"
    "3029443200	30	# 1 Jan 1996\n"
    "3076704000	31	# 1 Jul 1997\n"
    "3124137600	32	# 1 Jan 1999\n"
    "3345062400	33	# 1 Jan 2006\n"
    "3439756800	34	# 1 Jan 2009\n"
    "3550089600	35	# 1 Jul 2012\n"
    "#\n"
    "#h	dc2e6b0b 5aade95d a0587abd 4e0dacb4 e4d5049e\n"
    "#\n";

static const char leap2 [] =
    "#\n"
    "#@ 	2950473700\n"
    "#\n"
    "2272060800 10	# 1 Jan 1972\n"
    "2287785600	11	# 1 Jul 1972\n"
    "2303683200	12	# 1 Jan 1973\n"
    "2335219200	13	# 1 Jan 1974\n"
    "2366755200	14	# 1 Jan 1975\n"
    "2398291200	15	# 1 Jan 1976\n"
    "2429913600	16	# 1 Jan 1977\n"
    "2461449600	17	# 1 Jan 1978\n"
    "2492985600	18	# 1 Jan 1979\n"
    "2524521600	19	# 1 Jan 1980\n"
    "2571782400	20	# 1 Jul 1981\n"
    "2603318400	21	# 1 Jul 1982\n"
    "2634854400	22	# 1 Jul 1983\n"
    "2698012800	23	# 1 Jul 1985\n"
    "2776982400	24	# 1 Jan 1988\n"
    "2840140800	25	# 1 Jan 1990\n"
    "2871676800	26	# 1 Jan 1991\n"
    "2918937600	27	# 1 Jul 1992\n"
    "2950473600	28	# 1 Jul 1993\n"
    "#\n";

// Faked table with a leap second removal at 2009 
static const char leap3 [] =
    "#\n"
    "#@ 	3610569600\n"
    "#\n"
    "2272060800 10	# 1 Jan 1972\n"
    "2287785600	11	# 1 Jul 1972\n"
    "2303683200	12	# 1 Jan 1973\n"
    "2335219200	13	# 1 Jan 1974\n"
    "2366755200	14	# 1 Jan 1975\n"
    "2398291200	15	# 1 Jan 1976\n"
    "2429913600	16	# 1 Jan 1977\n"
    "2461449600	17	# 1 Jan 1978\n"
    "2492985600	18	# 1 Jan 1979\n"
    "2524521600	19	# 1 Jan 1980\n"
    "2571782400	20	# 1 Jul 1981\n"
    "2603318400	21	# 1 Jul 1982\n"
    "2634854400	22	# 1 Jul 1983\n"
    "2698012800	23	# 1 Jul 1985\n"
    "2776982400	24	# 1 Jan 1988\n"
    "2840140800	25	# 1 Jan 1990\n"
    "2871676800	26	# 1 Jan 1991\n"
    "2918937600	27	# 1 Jul 1992\n"
    "2950473600	28	# 1 Jul 1993\n"
    "2982009600	29	# 1 Jul 1994\n"
    "3029443200	30	# 1 Jan 1996\n"
    "3076704000	31	# 1 Jul 1997\n"
    "3124137600	32	# 1 Jan 1999\n"
    "3345062400	33	# 1 Jan 2006\n"
    "3439756800	32	# 1 Jan 2009\n"
    "3550089600	33	# 1 Jul 2012\n"
    "#\n";

// short table with good hash
static const char leap_ghash [] =
    "#\n"
    "#@ 	3610569600\n"
    "#$ 	3610566000\n"
    "#\n"
    "2272060800 10	# 1 Jan 1972\n"
    "2287785600	11	# 1 Jul 1972\n"
    "2303683200	12	# 1 Jan 1973\n"
    "2335219200	13	# 1 Jan 1974\n"
    "2366755200	14	# 1 Jan 1975\n"
    "2398291200	15	# 1 Jan 1976\n"
    "2429913600	16	# 1 Jan 1977\n"
    "2461449600	17	# 1 Jan 1978\n"
    "2492985600	18	# 1 Jan 1979\n"
    "2524521600	19	# 1 Jan 1980\n"
    "#\n"
    "#h 4b304e10 95642b3f c10b91f9 90791725 25f280d0\n"
    "#\n";

// short table with bad hash
static const char leap_bhash [] =
    "#\n"
    "#@ 	3610569600\n"
    "#$ 	3610566000\n"
    "#\n"
    "2272060800 10	# 1 Jan 1972\n"
    "2287785600	11	# 1 Jul 1972\n"
    "2303683200	12	# 1 Jan 1973\n"
    "2335219200	13	# 1 Jan 1974\n"
    "2366755200	14	# 1 Jan 1975\n"
    "2398291200	15	# 1 Jan 1976\n"
    "2429913600	16	# 1 Jan 1977\n"
    "2461449600	17	# 1 Jan 1978\n"
    "2492985600	18	# 1 Jan 1979\n"
    "2524521600	19	# 1 Jan 1980\n"
    "#\n"
    "#h	dc2e6b0b 5aade95d a0587abd 4e0dacb4 e4d5049e\n"
    "#\n";

// short table with malformed hash
static const char leap_mhash [] =
    "#\n"
    "#@ 	3610569600\n"
    "#$ 	3610566000\n"
    "#\n"
    "2272060800 10	# 1 Jan 1972\n"
    "2287785600	11	# 1 Jul 1972\n"
    "2303683200	12	# 1 Jan 1973\n"
    "2335219200	13	# 1 Jan 1974\n"
    "2366755200	14	# 1 Jan 1975\n"
    "2398291200	15	# 1 Jan 1976\n"
    "2429913600	16	# 1 Jan 1977\n"
    "2461449600	17	# 1 Jan 1978\n"
    "2492985600	18	# 1 Jan 1979\n"
    "2524521600	19	# 1 Jan 1980\n"
    "#\n"
    "#h f2349a02 788b9534 a8f2e141 f2029Q6d 4064a7ee\n"
    "#\n";

// short table with only 4 hash groups
static const char leap_shash [] =
    "#\n"
    "#@ 	3610569600\n"
    "#$ 	3610566000\n"
    "#\n"
    "2272060800 10	# 1 Jan 1972\n"
    "2287785600	11	# 1 Jul 1972\n"
    "2303683200	12	# 1 Jan 1973\n"
    "2335219200	13	# 1 Jan 1974\n"
    "2366755200	14	# 1 Jan 1975\n"
    "2398291200	15	# 1 Jan 1976\n"
    "2429913600	16	# 1 Jan 1977\n"
    "2461449600	17	# 1 Jan 1978\n"
    "2492985600	18	# 1 Jan 1979\n"
    "2524521600	19	# 1 Jan 1980\n"
    "#\n"
    "#h f2349a02 788b9534 a8f2e141 f2029Q6d\n"
    "#\n";

// table with good hash and truncated/missing leading zeros
static const char leap_gthash [] = {
    "#\n"
    "#$	 3535228800\n"
    "#\n"
    "#	Updated through IERS Bulletin C46\n"
    "#	File expires on:  28 June 2014\n"
    "#\n"
    "#@	3612902400\n"
    "#\n"
    "2272060800	10	# 1 Jan 1972\n"
    "2287785600	11	# 1 Jul 1972\n"
    "2303683200	12	# 1 Jan 1973\n"
    "2335219200	13	# 1 Jan 1974\n"
    "2366755200	14	# 1 Jan 1975\n"
    "2398291200	15	# 1 Jan 1976\n"
    "2429913600	16	# 1 Jan 1977\n"
    "2461449600	17	# 1 Jan 1978\n"
    "2492985600	18	# 1 Jan 1979\n"
    "2524521600	19	# 1 Jan 1980\n"
    "2571782400	20	# 1 Jul 1981\n"
    "2603318400	21	# 1 Jul 1982\n"
    "2634854400	22	# 1 Jul 1983\n"
    "2698012800	23	# 1 Jul 1985\n"
    "2776982400	24	# 1 Jan 1988\n"
    "2840140800	25	# 1 Jan 1990\n"
    "2871676800	26	# 1 Jan 1991\n"
    "2918937600	27	# 1 Jul 1992\n"
    "2950473600	28	# 1 Jul 1993\n"
    "2982009600	29	# 1 Jul 1994\n"
    "3029443200	30	# 1 Jan 1996\n"
    "3076704000	31	# 1 Jul 1997\n"
    "3124137600	32	# 1 Jan 1999\n"
    "3345062400	33	# 1 Jan 2006\n"
    "3439756800	34	# 1 Jan 2009\n"
    "3550089600	35	# 1 Jul 2012\n"
    "#\n"
    "#h	1151a8f e85a5069 9000fcdb 3d5e5365 1d505b37"
};

static const uint32_t lsec2006 = 3345062400u; // +33, 1 Jan 2006, 00:00:00 utc
static const uint32_t lsec2009 = 3439756800u; // +34, 1 Jan 2009, 00:00:00 utc
static const uint32_t lsec2012 = 3550089600u; // +35, 1 Jul 2012, 00:00:00 utc
static const uint32_t lsec2015 = 3644697600u; // +36, 1 Jul 2015, 00:00:00 utc

int stringreader(void* farg)
{
	const char ** cpp = (const char**)farg;
	if (**cpp)
		return *(*cpp)++;
	else
		return EOF;
}

static int/*BOOL*/
setup_load_table(
	const char * cp,
	int          blim=FALSE)
{
	int            rc;
	leap_table_t * pt = leapsec_get_table(0);
	rc = (pt != NULL) && leapsec_load(pt, stringreader, &cp, blim);
	rc = rc && leapsec_set_table(pt);
	return rc;
}

static int/*BOOL*/
setup_clear_table()
{
	int            rc;
	leap_table_t * pt = leapsec_get_table(0);
	if (pt)
		leapsec_clear(pt);
	rc = leapsec_set_table(pt);
	return rc;
}


class leapsecTest : public ntpdtest {
protected:
	virtual void SetUp();
	virtual void TearDown();

	std::string CalendarToString(const calendar &cal) {
		std::ostringstream ss;
		ss << cal.year << "-" << (u_int)cal.month << "-" << (u_int)cal.monthday
		   << " (" << cal.yearday << ") " << (u_int)cal.hour << ":"
		   << (u_int)cal.minute << ":" << (u_int)cal.second;
		return ss.str();
	}

	::testing::AssertionResult IsEqual(const calendar &expected, const calendar &actual) {
		if (expected.year == actual.year &&
			(expected.yearday == actual.yearday ||
			 (expected.month == actual.month &&
			  expected.monthday == actual.monthday)) &&
			expected.hour == actual.hour &&
			expected.minute == actual.minute &&
			expected.second == actual.second) {
			return ::testing::AssertionSuccess();
		} else {
			return ::testing::AssertionFailure()
				<< "expected: " << CalendarToString(expected) << " but was "
				<< CalendarToString(actual);
		}
	}
};

void leapsecTest::SetUp()
{
    ntpcal_set_timefunc(timefunc);
    settime(1970, 1, 1, 0, 0, 0);
    leapsec_ut_pristine();
}

void leapsecTest::TearDown()
{
    ntpcal_set_timefunc(NULL);
}

// =====================================================================
// VALIDATION TESTS
// =====================================================================

// ----------------------------------------------------------------------
TEST_F(leapsecTest, ValidateGood) {
	const char *cp = leap_ghash;
	int         rc = leapsec_validate(stringreader, &cp);
	EXPECT_EQ(LSVALID_GOODHASH, rc);
}

// ----------------------------------------------------------------------
TEST_F(leapsecTest, ValidateNoHash) {
	const char *cp = leap2;
	int         rc = leapsec_validate(stringreader, &cp);
	EXPECT_EQ(LSVALID_NOHASH, rc);
}

// ----------------------------------------------------------------------
TEST_F(leapsecTest, ValidateBad) {
	const char *cp = leap_bhash;
	int         rc = leapsec_validate(stringreader, &cp);
	EXPECT_EQ(LSVALID_BADHASH, rc);
}

// ----------------------------------------------------------------------
TEST_F(leapsecTest, ValidateMalformed) {
	const char *cp = leap_mhash;
	int         rc = leapsec_validate(stringreader, &cp);
	EXPECT_EQ(LSVALID_BADFORMAT, rc);
}

// ----------------------------------------------------------------------
TEST_F(leapsecTest, ValidateMalformedShort) {
	const char *cp = leap_shash;
	int         rc = leapsec_validate(stringreader, &cp);
	EXPECT_EQ(LSVALID_BADFORMAT, rc);
}

// ----------------------------------------------------------------------
TEST_F(leapsecTest, ValidateNoLeadZero) {
	const char *cp = leap_gthash;
	int         rc = leapsec_validate(stringreader, &cp);
	EXPECT_EQ(LSVALID_GOODHASH, rc);
}

// =====================================================================
// BASIC FUNCTIONS
// =====================================================================

// ----------------------------------------------------------------------
// test table selection
TEST_F(leapsecTest, tableSelect) {
	leap_table_t *pt1, *pt2, *pt3, *pt4;

	pt1 = leapsec_get_table(0);
	pt2 = leapsec_get_table(0);
	EXPECT_EQ(pt1, pt2);

	pt1 = leapsec_get_table(1);
	pt2 = leapsec_get_table(1);
	EXPECT_EQ(pt1, pt2);

	pt1 = leapsec_get_table(1);
	pt2 = leapsec_get_table(0);
	EXPECT_NE(pt1, pt2);

	pt1 = leapsec_get_table(0);
	pt2 = leapsec_get_table(1);
	EXPECT_NE(pt1, pt2);

	leapsec_set_table(pt1);
	pt2 = leapsec_get_table(0);
	pt3 = leapsec_get_table(1);
	EXPECT_EQ(pt1, pt2);
	EXPECT_NE(pt2, pt3);

	pt1 = pt3;
	leapsec_set_table(pt1);
	pt2 = leapsec_get_table(0);
	pt3 = leapsec_get_table(1);
	EXPECT_EQ(pt1, pt2);
	EXPECT_NE(pt2, pt3);
}

// ----------------------------------------------------------------------
// load file & check expiration
TEST_F(leapsecTest, loadFileExpire) {
	const char *cp = leap1;
	int rc;
	leap_table_t * pt = leapsec_get_table(0);

	rc =   leapsec_load(pt, stringreader, &cp, FALSE)
	    && leapsec_set_table(pt);
	EXPECT_EQ(1, rc);
	rc = leapsec_expired(3439756800u, NULL);
	EXPECT_EQ(0, rc);
	rc = leapsec_expired(3610569601u, NULL);
	EXPECT_EQ(1, rc);
}

// ----------------------------------------------------------------------
// load file & check time-to-live
TEST_F(leapsecTest, loadFileTTL) {
	const char *cp = leap1;
	int rc;
	leap_table_t * pt = leapsec_get_table(0);
	time_t         pivot = 0x70000000u;

	const uint32_t limit = 3610569600u;

	rc =   leapsec_load(pt, stringreader, &cp, FALSE)
	    && leapsec_set_table(pt);
	ASSERT_EQ(1, rc);

	// exactly 1 day to live
	rc = leapsec_daystolive(limit - 86400, &pivot);
	EXPECT_EQ( 1, rc);	
	// less than 1 day to live
	rc = leapsec_daystolive(limit - 86399, &pivot);
	EXPECT_EQ( 0, rc);	
	// hit expiration exactly
	rc = leapsec_daystolive(limit, &pivot);
	EXPECT_EQ( 0, rc);	
	// expired since 1 sec
	rc = leapsec_daystolive(limit + 1, &pivot);
	EXPECT_EQ(-1, rc);	
}

// =====================================================================
// RANDOM QUERY TESTS
// =====================================================================

// ----------------------------------------------------------------------
// test query in pristine state (bug#2745 misbehaviour)
TEST_F(leapsecTest, lsQueryPristineState) {
	int            rc;
	leap_result_t  qr;
	
	rc = leapsec_query(&qr, lsec2012, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
}

// ----------------------------------------------------------------------
// ad-hoc jump: leap second at 2009.01.01 -60days
TEST_F(leapsecTest, ls2009faraway) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1);
	EXPECT_EQ(1, rc);

	// test 60 days before leap. Nothing scheduled or indicated.
	rc = leapsec_query(&qr, lsec2009 - 60*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(33, qr.tai_offs);
	EXPECT_EQ(0,  qr.tai_diff);
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
}

// ----------------------------------------------------------------------
// ad-hoc jump: leap second at 2009.01.01 -1week
TEST_F(leapsecTest, ls2009weekaway) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1);
	EXPECT_EQ(1, rc);

	// test 7 days before leap. Leap scheduled, but not yet indicated.
	rc = leapsec_query(&qr, lsec2009 - 7*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(33, qr.tai_offs);
	EXPECT_EQ(1,  qr.tai_diff);
	EXPECT_EQ(LSPROX_SCHEDULE, qr.proximity);
}

// ----------------------------------------------------------------------
// ad-hoc jump: leap second at 2009.01.01 -1hr
TEST_F(leapsecTest, ls2009houraway) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1);
	EXPECT_EQ(1, rc);

	// test 1 hour before leap. 61 true seconds to go.
	rc = leapsec_query(&qr, lsec2009 - SECSPERHR, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(33, qr.tai_offs);
	EXPECT_EQ(1,  qr.tai_diff);
	EXPECT_EQ(LSPROX_ANNOUNCE, qr.proximity);
}

// ----------------------------------------------------------------------
// ad-hoc jump: leap second at 2009.01.01 -1sec
TEST_F(leapsecTest, ls2009secaway) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1);
	EXPECT_EQ(1, rc);

	// test 1 second before leap (last boundary...) 2 true seconds to go.
	rc = leapsec_query(&qr, lsec2009 - 1, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(33, qr.tai_offs);
	EXPECT_EQ(1,  qr.tai_diff);
	EXPECT_EQ(LSPROX_ALERT, qr.proximity);
}

// ----------------------------------------------------------------------
// ad-hoc jump to leap second at 2009.01.01
TEST_F(leapsecTest, ls2009onspot) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1);
	EXPECT_EQ(1, rc);

	// test on-spot: treat leap second as already gone.
	rc = leapsec_query(&qr, lsec2009, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(34, qr.tai_offs);
	EXPECT_EQ(0,  qr.tai_diff);
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
}

// ----------------------------------------------------------------------
// test handling of the leap second at 2009.01.01 without table
TEST_F(leapsecTest, ls2009nodata) {
	int            rc;
	leap_result_t  qr;

	rc = setup_clear_table();
	EXPECT_EQ(1, rc);

	// test on-spot with empty table
	rc = leapsec_query(&qr, lsec2009, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,  qr.tai_offs);
	EXPECT_EQ(0,  qr.tai_diff);
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
}

// ----------------------------------------------------------------------
// test handling of the leap second at 2009.01.01 with culled data
TEST_F(leapsecTest, ls2009limdata) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1, TRUE);
	EXPECT_EQ(1, rc);

	// test on-spot with limited table - this is tricky.
	// The table used ends 2012; depending on the build date, the 2009 entry
	// might be included or culled. The resulting TAI offset must be either
	// 34 or 35 seconds, depending on the build date of the test. 
	rc = leapsec_query(&qr, lsec2009, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_LE(34, qr.tai_offs);
	EXPECT_GE(35, qr.tai_offs);
	EXPECT_EQ(0,  qr.tai_diff);
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
}

// ----------------------------------------------------------------------
// Far-distance forward jump into a transiton window.
TEST_F(leapsecTest, qryJumpFarAhead) {
	int            rc;
	leap_result_t  qr;
	int            last, idx;

	for (int mode=0; mode < 2; ++mode) {
		leapsec_ut_pristine();
		rc = setup_load_table(leap1, FALSE);
		EXPECT_EQ(1, rc);
		leapsec_electric(mode);

		rc = leapsec_query(&qr, lsec2006, NULL);
		EXPECT_EQ(FALSE, rc);

		rc = leapsec_query(&qr, lsec2012, NULL);
		EXPECT_EQ(FALSE, rc);
	}
}

// ----------------------------------------------------------------------
// Forward jump into the next transition window
TEST_F(leapsecTest, qryJumpAheadToTransition) {
	int            rc;
	leap_result_t  qr;
	int            last, idx;

	for (int mode=0; mode < 2; ++mode) {
		leapsec_ut_pristine();
		rc = setup_load_table(leap1, FALSE);
		EXPECT_EQ(1, rc);
		leapsec_electric(mode);

		rc = leapsec_query(&qr, lsec2009-SECSPERDAY, NULL);
		EXPECT_EQ(FALSE, rc);

		rc = leapsec_query(&qr, lsec2009+1, NULL);
		EXPECT_EQ(TRUE, rc);
	}
}

// ----------------------------------------------------------------------
// Forward jump over the next transition window
TEST_F(leapsecTest, qryJumpAheadOverTransition) {
	int            rc;
	leap_result_t  qr;
	int            last, idx;

	for (int mode=0; mode < 2; ++mode) {
		leapsec_ut_pristine();
		rc = setup_load_table(leap1, FALSE);
		EXPECT_EQ(1, rc);
		leapsec_electric(mode);

		rc = leapsec_query(&qr, lsec2009-SECSPERDAY, NULL);
		EXPECT_EQ(FALSE, rc);

		rc = leapsec_query(&qr, lsec2009+5, NULL);
		EXPECT_EQ(FALSE, rc);
	}
}

// =====================================================================
// TABLE MODIFICATION AT RUNTIME
// =====================================================================

// ----------------------------------------------------------------------
// add dynamic leap second (like from peer/clock)
TEST_F(leapsecTest, addDynamic) {
	int            rc;
	leap_result_t  qr;

	static const uint32_t insns[] = {
		2982009600u,	//	29	# 1 Jul 1994
		3029443200u,	//	30	# 1 Jan 1996
		3076704000u,	//	31	# 1 Jul 1997
		3124137600u,	//	32	# 1 Jan 1999
		3345062400u,	//	33	# 1 Jan 2006
		3439756800u,	//	34	# 1 Jan 2009
		3550089600u,	//	35	# 1 Jul 2012
		0 // sentinel
	};

	rc = setup_load_table(leap2, FALSE);
	EXPECT_EQ(1, rc);

	leap_table_t * pt = leapsec_get_table(0);
	for (int idx=1; insns[idx]; ++idx) {
		rc = leapsec_add_dyn(TRUE, insns[idx] - 20*SECSPERDAY - 100, NULL);
		EXPECT_EQ(TRUE, rc);
	}
	// try to slip in a previous entry
	rc = leapsec_add_dyn(TRUE, insns[0] - 20*SECSPERDAY - 100, NULL);
	EXPECT_EQ(FALSE, rc);
	//leapsec_dump(pt, (leapsec_dumper)fprintf, stdout);
}

// ----------------------------------------------------------------------
// add fixed leap seconds (like from network packet)
#if 0 /* currently unused -- possibly revived later */
TEST_F(leapsecTest, addFixed) {
	int            rc;
	leap_result_t  qr;

	static const struct { uint32_t tt; int of; } insns[] = {
		{2982009600u, 29},//	# 1 Jul 1994
		{3029443200u, 30},//	# 1 Jan 1996
		{3076704000u, 31},//	# 1 Jul 1997
		{3124137600u, 32},//	# 1 Jan 1999
		{3345062400u, 33},//	# 1 Jan 2006
		{3439756800u, 34},//	# 1 Jan 2009
		{3550089600u, 35},//	# 1 Jul 2012
		{0,0} // sentinel
	};

	rc = setup_load_table(leap2, FALSE);
	EXPECT_EQ(1, rc);

	leap_table_t * pt = leapsec_get_table(0);
	// try to get in BAD time stamps...
	for (int idx=0; insns[idx].tt; ++idx) {
	    rc = leapsec_add_fix(
		insns[idx].of,
		insns[idx].tt - 20*SECSPERDAY - 100,
		insns[idx].tt + SECSPERDAY,
		NULL);
		EXPECT_EQ(FALSE, rc);
	}
	// now do it right
	for (int idx=0; insns[idx].tt; ++idx) {
		rc = leapsec_add_fix(
		    insns[idx].of,
		    insns[idx].tt,
		    insns[idx].tt + SECSPERDAY,
		    NULL);
		EXPECT_EQ(TRUE, rc);
	}
	// try to slip in a previous entry
	rc = leapsec_add_fix(
	    insns[0].of,
	    insns[0].tt,
	    insns[0].tt + SECSPERDAY,
	    NULL);
	EXPECT_EQ(FALSE, rc);
	//leapsec_dump(pt, (leapsec_dumper)fprintf, stdout);
}
#endif

// ----------------------------------------------------------------------
// add fixed leap seconds (like from network packet)
#if 0 /* currently unused -- possibly revived later */
TEST_F(leapsecTest, addFixedExtend) {
	int            rc;
	leap_result_t  qr;
	int            last, idx;

	static const struct { uint32_t tt; int of; } insns[] = {
		{2982009600u, 29},//	# 1 Jul 1994
		{3029443200u, 30},//	# 1 Jan 1996
		{0,0} // sentinel
	};

	rc = setup_load_table(leap2, FALSE);
	EXPECT_EQ(1, rc);

	leap_table_t * pt = leapsec_get_table(FALSE);
	for (last=idx=0; insns[idx].tt; ++idx) {
		last = idx;
		rc = leapsec_add_fix(
		    insns[idx].of,
		    insns[idx].tt,
		    insns[idx].tt + SECSPERDAY,
		    NULL);
		EXPECT_EQ(TRUE, rc);
	}
	
	// try to extend the expiration of the last entry
	rc = leapsec_add_fix(
	    insns[last].of,
	    insns[last].tt,
	    insns[last].tt + 128*SECSPERDAY,
	    NULL);
	EXPECT_EQ(TRUE, rc);
	
	// try to extend the expiration of the last entry with wrong offset
	rc = leapsec_add_fix(
	    insns[last].of+1,
	    insns[last].tt,
	    insns[last].tt + 129*SECSPERDAY,
	    NULL);
	EXPECT_EQ(FALSE, rc);
	//leapsec_dump(pt, (leapsec_dumper)fprintf, stdout);
}
#endif

// ----------------------------------------------------------------------
// add fixed leap seconds (like from network packet) in an otherwise
// empty table and test queries before / between /after the tabulated
// values.
#if 0 /* currently unused -- possibly revived later */
TEST_F(leapsecTest, setFixedExtend) {
	int            rc;
	leap_result_t  qr;
	int            last, idx;

	static const struct { uint32_t tt; int of; } insns[] = {
		{2982009600u, 29},//	# 1 Jul 1994
		{3029443200u, 30},//	# 1 Jan 1996
		{0,0} // sentinel
	};

	leap_table_t * pt = leapsec_get_table(0);
	for (last=idx=0; insns[idx].tt; ++idx) {
		last = idx;
		rc = leapsec_add_fix(
		    insns[idx].of,
		    insns[idx].tt,
		    insns[idx].tt + 128*SECSPERDAY,
		    NULL);
		EXPECT_EQ(TRUE, rc);
	}
	
	rc = leapsec_query(&qr, insns[0].tt - 86400, NULL);
	EXPECT_EQ(28, qr.tai_offs);

	rc = leapsec_query(&qr, insns[0].tt + 86400, NULL);
	EXPECT_EQ(29, qr.tai_offs);

	rc = leapsec_query(&qr, insns[1].tt - 86400, NULL);
	EXPECT_EQ(29, qr.tai_offs);

	rc = leapsec_query(&qr, insns[1].tt + 86400, NULL);
	EXPECT_EQ(30, qr.tai_offs);

	//leapsec_dump(pt, (leapsec_dumper)fprintf, stdout);
}
#endif

// =====================================================================
// AUTOKEY LEAP TRANSFER TESTS
// =====================================================================

// ----------------------------------------------------------------------
// Check if the offset can be applied to an empty table ONCE
TEST_F(leapsecTest, taiEmptyTable) {
	int rc;

	rc = leapsec_autokey_tai(35, lsec2015-30*86400, NULL);	
	EXPECT_EQ(TRUE, rc);

	rc = leapsec_autokey_tai(35, lsec2015-29*86400, NULL);
	EXPECT_EQ(FALSE, rc);
}

// ----------------------------------------------------------------------
// Check that with fixed entries the operation fails
TEST_F(leapsecTest, taiTableFixed) {
	int rc;

	rc = setup_load_table(leap1, FALSE);
	EXPECT_EQ(1, rc);

	rc = leapsec_autokey_tai(35, lsec2015-30*86400, NULL);
	EXPECT_EQ(FALSE, rc);
}

// ----------------------------------------------------------------------
// test adjustment with a dynamic entry already there
TEST_F(leapsecTest, taiTableDynamic) {
	int        rc;
	leap_era_t era;

	rc = leapsec_add_dyn(TRUE, lsec2015-20*SECSPERDAY, NULL);
	EXPECT_EQ(TRUE, rc);

	leapsec_query_era(&era, lsec2015-10, NULL);
	EXPECT_EQ(0, era.taiof);
	leapsec_query_era(&era, lsec2015+10, NULL);
	EXPECT_EQ(1, era.taiof);

	rc = leapsec_autokey_tai(35, lsec2015-19*86400, NULL);	
	EXPECT_EQ(TRUE, rc);

	rc = leapsec_autokey_tai(35, lsec2015-19*86400, NULL);
	EXPECT_EQ(FALSE, rc);

	leapsec_query_era(&era, lsec2015-10, NULL);
	EXPECT_EQ(35, era.taiof);
	leapsec_query_era(&era, lsec2015+10, NULL);
	EXPECT_EQ(36, era.taiof);
}

// ----------------------------------------------------------------------
// test adjustment with a dynamic entry already there in dead zone
TEST_F(leapsecTest, taiTableDynamicDeadZone) {
	int rc;

	rc = leapsec_add_dyn(TRUE, lsec2015-20*SECSPERDAY, NULL);
	EXPECT_EQ(TRUE, rc);

	rc = leapsec_autokey_tai(35, lsec2015-5, NULL);	
	EXPECT_EQ(FALSE, rc);

	rc = leapsec_autokey_tai(35, lsec2015+5, NULL);
	EXPECT_EQ(FALSE, rc);
}


// =====================================================================
// SEQUENCE TESTS
// =====================================================================

// ----------------------------------------------------------------------
// leap second insert at 2009.01.01, electric mode
TEST_F(leapsecTest, ls2009seqInsElectric) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1);
	EXPECT_EQ(1, rc);
	leapsec_electric(1);
	EXPECT_EQ(1, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2009 - 60*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 7*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - SECSPERHR, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 1, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2009, NULL);
	EXPECT_EQ(TRUE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	// second call, same time frame: no trigger!
	rc = leapsec_query(&qr, lsec2009, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
}

// ----------------------------------------------------------------------
// leap second insert at 2009.01.01, dumb mode
TEST_F(leapsecTest, ls2009seqInsDumb) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1);
	EXPECT_EQ(1, rc);
	EXPECT_EQ(0, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2009 - 60*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 7*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - SECSPERHR, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 1, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2009, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2009+1, NULL);
	EXPECT_EQ(TRUE, rc);
	EXPECT_EQ(-1,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	// second call, same time frame: no trigger!
	rc = leapsec_query(&qr, lsec2009, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
}


// ----------------------------------------------------------------------
// fake leap second remove at 2009.01.01, electric mode
TEST_F(leapsecTest, ls2009seqDelElectric) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap3);
	EXPECT_EQ(1, rc);
	leapsec_electric(1);
	EXPECT_EQ(1, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2009 - 60*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 7*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - SECSPERHR, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 1, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2009, NULL);
	EXPECT_EQ(TRUE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	// second call, same time frame: no trigger!
	rc = leapsec_query(&qr, lsec2009, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
}

// ----------------------------------------------------------------------
// fake leap second remove at 2009.01.01. dumb mode
TEST_F(leapsecTest, ls2009seqDelDumb) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap3);
	EXPECT_EQ(1, rc);
	EXPECT_EQ(0, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2009 - 60*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 7*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - SECSPERHR, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 2, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 1, NULL);
	EXPECT_EQ(TRUE, rc);
	EXPECT_EQ(1,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	// second call, same time frame: no trigger!
	rc = leapsec_query(&qr, lsec2009, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
}

// ----------------------------------------------------------------------
// leap second insert at 2012.07.01, electric mode
TEST_F(leapsecTest, ls2012seqInsElectric) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1);
	EXPECT_EQ(1, rc);
	leapsec_electric(1);
	EXPECT_EQ(1, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2012 - 60*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - 7*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - SECSPERHR, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - 1, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2012, NULL);
	EXPECT_EQ(TRUE, rc);
	EXPECT_EQ(0,            qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	// second call, same time frame: no trigger!
	rc = leapsec_query(&qr, lsec2012, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
}

// ----------------------------------------------------------------------
// leap second insert at 2012.07.01, dumb mode
TEST_F(leapsecTest, ls2012seqInsDumb) {
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1);
	EXPECT_EQ(1, rc);
	EXPECT_EQ(0, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2012 - 60*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - 7*SECSPERDAY, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - SECSPERHR, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - 1, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,               qr.warped   );
	EXPECT_EQ(LSPROX_ALERT,    qr.proximity);

	// This is just 1 sec before transition!
	rc = leapsec_query(&qr, lsec2012, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,            qr.warped   );
	EXPECT_EQ(LSPROX_ALERT, qr.proximity);

	// NOW the insert/backwarp must happen
	rc = leapsec_query(&qr, lsec2012+1, NULL);
	EXPECT_EQ(TRUE, rc);
	EXPECT_EQ(-1,            qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);

	// second call with transition time: no trigger!
	rc = leapsec_query(&qr, lsec2012, NULL);
	EXPECT_EQ(FALSE, rc);
	EXPECT_EQ(0,             qr.warped   );
	EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
}

// ----------------------------------------------------------------------
// test repeated query on empty table in dumb mode
TEST_F(leapsecTest, lsEmptyTableDumb) {
	int            rc;
	leap_result_t  qr;

	const time_t   pivot(lsec2012);	
	const uint32_t t0   (lsec2012 - 10);
	const uint32_t tE   (lsec2012 + 10);

	EXPECT_EQ(0, leapsec_electric(-1));

	for (uint32_t t = t0; t != tE; ++t) {
		rc = leapsec_query(&qr, t, &pivot);
		EXPECT_EQ(FALSE, rc);
		EXPECT_EQ(0,             qr.warped   );
		EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
	}
}

// ----------------------------------------------------------------------
// test repeated query on empty table in electric mode
TEST_F(leapsecTest, lsEmptyTableElectric) {
	int            rc;
	leap_result_t  qr;
	
	leapsec_electric(1);
	EXPECT_EQ(1, leapsec_electric(-1));

	const time_t   pivot(lsec2012);	
	const uint32_t t0   (lsec2012 - 10);
	const uint32_t tE   (lsec2012 + 10);

	for (time_t t = t0; t != tE; ++t) {
		rc = leapsec_query(&qr, t, &pivot);
		EXPECT_EQ(FALSE, rc);
		EXPECT_EQ(0,             qr.warped   );
		EXPECT_EQ(LSPROX_NOWARN, qr.proximity);
	}
}
