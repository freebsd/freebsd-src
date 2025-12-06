# sources for <stdbit.h>
.PATH: ${LIBC_SRCTOP}/stdbit

SRCS+=	stdc_bit_ceil.c \
	stdc_bit_floor.c \
	stdc_bit_width.c \
	stdc_count_ones.c \
	stdc_count_zeros.c \
	stdc_first_leading_one.c \
	stdc_first_leading_zero.c \
	stdc_first_trailing_one.c \
	stdc_first_trailing_zero.c \
	stdc_has_single_bit.c \
	stdc_leading_ones.c \
	stdc_leading_zeros.c \
	stdc_trailing_ones.c \
	stdc_trailing_zeros.c

SYM_MAPS+=${LIBC_SRCTOP}/stdbit/Symbol.map

MAN+=	stdc_bit_ceil.3 \
	stdc_bit_floor.3 \
	stdc_bit_width.3 \
	stdc_count_ones.3 \
	stdc_count_zeros.3 \
	stdc_first_leading_one.3 \
	stdc_first_leading_zero.3 \
	stdc_first_trailing_one.3 \
	stdc_first_trailing_zero.3 \
	stdc_has_single_bit.3 \
	stdc_leading_ones.3 \
	stdc_leading_zeros.3 \
	stdc_trailing_ones.3 \
	stdc_trailing_zeros.3
