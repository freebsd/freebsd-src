#include "config.h"
#include "ntp_calendar.h"

#include <stdlib.h>
#include <errno.h>

#include "ntp_types.h"
#include "ntp_fp.h"
#include "vint64ops.h"

/*
 * If we're called with 1 arg, it's a u_long timestamp.
 * If we're called with 3 args, we're expecting YYYY MM DD,
 *   and MM must be 6 or 12, and DD must be 28,
 * If we're called with 2 args, we're expecting YYYY MM, and
 *   MM mst be 6 or 12, and we assume DD is 28.
 */

char *progname;
static const char *MONTHS[] =
                { "January", "February", "March", "April", "May", "June",
		  "July", "August", "September", "October", "November",
		  "December" };

void usage(void);

void
usage(void)
{
	printf("Usage:\n");
	printf(" %s nnnnnn\n", progname);
	printf(" %s YYYY [6|12]\n", progname);
	printf(" %s YYYY [6|12] 28\n", progname);

	return;
}


int
main(
	int	argc,		/* command line options */
	char	**argv		/* poiniter to list of tokens */
	)
{
	int err = 0;
	vint64 expires;
	unsigned int year = 0;
	unsigned int mon = 0;
	unsigned int dom = 0;
	int scount;
	char *ep;
	struct calendar cal = {0};

	progname = argv[0];

	switch(argc) {
	 case 2:	/* 1 arg, must be a string of digits */
		expires = strtouv64(argv[1], &ep, 10);

		if (0 == *ep) {
			ntpcal_ntp64_to_date(&cal, &expires);

			printf("%02u %s %04u %02u:%02u:%02u\n"
				, cal.monthday
				, MONTHS[cal.month - 1]
				, cal.year
				, cal.hour
				, cal.minute
				, cal.second
				);

			exit(0);
		} else {
			printf("1 arg, but not a string of digits: <%s>\n",
				argv[1]);
			err = 1;
		}
		break;
		;;
	 case 3:	/* 2 args, must be YY MM, where MM is 6 or 12 */
		dom = 28;
		scount = sscanf(argv[1], "%u", &year);
		if (1 == scount) {
			// printf("2 args: year %u\n", year);
		} else {
			printf("2 args, but #1 is not a string of digits: <%s>\n", argv[1]);
			err = 1;
		}

		scount = sscanf(argv[2], "%u", &mon);
		if (1 == scount) {
			if (6 == mon || 12 == mon) {
				// printf("2 args: month %u\n", mon);
			} else {
				printf("2 arg, but #2 is not 6 or 12: <%d>\n", mon);
				err = 1;
			}
		} else {
			printf("2 arg, but #2 is not a string of digits: <%s>\n", argv[2]);
			err = 1;
		}

		break;
		;;
	 case 4:	/* 3 args, YY MM DD, where MM is 6 or 12, DD is 28 */
		scount = sscanf(argv[1], "%u", &year);
		if (1 == scount) {
			// printf("3 args: year %u\n", year);
		} else {
			printf("3 args, but #1 is not a string of digits: <%s>\n", argv[1]);
			err = 1;
		}

		scount = sscanf(argv[2], "%u", &mon);
		if (1 == scount) {
			if (6 == mon || 12 == mon) {
				// printf("3 args: month %u\n", mon);
			} else {
				printf("3 arg, but #2 is not 6 or 12: <%d>\n", mon);
				err = 1;
			}
		} else {
			printf("3 arg, but #2 is not a string of digits: <%s>\n", argv[2]);
			err = 1;
		}

		scount = sscanf(argv[3], "%u", &dom);
		if (1 == scount) {
			if (28 == dom) {
				// printf("3 args: dom %u\n", dom);
			} else {
				printf("3 arg, but #3 is not 28: <%d>\n", dom);
				err = 1;
			}
		} else {
			printf("3 arg, but #3 is not a string of digits: <%s>\n", argv[2]);
			err = 1;
		}

		break;
		;;
	 default:
		err = 1;
		break;
		;;
	}

	if (err) {
		usage();
		exit(err);
	}

	cal.year = year;
	cal.month = mon;
	cal.monthday = dom;
	cal.hour = 0;
	cal.minute = 0;
	cal.second = 0;

	printf("%u ", ntpcal_date_to_ntp(&cal));

	printf("%02d %s %04d "
		, cal.monthday
		, MONTHS[cal.month - 1]
		, cal.year
		);
	printf("\n");

	exit(err);
}

#if 0


void
test_DateGivenMonthDay(void) {
	// 2010-06-24 12:50:00
	struct calendar input = {2010, 0, 6, 24, 12, 50, 0};

	u_long expected = 3486372600UL; // This is the timestamp above.

	TEST_ASSERT_EQUAL_UINT(expected, caltontp(&input));
}

void
test_DateGivenYearDay(void) {
	// 2010-06-24 12:50:00
	// This is the 175th day of 2010.
	struct calendar input = {2010, 175, 0, 0, 12, 50, 0};

	u_long expected = 3486372600UL; // This is the timestamp above.

	TEST_ASSERT_EQUAL_UINT(expected, caltontp(&input));
}

void
test_DateLeapYear(void) {
	// 2012-06-24 12:00:00
	// This is the 176th day of 2012 (since 2012 is a leap year).
	struct calendar inputYd = {2012, 176, 0, 0, 12, 00, 00};
	struct calendar inputMd = {2012, 0, 6, 24, 12, 00, 00};

	u_long expected = 3549528000UL;

	TEST_ASSERT_EQUAL_UINT(expected, caltontp(&inputYd));
	TEST_ASSERT_EQUAL_UINT(expected, caltontp(&inputMd));
}

void
test_WraparoundDateIn2036(void) {
	// 2036-02-07 06:28:16
	// This is (one) wrapping boundary where we go from ULONG_MAX to 0.
	struct calendar input = {2036, 0, 2, 7, 6, 28, 16};

	u_long expected = 0UL;

	TEST_ASSERT_EQUAL_UINT(expected, caltontp(&input));
}

#endif
