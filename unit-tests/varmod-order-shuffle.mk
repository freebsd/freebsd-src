# $NetBSD: varmod-order-shuffle.mk,v 1.5 2020/10/24 08:46:08 rillig Exp $
#
# Tests for the :Ox variable modifier, which returns the words of the
# variable, shuffled.
#
# As of 2020-08-16, make uses random(3) seeded by the current time in seconds.
# This makes the random numbers completely predictable since there is no other
# part of make that uses random numbers.
#
# Tags: probabilistic

NUMBERS=	one two three four five six seven eight nine ten

# Note that 1 in every 10! trials two independently generated
# randomized orderings will be the same.  The test framework doesn't
# support checking probabilistic output, so we accept that each of the
# 3 :Ox tests will incorrectly fail with probability 2.756E-7, which
# lets the whole test fail once in 1.209.600 runs, on average.

# Create two shuffles using the := assignment operator.
shuffled1:=	${NUMBERS:Ox}
shuffled2:=	${NUMBERS:Ox}
.if ${shuffled1} == ${shuffled2}
.  error ${shuffled1} == ${shuffled2}
.endif

# Sorting the list before shuffling it has no effect.
shuffled1:=	${NUMBERS:O:Ox}
shuffled2:=	${NUMBERS:O:Ox}
.if ${shuffled1} == ${shuffled2}
.  error ${shuffled1} == ${shuffled2}
.endif

# Sorting after shuffling must produce the original numbers.
sorted:=	${NUMBERS:Ox:O}
.if ${sorted} != ${NUMBERS:O}
.  error ${sorted} != ${NUMBERS:O}
.endif

all:
	@:;
