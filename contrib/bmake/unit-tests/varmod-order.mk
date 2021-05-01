# $NetBSD: varmod-order.mk,v 1.5 2020/10/24 08:46:08 rillig Exp $
#
# Tests for the :O variable modifier, which returns the words, sorted in
# ascending order.

NUMBERS=	one two three four five six seven eight nine ten

.if ${NUMBERS:O} != "eight five four nine one seven six ten three two"
.  error ${NUMBERS:O}
.endif

# Unknown modifier "OX"
_:=	${NUMBERS:OX}

# Unknown modifier "OxXX"
_:=	${NUMBERS:OxXX}

all:
	@:;
