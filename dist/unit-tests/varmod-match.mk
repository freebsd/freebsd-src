# $NetBSD: varmod-match.mk,v 1.3 2020/08/16 20:03:53 rillig Exp $
#
# Tests for the :M variable modifier, which filters words that match the
# given pattern.

all: match-char-class
all: slow


NUMBERS=	One Two Three Four five six seven

match-char-class:
	@echo '$@:'
	@echo '  uppercase numbers: ${NUMBERS:M[A-Z]*}'
	@echo '  all the others: ${NUMBERS:M[^A-Z]*}'
	@echo '  starts with non-s, ends with [ex]: ${NUMBERS:M[^s]*[ex]}'


# Before 2020-06-13, this expression took quite a long time in Str_Match,
# calling itself 601080390 times for 16 asterisks.
slow:
	@: ${:U****************:M****************b}
