/*-
 * Copyright (c) 1998 Doug Rabson
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

#define S_FORMAT	0	/* IEEE single */
#define T_FORMAT	2	/* IEEE double */
#define Q_FORMAT	3	/* 64 bit fixed */

#define ROUND_CHOP	0	/* truncate fraction */
#define ROUND_MINUS_INF	1	/* round to -INF */
#define ROUND_NORMAL	2	/* round to nearest */
#define ROUND_PLUS_INF	3	/* round to +INF */

typedef union fp_register {
	struct {
		u_int64_t	fraction:	52;
		u_int64_t	exponent:	11;
		u_int64_t	sign:		1;
	} t;
	u_int64_t q;
} fp_register_t;

fp_register_t
ieee_add(fp_register_t fa, fp_register_t fb,
	 int src, int rnd,
	 u_int64_t control, u_int64_t *status);

fp_register_t
ieee_sub(fp_register_t fa, fp_register_t fb,
	 int src, int rnd,
	 u_int64_t control, u_int64_t *status);

fp_register_t
ieee_mul(fp_register_t fa, fp_register_t fb,
	 int src, int rnd,
	 u_int64_t control, u_int64_t *status);

fp_register_t
ieee_div(fp_register_t fa, fp_register_t fb,
	 int src, int rnd,
	 u_int64_t control, u_int64_t *status);

fp_register_t
ieee_cmpun(fp_register_t fa, fp_register_t fb, u_int64_t *status);

fp_register_t
ieee_cmpeq(fp_register_t fa, fp_register_t fb, u_int64_t *status);

fp_register_t
ieee_cmplt(fp_register_t fa, fp_register_t fb, u_int64_t *status);

fp_register_t
ieee_cmple(fp_register_t fa, fp_register_t fb, u_int64_t *status);

fp_register_t
ieee_convert_S_T(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status);

fp_register_t
ieee_convert_T_S(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status);

fp_register_t
ieee_convert_Q_T(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status);

fp_register_t
ieee_convert_Q_S(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status);

fp_register_t
ieee_convert_T_Q(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status);

fp_register_t
ieee_convert_S_Q(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status);

