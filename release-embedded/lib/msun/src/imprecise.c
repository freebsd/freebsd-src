/*-
 * Copyright (c) 2013 David Chisnall
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

#include <float.h>
#include <math.h>

/*
 * If long double is not the same size as double, then these will lose
 * precision and we should emit a warning whenever something links against
 * them.
 */
#if (LDBL_MANT_DIG > 53)
#define WARN_IMPRECISE(x) \
	__warn_references(x, # x " has lower than advertised precision");
#else
#define WARN_IMPRECISE(x)
#endif
/*
 * Declare the functions as weak variants so that other libraries providing
 * real versions can override them.
 */
#define	DECLARE_WEAK(x)\
	__weak_reference(imprecise_## x, x);\
	WARN_IMPRECISE(x)

long double
imprecise_powl(long double x, long double y)
{

	return pow(x, y);
}
DECLARE_WEAK(powl);

#define DECLARE_IMPRECISE(f) \
	long double imprecise_ ## f ## l(long double v) { return f(v); }\
	DECLARE_WEAK(f ## l)

DECLARE_IMPRECISE(cosh);
DECLARE_IMPRECISE(erfc);
DECLARE_IMPRECISE(erf);
DECLARE_IMPRECISE(lgamma);
DECLARE_IMPRECISE(sinh);
DECLARE_IMPRECISE(tanh);
DECLARE_IMPRECISE(tgamma);
