# $NetBSD: varmod-order-reverse.mk,v 1.3 2020/08/16 20:13:10 rillig Exp $
#
# Tests for the :Or variable modifier, which returns the words, sorted in
# descending order.

NUMBERS=	one two three four five six seven eight nine ten

.if ${NUMBERS:Or} != "two three ten six seven one nine four five eight"
.error ${NUMBERS:Or}
.endif

all:
	@:;
