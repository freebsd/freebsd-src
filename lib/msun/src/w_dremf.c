/*
 * dremf() wrapper for remainderf().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */
/* $FreeBSD: src/lib/msun/src/w_dremf.c,v 1.3.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */

#include "math.h"
#include "math_private.h"

float
dremf(float x, float y)
{
	return remainderf(x, y);
}
