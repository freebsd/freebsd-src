#include "config.h"

#include "ntp_calendar.h"
#include "ntp_stdlib.h"

#include "unity.h"

void test_RegularTime(void);
void test_CurrentTime(void);


void
test_RegularTime(void)
{
	time_t sample = 1276601278;
	char expected[15];
	struct tm* tm;

	tm = localtime(&sample);
	TEST_ASSERT_TRUE(time != NULL);

	snprintf(expected, 15, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);

	TEST_ASSERT_EQUAL_STRING(expected, humantime(sample));
}

void
test_CurrentTime(void)
{
	time_t sample;
	char expected[15];
	struct tm* tm;

	time(&sample);

	tm = localtime(&sample);
	TEST_ASSERT_TRUE(time != NULL);

	snprintf(expected, 15, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);

	TEST_ASSERT_EQUAL_STRING(expected, humantime(sample));
}
