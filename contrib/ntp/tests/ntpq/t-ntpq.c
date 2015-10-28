#include "config.h"

#include "ntp.h"
//#include "ntp_stdlib.h"


//#include "ntp_calendar.h"

#include "unity.h"
#include "ntpq.h"

//very tricky to test static functions. It might be a good idea to use cmock here
//#define HAVE_NTPQ
//#include "ntpq.c"



//extern int main(int argc, char *argv[]);

void testPrimary(void);

void testPrimary(void){
	//main(NULL,NULL);
/*
	char ** tokens;
	int * num = 0;
	tokenize("a bc de1 234",tokens, num);
*/
}







#define HAVE_NTP_SIGND
