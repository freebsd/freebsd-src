/* rnd.c - version 1.0.2 */
/* $FreeBSD$ */

#include <stdlib.h>

#define RND(x)  (random() % x)

rn1(x,y)
x,y;
{
	return(RND(x)+y);
}

rn2(x)
x;
{
	return(RND(x));
}

rnd(x)
x;
{
	return(RND(x)+1);
}

d(n,x)
n,x;
{
	tmp = n;

	while(n--) tmp += RND(x);
	return(tmp);
}
