/*
 * Copyright (C) 2012 Andrew Turner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "softfloat-for-gcc.h"
#include "milieu.h"
#include "softfloat.h"

float64 __adddf3(float64 a, float64 b);
float64 __divdf3(float64 a, float64 b);
float64 __muldf3(float64 a, float64 b);
float64 __subdf3(float64 a, float64 b);

float32 __truncdfsf2(float64 a);

int32 __fixdfsi(float64);
float64 __floatsidf(int32 a);
flag __gedf2(float64, float64);
flag __ledf2(float64, float64);
flag __unorddf2(float64, float64);

int __aeabi_dcmpeq(double a, double b)
{
	return __ledf2(a, b) == 0;
}

int __aeabi_dcmplt(double a, double b)
{
	return __ledf2(a, b) < 0;
}

int __aeabi_dcmple(double a, double b)
{
	return __ledf2(a, b) <= 0;
}

int __aeabi_dcmpge(double a, double b)
{
	return __gedf2(a, b) >= 0;
}

int __aeabi_dcmpgt(double a, double b)
{
	return __gedf2(a, b) > 0;
}

int __aeabi_dcmpun(double a, double b)
{
	return __unorddf2(a, b);
}

int __aeabi_d2iz(double a)
{
	return __fixdfsi(a);
}

float __aeabi_d2f(double a)
{
	return __truncdfsf2(a);
}

double __aeabi_i2d(int a)
{
	return __floatsidf(a);
}

double __aeabi_dadd(double a, double b)
{
	return __adddf3(a, b);
}

double __aeabi_ddiv(double a, double b)
{
	return __divdf3(a, b);
}

double __aeabi_dmul(double a, double b)
{
	return __muldf3(a, b);
}

double __aeabi_dsub(double a, double b)
{
	return __subdf3(a, b);
}

