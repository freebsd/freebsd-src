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
