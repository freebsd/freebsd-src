# $NetBSD: varmod-order-string.mk,v 1.2 2021/08/03 04:46:49 rillig Exp $
#
# Tests for the :O variable modifier, which returns the words, sorted in
# ascending order.

# Simple words are sorted lexicographically.
WORDS=		one two three four five six seven eight nine ten
.if ${WORDS:O} != "eight five four nine one seven six ten three two"
.  error ${WORDS:O}
.endif

# Double quotes and single quotes delimit words, while backticks are just
# regular characters.  Therefore '`in' is a separate word from 'backticks`',
# and the additional spaces between them are removed.
QUOTED_WORDS=	none "double   quoted" 'single   quoted' `in   backticks`
.if ${QUOTED_WORDS:O} != "\"double   quoted\" 'single   quoted' `in backticks` none"
.  error ${QUOTED_WORDS:O}
.endif

# Numbers are sorted lexicographically as well.
# To sort the words numerically, use ':On' instead; since var.c 1.939 from
# 2021-07-30.
NUMBERS=	-100g -50m -7k -50 -13 0 000 13 50 5k1 7k 50m 100G
.if ${NUMBERS:O} != "-100g -13 -50 -50m -7k 0 000 100G 13 50 50m 5k1 7k"
.  error ${NUMBERS:O}
.endif

all:
