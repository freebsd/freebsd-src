#ifndef NTP_TESTS_LFPTEST_H
#define NTP_TESTS_LFPTEST_H

#include "config.h"
#include "ntp_fp.h"


static const int32 HALF = -2147483647L - 1L;
static const int32 HALF_PROMILLE_UP = 2147484; // slightly more than 0.0005
static const int32 HALF_PROMILLE_DOWN = 2147483; // slightly less than 0.0005
static const int32 QUARTER = 1073741824L;
static const int32 QUARTER_PROMILLE_APPRX = 1073742L;

int IsEqual(const l_fp expected, const l_fp actual);

#endif
