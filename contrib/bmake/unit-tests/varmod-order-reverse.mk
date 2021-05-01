# $NetBSD: varmod-order-reverse.mk,v 1.4 2020/10/24 08:46:08 rillig Exp $
#
# Tests for the :Or variable modifier, which returns the words, sorted in
# descending order.

NUMBERS=	one two three four five six seven eight nine ten

.if ${NUMBERS:Or} != "two three ten six seven one nine four five eight"
.  error ${NUMBERS:Or}
.endif

all:
	@:;
