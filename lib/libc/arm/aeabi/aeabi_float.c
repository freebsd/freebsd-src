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

float32 __addsf3(float32 a, float32 b);
float32 __divsf3(float32 a, float32 b);
float32 __mulsf3(float32 a, float32 b);
float32 __subsf3(float32 a, float32 b);

float64 __extendsfdf2(float32 a);

int32 __fixsfsi(float32);
float32 __floatsisf(int32 a);
flag __gesf2(float32, float32);
flag __lesf2(float32, float32);
flag __unordsf2(float32, float32);

int __aeabi_fcmpeq(float a, float b)
{
	return __lesf2(a, b) == 0;
}

int __aeabi_fcmplt(float a, float b)
{
	return __lesf2(a, b) < 0;
}

int __aeabi_fcmple(float a, float b)
{
	return __lesf2(a, b) <= 0;
}

int __aeabi_fcmpge(float a, float b)
{
	return __gesf2(a, b) >= 0;
}

int __aeabi_fcmpgt(float a, float b)
{
	return __gesf2(a, b) > 0;
}

int __aeabi_fcmpun(float a, float b)
{
	return __unordsf2(a, b);
}

int __aeabi_f2iz(float a)
{
	return __fixsfsi(a);
}

double __aeabi_f2d(float a)
{
	return __extendsfdf2(a);
}

float __aeabi_i2f(int a)
{
	return __floatsisf(a);
}

float __aeabi_fadd(float a, float b)
{
	return __addsf3(a, b);
}

float __aeabi_fdiv(float a, float b)
{
	return __divsf3(a, b);
}

float __aeabi_fmul(float a, float b)
{
	return __mulsf3(a, b);
}

float __aeabi_fsub(float a, float b)
{
	return __subsf3(a, b);
}

