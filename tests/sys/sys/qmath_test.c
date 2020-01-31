/*-
 * Copyright (c) 2018 Netflix, Inc.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

/*
 * Author: Lawrence Stewart <lstewart@netflix.com>
 */

#include <sys/param.h>
#include <sys/qmath.h>

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

#define	QTEST_IV 3
#define	QTEST_IVSTR "3.00"
#define	QTEST_RPSHFT 2
#define	QTEST_INTBITS(q) (Q_NTBITS(q) - Q_SIGNED(q) - Q_NFBITS(q) - Q_NCBITS)
#define	QTEST_QITRUNC(q, iv) ((iv) >> Q_RPSHFT(q))
#define	QTEST_FFACTOR 32.0

#define	bitsperrand 31
#define	GENRAND(a, lb, ub)						\
({									\
	int _rembits;							\
	do {								\
		_rembits = Q_BITSPERBASEUP(ub) + Q_LTZ(lb);		\
		*(a) = (__typeof(*(a)))0;				\
		while (_rembits > 0) {					\
			*(a) |= (((uint64_t)random()) &			\
			    ((1ULL << (_rembits > bitsperrand ?		\
			    bitsperrand : _rembits)) - 1));		\
			*(a) <<= (_rembits - (_rembits > bitsperrand ?	\
			    bitsperrand : _rembits));			\
			_rembits -= bitsperrand;			\
		}							\
		*(a) += lb;						\
	} while (*(a) < (lb) || (uint64_t)*(a) > (ub));			\
	*(a);								\
})

/*
 * Smoke tests for basic qmath operations, such as initialization
 * or string formatting.
 */
ATF_TC_WITHOUT_HEAD(basic_s8q);
ATF_TC_BODY(basic_s8q, tc)
{
	char buf[128];
	s8q_t s8;

	Q_INI(&s8, QTEST_IV, 0, QTEST_RPSHFT);
	Q_TOSTR(s8, -1, 10, buf, sizeof(buf));
	ATF_CHECK_STREQ(QTEST_IVSTR, buf);
	ATF_CHECK_EQ(sizeof(s8) << 3, Q_NTBITS(s8));
	ATF_CHECK_EQ(QTEST_RPSHFT, Q_NFBITS(s8));
	ATF_CHECK_EQ(QTEST_INTBITS(s8), Q_NIBITS(s8));
	ATF_CHECK_EQ(QTEST_QITRUNC(s8, INT8_MAX), Q_IMAXVAL(s8));
	ATF_CHECK_EQ(-Q_IMAXVAL(s8), Q_IMINVAL(s8));
}

ATF_TC_WITHOUT_HEAD(basic_s16q);
ATF_TC_BODY(basic_s16q, tc)
{
	char buf[128];
	s16q_t s16;

	Q_INI(&s16, QTEST_IV, 0, QTEST_RPSHFT);
	Q_TOSTR(s16, -1, 10, buf, sizeof(buf));
	ATF_CHECK_STREQ(QTEST_IVSTR, buf);
	ATF_CHECK_EQ(sizeof(s16) << 3, Q_NTBITS(s16));
	ATF_CHECK_EQ(QTEST_RPSHFT, Q_NFBITS(s16));
	ATF_CHECK_EQ(QTEST_INTBITS(s16), Q_NIBITS(s16));
	ATF_CHECK_EQ(QTEST_QITRUNC(s16, INT16_MAX), Q_IMAXVAL(s16));
	ATF_CHECK_EQ(-Q_IMAXVAL(s16), Q_IMINVAL(s16));
}

ATF_TC_WITHOUT_HEAD(basic_s32q);
ATF_TC_BODY(basic_s32q, tc)
{
	char buf[128];
	s32q_t s32;

	Q_INI(&s32, QTEST_IV, 0, QTEST_RPSHFT);
	Q_TOSTR(s32, -1, 10, buf, sizeof(buf));
	ATF_CHECK_STREQ(QTEST_IVSTR, buf);
	ATF_CHECK_EQ(sizeof(s32) << 3, Q_NTBITS(s32));
	ATF_CHECK_EQ(QTEST_RPSHFT, Q_NFBITS(s32));
	ATF_CHECK_EQ(QTEST_INTBITS(s32), Q_NIBITS(s32));
	ATF_CHECK_EQ(QTEST_QITRUNC(s32, INT32_MAX), Q_IMAXVAL(s32));
	ATF_CHECK_EQ(-Q_IMAXVAL(s32), Q_IMINVAL(s32));
}

ATF_TC_WITHOUT_HEAD(basic_s64q);
ATF_TC_BODY(basic_s64q, tc)
{
	char buf[128];
	s64q_t s64;

	Q_INI(&s64, QTEST_IV, 0, QTEST_RPSHFT);
	Q_TOSTR(s64, -1, 10, buf, sizeof(buf));
	ATF_CHECK_STREQ(QTEST_IVSTR, buf);
	ATF_CHECK_EQ(sizeof(s64) << 3, Q_NTBITS(s64));
	ATF_CHECK_EQ(QTEST_RPSHFT, Q_NFBITS(s64));
	ATF_CHECK_EQ(QTEST_INTBITS(s64), Q_NIBITS(s64));
	ATF_CHECK_EQ(QTEST_QITRUNC(s64, INT64_MAX), Q_IMAXVAL(s64));
	ATF_CHECK_EQ(-Q_IMAXVAL(s64), Q_IMINVAL(s64));
}

ATF_TC_WITHOUT_HEAD(basic_u8q);
ATF_TC_BODY(basic_u8q, tc)
{
	char buf[128];
	u8q_t u8;

	Q_INI(&u8, QTEST_IV, 0, QTEST_RPSHFT);
	Q_TOSTR(u8, -1, 10, buf, sizeof(buf));
	ATF_CHECK_STREQ(QTEST_IVSTR, buf);
	ATF_CHECK_EQ(sizeof(u8) << 3, Q_NTBITS(u8));
	ATF_CHECK_EQ(QTEST_RPSHFT, Q_NFBITS(u8));
	ATF_CHECK_EQ(QTEST_INTBITS(u8), Q_NIBITS(u8));
	ATF_CHECK_EQ(QTEST_QITRUNC(u8, UINT8_MAX), Q_IMAXVAL(u8));
	ATF_CHECK_EQ(0, Q_IMINVAL(u8));
}

ATF_TC_WITHOUT_HEAD(basic_u16q);
ATF_TC_BODY(basic_u16q, tc)
{
	char buf[128];
	u16q_t u16;

	Q_INI(&u16, QTEST_IV, 0, QTEST_RPSHFT);
	Q_TOSTR(u16, -1, 10, buf, sizeof(buf));
	ATF_CHECK_STREQ(QTEST_IVSTR, buf);
	ATF_CHECK_EQ(sizeof(u16) << 3, Q_NTBITS(u16));
	ATF_CHECK_EQ(QTEST_RPSHFT, Q_NFBITS(u16));
	ATF_CHECK_EQ(QTEST_INTBITS(u16), Q_NIBITS(u16));
	ATF_CHECK_EQ(QTEST_QITRUNC(u16, UINT16_MAX), Q_IMAXVAL(u16));
	ATF_CHECK_EQ(0, Q_IMINVAL(u16));
}

ATF_TC_WITHOUT_HEAD(basic_u32q);
ATF_TC_BODY(basic_u32q, tc)
{
	char buf[128];
	u32q_t u32;

	Q_INI(&u32, QTEST_IV, 0, QTEST_RPSHFT);
	Q_TOSTR(u32, -1, 10, buf, sizeof(buf));
	ATF_CHECK_STREQ(QTEST_IVSTR, buf);
	ATF_CHECK_EQ(sizeof(u32) << 3, Q_NTBITS(u32));
	ATF_CHECK_EQ(QTEST_RPSHFT, Q_NFBITS(u32));
	ATF_CHECK_EQ(QTEST_INTBITS(u32), Q_NIBITS(u32));
	ATF_CHECK_EQ(QTEST_QITRUNC(u32, UINT32_MAX), Q_IMAXVAL(u32));
	ATF_CHECK_EQ(0, Q_IMINVAL(u32));
}

ATF_TC_WITHOUT_HEAD(basic_u64q);
ATF_TC_BODY(basic_u64q, tc)
{
	char buf[128];
	u64q_t u64;

	Q_INI(&u64, QTEST_IV, 0, QTEST_RPSHFT);
	Q_TOSTR(u64, -1, 10, buf, sizeof(buf));
	ATF_CHECK_STREQ(QTEST_IVSTR, buf);
	ATF_CHECK_EQ(sizeof(u64) << 3, Q_NTBITS(u64));
	ATF_CHECK_EQ(QTEST_RPSHFT, Q_NFBITS(u64));
	ATF_CHECK_EQ(QTEST_INTBITS(u64), Q_NIBITS(u64));
	ATF_CHECK_EQ(QTEST_QITRUNC(u64, UINT64_MAX), Q_IMAXVAL(u64));
	ATF_CHECK_EQ(0, Q_IMINVAL(u64));
}

/*
 * Test Q_QMULQ(3) by applying it to two random Q numbers and comparing
 * the result with its floating-point counterpart.
 */
ATF_TC_WITHOUT_HEAD(qmulq_s64q);
ATF_TC_BODY(qmulq_s64q, tc)
{
	s64q_t a_s64q, b_s64q, r_s64q;
	double a_dbl, b_dbl, r_dbl, maxe_dbl, delta_dbl;
#ifdef notyet
	int64_t a_int, b_int;
#endif
	int error;

	srandomdev();

	for (int i = 0; i < 10;) {
		GENRAND(&a_s64q, INT64_MIN, UINT64_MAX);
		GENRAND(&b_s64q, INT64_MIN, UINT64_MAX);

		/*
		 * XXX: We cheat a bit, to stand any chance of multiplying
		 * 	without overflow.
		 */
		error = Q_QDIVQ(&a_s64q, b_s64q);
		if (error == EOVERFLOW || error == ERANGE)
			continue;
		ATF_CHECK_EQ(0, error);

		/*
		 * XXXLAS: Until Qmath handles precision normalisation, only
		 * test with equal precision.
		 */
		Q_SCVAL(b_s64q, Q_GCVAL(a_s64q));

		/* Q<op>Q testing. */
		a_dbl = Q_Q2D(a_s64q);
		b_dbl = Q_Q2D(b_s64q);

		r_s64q = a_s64q;
		error = Q_QMULQ(&r_s64q, b_s64q);
		if (error == EOVERFLOW || error == ERANGE)
			continue;
		i++;
		ATF_CHECK_EQ(0, error);

		r_dbl = a_dbl * b_dbl;
#ifdef notyet
		a_int = Q_GIVAL(a_s64q);
		b_int = Q_GIVAL(b_s64q);

		maxe_dbl = fabs(((1.0 / Q_NFBITS(a_s64q)) * (double)b_int) +
		    ((1.0 / Q_NFBITS(b_s64q)) * (double)a_int));
#else
		maxe_dbl = QTEST_FFACTOR;
#endif
		delta_dbl = fabs(r_dbl - Q_Q2D(r_s64q));
		ATF_CHECK_MSG(delta_dbl <= maxe_dbl,
		    "\tQMULQ(%10f * %10f): |%10f - %10f| = %10f "
		    "(max err %f)\n",
		    Q_Q2D(a_s64q), Q_Q2D(b_s64q), Q_Q2D(r_s64q), r_dbl,
		    delta_dbl, maxe_dbl);
	}
}

/*
 * Test Q_QDIVQ(3) by applying it to two random Q numbers and comparing
 * the result with its floating-point counterpart.
 */
ATF_TC_WITHOUT_HEAD(qdivq_s64q);
ATF_TC_BODY(qdivq_s64q, tc)
{
	s64q_t a_s64q, b_s64q, r_s64q;
	double a_dbl, b_dbl, r_dbl, maxe_dbl, delta_dbl;
	int error;

	if (atf_tc_get_config_var_as_bool_wd(tc, "ci", false))
		atf_tc_skip("https://bugs.freebsd.org/240219");


	srandomdev();

	for (int i = 0; i < 10; i++) {
		GENRAND(&a_s64q, INT64_MIN, UINT64_MAX);
		GENRAND(&b_s64q, INT64_MIN, UINT64_MAX);
		/*
		 * XXXLAS: Until Qmath handles precision normalisation, only
		 * test with equal precision.
		 */
		Q_SCVAL(b_s64q, Q_GCVAL(a_s64q));

		/* Q<op>Q testing. */
		a_dbl = Q_Q2D(a_s64q);
		b_dbl = Q_Q2D(b_s64q);

		r_s64q = a_s64q;
		error = Q_QDIVQ(&r_s64q, b_s64q);
		ATF_CHECK_EQ(0, error);

		r_dbl = a_dbl / b_dbl;
#ifdef notyet
		maxe_dbl = fabs(1.0 / (1ULL << Q_NFBITS(a_s64q)));
#else
		maxe_dbl = QTEST_FFACTOR * 2;
#endif
		delta_dbl = fabs(r_dbl - Q_Q2D(r_s64q));
		ATF_CHECK_MSG(delta_dbl <= maxe_dbl,
		    "\tQDIVQ(%10f / %10f): |%10f - %10f| = %10f "
		    "(max err %f)\n",
		    Q_Q2D(a_s64q), Q_Q2D(b_s64q), Q_Q2D(r_s64q), r_dbl,
		    delta_dbl, maxe_dbl);
	}
}

/*
 * Test Q_QADDQ(3) by applying it to two random Q numbers and comparing
 * the result with its floating-point counterpart.
 */
ATF_TC_WITHOUT_HEAD(qaddq_s64q);
ATF_TC_BODY(qaddq_s64q, tc)
{
	s64q_t a_s64q, b_s64q, r_s64q;
	double a_dbl, b_dbl, r_dbl, maxe_dbl, delta_dbl;
	int error;

	srandomdev();

	for (int i = 0; i < 10;) {
		GENRAND(&a_s64q, INT64_MIN, UINT64_MAX);
		GENRAND(&b_s64q, INT64_MIN, UINT64_MAX);
		/*
		 * XXXLAS: Until Qmath handles precision normalisation, only
		 * test with equal precision.
		 */
		Q_SCVAL(b_s64q, Q_GCVAL(a_s64q));

		/* Q<op>Q testing. */
		a_dbl = Q_Q2D(a_s64q);
		b_dbl = Q_Q2D(b_s64q);

		r_s64q = a_s64q;
		error = Q_QADDQ(&r_s64q, b_s64q);
		if (error == EOVERFLOW || error == ERANGE)
			continue;
		i++;
		ATF_CHECK_EQ(0, error);

		r_dbl = a_dbl + b_dbl;
#ifdef notyet
		maxe_dbl = 0.5;
#else
		maxe_dbl = QTEST_FFACTOR;
#endif
		delta_dbl = fabs(r_dbl - Q_Q2D(r_s64q));
		ATF_CHECK_MSG(delta_dbl <= maxe_dbl,
		    "\tQADDQ(%10f + %10f): |%10f - %10f| = %10f "
		    "(max err %f)\n",
		    Q_Q2D(a_s64q), Q_Q2D(b_s64q), Q_Q2D(r_s64q), r_dbl,
		    delta_dbl, maxe_dbl);
	}
}

/*
 * Test Q_QSUBQ(3) by applying it to two random Q numbers and comparing
 * the result with its floating-point counterpart.
 */
ATF_TC_WITHOUT_HEAD(qsubq_s64q);
ATF_TC_BODY(qsubq_s64q, tc)
{
	s64q_t a_s64q, b_s64q, r_s64q;
	double a_dbl, b_dbl, r_dbl, maxe_dbl, delta_dbl;
	int error;

	srandomdev();

	for (int i = 0; i < 10; i++) {
		GENRAND(&a_s64q, INT64_MIN, UINT64_MAX);
		GENRAND(&b_s64q, INT64_MIN, UINT64_MAX);
		/*
		 * XXXLAS: Until Qmath handles precision normalisation, only
		 * test with equal precision.
		 */
		Q_SCVAL(b_s64q, Q_GCVAL(a_s64q));

		/* Q<op>Q testing. */
		a_dbl = Q_Q2D(a_s64q);
		b_dbl = Q_Q2D(b_s64q);

		r_s64q = a_s64q;
		error = Q_QSUBQ(&r_s64q, b_s64q);
		ATF_CHECK_EQ(0, error);

		r_dbl = a_dbl - b_dbl;
#ifdef notyet
		maxe_dbl = 0.5;
#else
		maxe_dbl = QTEST_FFACTOR;
#endif
		delta_dbl = fabs(r_dbl - Q_Q2D(r_s64q));
		ATF_CHECK_MSG(delta_dbl <= maxe_dbl,
		    "\tQSUBQ(%10f - %10f): |%10f - %10f| = %10f "
		    "(max err %f)\n",
		    Q_Q2D(a_s64q), Q_Q2D(b_s64q), Q_Q2D(r_s64q), r_dbl,
		    delta_dbl, maxe_dbl);
	}
}

/*
 * Test Q_QFRACI(3) by applying it to two random integers and comparing
 * the result with its floating-point counterpart.
 */
ATF_TC_WITHOUT_HEAD(qfraci_s64q);
ATF_TC_BODY(qfraci_s64q, tc)
{
	s64q_t a_s64q, b_s64q, r_s64q;
	double a_dbl, b_dbl, r_dbl, maxe_dbl, delta_dbl;
	int64_t a_int, b_int;
	int error;

	srandomdev();

	for (int i = 0; i < 10;) {
		GENRAND(&a_s64q, INT64_MIN, UINT64_MAX);
		GENRAND(&b_s64q, INT64_MIN, UINT64_MAX);
		/*
		 * XXXLAS: Until Qmath handles precision normalisation, only
		 * test with equal precision.
		 */
		Q_SCVAL(b_s64q, Q_GCVAL(a_s64q));
		a_int = Q_GIVAL(a_s64q);
		b_int = Q_GIVAL(b_s64q);

		/* Q<op>I testing. */
		a_dbl = a_int;
		b_dbl = b_int;

		Q_INI(&r_s64q, 0, 0, Q_NFBITS(a_s64q));
		error = Q_QFRACI(&r_s64q, a_int, b_int);
		if (error == EOVERFLOW || error == ERANGE || error == EINVAL)
			continue;
		i++;
		ATF_CHECK_EQ(0, error);

		r_dbl = a_dbl / b_dbl;
		maxe_dbl = fabs(1.0 / Q_NFBITS(a_s64q));
		delta_dbl = fabs(r_dbl - Q_Q2D(r_s64q));
		ATF_CHECK_MSG(delta_dbl <= maxe_dbl,
		    "\tQFRACI(%jd / %jd): |%10f - %10f| = %10f "
		    "(max err %f)\n",
		    (intmax_t)a_int, (intmax_t)b_int, Q_Q2D(r_s64q),
		    r_dbl, delta_dbl, maxe_dbl);
	}
}

/*
 * Test Q_QMULI(3) by applying it to a random Q number and a random integer
 * and comparing the result with its floating-point counterpart.
 */
ATF_TC_WITHOUT_HEAD(qmuli_s64q);
ATF_TC_BODY(qmuli_s64q, tc)
{
	s64q_t a_s64q, b_s64q, r_s64q;
	double a_dbl, b_dbl, r_dbl, maxe_dbl, delta_dbl;
	int64_t a_int, b_int;
	int error;

	srandomdev();

	for (int i = 0; i < 10;) {
		GENRAND(&a_s64q, INT64_MIN, UINT64_MAX);
		GENRAND(&b_s64q, INT64_MIN, UINT64_MAX);
		/*
		 * XXXLAS: Until Qmath handles precision normalisation, only
		 * test with equal precision.
		 */
		Q_SCVAL(b_s64q, Q_GCVAL(a_s64q));
		a_int = Q_GIVAL(a_s64q);
		b_int = Q_GIVAL(b_s64q);

		/* Q<op>I testing. */
		a_dbl = a_int;
		b_dbl = b_int;

		Q_INI(&r_s64q, a_int, 0, Q_NFBITS(a_s64q));
		error = Q_QMULI(&r_s64q, b_int);
		if (error == EOVERFLOW || error == ERANGE)
			continue;
		i++;
		ATF_CHECK_EQ(0, error);

		r_dbl = a_dbl * b_dbl;
		maxe_dbl = fabs((1.0 / Q_NFBITS(a_s64q)) * (double)b_int);
		delta_dbl = fabs(r_dbl - Q_Q2D(r_s64q));
		ATF_CHECK_MSG(delta_dbl <= maxe_dbl,
		    "\tQMULI(%jd * %jd): |%10f - %10f| = %10f "
		    "(max err %f)\n",
		    (intmax_t)(intmax_t)a_int, b_int, Q_Q2D(r_s64q),
		    r_dbl, delta_dbl, maxe_dbl);
	}
}

/*
 * Test Q_QADDI(3) by applying it to a random Q number and a random integer
 * and comparing the result with its floating-point counterpart.
 */
ATF_TC_WITHOUT_HEAD(qaddi_s64q);
ATF_TC_BODY(qaddi_s64q, tc)
{
	s64q_t a_s64q, b_s64q, r_s64q;
	double a_dbl, b_dbl, r_dbl, maxe_dbl, delta_dbl;
	int64_t a_int, b_int;
	int error;

	srandomdev();

	for (int i = 0; i < 10;) {
		GENRAND(&a_s64q, INT64_MIN, UINT64_MAX);
		GENRAND(&b_s64q, INT64_MIN, UINT64_MAX);
		/*
		 * XXXLAS: Until Qmath handles precision normalisation, only
		 * test with equal precision.
		 */
		Q_SCVAL(b_s64q, Q_GCVAL(a_s64q));
		a_int = Q_GIVAL(a_s64q);
		b_int = Q_GIVAL(b_s64q);

		/* Q<op>I testing. */
		a_dbl = a_int;
		b_dbl = b_int;

		Q_INI(&r_s64q, a_int, 0, Q_NFBITS(a_s64q));
		error = Q_QADDI(&r_s64q, b_int);
		if (error == EOVERFLOW || error == ERANGE)
			continue;
		i++;
		ATF_CHECK_EQ(0, error);

		r_dbl = a_dbl + b_dbl;
#ifdef notyet
		maxe_dbl = 0.5;
#else
		maxe_dbl = QTEST_FFACTOR;
#endif
		delta_dbl = fabs(r_dbl - Q_Q2D(r_s64q));
		ATF_CHECK_MSG(delta_dbl <= maxe_dbl,
		    "\tQADDI(%jd + %jd): |%10f - %10f| = %10f "
		    "(max err %f)\n",
		    (intmax_t)a_int, (intmax_t)b_int, Q_Q2D(r_s64q),
		    r_dbl, delta_dbl, maxe_dbl);
	}
}

/*
 * Test Q_QSUBI(3) by applying it to a random Q number and a random integer
 * and comparing the result with its floating-point counterpart.
 */
ATF_TC_WITHOUT_HEAD(qsubi_s64q);
ATF_TC_BODY(qsubi_s64q, tc)
{
	s64q_t a_s64q, b_s64q, r_s64q;
	double a_dbl, b_dbl, r_dbl, maxe_dbl, delta_dbl;
	int64_t a_int, b_int;
	int error;

	srandomdev();

	for (int i = 0; i < 10; i++) {
		GENRAND(&a_s64q, INT64_MIN, UINT64_MAX);
		GENRAND(&b_s64q, INT64_MIN, UINT64_MAX);
		/*
		 * XXXLAS: Until Qmath handles precision normalisation, only
		 * test with equal precision.
		 */
		Q_SCVAL(b_s64q, Q_GCVAL(a_s64q));
		a_int = Q_GIVAL(a_s64q);
		b_int = Q_GIVAL(b_s64q);

		/* Q<op>I testing. */
		a_dbl = a_int;
		b_dbl = b_int;

		Q_INI(&r_s64q, a_int, 0, Q_NFBITS(a_s64q));
		error = Q_QSUBI(&r_s64q, b_int);
		ATF_CHECK_EQ(0, error);

		r_dbl = a_dbl - b_dbl;
#ifdef notyet
		maxe_dbl = 0.5;
#else
		maxe_dbl = QTEST_FFACTOR;
#endif
		delta_dbl = fabs(r_dbl - Q_Q2D(r_s64q));
		ATF_CHECK_MSG(delta_dbl <= maxe_dbl,
		    "\tQSUBI(%jd - %jd): |%10f - %10f| = %10f "
		    "(max err %f)\n",
		    (intmax_t)a_int, (intmax_t)b_int, Q_Q2D(r_s64q),
		    r_dbl, delta_dbl, maxe_dbl);
	}
}

/*
 * Calculate area of a circle with r=42.
 */
ATF_TC_WITHOUT_HEAD(circle_u64q);
ATF_TC_BODY(circle_u64q, tc)
{
	char buf[128];
	u64q_t a, pi, r;
	int error;

	Q_INI(&a, 0, 0, 16);
	Q_INI(&pi, 3, 14159, 16);
	Q_INI(&r, 4, 2, 16);

	error = Q_QCLONEQ(&a, r);
	ATF_CHECK_EQ(0, error);
	error = Q_QMULQ(&a, r);
	ATF_CHECK_EQ(0, error);
	error = Q_QMULQ(&a, pi);
	ATF_CHECK_EQ(0, error);

	Q_TOSTR(a, -1, 10, buf, sizeof(buf));
	ATF_CHECK_STREQ("55.4174804687500000", buf);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, basic_s8q);
	ATF_TP_ADD_TC(tp, basic_s16q);
	ATF_TP_ADD_TC(tp, basic_s32q);
	ATF_TP_ADD_TC(tp, basic_s64q);
	ATF_TP_ADD_TC(tp, basic_u8q);
	ATF_TP_ADD_TC(tp, basic_u16q);
	ATF_TP_ADD_TC(tp, basic_u32q);
	ATF_TP_ADD_TC(tp, basic_u64q);

	ATF_TP_ADD_TC(tp, qmulq_s64q);
	ATF_TP_ADD_TC(tp, qdivq_s64q);
	ATF_TP_ADD_TC(tp, qaddq_s64q);
	ATF_TP_ADD_TC(tp, qsubq_s64q);
	ATF_TP_ADD_TC(tp, qfraci_s64q);
	ATF_TP_ADD_TC(tp, qmuli_s64q);
	ATF_TP_ADD_TC(tp, qaddi_s64q);
	ATF_TP_ADD_TC(tp, qsubi_s64q);

	ATF_TP_ADD_TC(tp, circle_u64q);

	return (atf_no_error());
}
