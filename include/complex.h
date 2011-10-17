/*-
 * Copyright (c) 2001-2011 The FreeBSD Project.
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
 * $FreeBSD$
 */

#ifndef _COMPLEX_H
#define	_COMPLEX_H

#ifdef __GNUC__
#if __STDC_VERSION__ < 199901
#define	_Complex	__complex__
#endif
#define	_Complex_I	1.0fi
#endif

#define	complex		_Complex
#define	I		_Complex_I

#include <sys/cdefs.h>

__BEGIN_DECLS

double		cabs(double complex);
float		cabsf(float complex);
long double	cabsl(long double complex);
double		carg(double complex);
float		cargf(float complex);
long double	cargl(long double complex);
double complex	ccos(double complex);
float complex	ccosf(float complex);
double complex	ccosh(double complex);
float complex	ccoshf(float complex);
double complex	cexp(double complex);
float complex	cexpf(float complex);
double		cimag(double complex) __pure2;
float		cimagf(float complex) __pure2;
long double	cimagl(long double complex) __pure2;
double complex	conj(double complex) __pure2;
float complex	conjf(float complex) __pure2;
long double complex
		conjl(long double complex) __pure2;
float complex	cprojf(float complex) __pure2;
double complex	cproj(double complex) __pure2;
long double complex
		cprojl(long double complex) __pure2;
double		creal(double complex) __pure2;
float		crealf(float complex) __pure2;
long double	creall(long double complex) __pure2;
double complex	csin(double complex);
float complex	csinf(float complex);
double complex	csinh(double complex);
float complex	csinhf(float complex);
double complex	csqrt(double complex);
float complex	csqrtf(float complex);
long double complex
		csqrtl(long double complex);
double complex	ctan(double complex);
float complex	ctanf(float complex);
double complex	ctanh(double complex);
float complex	ctanhf(float complex);

__END_DECLS

#endif /* _COMPLEX_H */
