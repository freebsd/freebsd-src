/*-
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef __STDC_VERSION_STDBIT_H__
#define __STDC_VERSION_STDBIT_H__ 202311L

#include <sys/cdefs.h>

/* byte order */
#define  __STDC_ENDIAN_LITTLE__ __ORDER_LITTLE_ENDIAN__
#define  __STDC_ENDIAN_BIG__ __ORDER_BIG_ENDIAN__
#define  __STDC_ENDIAN_NATIVE__ __BYTE_ORDER__

#define __generic_bitfunc(func, x) (_Generic(x,				\
	unsigned char: func ## _uc,					\
	unsigned short: func ## _us,					\
	unsigned int: func ## _ui,					\
	unsigned long: func ## _ul,					\
	unsigned long long: func ## _ull)(x))

__BEGIN_DECLS
unsigned int stdc_leading_zeros_uc(unsigned char) __pure2;
unsigned int stdc_leading_zeros_us(unsigned short) __pure2;
unsigned int stdc_leading_zeros_ui(unsigned int) __pure2;
unsigned int stdc_leading_zeros_ul(unsigned long) __pure2;
unsigned int stdc_leading_zeros_ull(unsigned long long) __pure2;
#define stdc_leading_zeros(x) __generic_bitfunc(stdc_leading_zeros, x)

unsigned int stdc_leading_ones_uc(unsigned char) __pure2;
unsigned int stdc_leading_ones_us(unsigned short) __pure2;
unsigned int stdc_leading_ones_ui(unsigned int) __pure2;
unsigned int stdc_leading_ones_ul(unsigned long) __pure2;
unsigned int stdc_leading_ones_ull(unsigned long long) __pure2;
#define stdc_leading_ones(x) __generic_bitfunc(stdc_leading_ones, x)

unsigned int stdc_trailing_zeros_uc(unsigned char) __pure2;
unsigned int stdc_trailing_zeros_us(unsigned short) __pure2;
unsigned int stdc_trailing_zeros_ui(unsigned int) __pure2;
unsigned int stdc_trailing_zeros_ul(unsigned long) __pure2;
unsigned int stdc_trailing_zeros_ull(unsigned long long) __pure2;
#define stdc_trailing_zeros(x) __generic_bitfunc(stdc_trailing_zeros, x)

unsigned int stdc_trailing_ones_uc(unsigned char) __pure2;
unsigned int stdc_trailing_ones_us(unsigned short) __pure2;
unsigned int stdc_trailing_ones_ui(unsigned int) __pure2;
unsigned int stdc_trailing_ones_ul(unsigned long) __pure2;
unsigned int stdc_trailing_ones_ull(unsigned long long) __pure2;
#define stdc_trailing_ones(x) __generic_bitfunc(stdc_trailing_ones, x)

unsigned int stdc_first_leading_zero_uc(unsigned char) __pure2;
unsigned int stdc_first_leading_zero_us(unsigned short) __pure2;
unsigned int stdc_first_leading_zero_ui(unsigned int) __pure2;
unsigned int stdc_first_leading_zero_ul(unsigned long) __pure2;
unsigned int stdc_first_leading_zero_ull(unsigned long long) __pure2;
#define stdc_first_leading_zero(x) __generic_bitfunc(stdc_first_leading_zero, x)

unsigned int stdc_first_leading_one_uc(unsigned char) __pure2;
unsigned int stdc_first_leading_one_us(unsigned short) __pure2;
unsigned int stdc_first_leading_one_ui(unsigned int) __pure2;
unsigned int stdc_first_leading_one_ul(unsigned long) __pure2;
unsigned int stdc_first_leading_one_ull(unsigned long long) __pure2;
#define stdc_first_leading_one(x) __generic_bitfunc(stdc_first_leading_one, x)

unsigned int stdc_first_trailing_zero_uc(unsigned char) __pure2;
unsigned int stdc_first_trailing_zero_us(unsigned short) __pure2;
unsigned int stdc_first_trailing_zero_ui(unsigned int) __pure2;
unsigned int stdc_first_trailing_zero_ul(unsigned long) __pure2;
unsigned int stdc_first_trailing_zero_ull(unsigned long long) __pure2;
#define stdc_first_trailing_zero(x) __generic_bitfunc(stdc_first_trailing_zero, x)

unsigned int stdc_first_trailing_one_uc(unsigned char) __pure2;
unsigned int stdc_first_trailing_one_us(unsigned short) __pure2;
unsigned int stdc_first_trailing_one_ui(unsigned int) __pure2;
unsigned int stdc_first_trailing_one_ul(unsigned long) __pure2;
unsigned int stdc_first_trailing_one_ull(unsigned long long) __pure2;
#define stdc_first_trailing_one(x) __generic_bitfunc(stdc_first_trailing_one, x)

unsigned int stdc_count_zeros_uc(unsigned char) __pure2;
unsigned int stdc_count_zeros_us(unsigned short) __pure2;
unsigned int stdc_count_zeros_ui(unsigned int) __pure2;
unsigned int stdc_count_zeros_ul(unsigned long) __pure2;
unsigned int stdc_count_zeros_ull(unsigned long long) __pure2;
#define stdc_count_zeros(x) __generic_bitfunc(stdc_count_zeros, x)

unsigned int stdc_count_ones_uc(unsigned char) __pure2;
unsigned int stdc_count_ones_us(unsigned short) __pure2;
unsigned int stdc_count_ones_ui(unsigned int) __pure2;
unsigned int stdc_count_ones_ul(unsigned long) __pure2;
unsigned int stdc_count_ones_ull(unsigned long long) __pure2;
#define stdc_count_ones(x) __generic_bitfunc(stdc_count_ones, x)

_Bool stdc_has_single_bit_uc(unsigned char) __pure2;
_Bool stdc_has_single_bit_us(unsigned short) __pure2;
_Bool stdc_has_single_bit_ui(unsigned int) __pure2;
_Bool stdc_has_single_bit_ul(unsigned long) __pure2;
_Bool stdc_has_single_bit_ull(unsigned long long) __pure2;
#define stdc_has_single_bit(x) __generic_bitfunc(stdc_has_single_bit, x)

unsigned int stdc_bit_width_uc(unsigned char) __pure2;
unsigned int stdc_bit_width_us(unsigned short) __pure2;
unsigned int stdc_bit_width_ui(unsigned int) __pure2;
unsigned int stdc_bit_width_ul(unsigned long) __pure2;
unsigned int stdc_bit_width_ull(unsigned long long) __pure2;
#define stdc_bit_width(x) __generic_bitfunc(stdc_bit_width, x)

unsigned char stdc_bit_floor_uc(unsigned char) __pure2;
unsigned short stdc_bit_floor_us(unsigned short) __pure2;
unsigned stdc_bit_floor_ui(unsigned int) __pure2;
unsigned long stdc_bit_floor_ul(unsigned long) __pure2;
unsigned long long stdc_bit_floor_ull(unsigned long long) __pure2;
#define stdc_bit_floor(x) __generic_bitfunc(stdc_bit_floor, x)

unsigned char stdc_bit_ceil_uc(unsigned char) __pure2;
unsigned short stdc_bit_ceil_us(unsigned short) __pure2;
unsigned int stdc_bit_ceil_ui(unsigned int) __pure2;
unsigned long stdc_bit_ceil_ul(unsigned long) __pure2;
unsigned long long stdc_bit_ceil_ull(unsigned long long) __pure2;
#define stdc_bit_ceil(x) __generic_bitfunc(stdc_bit_ceil, x)
__END_DECLS

#endif /* __STDC_VERSION_STDBIT_H__ */
