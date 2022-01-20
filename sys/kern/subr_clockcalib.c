/*-
 * Copyright (c) 2022 Colin Percival
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timetc.h>
#include <sys/tslog.h>
#include <machine/cpu.h>

/**
 * clockcalib(clk, clkname):
 * Return the frequency of the provided timer, as calibrated against the
 * current best-available timecounter.
 */
uint64_t
clockcalib(uint64_t (*clk)(void), const char *clkname)
{
	struct timecounter *tc = atomic_load_ptr(&timecounter);
	uint64_t clk0, clk1, clk_delay, n, passes = 0;
	uint64_t t0, t1, tadj, tlast;
	double mu_clk = 0;
	double mu_t = 0;
	double va_clk = 0;
	double va_t = 0;
	double cva = 0;
	double d1, d2;
	double inv_n;
	uint64_t freq;

	TSENTER();
	/*-
	 * The idea here is to compute a best-fit linear regression between
	 * the clock we're calibrating and the reference clock; the slope of
	 * that line multiplied by the frequency of the reference clock gives
	 * us the frequency we're looking for.
	 *
	 * To do this, we calculate the
	 * (a) mean of the target clock measurements,
	 * (b) variance of the target clock measurements,
	 * (c) mean of the reference clock measurements,
	 * (d) variance of the reference clock measurements, and
	 * (e) covariance of the target clock and reference clock measurements
	 * on an ongoing basis, updating all five values after each new data
	 * point arrives, stopping when we're confident that we've accurately
	 * measured the target clock frequency.
	 *
	 * Given those five values, the important formulas to remember from
	 * introductory statistics are:
	 * 1. slope of regression line = covariance(x, y) / variance(x)
	 * 2. (relative uncertainty in slope)^2 =
	 *    (variance(x) * variance(y) - covariance(x, y)^2)
	 *    ------------------------------------------------
	 *              covariance(x, y)^2 * (N - 2)
	 *
	 * We adjust the second formula slightly, adding a term to each of
	 * the variance values to reflect the measurement quantization.
	 *
	 * Finally, we need to determine when to stop gathering data.  We
	 * can't simply stop as soon as the computed uncertainty estimate
	 * is below our threshold; this would make us overconfident since it
	 * would introduce a multiple-comparisons problem (cf. sequential
	 * analysis in clinical trials).  Instead, we stop with N data points
	 * if the estimated uncertainty of the first k data points meets our
	 * target for all N/2 < k <= N; this is not theoretically optimal,
	 * but in practice works well enough.
	 */

	/*
	 * Initial values for clocks; we'll subtract these off from values
	 * we measure later in order to reduce floating-point rounding errors.
	 * We keep track of an adjustment for values read from the reference
	 * timecounter, since it can wrap.
	 */
	clk0 = clk();
	t0 = tc->tc_get_timecount(tc) & tc->tc_counter_mask;
	tadj = 0;
	tlast = t0;

	/* Loop until we give up or decide that we're calibrated. */
	for (n = 1; ; n++) {
		/* Get a new data point. */
		clk1 = clk() - clk0;
		t1 = tc->tc_get_timecount(tc) & tc->tc_counter_mask;
		while (t1 + tadj < tlast)
			tadj += (uint64_t)tc->tc_counter_mask + 1;
		tlast = t1 + tadj;
		t1 += tadj - t0;

		/* If we spent too long, bail. */
		if (t1 > tc->tc_frequency) {
			printf("Statistical %s calibration failed!  "
			    "Clocks might be ticking at variable rates.\n",
			     clkname);
			printf("Falling back to slow %s calibration.\n",
			    clkname);
			freq = (double)(tc->tc_frequency) * clk1 / t1;
			break;
		}

		/* Precompute to save on divisions later. */
		inv_n = 1.0 / n;

		/* Update mean and variance of recorded TSC values. */
		d1 = clk1 - mu_clk;
		mu_clk += d1 * inv_n;
		d2 = d1 * (clk1 - mu_clk);
		va_clk += (d2 - va_clk) * inv_n;

		/* Update mean and variance of recorded time values. */
		d1 = t1 - mu_t;
		mu_t += d1 * inv_n;
		d2 = d1 * (t1 - mu_t);
		va_t += (d2 - va_t) * inv_n;

		/* Update covariance. */
		d2 = d1 * (clk1 - mu_clk);
		cva += (d2 - cva) * inv_n;

		/*
		 * Count low-uncertainty iterations.  This is a rearrangement
		 * of "relative uncertainty < 1 PPM" avoiding division.
		 */
#define TSC_PPM_UNCERTAINTY	1
#define TSC_UNCERTAINTY		TSC_PPM_UNCERTAINTY * 0.000001
#define TSC_UNCERTAINTY_SQR	TSC_UNCERTAINTY * TSC_UNCERTAINTY
		if (TSC_UNCERTAINTY_SQR * (n - 2) * cva * cva >
		    (va_t + 4) * (va_clk + 4) - cva * cva)
			passes++;
		else
			passes = 0;

		/* Break if we're consistently certain. */
		if (passes * 2 > n) {
			freq = (double)(tc->tc_frequency) * cva / va_t;
			if (bootverbose)
				printf("Statistical %s calibration took"
				    " %lu us and %lu data points\n",
				    clkname, (unsigned long)(t1 *
					1000000.0 / tc->tc_frequency),
				    (unsigned long)n);
			break;
		}

		/*
		 * Add variable delay to avoid theoretical risk of aliasing
		 * resulting from this loop synchronizing with the frequency
		 * of the reference clock.  On the nth iteration, we spend
		 * O(1 / n) time here -- long enough to avoid aliasing, but
		 * short enough to be insignificant as n grows.
		 */
		clk_delay = clk() + (clk() - clk0) / (n * n);
		while (clk() < clk_delay)
			cpu_spinwait(); /* Do nothing. */
	}
	TSEXIT();
	return (freq);
}
