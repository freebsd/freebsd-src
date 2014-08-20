/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef libcss_fpmath_h_
#define libcss_fpmath_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <limits.h>

/* 22:10 fixed point math */
#define CSS_RADIX_POINT 10

/* type for fixed point numbers */
typedef int32_t css_fixed;

static inline css_fixed 
css_add_fixed(const css_fixed x, const css_fixed y) {
	int32_t ux = x;
	int32_t uy = y;
	int32_t res = ux + uy;
	
	/* Calculate overflowed result. (Don't change the sign bit of ux) */
	ux = (ux >> 31) + INT_MAX;
	
	/* Force compiler to use cmovns instruction */
	if ((int32_t) ((ux ^ uy) | ~(uy ^ res)) >= 0) {
		res = ux;
	}
		
	return res;
}

static inline css_fixed 
css_subtract_fixed(const css_fixed x, const css_fixed y) {
	int32_t ux = x;
	int32_t uy = y;
	int32_t res = ux - uy;
	
	ux = (ux >> 31) + INT_MAX;
	
	/* Force compiler to use cmovns instruction */
	if ((int32_t)((ux ^ uy) & (ux ^ res)) < 0) {
		res = ux;
	}
		
	return res;
}

static inline css_fixed 
css_divide_fixed(const css_fixed x, const css_fixed y) {
	int64_t xx = ((int64_t)x << CSS_RADIX_POINT) / y;
	
	if (xx < INT_MIN)
		xx = INT_MIN;

	if (xx > INT_MAX)
		xx = INT_MAX;
	
	return xx;
}

static inline css_fixed 
css_multiply_fixed(const css_fixed x, const css_fixed y) {
	int64_t xx = ((int64_t)x * (int64_t)y) >> CSS_RADIX_POINT;
	
	if (xx < INT_MIN)
		xx = INT_MIN;

	if (xx > INT_MAX)
		xx = INT_MAX;
	
	return xx;
}

static inline css_fixed 
css_int_to_fixed(const int a) {
	int64_t xx = ((int64_t) a) << CSS_RADIX_POINT;

	if (xx < INT_MIN)
		xx = INT_MIN;

	if (xx > INT_MAX)
		xx = INT_MAX;
	
	return xx;
}

static inline css_fixed 
css_float_to_fixed(const float a) {
	float xx = a * (float) (1 << CSS_RADIX_POINT);

	if (xx < INT_MIN)
		xx = INT_MIN;

	if (xx > INT_MAX)
		xx = INT_MAX;
	
	return (css_fixed) xx;
}

/* Add two fixed point values */
#define FADD(a, b) (css_add_fixed((a), (b)))
/* Subtract two fixed point values */
#define FSUB(a, b) (css_subtract_fixed((a), (b)))
/* Multiply two fixed point values */
#define FMUL(a, b) (css_multiply_fixed((a), (b)))
/* Divide two fixed point values */
#define FDIV(a, b) (css_divide_fixed((a), (b)))

/* Convert a floating point value to fixed point */
#define FLTTOFIX(a) ((css_fixed) ((a) * (float) (1 << CSS_RADIX_POINT)))
/* Convert a fixed point value to floating point */
#define FIXTOFLT(a) ((float) (a) / (float) (1 << CSS_RADIX_POINT))

/* Convert an integer to a fixed point value */
#define INTTOFIX(a) (css_int_to_fixed(a))
/* Convert a fixed point value to an integer */
#define FIXTOINT(a) ((a) >> CSS_RADIX_POINT)

/* truncate a fixed point value */
#define TRUNCATEFIX(a) (a & ~((1 << CSS_RADIX_POINT)- 1 ))

/* Useful values */
#define F_PI_2	0x00000648	/* 1.5708 (PI/2) */
#define F_PI	0x00000c91	/* 3.1415 (PI) */
#define F_3PI_2	0x000012d9	/* 4.7124 (3PI/2) */
#define F_2PI	0x00001922	/* 6.2831 (2 PI) */

#define F_90	0x00016800	/*  90 */
#define F_180	0x0002d000	/* 180 */
#define F_270	0x00043800	/* 270 */
#define F_360	0x0005a000	/* 360 */

#define F_0_5	0x00000200	/* 0.5 */
#define F_1	0x00000400	/*   1 */
#define F_10	0x00002800	/*  10 */
#define F_72	0x00012000	/*  72 */
#define F_100	0x00019000	/* 100 */
#define F_200	0x00032000	/* 200 */
#define F_255	0x0003FC00	/* 255 */
#define F_300	0x0004b000	/* 300 */
#define F_400	0x00064000	/* 400 */

#ifdef __cplusplus
}
#endif

#endif

