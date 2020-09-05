# $NetBSD: varmod-order.mk,v 1.4 2020/08/16 20:43:01 rillig Exp $
#
# Tests for the :O variable modifier, which returns the words, sorted in
# ascending order.

NUMBERS=	one two three four five six seven eight nine ten

.if ${NUMBERS:O} != "eight five four nine one seven six ten three two"
.error ${NUMBERS:O}
.endif

# Unknown modifier "OX"
_:=	${NUMBERS:OX}

# Unknown modifier "OxXX"
_:=	${NUMBERS:OxXX}

all:
	@:;
