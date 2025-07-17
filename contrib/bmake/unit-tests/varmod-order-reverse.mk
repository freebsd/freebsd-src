# $NetBSD: varmod-order-reverse.mk,v 1.5 2021/08/03 04:46:49 rillig Exp $
#
# Tests for the :Or variable modifier, which returns the words, sorted in
# descending order.

WORDS=		one two three four five six seven eight nine ten

.if ${WORDS:Or} != "two three ten six seven one nine four five eight"
.  error ${WORDS:Or}
.endif

all:
