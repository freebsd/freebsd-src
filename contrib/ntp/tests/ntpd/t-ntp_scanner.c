#include "config.h"

#include "unity.h"

//#include <stdio.h>
//#include <ctype.h>
//#include <stdlib.h>
//#include <errno.h>
//#include <string.h>

//#include "ntpd.h"
//#include "ntp_config.h"
//#include "ntpsim.h"
//#include "ntp_scanner.h"
//#include "ntp_parser.h"

#include "ntp_scanner.c"
/* ntp_keyword.h declares finite state machine and token text */
//#include "ntp_keyword.h"

void test_keywordIncorrectToken(void);
void test_keywordServerToken(void);
void test_DropUninitializedStack(void);
void test_IncorrectlyInitializeLexStack(void);
void test_InitializeLexStack(void);


void test_keywordIncorrectToken(void){
	const char * temp = keyword(999);
	//printf("%s\n",temp);
	TEST_ASSERT_EQUAL_STRING("(keyword not found)",temp);
}

void test_keywordServerToken(void){
	const char * temp = keyword(T_Server);
	//printf("%s",temp); //143 or 401 ?
	TEST_ASSERT_EQUAL_STRING("server",temp);
}

void test_DropUninitializedStack(void){
	lex_drop_stack();
}

void test_IncorrectlyInitializeLexStack(void){

	TEST_ASSERT_FALSE(lex_init_stack(NULL,NULL));
	lex_drop_stack();
}

void test_InitializeLexStack(void){
	
	//Some sort of server is required for this to work.
	sockaddr_u *	remote_addr;
	char origin[128] ={ "" } ;
	strcat(origin,"127.0.0.1");
	//snprintf(origin, sizeof(origin), "remote config from %s", stoa(remote_addr));
	TEST_ASSERT_TRUE(lex_init_stack(origin,NULL)); //path, mode -> NULL is ok!
	lex_drop_stack();
}

void test_PopEmptyStack(void){
	int temp = lex_pop_file();

	TEST_ASSERT_FALSE(temp);
}



void test_IsInteger(void){ //boolean
	int temp = is_integer("123");
	TEST_ASSERT_TRUE(temp);
	temp = is_integer("-999");
	TEST_ASSERT_TRUE(temp);
	temp = is_integer("0"); //what about -0?
	TEST_ASSERT_TRUE(temp);
	temp = is_integer("16.5");
	TEST_ASSERT_FALSE(temp);
	temp = is_integer("12ab");
	TEST_ASSERT_FALSE(temp);
	temp = is_integer("2147483647");
	TEST_ASSERT_TRUE(temp);
	temp = is_integer("2347483647"); //too big for signed int
	TEST_ASSERT_FALSE(temp);

}

void test_IsUint(void){
	int temp;
	temp = is_u_int("-123");
	TEST_ASSERT_FALSE(temp);
	temp = is_u_int("0");
	TEST_ASSERT_TRUE(temp); //-0 fails btw
	temp = is_u_int("2347483647"); //fits into u_int
	TEST_ASSERT_TRUE(temp);
	temp = is_u_int("112347483647"); //too big even for uint
	TEST_ASSERT_TRUE(temp);		
}

void test_IsDouble(void){
	int temp;	
	temp = is_double("0");
	TEST_ASSERT_TRUE(temp);
	temp = is_double("123");
	TEST_ASSERT_TRUE(temp);
	temp = is_double("123.45"); //DOESN'T WORK WITH 123,45, not sure if intented?
	TEST_ASSERT_TRUE(temp);
	temp = is_double("-123.45"); //DOESN'T WORK WITH 123,45, not sure if intented?
	TEST_ASSERT_TRUE(temp);
}

void test_SpecialSymbols(void){
	int temp ;
	temp = is_special('a');
	TEST_ASSERT_FALSE(temp);
	temp = is_special('?');
	TEST_ASSERT_FALSE(temp);

}

void test_EOC(void){
	int temp;
	if(old_config_style){
		temp = is_EOC('\n');
		TEST_ASSERT_TRUE(temp);
	}
	else {
		temp = is_EOC(';');
		TEST_ASSERT_TRUE(temp);
	}
	temp = is_EOC('A');
	TEST_ASSERT_FALSE(temp);
	temp = is_EOC('1');
	TEST_ASSERT_FALSE(temp);

}

