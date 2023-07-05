# $NetBSD: cond-cmp-numeric-eq.mk,v 1.6 2023/06/01 20:56:35 rillig Exp $
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

# Because an IEEE 754 double can only hold integers with a mantissa of 53
# bits, these two numbers are considered the same.  The 993 is rounded down
# to the 992.
.if 9007199254740993 == 9007199254740992
.else
. error
.endif
# The 995 is rounded up, the 997 is rounded down.
.if 9007199254740995 == 9007199254740997
.else
. error Probably a misconfiguration in the floating point environment, \
	or maybe a machine without IEEE 754 floating point support.
.endif

# There is no = operator for numbers.
# expect+1: Malformed conditional (!(12345 = 12345))
.if !(12345 = 12345)
.  error
.else
.  error
.endif

# There is no === operator for numbers either.
# expect+1: Malformed conditional (!(12345 === 12345))
.if !(12345 === 12345)
.  error
.else
.  error
.endif

all:
	@:;
