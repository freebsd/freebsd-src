/* rnd.c - version 1.0.2 */
/* $FreeBSD: src/games/hack/rnd.c,v 1.5 1999/11/16 10:26:38 marcel Exp $ */

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
