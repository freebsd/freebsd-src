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

flag __unordsf2(float32, float32);

int __aeabi_fcmpeq(float32 a, float32 b)
{
	return float32_eq(a, b);
}

int __aeabi_fcmplt(float32 a, float32 b)
{
	return float32_lt(a, b);
}

int __aeabi_fcmple(float32 a, float32 b)
{
	return float32_le(a, b);
}

int __aeabi_fcmpge(float32 a, float32 b)
{
	return float32_le(b, a);
}

int __aeabi_fcmpgt(float32 a, float32 b)
{
	return float32_lt(b, a);
}

int __aeabi_fcmpun(float32 a, float32 b)
{
	return __unordsf2(a, b);
}

int __aeabi_f2iz(float32 a)
{
	return float32_to_int32_round_to_zero(a);
}

float32 __aeabi_f2d(float32 a)
{
	return float32_to_float64(a);
}

float32 __aeabi_i2f(int a)
{
	return int32_to_float32(a);
}

float32 __aeabi_fadd(float32 a, float32 b)
{
	return float32_add(a, b);
}

float32 __aeabi_fdiv(float32 a, float32 b)
{
	return float32_div(a, b);
}

float32 __aeabi_fmul(float32 a, float32 b)
{
	return float32_mul(a, b);
}

float32 __aeabi_fsub(float32 a, float32 b)
{
	return float32_sub(a, b);
}

