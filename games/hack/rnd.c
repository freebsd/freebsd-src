/* rnd.c - version 1.0.2 */
/* $FreeBSD$ */

#include <stdlib.h>

#define RND(x)  (random() % x)

rn1(x,y)
int x,y;
{
	return(RND(x)+y);
}

rn2(x)
int x;
{
	return(RND(x));
}

rnd(x)
int x;
{
	return(RND(x)+1);
}

d(n,x)
int n,x;
{
	int tmp = n;

	while(n--) tmp += RND(x);
	return(tmp);
}
