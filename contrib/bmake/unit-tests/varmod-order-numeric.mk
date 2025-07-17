# $NetBSD: varmod-order-numeric.mk,v 1.8 2022/09/27 19:18:45 rillig Exp $
#
# Tests for the variable modifiers ':On', which returns the words, sorted in
# ascending numeric order, and for ':Orn' and ':Onr', which additionally
# reverse the order.
#
# The variable modifiers ':On', ':Onr' and ':Orn' were added in var.c 1.939
# from 2021-07-30.

# This list contains only 32-bit numbers since the make code needs to conform
# to C90, which does not necessarily provide integer types larger than 32 bit.
# Make uses 'long long' for C99 or later, and 'long' for older C versions.
#
# To get 53-bit integers even in C90, it would be possible to switch to
# 'double' instead, but that would allow floating-point numbers as well, which
# is out of scope for this variable modifier.
NUMBERS=	3 5 7 1 42 -42 5K -3m 1M 1k -2G

.if ${NUMBERS:On} != "-2G -3m -42 1 3 5 7 42 1k 5K 1M"
.  error ${NUMBERS:On}
.endif

.if ${NUMBERS:Orn} != "1M 5K 1k 42 7 5 3 1 -42 -3m -2G"
.  error ${NUMBERS:Orn}
.endif

# Both ':Onr' and ':Orn' have the same effect.
.if ${NUMBERS:Onr} != "1M 5K 1k 42 7 5 3 1 -42 -3m -2G"
.  error ${NUMBERS:Onr}
.endif

# Duplicate numbers are preserved in the output.  In this case the
# equal-valued numbers are spelled the same, so they are indistinguishable in
# the output.
DUPLICATES=	3 1 2 2 1 1	# subsequence of https://oeis.org/A034002
.if ${DUPLICATES:On} != "1 1 1 2 2 3"
.  error ${DUPLICATES:On}
.endif

# If there are several numbers that have the same integer value, they are
# returned in unspecified order.
SAME_VALUE:=	${:U 79 80 0x0050 81 :On}
.if ${SAME_VALUE} != "79 80 0x0050 81" && ${SAME_VALUE} != "79 0x0050 80 81"
.  error ${SAME_VALUE}
.endif

# Hexadecimal and octal numbers can be sorted as well.
MIXED_BASE=	0 010 0x7 9
.if ${MIXED_BASE:On} != "0 0x7 010 9"
.  error ${MIXED_BASE:On}
.endif

# The measurement units for suffixes are k, M, G, but not T.
# The string '3T' evaluates to 3, the string 'x' evaluates as '0'.
.if ${4 3T 2M x:L:On} != "x 3T 4 2M"
.  error
.endif

all:
