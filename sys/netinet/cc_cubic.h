/*-
 * Copyright (c) 2008-2009 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed while studying at the Centre for Advanced
 * Internet Architectures, Swinburne University (http://caia.swin.edu.au),
 * made possible in part by a grant from the Cisco University Research Program
 * Fund at Community Foundation Silicon Valley.
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

#ifndef _NETINET_CC_CUBIC_H_
#define _NETINET_CC_CUBIC_H_

/* Number of bits of precision for fixed point math calcs. */
#define CUBIC_SHIFT		8

#define CUBIC_SHIFT_4		32

/* 0.5 with a shift << 8. */
#define RENO_BETA		128

/* ~0.8 with a shift << 8. */
#define CUBIC_BETA		204

/* ~0.2 with a shift << 8. */
#define ONE_MINUS_CUBIC_BETA	51

/* ~0.4 with a shift << 8. */
#define CUBIC_C_FACTOR		102

/* CUBIC fast convergence factor ~0.9 with a shift << 8. */
#define CUBIC_FC_FACTOR		230

/* Don't trust s_rtt until this many rtt samples have been taken. */
#define CUBIC_MIN_RTT_SAMPLES	8

/* Userspace only bits. */
#ifndef _KERNEL

extern int hz;

inline float
theoretical_cubic_k(double wmax_pkts)
{
	double C = 0.4;
	return pow((wmax_pkts*0.2)/C, (1.0/3.0)) * pow(2, CUBIC_SHIFT);
}

inline u_long
theoretical_cubic_cwnd(u_long ticks_since_cong, u_long wmax, u_int smss)
{
	double C = 0.4;
	double wmax_pkts = wmax/(double)smss;
	return smss * (wmax_pkts +
	    (C * pow(ticks_since_cong/(double)hz -
		theoretical_cubic_k(wmax_pkts) / pow(2, CUBIC_SHIFT), 3.0)));
}

inline u_long
theoretical_reno_cwnd(	u_long ticks_since_cong, u_long rtt_ticks, u_long wmax,
			u_int smss)
{
	return (wmax * 0.5) + ((ticks_since_cong/(float)rtt_ticks) * smss);
}

#endif /* !_KERNEL */

/*
 * Compute the CUBIC K value used in the cwnd calculation, using an
 * implementation of equation 2 in draft-rhee-tcpm-cubic-02. The method used
 * here is adapted from Apple Computer Technical Report #KT-32.
 */
static __inline
int64_t cubic_k(u_long wmax_pkts)
{
	register int64_t s = 0, K = 0;
	register uint16_t p = 0;

	/* (wmax * beta)/C with CUBIC_SHIFT worth of precision. */
	s = ((wmax_pkts * ONE_MINUS_CUBIC_BETA) << CUBIC_SHIFT) /
	    CUBIC_C_FACTOR;

	/* printf("s: %lld\n", s); */

	/*
	 * Rebase s such that it is between 1 and 1/8 with
	 * a shift of CUBIC_SHIFT.
	 */
	while (s >= 256) {
		s >>= 3; 
		p++;
	}

	/* s is now between 1/8 and 1 (shifted by CUBIC_SHIFT). */
	/* printf("rebased s: %lld\n", s); */

	/*
	 * Some magic constants taken from the Apple TR with
	 * appropriate shifts:
	 * 275 == 1.072302 << CUBIC_SHIFT (8)
	 * 98 == 0.3812513 << CUBIC_SHIFT (8)
	 * 120 == 0.46946116 << CUBIC_SHIFT (8)
	 */
	K = (((s * 275) >> CUBIC_SHIFT) + 98) -
		(((s * s * 120) >> CUBIC_SHIFT) >> CUBIC_SHIFT);

	/*
	 * Multiply by 2^p to undo the "divide by 8" transform from the
	 * while loop.
	 */
	return (K <<= p);
}

/*
 * Compute the new CUBIC cwnd value using an implementation of equation 1 in
 * draft-rhee-tcpm-cubic-02.
 * XXXLS: Characterise bounds for overflow.
 * Debugging acknowledgments: Kip Macy
 */
static __inline
u_long cubic_cwnd(u_long ticks_since_cong, u_long wmax, u_int smss)
{
	int64_t cwnd, K;
	/*
	 * int64_t start, end;
	 * start = rdtsc();
	 */

	K = cubic_k(wmax / smss);

	/* K is in fixed point form with CUBIC_SHIFT worth of precision */

	/* t - K, with CUBIC_SHIFT worth of precision. */
	cwnd = ((int64_t)(ticks_since_cong << CUBIC_SHIFT) - (K * hz)) / hz;

	/* printf("t-k: %lld\n", cwnd); */

	/* (t - K)^3, with CUBIC_SHIFT^3 worth of precision. */
	cwnd *= (cwnd * cwnd);
	
	/* printf("(t-k)^3: %lld\n", cwnd); */

	/*
	 * C(t - K)^3 + wmax
	 * The down shift by CUBIC_SHIFT_4 is because cwnd has 4 lots of
	 * CUBIC_SHIFT included in the value. 3 from the cubing of cwnd above,
	 * and an extra from multiplying through by CUBIC_C_FACTOR.
	 */
	cwnd = ((cwnd * CUBIC_C_FACTOR * smss) >> CUBIC_SHIFT_4) + wmax;

	/* printf("final cwnd: %lld\n", cwnd); */

	/*
	 * end = rdtsc();
	 * printf("%lld TSC ticks\n", end - start);
	 */

	return ((u_long)cwnd);
}

static __inline
u_long reno_cwnd(u_long ticks_since_cong, u_long rtt_ticks, u_long wmax,
		 u_int smss)
{
	/*
	 * Simplified form of equation 4 from I-D
	 * For reno, beta = 0.5, therefore W_tcp(t) = wmax*0.5 + t/RTT
	 * Equation 4 deals with cwnd/wmax in pkts, so because our cwnd is
	 * in bytes, we have to multiply by smss.
	 */
	return (((wmax * RENO_BETA) + (((ticks_since_cong * smss)
	    << CUBIC_SHIFT) / rtt_ticks)) >> CUBIC_SHIFT);
}

#endif /* _NETINET_CC_CUBIC_H_ */
