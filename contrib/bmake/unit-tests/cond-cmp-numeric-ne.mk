# $NetBSD: cond-cmp-numeric-ne.mk,v 1.3 2023/09/07 05:36:33 rillig Exp $
#
# Tests for numeric comparisons with the != operator in .if conditions.

# When both sides are equal, the != operator always yields false.
.if 1 != 1
.  error
.endif

# This comparison yields the same result, whether numeric or character-based.
.if 1 != 2
.else
.  error
.endif

.if 2 != 1
.else
.  error
.endif

# Scientific notation is supported, as per strtod.
.if 2e7 != 2000e4
.  error
.endif

.if 2000e4 != 2e7
.  error
.endif

# Trailing zeroes after the decimal point are irrelevant for the numeric
# value.
.if 3.30000 != 3.3
.  error
.endif

.if 3.3 != 3.30000
.  error
.endif

# Numeric comparison works by parsing both sides
# as double, and then performing a normal comparison.  The range of double is
# typically 16 or 17 significant digits, therefore these two numbers seem to
# be equal.
.if 1.000000000000000001 != 1.000000000000000002
.  error
.endif

all:
	@:;
