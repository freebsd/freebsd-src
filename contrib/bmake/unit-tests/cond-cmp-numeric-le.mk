# $NetBSD: cond-cmp-numeric-le.mk,v 1.2 2020/10/24 08:46:08 rillig Exp $
#
# Tests for numeric comparisons with the <= operator in .if conditions.

# When both sides are equal, the <= operator always yields true.
.if 1 <= 1
.else
.  error
.endif

# This comparison yields the same result, whether numeric or character-based.
.if 1 <= 2
.else
.  error
.endif

.if 2 <= 1
.  error
.endif

# If this comparison were character-based instead of numerical, the
# 5 would be >= 14 since its first digit is greater.
.if 5 <= 14
.else
.  error
.endif

.if 14 <= 5
.  error
.endif

# Scientific notation is supported, as per strtod.
.if 2e7 <= 1e8
.else
.  error
.endif

.if 1e8 <= 2e7
.  error
.endif

# Floating pointer numbers can be compared as well.
# This might be tempting to use for version numbers, but there are a few pitfalls.
.if 3.141 <= 111.222
.else
.  error
.endif

.if 111.222 <= 3.141
.  error
.endif

# When parsed as a version number, 3.30 is greater than 3.7.
# Since make parses numbers as plain numbers, that leads to wrong results.
# Numeric comparisons are not suited for comparing version number.
.if 3.30 <= 3.7
.else
.  error
.endif

.if 3.7 <= 3.30
.  error
.endif

# As of 2020-08-23, numeric comparison is implemented as parsing both sides
# as double, and then performing a normal comparison.  The range of double is
# typically 16 or 17 significant digits, therefore these two numbers seem to
# be equal.
.if 1.000000000000000001 <= 1.000000000000000002
.else
.  error
.endif

all:
	@:;
