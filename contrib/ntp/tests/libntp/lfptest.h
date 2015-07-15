#ifndef NTP_TESTS_LFPTEST_H
#define NTP_TESTS_LFPTEST_H

#include "ntp_fp.h"

int IsEqual(const l_fp expected, const l_fp actual) {
	if (L_ISEQU(&expected, &actual)) {
		return TRUE;
	} else {
		//printf(" expected: ...");
		/*
		<< " expected: " << lfptoa(&expected, FRACTION_PREC)
				<< " (" << expected.l_ui << "." << expected.l_uf << ")"
				<< " but was: " << lfptoa(&actual, FRACTION_PREC)
				<< " (" << actual.l_ui << "." << actual.l_uf << ")";
		*/
		return FALSE;
	}

}

static const int32 HALF = -2147483647L - 1L;
static const int32 HALF_PROMILLE_UP = 2147484; // slightly more than 0.0005
static const int32 HALF_PROMILLE_DOWN = 2147483; // slightly less than 0.0005
static const int32 QUARTER = 1073741824L;
static const int32 QUARTER_PROMILLE_APPRX = 1073742L;

#endif


