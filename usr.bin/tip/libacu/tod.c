/*
 * tod.c -- time of day pseudo-class implementation
 *
 * Copyright (c) 1995 John H. Poplett
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    John H. Poplett.
 * 4. This work was done expressly for inclusion into FreeBSD.  Other use
 *    is allowed if this notation is included.
 * 5. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 */

#include <sys/types.h>
#include <sys/time.h>

#include <assert.h>
#include <stdio.h>

#include "tod.h"

#define USP 1000000

int tod_cmp (const struct timeval *a, const struct timeval *b)
{
	int rc;
	assert (a->tv_usec <= USP);
	assert (b->tv_usec <= USP);
	rc = a->tv_sec - b->tv_sec;
	if (rc == 0)
		rc = a->tv_usec - b->tv_usec;
	return rc;
}

/*
	TOD < command
*/
int tod_lt (const struct timeval *a, const struct timeval *b)
{
	return tod_cmp (a, b) < 0;
}

int tod_gt (const struct timeval *a, const struct timeval *b)
{
	return tod_cmp (a, b) > 0;
}

int tod_lte (const struct timeval *a, const struct timeval *b)
{
	return tod_cmp (a, b) <= 0;
}

int tod_gte (const struct timeval *a, const struct timeval *b)
{
	return tod_cmp (a, b) >= 0;
}

int tod_eq (const struct timeval *a, const struct timeval *b)
{
	return tod_cmp (a, b) == 0;
}

/*
	TOD += command
*/
void tod_addto (struct timeval *a, const struct timeval *b)
{
	a->tv_usec += b->tv_usec;
	a->tv_sec += b->tv_sec + a->tv_usec / USP;
	a->tv_usec %= USP;
}

/*
	TOD -= command
*/
void tod_subfrom (struct timeval *a, struct timeval b)
{
	assert (a->tv_usec <= USP);
	assert (b.tv_usec <= USP);
	if (b.tv_usec > a->tv_usec)
	{
		a->tv_usec += USP;
		a->tv_sec -= 1;
	}
	a->tv_usec -= b.tv_usec;
	a->tv_sec -= b.tv_sec;
}

void tod_gettime (struct timeval *tp)
{
	gettimeofday (tp, NULL);
	tp->tv_sec += tp->tv_usec / USP;
	tp->tv_usec %= USP;
}

/* end of tod.c */
