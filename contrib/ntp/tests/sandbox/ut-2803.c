//#include "bug-2803.h"
#include "unity.h"
//#include "code-2803.h"

#define VERSION 5 //change this to 5 and the test wont fail.


void setUp(void)
{
  
}

void tearDown(void)
{
}

/*
int main( void )
{

	// loop from {0.0} to {1.1000000} stepping by tv_sec by 1 and tv_usec by 100000
	test_loop( 0, 0,   1,  MICROSECONDS,   1,  MICROSECONDS / 10 );

	// test_loop( 0, 0,   5,  MICROSECONDS,   1,  MICROSECONDS / 1000 );
	// test_loop( 0, 0,  -5, -MICROSECONDS,  -1, -MICROSECONDS / 1000 );

	return 0;
}
*/
void test_main( void )
{
	TEST_ASSERT_EQUAL(0, main2());
}

//VERSION defined at the top of the file

void test_XPASS(void)  //expecting fail but passes, should we get an alert about that?
{	
	//TEST_ABORT
	TEST_EXPECT_FAIL();

	if(VERSION < 4 ){	
		TEST_FAIL_MESSAGE("expected to fail");
	}

	else TEST_ASSERT_EQUAL(1,1);
}

void test_XFAIL(void) //expecting fail, and XFAILs
{	

	TEST_EXPECT_FAIL();

	if(VERSION < 4 ){	
		TEST_FAIL_MESSAGE("Expected to fail");
	}

	else TEST_ASSERT_EQUAL(1,2); 
}

void test_XFAIL_WITH_MESSAGE(void) //expecting fail, and XFAILs
{	
	//TEST_ABORT
	TEST_EXPECT_FAIL_MESSAGE("Doesn't work on this OS");

	if(VERSION < 4 ){	
		TEST_FAIL_MESSAGE("Expected to fail");
	}

	else TEST_ASSERT_EQUAL(1,2); 
}

void test_main_incorrect(void){
	TEST_ASSERT_EQUAL(3, main2());
}

void test_ignored(void){
	//TEST_IGNORE();
	TEST_IGNORE_MESSAGE("This test is being ignored!");   
}
