//#include "ntpdtest.h"
#include "config.h"


#include "ntp.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"
#include "ntp_leapsec.h"

#include "unity.h"

#include <string.h>

#include "test-libntp.h"

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
	int          blim)
{
	int            rc;
	leap_table_t * pt = leapsec_get_table(0);

	rc = (pt != NULL) && leapsec_load(pt, stringreader, &cp, blim);
	rc = rc && leapsec_set_table(pt);
	return rc;
}

static int/*BOOL*/
setup_clear_table(void)
{
	int            rc;
	leap_table_t * pt = leapsec_get_table(0);

	if (pt)
		leapsec_clear(pt);
	rc = leapsec_set_table(pt);
	return rc;
}


char *
CalendarToString(const struct calendar cal)
{
	char * ss = malloc (sizeof (char) * 100);
	char buffer[100] ="";

	*ss = '\0';
	sprintf(buffer, "%u", cal.year);
	strcat(ss,buffer);
	strcat(ss,"-");
	sprintf(buffer, "%u", (u_int)cal.month);
	strcat(ss,buffer);
	strcat(ss,"-");
	sprintf(buffer, "%u", (u_int)cal.monthday);
	strcat(ss,buffer);
	strcat(ss," (");
	sprintf(buffer, "%u", (u_int) cal.yearday);
	strcat(ss,buffer);
	strcat(ss,") ");
	sprintf(buffer, "%u", (u_int)cal.hour);
	strcat(ss,buffer);
	strcat(ss,":");
	sprintf(buffer, "%u", (u_int)cal.minute);
	strcat(ss,buffer);
	strcat(ss,":");
	sprintf(buffer, "%u", (u_int)cal.second);
	strcat(ss,buffer);
	//ss << cal.year << "-" << (u_int)cal.month << "-" << (u_int)cal.monthday << " (" << cal.yearday << ") " << (u_int)cal.hour << ":" << (u_int)cal.minute << ":" << (u_int)cal.second;
	return ss;
}


int
IsEqual(const struct calendar expected, const struct calendar actual)
{

	if (   expected.year == actual.year
	    && (   expected.yearday == actual.yearday
		|| (   expected.month == actual.month
		    && expected.monthday == actual.monthday))
	    && expected.hour == actual.hour
	    && expected.minute == actual.minute
	    && expected.second == actual.second) {
		return TRUE;
	} else {
		char *p_exp = CalendarToString(expected);
		char *p_act = CalendarToString(actual);

		printf("expected: %s but was %s", p_exp, p_act);

		free(p_exp);
		free(p_act);
		return FALSE;
	}
}

//-------------------------

void
setUp(void)
{
    ntpcal_set_timefunc(timefunc);
    settime(1970, 1, 1, 0, 0, 0);
    leapsec_ut_pristine();

    return;
}

void
tearDown(void)
{
    ntpcal_set_timefunc(NULL);
    return;
}

// =====================================================================
// VALIDATION TESTS
// =====================================================================

// ----------------------------------------------------------------------
void
test_ValidateGood(void)
{
	const char *cp = leap_ghash;
	int         rc = leapsec_validate(stringreader, &cp);

	TEST_ASSERT_EQUAL(LSVALID_GOODHASH, rc);
	return;
}

// ----------------------------------------------------------------------
void
test_ValidateNoHash(void)
{
	const char *cp = leap2;
	int         rc = leapsec_validate(stringreader, &cp);

	TEST_ASSERT_EQUAL(LSVALID_NOHASH, rc);
	return;
}

// ----------------------------------------------------------------------
void
test_ValidateBad(void)
{
	const char *cp = leap_bhash;
	int         rc = leapsec_validate(stringreader, &cp);

	TEST_ASSERT_EQUAL(LSVALID_BADHASH, rc);

	return;
}

// ----------------------------------------------------------------------
void
test_ValidateMalformed(void)
{
	const char *cp = leap_mhash;
	int         rc = leapsec_validate(stringreader, &cp);

	TEST_ASSERT_EQUAL(LSVALID_BADFORMAT, rc);

	return;
}

// ----------------------------------------------------------------------
void
test_ValidateMalformedShort(void)
{
	const char *cp = leap_shash;
	int         rc = leapsec_validate(stringreader, &cp);

	TEST_ASSERT_EQUAL(LSVALID_BADFORMAT, rc);

	return;
}

// ----------------------------------------------------------------------
void
test_ValidateNoLeadZero(void)
{
	const char *cp = leap_gthash;
	int         rc = leapsec_validate(stringreader, &cp);

	TEST_ASSERT_EQUAL(LSVALID_GOODHASH, rc);

	return;
}

// =====================================================================
// BASIC FUNCTIONS
// =====================================================================

// ----------------------------------------------------------------------
// test table selection
void
test_tableSelect(void)
{
	leap_table_t *pt1, *pt2, *pt3, *pt4;

	pt1 = leapsec_get_table(0);
	pt2 = leapsec_get_table(0);
	TEST_ASSERT_EQUAL_MESSAGE(pt1, pt2,"first");

	pt1 = leapsec_get_table(1);
	pt2 = leapsec_get_table(1);
	TEST_ASSERT_EQUAL_MESSAGE(pt1, pt2,"second");

	pt1 = leapsec_get_table(1);
	pt2 = leapsec_get_table(0);
	TEST_ASSERT_NOT_EQUAL(pt1, pt2);

	pt1 = leapsec_get_table(0);
	pt2 = leapsec_get_table(1);
	TEST_ASSERT_NOT_EQUAL(pt1, pt2);

	leapsec_set_table(pt1);
	pt2 = leapsec_get_table(0);
	pt3 = leapsec_get_table(1);
	TEST_ASSERT_EQUAL(pt1, pt2);
	TEST_ASSERT_NOT_EQUAL(pt2, pt3);

	pt1 = pt3;
	leapsec_set_table(pt1);
	pt2 = leapsec_get_table(0);
	pt3 = leapsec_get_table(1);
	TEST_ASSERT_EQUAL(pt1, pt2);
	TEST_ASSERT_NOT_EQUAL(pt2, pt3);

	return;
}

// ----------------------------------------------------------------------
// load file & check expiration

void
test_loadFileExpire(void)
{
	const char *cp = leap1;
	int rc;
	leap_table_t * pt = leapsec_get_table(0);

	rc =   leapsec_load(pt, stringreader, &cp, FALSE)
	    && leapsec_set_table(pt);
	TEST_ASSERT_EQUAL_MESSAGE(1, rc,"first");
	rc = leapsec_expired(3439756800u, NULL);
	TEST_ASSERT_EQUAL(0, rc);
	rc = leapsec_expired(3610569601u, NULL);
	TEST_ASSERT_EQUAL(1, rc);

	return;
}

// ----------------------------------------------------------------------
// load file & check time-to-live

void
test_loadFileTTL(void)
{
	const char     *cp = leap1;
	int		rc;
	leap_table_t  * pt = leapsec_get_table(0);
	time_t		pivot = 0x70000000u;
	const uint32_t	limit = 3610569600u;

	rc =   leapsec_load(pt, stringreader, &cp, FALSE)
	    && leapsec_set_table(pt);
	TEST_ASSERT_EQUAL(1, rc); //

	// exactly 1 day to live
	rc = leapsec_daystolive(limit - 86400, &pivot);
	TEST_ASSERT_EQUAL( 1, rc);
	// less than 1 day to live
	rc = leapsec_daystolive(limit - 86399, &pivot);
	TEST_ASSERT_EQUAL( 0, rc);
	// hit expiration exactly
	rc = leapsec_daystolive(limit, &pivot);
	TEST_ASSERT_EQUAL( 0, rc);
	// expired since 1 sec
	rc = leapsec_daystolive(limit + 1, &pivot);
	TEST_ASSERT_EQUAL(-1, rc);

	return;
}

// =====================================================================
// RANDOM QUERY TESTS
// =====================================================================

// ----------------------------------------------------------------------
// test query in pristine state (bug#2745 misbehaviour)
void
test_lsQueryPristineState(void)
{
	int            rc;
	leap_result_t  qr;

	rc = leapsec_query(&qr, lsec2012, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// ad-hoc jump: leap second at 2009.01.01 -60days
void
test_ls2009faraway(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1,FALSE);
	TEST_ASSERT_EQUAL(1, rc);

	// test 60 days before leap. Nothing scheduled or indicated.
	rc = leapsec_query(&qr, lsec2009 - 60*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(33, qr.tai_offs);
	TEST_ASSERT_EQUAL(0,  qr.tai_diff);
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// ad-hoc jump: leap second at 2009.01.01 -1week
void
test_ls2009weekaway(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1,FALSE);
	TEST_ASSERT_EQUAL(1, rc);

	// test 7 days before leap. Leap scheduled, but not yet indicated.
	rc = leapsec_query(&qr, lsec2009 - 7*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(33, qr.tai_offs);
	TEST_ASSERT_EQUAL(1,  qr.tai_diff);
	TEST_ASSERT_EQUAL(LSPROX_SCHEDULE, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// ad-hoc jump: leap second at 2009.01.01 -1hr
void
test_ls2009houraway(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1,FALSE);
	TEST_ASSERT_EQUAL(1, rc);

	// test 1 hour before leap. 61 true seconds to go.
	rc = leapsec_query(&qr, lsec2009 - SECSPERHR, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(33, qr.tai_offs);
	TEST_ASSERT_EQUAL(1,  qr.tai_diff);
	TEST_ASSERT_EQUAL(LSPROX_ANNOUNCE, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// ad-hoc jump: leap second at 2009.01.01 -1sec
void
test_ls2009secaway(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1,FALSE);
	TEST_ASSERT_EQUAL(1, rc);

	// test 1 second before leap (last boundary...) 2 true seconds to go.
	rc = leapsec_query(&qr, lsec2009 - 1, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(33, qr.tai_offs);
	TEST_ASSERT_EQUAL(1,  qr.tai_diff);
	TEST_ASSERT_EQUAL(LSPROX_ALERT, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// ad-hoc jump to leap second at 2009.01.01
void
test_ls2009onspot(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1,FALSE);
	TEST_ASSERT_EQUAL(1, rc);

	// test on-spot: treat leap second as already gone.
	rc = leapsec_query(&qr, lsec2009, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(34, qr.tai_offs);
	TEST_ASSERT_EQUAL(0,  qr.tai_diff);
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// test handling of the leap second at 2009.01.01 without table
void
test_ls2009nodata(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_clear_table();
	TEST_ASSERT_EQUAL(1, rc);

	// test on-spot with empty table
	rc = leapsec_query(&qr, lsec2009, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,  qr.tai_offs);
	TEST_ASSERT_EQUAL(0,  qr.tai_diff);
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// test handling of the leap second at 2009.01.01 with culled data
void
test_ls2009limdata(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1, TRUE);
	TEST_ASSERT_EQUAL(1, rc);

	// test on-spot with limited table - this is tricky.
	// The table used ends 2012; depending on the build date, the 2009 entry
	// might be included or culled. The resulting TAI offset must be either
	// 34 or 35 seconds, depending on the build date of the test. 
	rc = leapsec_query(&qr, lsec2009, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_TRUE(34 <= qr.tai_offs);
	TEST_ASSERT_TRUE(35 >= qr.tai_offs);
	TEST_ASSERT_EQUAL(0,  qr.tai_diff);
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// Far-distance forward jump into a transiton window.
void
test_qryJumpFarAhead(void)
{
	int            rc;
	leap_result_t  qr;
	int            last, idx;
	int		mode;

	for (mode=0; mode < 2; ++mode) {
		leapsec_ut_pristine();
		rc = setup_load_table(leap1, FALSE);
		TEST_ASSERT_EQUAL(1, rc);
		leapsec_electric(mode);

		rc = leapsec_query(&qr, lsec2006, NULL);
		TEST_ASSERT_EQUAL(FALSE, rc);

		rc = leapsec_query(&qr, lsec2012, NULL);
		TEST_ASSERT_EQUAL(FALSE, rc);
	}
}

// ----------------------------------------------------------------------
// Forward jump into the next transition window
void test_qryJumpAheadToTransition(void) {
	int		rc;
	leap_result_t	qr;
	int		last, idx;
	int		mode;

	for (mode=0; mode < 2; ++mode) {
		leapsec_ut_pristine();
		rc = setup_load_table(leap1, FALSE);
		TEST_ASSERT_EQUAL(1, rc);
		leapsec_electric(mode);

		rc = leapsec_query(&qr, lsec2009-SECSPERDAY, NULL);
		TEST_ASSERT_EQUAL(FALSE, rc);

		rc = leapsec_query(&qr, lsec2009+1, NULL);
		TEST_ASSERT_EQUAL(TRUE, rc);
	}

	return;
}

// ----------------------------------------------------------------------
// Forward jump over the next transition window
void
test_qryJumpAheadOverTransition(void)
{
	int		rc;
	leap_result_t	qr;
	int		last, idx;
	int		mode;

	for (mode=0; mode < 2; ++mode) {
		leapsec_ut_pristine();
		rc = setup_load_table(leap1, FALSE);
		TEST_ASSERT_EQUAL(1, rc);
		leapsec_electric(mode);

		rc = leapsec_query(&qr, lsec2009-SECSPERDAY, NULL);
		TEST_ASSERT_EQUAL(FALSE, rc);

		rc = leapsec_query(&qr, lsec2009+5, NULL);
		TEST_ASSERT_EQUAL(FALSE, rc);
	}

	return;
}

// =====================================================================
// TABLE MODIFICATION AT RUNTIME
// =====================================================================

// ----------------------------------------------------------------------
// add dynamic leap second (like from peer/clock)
void
test_addDynamic(void)
{
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
	TEST_ASSERT_EQUAL(1, rc);

	int		idx;

	for (idx=1; insns[idx]; ++idx) {
		rc = leapsec_add_dyn(TRUE, insns[idx] - 20*SECSPERDAY - 100, NULL);
		TEST_ASSERT_EQUAL(TRUE, rc);
	}
	// try to slip in a previous entry
	rc = leapsec_add_dyn(TRUE, insns[0] - 20*SECSPERDAY - 100, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	//leap_table_t  * pt = leapsec_get_table(0);
	//leapsec_dump(pt, (leapsec_dumper)fprintf, stdout);

	return;
}

// ----------------------------------------------------------------------
// add fixed leap seconds (like from network packet)
#if 0 /* currently unused -- possibly revived later */
void
FAILtest_addFixed(void)
{
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
	TEST_ASSERT_EQUAL(1, rc);

	int idx;
	// try to get in BAD time stamps...
	for (idx=0; insns[idx].tt; ++idx) {
	    rc = leapsec_add_fix(
		insns[idx].of,
		insns[idx].tt - 20*SECSPERDAY - 100,
		insns[idx].tt + SECSPERDAY,
		NULL);
		TEST_ASSERT_EQUAL(FALSE, rc);
	}
	// now do it right
	for (idx=0; insns[idx].tt; ++idx) {
		rc = leapsec_add_fix(
		    insns[idx].of,
		    insns[idx].tt,
		    insns[idx].tt + SECSPERDAY,
		    NULL);
		TEST_ASSERT_EQUAL(TRUE, rc);
	}
	// try to slip in a previous entry
	rc = leapsec_add_fix(
	    insns[0].of,
	    insns[0].tt,
	    insns[0].tt + SECSPERDAY,
	    NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	//leap_table_t * pt = leapsec_get_table(0);
	//leapsec_dump(pt, (leapsec_dumper)fprintf, stdout);

	return;
}
#endif

// ----------------------------------------------------------------------
// add fixed leap seconds (like from network packet)
#if 0 /* currently unused -- possibly revived later */
void
FAILtest_addFixedExtend(void)
{
	int            rc;
	leap_result_t  qr;
	int            last, idx;

	static const struct { uint32_t tt; int of; } insns[] = {
		{2982009600u, 29},//	# 1 Jul 1994
		{3029443200u, 30},//	# 1 Jan 1996
		{0,0} // sentinel
	};

	rc = setup_load_table(leap2, FALSE);
	TEST_ASSERT_EQUAL(1, rc);

	for (last=idx=0; insns[idx].tt; ++idx) {
		last = idx;
		rc = leapsec_add_fix(
		    insns[idx].of,
		    insns[idx].tt,
		    insns[idx].tt + SECSPERDAY,
		    NULL);
		TEST_ASSERT_EQUAL(TRUE, rc);
	}

	// try to extend the expiration of the last entry
	rc = leapsec_add_fix(
	    insns[last].of,
	    insns[last].tt,
	    insns[last].tt + 128*SECSPERDAY,
	    NULL);
	TEST_ASSERT_EQUAL(TRUE, rc);

	// try to extend the expiration of the last entry with wrong offset
	rc = leapsec_add_fix(
	    insns[last].of+1,
	    insns[last].tt,
	    insns[last].tt + 129*SECSPERDAY,
	    NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	//leap_table_t * pt = leapsec_get_table(FALSE);
	//leapsec_dump(pt, (leapsec_dumper)fprintf, stdout);

	return;
}
#endif

// ----------------------------------------------------------------------
// add fixed leap seconds (like from network packet) in an otherwise
// empty table and test queries before / between /after the tabulated
// values.
#if 0 /* currently unused -- possibly revived later */
void
FAILtest_setFixedExtend(void)
{
	int            rc;
	leap_result_t  qr;
	int            last, idx;

	static const struct { uint32_t tt; int of; } insns[] = {
		{2982009600u, 29},//	# 1 Jul 1994
		{3029443200u, 30},//	# 1 Jan 1996
		{0,0} // sentinel
	};

	for (last=idx=0; insns[idx].tt; ++idx) {
		last = idx;
		rc = leapsec_add_fix(
		    insns[idx].of,
		    insns[idx].tt,
		    insns[idx].tt + 128*SECSPERDAY,
		    NULL);
		TEST_ASSERT_EQUAL(TRUE, rc);
	}

	rc = leapsec_query(&qr, insns[0].tt - 86400, NULL);
	TEST_ASSERT_EQUAL(28, qr.tai_offs);

	rc = leapsec_query(&qr, insns[0].tt + 86400, NULL);
	TEST_ASSERT_EQUAL(29, qr.tai_offs);

	rc = leapsec_query(&qr, insns[1].tt - 86400, NULL);
	TEST_ASSERT_EQUAL(29, qr.tai_offs);

	rc = leapsec_query(&qr, insns[1].tt + 86400, NULL);
	TEST_ASSERT_EQUAL(30, qr.tai_offs);

	//leap_table_t * pt = leapsec_get_table(0);
	//leapsec_dump(pt, (leapsec_dumper)fprintf, stdout);

	return;
}
#endif

// =====================================================================
// AUTOKEY LEAP TRANSFER TESTS
// =====================================================================

// ----------------------------------------------------------------------
// Check if the offset can be applied to an empty table ONCE
void test_taiEmptyTable(void) {
	int rc;

	rc = leapsec_autokey_tai(35, lsec2015-30*86400, NULL);
	TEST_ASSERT_EQUAL(TRUE, rc);

	rc = leapsec_autokey_tai(35, lsec2015-29*86400, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
}

// ----------------------------------------------------------------------
// Check that with fixed entries the operation fails
void
test_taiTableFixed(void)
{
	int rc;

	rc = setup_load_table(leap1, FALSE);
	TEST_ASSERT_EQUAL(1, rc);

	rc = leapsec_autokey_tai(35, lsec2015-30*86400, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);

	return;
}

// ----------------------------------------------------------------------
// test adjustment with a dynamic entry already there
void
test_taiTableDynamic(void)
{
	int        rc;
	leap_era_t era;

	rc = leapsec_add_dyn(TRUE, lsec2015-20*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(TRUE, rc);

	leapsec_query_era(&era, lsec2015-10, NULL);
	TEST_ASSERT_EQUAL(0, era.taiof);
	leapsec_query_era(&era, lsec2015+10, NULL);
	TEST_ASSERT_EQUAL(1, era.taiof);

	rc = leapsec_autokey_tai(35, lsec2015-19*86400, NULL);
	TEST_ASSERT_EQUAL(TRUE, rc);

	rc = leapsec_autokey_tai(35, lsec2015-19*86400, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);

	leapsec_query_era(&era, lsec2015-10, NULL);
	TEST_ASSERT_EQUAL(35, era.taiof);
	leapsec_query_era(&era, lsec2015+10, NULL);
	TEST_ASSERT_EQUAL(36, era.taiof);

	return;
}

// ----------------------------------------------------------------------
// test adjustment with a dynamic entry already there in dead zone
void
test_taiTableDynamicDeadZone(void)
{
	int rc;

	rc = leapsec_add_dyn(TRUE, lsec2015-20*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(TRUE, rc);

	rc = leapsec_autokey_tai(35, lsec2015-5, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);

	rc = leapsec_autokey_tai(35, lsec2015+5, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);

	return;
}


// =====================================================================
// SEQUENCE TESTS
// =====================================================================

// ----------------------------------------------------------------------
// leap second insert at 2009.01.01, electric mode
void
test_ls2009seqInsElectric(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1,FALSE);
	TEST_ASSERT_EQUAL(1, rc);
	leapsec_electric(1);
	TEST_ASSERT_EQUAL(1, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2009 - 60*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 7*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - SECSPERHR, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 1, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2009, NULL);
	TEST_ASSERT_EQUAL(TRUE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	// second call, same time frame: no trigger!
	rc = leapsec_query(&qr, lsec2009, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// leap second insert at 2009.01.01, dumb mode
void
test_ls2009seqInsDumb(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1,FALSE);
	TEST_ASSERT_EQUAL(1, rc);
	TEST_ASSERT_EQUAL(0, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2009 - 60*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 7*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - SECSPERHR, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 1, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2009, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2009+1, NULL);
	TEST_ASSERT_EQUAL(TRUE, rc);
	TEST_ASSERT_EQUAL(-1,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	// second call, same time frame: no trigger!
	rc = leapsec_query(&qr, lsec2009, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	return;
}


// ----------------------------------------------------------------------
// fake leap second remove at 2009.01.01, electric mode
void
test_ls2009seqDelElectric(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap3,FALSE);
	TEST_ASSERT_EQUAL(1, rc);
	leapsec_electric(1);
	TEST_ASSERT_EQUAL(1, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2009 - 60*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 7*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - SECSPERHR, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 1, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2009, NULL);
	TEST_ASSERT_EQUAL(TRUE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	// second call, same time frame: no trigger!
	rc = leapsec_query(&qr, lsec2009, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// fake leap second remove at 2009.01.01. dumb mode
void
test_ls2009seqDelDumb(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap3,FALSE);
	TEST_ASSERT_EQUAL(1, rc);
	TEST_ASSERT_EQUAL(0, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2009 - 60*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 7*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - SECSPERHR, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 2, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2009 - 1, NULL);
	TEST_ASSERT_EQUAL(TRUE, rc);
	TEST_ASSERT_EQUAL(1,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	// second call, same time frame: no trigger!
	rc = leapsec_query(&qr, lsec2009, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// leap second insert at 2012.07.01, electric mode
void
test_ls2012seqInsElectric(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1,FALSE);
	TEST_ASSERT_EQUAL(1, rc);
	leapsec_electric(1);
	TEST_ASSERT_EQUAL(1, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2012 - 60*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - 7*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - SECSPERHR, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - 1, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ALERT,    qr.proximity);

	rc = leapsec_query(&qr, lsec2012, NULL);
	TEST_ASSERT_EQUAL(TRUE, rc);
	TEST_ASSERT_EQUAL(0,            qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	// second call, same time frame: no trigger!
	rc = leapsec_query(&qr, lsec2012, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// leap second insert at 2012.07.01, dumb mode
void
test_ls2012seqInsDumb(void)
{
	int            rc;
	leap_result_t  qr;

	rc = setup_load_table(leap1,FALSE);
	TEST_ASSERT_EQUAL(1, rc);
	TEST_ASSERT_EQUAL(0, leapsec_electric(-1));

	rc = leapsec_query(&qr, lsec2012 - 60*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - 7*SECSPERDAY, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_SCHEDULE, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - SECSPERHR, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ANNOUNCE, qr.proximity);

	rc = leapsec_query(&qr, lsec2012 - 1, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,               qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ALERT,    qr.proximity);

	// This is just 1 sec before transition!
	rc = leapsec_query(&qr, lsec2012, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,            qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_ALERT, qr.proximity);

	// NOW the insert/backwarp must happen
	rc = leapsec_query(&qr, lsec2012+1, NULL);
	TEST_ASSERT_EQUAL(TRUE, rc);
	TEST_ASSERT_EQUAL(-1,            qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	// second call with transition time: no trigger!
	rc = leapsec_query(&qr, lsec2012, NULL);
	TEST_ASSERT_EQUAL(FALSE, rc);
	TEST_ASSERT_EQUAL(0,             qr.warped   );
	TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);

	return;
}

// ----------------------------------------------------------------------
// test repeated query on empty table in dumb mode
void
test_lsEmptyTableDumb(void)
{
	int            rc;
	leap_result_t  qr;

	//const
	time_t pivot;
	pivot = lsec2012;
	//	const 
	//time_t   pivot(lsec2012);
	const uint32_t t0 = lsec2012 - 10;
	const uint32_t tE = lsec2012 + 10;

	TEST_ASSERT_EQUAL(0, leapsec_electric(-1));

	uint32_t t;
	for (t = t0; t != tE; ++t) {
		rc = leapsec_query(&qr, t, &pivot);
		TEST_ASSERT_EQUAL(FALSE, rc);
		TEST_ASSERT_EQUAL(0,             qr.warped   );
		TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);
	}

	return;
}

// ----------------------------------------------------------------------
// test repeated query on empty table in electric mode
void
test_lsEmptyTableElectric(void)
{
	int            rc;
	leap_result_t  qr;

	leapsec_electric(1);
	TEST_ASSERT_EQUAL(1, leapsec_electric(-1));

	//const 
	time_t   pivot;//(lsec2012);
	pivot = lsec2012;
	const uint32_t t0 = lsec2012 - 10;
	const uint32_t tE = lsec2012 + 10;

	time_t t;
	for (t = t0; t != tE; ++t) {
		rc = leapsec_query(&qr, t, &pivot);
		TEST_ASSERT_EQUAL(FALSE, rc);
		TEST_ASSERT_EQUAL(0,             qr.warped   );
		TEST_ASSERT_EQUAL(LSPROX_NOWARN, qr.proximity);
	}

	return;
}
