/* 
 * Copyright (c) 1995 Wolfram Schneider. Public domain.
 *
 * $Id: ostern.c,v 1.2 1996/05/10 16:29:42 ache Exp $
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "calendar.h"

/* return year day for Easter */

int easter (year)
    int year;            /* 0 ... abcd, NOT since 1900 */
{

    int e_a, e_b, e_c, e_d, e_e,e_f, e_g, e_h, e_i, e_k,
        e_l, e_m, e_n, e_p, e_q;

    /* silly, but it works */
    e_a = year % 19;
    e_b = year / 100;
    e_c = year % 100;

    e_d = e_b / 4;
    e_e = e_b % 4;
    e_f = (e_b + 8) / 25;
    e_g = (e_b + 1 - e_f) / 3;
    e_h = ((19 * e_a) + 15 + e_b - (e_d + e_g)) % 30;
    e_i = e_c / 4;
    e_k = e_c % 4;
    e_l = (32 + 2 * e_e + 2 * e_i - (e_h + e_k)) % 7;
    e_m = (e_a + 11 * e_h + 22 * e_l) / 451;
    e_n = (e_h + e_l + 114 - (7 * e_m)) / 31;
    e_p = (e_h + e_l + 114 - (7 * e_m)) % 31;
    e_p = e_p + 1;

    e_q = 31 + 28;

    if (e_k == 0 && e_c != 0)
	e_q += 1;

    if (e_n == 4)
	e_q += 31;

    e_q += e_p;

#if DEBUG
    printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", e_a , e_b , e_c , e_d , e_e , e_f , e_g , e_h , e_i , e_k , e_l , e_m , e_n  , e_p , e_q);
#endif

    return (e_q);
}

/* return year day for  Easter or easter depending days 
 * Match: Easter([+-][0-9]+)?
 * e.g: Easter-2 is  Good Friday (2 days before Easter)
 */

int
geteaster(s, year)
	char *s;
        int year;
{
	register int offset = 0;
	extern struct fixs neaster;

#define EASTER "easter"
#define EASTERNAMELEN (sizeof(EASTER) - 1)

	if (strncasecmp(s, EASTER, EASTERNAMELEN) == 0)
	    s += EASTERNAMELEN;
	else if (   neaster.name != NULL
		 && strncasecmp(s, neaster.name, neaster.len) == 0
		)
	    s += neaster.len;
	else
	    return(0);

#if DEBUG
	printf("%s %d %d\n", s, year, EASTERNAMELEN);
#endif

	/* Easter+1  or Easter-2
	 *       ^            ^   */

	switch(*s) {

	case '-':
	case '+':
	    offset = atoi(s);
	    break;

	default:
	    offset = 0;
	}
	    
	return (easter(year) + offset);
}
