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

flag __unorddf2(float64, float64);

int __aeabi_dcmpeq(float64 a, float64 b)
{
	return float64_eq(a, b);
}

int __aeabi_dcmplt(float64 a, float64 b)
{
	return float64_lt(a, b);
}

int __aeabi_dcmple(float64 a, float64 b)
{
	return float64_le(a, b);
}

int __aeabi_dcmpge(float64 a, float64 b)
{
	return float64_le(b, a);
}

int __aeabi_dcmpgt(float64 a, float64 b)
{
	return float64_lt(b, a);
}

int __aeabi_dcmpun(float64 a, float64 b)
{
	return __unorddf2(a, b);
}

int __aeabi_d2iz(float64 a)
{
	return float64_to_int32_round_to_zero(a);
}

float32 __aeabi_d2f(float64 a)
{
	return float64_to_float32(a);
}

float64 __aeabi_i2d(int a)
{
	return int32_to_float64(a);
}

float64 __aeabi_dadd(float64 a, float64 b)
{
	return float64_add(a, b);
}

float64 __aeabi_ddiv(float64 a, float64 b)
{
	return float64_div(a, b);
}

float64 __aeabi_dmul(float64 a, float64 b)
{
	return float64_mul(a, b);
}

float64 __aeabi_dsub(float64 a, float64 b)
{
	return float64_sub(a, b);
}

