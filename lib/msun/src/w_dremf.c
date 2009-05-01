/*
 * dremf() wrapper for remainderf().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */
/* $FreeBSD: src/lib/msun/src/w_dremf.c,v 1.3.28.1 2009/04/15 03:14:26 kensmith Exp $ */

#include "math.h"
#include "math_private.h"

float
dremf(float x, float y)
{
	return remainderf(x, y);
}
