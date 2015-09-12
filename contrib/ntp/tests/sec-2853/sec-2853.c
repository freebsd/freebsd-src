#include <config.h>

#include "unity.h"

void setUp(void);
void tearDown(void);

void test_main( void );
int basic_good( void );
int embedded_nul( void );
int trailing_space( void );

extern size_t remoteconfig_cmdlength(const char *, const char *);

static int verbose = 1;        // if not 0, also print results if test passed
static int exit_on_err = 0;    // if not 0, exit if test failed


void setUp(void)
{
}


void tearDown(void)
{
}


/*
 * Test function calling the remote config buffer checker
 * http://bugs.ntp.org/show_bug.cgi?id=2853
 *
 * size_t remoteconfig_cmdlength(const char *src_buf, const char *src_end)
 * - trims whitespace & garbage from the right
 * then looks for only \tSP-\127 starting from the left.
 * It returns the number of "good" characters it found.
 */


void test_main( void )
{
	TEST_ASSERT_EQUAL(0, basic_good());
	TEST_ASSERT_EQUAL(0, embedded_nul());
	TEST_ASSERT_EQUAL(0, trailing_space());
}


int basic_good( void )
{
	const char string[] = "good";
	const char *EOstring;
	char *cp;
	size_t len;
	int failed;

	EOstring = string + sizeof string;

	len = remoteconfig_cmdlength(string, EOstring);

	failed = ( 4 != len );

	if ( failed || verbose )
		printf( "remoteconfig_cmdlength(\"%s\") returned %d, expected %d: %s\n",
			string,
			len,
			4,
			failed ? "NO <<" : "yes" );

	return failed ? -1 : 0;
}


int embedded_nul( void )
{
	const char string[] = "nul\0 there";
	const char *EOstring;
	char *cp;
	size_t len;
	int failed;

	EOstring = string + sizeof string;

	len = remoteconfig_cmdlength(string, EOstring);

	failed = ( 3 != len );

	if ( failed || verbose )
		printf( "remoteconfig_cmdlength(\"%s\") returned %d, expected %d: %s\n",
			string,
			len,
			3,
			failed ? "NO <<" : "yes" );

	return failed ? -1 : 0;
}


int trailing_space( void )
{
	const char string[] = "trailing space ";
	const char *EOstring;
	char *cp;
	size_t len;
	int failed;

	EOstring = string + sizeof string;

	len = remoteconfig_cmdlength(string, EOstring);

	failed = ( 14 != len );

	if ( failed || verbose )
		printf( "remoteconfig_cmdlength(\"%s\") returned %d, expected %d: %s\n",
			string,
			len,
			14,
			failed ? "NO <<" : "yes" );

	return failed ? -1 : 0;
}
