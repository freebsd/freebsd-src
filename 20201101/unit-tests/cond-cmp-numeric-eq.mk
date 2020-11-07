# $NetBSD: cond-cmp-numeric-eq.mk,v 1.4 2020/10/24 08:46:08 rillig Exp $
#
# Tests for numeric comparisons with the == operator in .if conditions.

# This comparison yields the same result, whether numeric or character-based.
.if 1 == 1
.else
.  error
.endif

# This comparison yields the same result, whether numeric or character-based.
.if 1 == 2
.  error
.endif

.if 2 == 1
.  error
.endif

# Scientific notation is supported, as per strtod.
.if 2e7 == 2000e4
.else
.  error
.endif

.if 2000e4 == 2e7
.else
.  error
.endif

# Trailing zeroes after the decimal point are irrelevant for the numeric
# value.
.if 3.30000 == 3.3
.else
.  error
.endif

.if 3.3 == 3.30000
.else
.  error
.endif

# As of 2020-08-23, numeric comparison is implemented as parsing both sides
# as double, and then performing a normal comparison.  The range of double is
# typically 16 or 17 significant digits, therefore these two numbers seem to
# be equal.
.if 1.000000000000000001 == 1.000000000000000002
.else
.  error
.endif


# There is no = operator for numbers.
.if !(12345 = 12345)
.  error
.else
.  error
.endif

# There is no === operator for numbers either.
.if !(12345 === 12345)
.  error
.else
.  error
.endif

all:
	@:;
