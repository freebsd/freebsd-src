#include "config.h"

#include <ntp.h>
#include <ntp_fp.h>
#include <refidsmear.h>

//#include "ntp_stdlib.h"
//#include "ntp_calendar.h"

#include "unity.h"

#include <stdio.h>

/*
 * we want to test a refid format of:
 * 254.x.y.x
 *
 * where x.y.z are 24 bits containing 2 (signed) integer bits
 * and 22 fractional bits.
 *
 * we want functions to convert to/from this format, with unit tests.
 *
 * Interesting test cases include:
 * 254.0.0.0
 * 254.0.0.1
 * 254.127.255.255
 * 254.128.0.0
 * 254.255.255.255
 */



void rtol(uint32_t r, char *es);
void rtoltor(uint32_t er, char *es);
void ltor(l_fp l, char *er);
void test_refidsmear(void);

void
rtol(uint32_t r, char *es)
{
	l_fp l;
	char *as;
	char msg[100];

	TEST_ASSERT_NOT_NULL(es);

	snprintf(msg, 100, "rtol was called with r=%#.8x, es=%s", r, es);

	l = convertRefIDToLFP(htonl(r));
	as = lfptoa(&l, 8);
	
	//printf("refid %#x, smear %s\n", r, as);

	TEST_ASSERT_NOT_NULL_MESSAGE(as, msg);
	TEST_ASSERT_EQUAL_STRING_MESSAGE(es, as, msg);

	return;
}




void
rtoltor(uint32_t er, char *es)
{
	l_fp l;
	char *as;
	uint32_t ar;
	char msg[100];

	TEST_ASSERT_NOT_NULL(es);

	snprintf(msg, 100, "rtoltor was called with er=%#.8x, es=%s", er, es);

	l = convertRefIDToLFP(htonl(er));
	as = lfptoa(&l, 8);

	ar = convertLFPToRefID(l);

	//printf("smear %s, refid %#.8x\n", lfptoa(&l, 8), ntohl(ar));

	TEST_ASSERT_NOT_NULL_MESSAGE(as, msg);
	TEST_ASSERT_EQUAL_STRING_MESSAGE(es, as, msg);
	TEST_ASSERT_EQUAL_UINT_MESSAGE(er, ntohl(ar), msg);

	return;
}


void
ltor(l_fp l, char *er)
{
	uint32_t r;

	printf("ltor: ");

	r = convertLFPToRefID(l);
	printf("smear %s, refid %#.8x\n", lfptoa(&l, 8), ntohl(r));

	return;
}


void test_refidsmear(void)
{

	rtol(0xfe800000, "-2.00000000");
	rtol(0xfe800001, "-1.99999976");
	rtol(0xfe8ffffe, "-1.75000048");
	rtol(0xfe8fffff, "-1.75000024");
	rtol(0xfef00000, "-0.25000000");
	rtol(0xfef00001, "-0.24999976");
	rtol(0xfefffffe, "-0.00000048");
	rtol(0xfeffffff, "-0.00000024");

	rtol(0xfe000000, "0.00000000");
	rtol(0xfe000001, "0.00000024");
	rtol(0xfe6ffffe, "1.74999952");
	rtol(0xfe6fffff, "1.74999976");
	rtol(0xfe700000, "1.75000000");
	rtol(0xfe700001, "1.75000024");
	rtol(0xfe7ffffe, "1.99999952");
	rtol(0xfe7fffff, "1.99999976");

	rtoltor(0xfe800000, "-2.00000000");
	rtoltor(0xfe800001, "-1.99999976");
	rtoltor(0xfe8ffffe, "-1.75000048");
	rtoltor(0xfe8fffff, "-1.75000024");
	rtoltor(0xfef00000, "-0.25000000");
	rtoltor(0xfef00001, "-0.24999976");
	rtoltor(0xfefffffe, "-0.00000048");
	rtoltor(0xfeffffff, "-0.00000024");

	rtoltor(0xfe000000, "0.00000000");
	rtoltor(0xfe000001, "0.00000024");
	rtoltor(0xfe6ffffe, "1.74999952");
	rtoltor(0xfe6fffff, "1.74999976");
	rtoltor(0xfe700000, "1.75000000");
	rtoltor(0xfe700001, "1.75000024");
	rtoltor(0xfe7ffffe, "1.99999952");
	rtoltor(0xfe7fffff, "1.99999976");

	return;
}
