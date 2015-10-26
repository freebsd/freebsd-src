#include <config.h>

#include <stdio.h>
#include <sys/time.h>

#include <ntp_fp.h>
#include <timevalops.h>

#include "unity.h"
//#include "bug-2803.h"

/* microseconds per second */
#define MICROSECONDS 1000000

int simpleTest(void);
void setUp(void);
void tearDown(void);
//void test_main(void);

static int verbose = 1;        // if not 0, also print results if test passed
static int exit_on_err = 0;    // if not 0, exit if test failed


/*
 * Test function calling the old and new code mentioned in
 * http://bugs.ntp.org/show_bug.cgi?id=2803#c22
 */
static int do_test( struct timeval timetv, struct timeval tvlast )
{
	struct timeval tvdiff_old;
	struct timeval tvdiff_new;

	int cond_old;
	int cond_new;
	int failed;

	cond_old = 0;
	cond_new = 0;

	// Here is the old code:
	tvdiff_old = abs_tval(sub_tval(timetv, tvlast));
	if (tvdiff_old.tv_sec > 0) {
		cond_old = 1;
	}

	// Here is the new code:
	tvdiff_new = sub_tval(timetv, tvlast);
	if (tvdiff_new.tv_sec != 0) {
		cond_new = 1;
	}

	failed = cond_new != cond_old;

	if ( failed || verbose )
		printf( "timetv %lli|%07li, tvlast  %lli|%07li: tvdiff_old: %lli|%07li -> %i, tvdiff_new: %lli|%07li -> %i, same cond: %s\n",
			(long long) timetv.tv_sec, timetv.tv_usec,
			(long long) tvlast.tv_sec, tvlast.tv_usec,
			(long long) tvdiff_old.tv_sec, tvdiff_old.tv_usec, cond_old,
			(long long) tvdiff_new.tv_sec, tvdiff_new.tv_usec, cond_new,
			failed ? "NO <<" : "yes" );

	return failed ? -1 : 0;
}



/*
 * Call the test function in a loop for a given set of parameters.
 * Both timetv and tvlast iterate over the given range, in all combinations.
 */
static
int test_loop( long long start_sec, long start_usec,
	       long long stop_sec, long stop_usec,
	       long long step_sec, long step_usec )
{
	struct timeval timetv;
	struct timeval tvlast;

	for ( timetv.tv_sec = start_sec; timetv.tv_sec <= stop_sec; timetv.tv_sec += step_sec )
	  for ( timetv.tv_usec = start_usec; timetv.tv_usec <= stop_usec; timetv.tv_usec += step_usec )
	    for ( tvlast.tv_sec = start_sec; tvlast.tv_sec <= stop_sec; tvlast.tv_sec += step_sec )
	      for ( tvlast.tv_usec = start_usec; tvlast.tv_usec <= stop_usec; tvlast.tv_usec += step_usec )
	      {
		int rc = do_test( timetv, tvlast );
		if (rc < 0 && exit_on_err )
			return rc;
	      }

	return 0;
}



int simpleTest( void )
{
	int x;
	// loop from {0.0} to {1.1000000} stepping by tv_sec by 1 and tv_usec by 100000
	x = test_loop( 0, 0,   1,  MICROSECONDS,   1,  MICROSECONDS / 10 );

	// x = test_loop( 0, 0,   5,  MICROSECONDS,   1,  MICROSECONDS / 1000 );
	// x = test_loop( 0, 0,  -5, -MICROSECONDS,  -1, -MICROSECONDS / 1000 );

	return x;
}





void setUp(void)
{
  
}

void tearDown(void)
{
}


void test_main( void )
{
	TEST_ASSERT_EQUAL(0, simpleTest());
}
