/*
 * Implementation of the true gamma function (as opposed to lgamma)
 * for 128-bit long double.
 *
 * Copyright (c) 2006-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/*
 * This module implements the float128 gamma function under the name
 * tgamma128. It's expected to be suitable for integration into system
 * maths libraries under the standard name tgammal, if long double is
 * 128-bit. Such a library will probably want to check the error
 * handling and optimize the initial process of extracting the
 * exponent, which is done here by simple and portable (but
 * potentially slower) methods.
 */

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

/* Only binary128 format is supported.  */
#if LDBL_MANT_DIG == 113

#include "tgamma128.h"

#define lenof(x) (sizeof(x)/sizeof(*(x)))

/*
 * Helper routine to evaluate a polynomial via Horner's rule
 */
static long double poly(const long double *coeffs, size_t n, long double x)
{
    long double result = coeffs[--n];

    while (n > 0)
        result = (result * x) + coeffs[--n];

    return result;
}

/*
 * Compute sin(pi*x) / pi, for use in the reflection formula that
 * relates gamma(-x) and gamma(x).
 */
static long double sin_pi_x_over_pi(long double x)
{
    int quo;
    long double fracpart = remquol(x, 0.5L, &quo);

    long double sign = 1.0L;
    if (quo & 2)
        sign = -sign;
    quo &= 1;

    if (quo == 0 && fabsl(fracpart) < 0x1.p-58L) {
        /* For numbers this size, sin(pi*x) is so close to pi*x that
         * sin(pi*x)/pi is indistinguishable from x in float128 */
        return sign * fracpart;
    }

    if (quo == 0) {
        return sign * sinl(pi*fracpart) / pi;
    } else {
        return sign * cosl(pi*fracpart) / pi;
    }
}

/* Return tgamma(x) on the assumption that x >= 8. */
static long double tgamma_large(long double x,
                                bool negative, long double negadjust)
{
    /*
     * In this range we compute gamma(x) as x^(x-1/2) * e^-x * K,
     * where K is a correction factor computed as a polynomial in 1/x.
     *
     * (Vaguely inspired by the form of the Lanczos approximation, but
     * I tried the Lanczos approximation itself and it suffers badly
     * from big cancellation leading to loss of significance.)
     */
    long double t = 1/x;
    long double p = poly(coeffs_large, lenof(coeffs_large), t);

    /*
     * To avoid overflow in cases where x^(x-0.5) does overflow
     * but gamma(x) does not, we split x^(x-0.5) in half and
     * multiply back up _after_ multiplying the shrinking factor
     * of exp(-(x-0.5)).
     *
     * Note that computing x-0.5 and (x-0.5)/2 is exact for the
     * relevant range of x, so the only sources of error are pow
     * and exp themselves, plus the multiplications.
     */
    long double powhalf = powl(x, (x-0.5L)/2.0L);
    long double expret = expl(-(x-0.5L));

    if (!negative) {
        return (expret * powhalf) * powhalf * p;
    } else {
        /*
         * Apply the reflection formula as commented below, but
         * carefully: negadjust has magnitude less than 1, so it can
         * turn a case where gamma(+x) would overflow into a case
         * where gamma(-x) doesn't underflow. Not only that, but the
         * FP format has greater range in the tiny domain due to
         * denormals. For both reasons, it's not good enough to
         * compute the positive result and then adjust it.
         */
        long double ret = 1 / ((expret * powhalf) * (x * negadjust) * p);
        return ret / powhalf;
    }
}

/* Return tgamma(x) on the assumption that 0 <= x < 1/32. */
static long double tgamma_tiny(long double x,
                               bool negative, long double negadjust)
{
    /*
     * For x near zero, we use a polynomial approximation to
     * g = 1/(x*gamma(x)), and then return 1/(g*x).
     */
    long double g = poly(coeffs_tiny, lenof(coeffs_tiny), x);
    if (!negative)
        return 1.0L / (g*x);
    else
        return g / negadjust;
}

/* Return tgamma(x) on the assumption that 0 <= x < 2^-113. */
static long double tgamma_ultratiny(long double x, bool negative,
                                    long double negadjust)
{
    /* On this interval, gamma can't even be distinguished from 1/x,
     * so we skip the polynomial evaluation in tgamma_tiny, partly to
     * save time and partly to avoid the tiny intermediate values
     * setting the underflow exception flag. */
    if (!negative)
        return 1.0L / x;
    else
        return 1.0L / negadjust;
}

/* Return tgamma(x) on the assumption that 1 <= x <= 2. */
static long double tgamma_central(long double x)
{
    /*
     * In this central interval, our strategy is to finding the
     * difference between x and the point where gamma has a minimum,
     * and approximate based on that.
     */

    /* The difference between the input x and the minimum x. The first
     * subtraction is expected to be exact, since x and min_hi have
     * the same exponent (unless x=2, in which case it will still be
     * exact). */
    long double t = (x - min_x_hi) - min_x_lo;

    /*
     * Now use two different polynomials for the intervals [1,m] and
     * [m,2].
     */
    long double p;
    if (t < 0)
        p = poly(coeffs_central_neg, lenof(coeffs_central_neg), -t);
    else
        p = poly(coeffs_central_pos, lenof(coeffs_central_pos), t);

    return (min_y_lo + p * (t*t)) + min_y_hi;
}

long double tgamma128(long double x)
{
    /*
     * Start by extracting the number's sign and exponent, and ruling
     * out cases of non-normalized numbers.
     *
     * For an implementation integrated into a system libm, it would
     * almost certainly be quicker to do this by direct bitwise access
     * to the input float128 value, using whatever is the local idiom
     * for knowing its endianness.
     *
     * Integration into a system libc may also need to worry about
     * setting errno, if that's the locally preferred way to report
     * math.h errors.
     */
    int sign = signbit(x);
    int exponent;
    switch (fpclassify(x)) {
      case FP_NAN:
        return x+x; /* propagate QNaN, make SNaN throw an exception */
      case FP_ZERO:
        return 1/x; /* divide by zero on purpose to indicate a pole */
      case FP_INFINITE:
        if (sign) {
            return x-x; /* gamma(-inf) has indeterminate sign, so provoke an
                         * IEEE invalid operation exception to indicate that */
        }
        return x;     /* but gamma(+inf) is just +inf with no error */
      case FP_SUBNORMAL:
        exponent = -16384;
        break;
      default:
        frexpl(x, &exponent);
        exponent--;
        break;
    }

    bool negative = false;
    long double negadjust = 0.0L;

    if (sign) {
        /*
         * Euler's reflection formula is
         *
         *    gamma(1-x) gamma(x) = pi/sin(pi*x)
         *
         *                        pi
         * => gamma(x) = --------------------
         *               gamma(1-x) sin(pi*x)
         *
         * But computing 1-x is going to lose a lot of accuracy when x
         * is very small, so instead we transform using the recurrence
         * gamma(t+1)=t gamma(t). Setting t=-x, this gives us
         * gamma(1-x) = -x gamma(-x), so we now have
         *
         *                         pi
         *    gamma(x) = ----------------------
         *               -x gamma(-x) sin(pi*x)
         *
         * which relates gamma(x) to gamma(-x), which is much nicer,
         * since x can be turned into -x without rounding.
         */
        negadjust = sin_pi_x_over_pi(x);
        negative = true;
        x = -x;

        /*
         * Now the ultimate answer we want is
         *
         *    1 / (gamma(x) * x * negadjust)
         *
         * where x is the positive value we've just turned it into.
         *
         * For some of the cases below, we'll compute gamma(x)
         * normally and then compute this adjusted value afterwards.
         * But for others, we can implement the reciprocal operation
         * in this formula by _avoiding_ an inversion that the
         * sub-case was going to do anyway.
         */

        if (negadjust == 0) {
            /*
             * Special case for negative integers. Applying the
             * reflection formula would cause division by zero, but
             * standards would prefer we treat this error case as an
             * invalid operation and return NaN instead. (Possibly
             * because otherwise you'd have to decide which sign of
             * infinity to return, and unlike the x=0 case, there's no
             * sign of zero available to disambiguate.)
             */
            return negadjust / negadjust;
        }
    }

    /*
     * Split the positive domain into various cases. For cases where
     * we do the negative-number adjustment the usual way, we'll leave
     * the answer in 'g' and drop out of the if statement.
     */
    long double g;

    if (exponent >= 11) {
        /*
         * gamma of any positive value this large overflows, and gamma
         * of any negative value underflows.
         */
        if (!negative) {
            long double huge = 0x1p+12288L;
            return huge * huge; /* provoke an overflow */
        } else {
            long double tiny = 0x1p-12288L;
            return tiny * tiny * negadjust; /* underflow, of the right sign */
        }
    } else if (exponent >= 3) {
        /* Negative-number adjustment happens inside here */
        return tgamma_large(x, negative, negadjust);
    } else if (exponent < -113) {
        /* Negative-number adjustment happens inside here */
        return tgamma_ultratiny(x, negative, negadjust);
    } else if (exponent < -5) {
        /* Negative-number adjustment happens inside here */
        return tgamma_tiny(x, negative, negadjust);
    } else if (exponent == 0) {
        g = tgamma_central(x);
    } else if (exponent < 0) {
        /*
         * For x in [1/32,1) we range-reduce upwards to the interval
         * [1,2), using the inverse of the normal recurrence formula:
         * gamma(x) = gamma(x+1)/x.
         */
        g = tgamma_central(1+x) / x;
    } else {
        /*
         * For x in [2,8) we range-reduce downwards to the interval
         * [1,2) by repeated application of the recurrence formula.
         *
         * Actually multiplying (x-1) by (x-2) by (x-3) and so on
         * would introduce multiple ULPs of rounding error. We can get
         * better accuracy by writing x = (k+1/2) + t, where k is an
         * integer and |t|<1/2, and expanding out the obvious factor
         * (x-1)(x-2)...(x-k+1) as a polynomial in t.
         */
        long double mult;
        int i = x;
        if (i == 2) { /* x in [2,3) */
            mult = (x-1);
        } else {
            long double t = x - (i + 0.5L);
            switch (i) {
                /* E.g. for x=3.5+t, we want
                 * (x-1)(x-2) = (2.5+t)(1.5+t) = 3.75 + 4t + t^2 */
              case 3:
                mult = 3.75L+t*(4.0L+t);
                break;
              case 4:
                mult = 13.125L+t*(17.75L+t*(7.5L+t));
                break;
              case 5:
                mult = 59.0625L+t*(93.0L+t*(51.50L+t*(12.0L+t)));
                break;
              case 6:
                mult = 324.84375L+t*(570.5625L+t*(376.250L+t*(
                    117.5L+t*(17.5L+t))));
                break;
              case 7:
                mult = 2111.484375L+t*(4033.5L+t*(3016.1875L+t*(
                    1140.0L+t*(231.25L+t*(24.0L+t)))));
                break;
            }
        }

        g = tgamma_central(x - (i-1)) * mult;
    }

    if (!negative) {
        /* Positive domain: return g unmodified */
        return g;
    } else {
        /* Negative domain: apply the reflection formula as commented above */
        return 1.0L / (g * x * negadjust);
    }
}

#endif
