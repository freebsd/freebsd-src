/*
 *	from: unknown
 *	$Id: test_def.h,v 1.2 1993/10/16 21:33:26 rgrimes Exp $
 */

struct blah {
	unsigned int blahfield;
	int		dummyi;
	char 	dummyc;
};

struct test_pcbstruct {
	int test_pcbfield;
	int test_state;
};

#define MACRO1(arg) if(arg != 0) { printf("macro1\n"); }
