# $NetBSD: cond-cmp-numeric.mk,v 1.3 2020/09/12 18:01:51 rillig Exp $
#
# Tests for numeric comparisons in .if conditions.

.MAKEFLAGS: -dc

# The ${:U...} on the left-hand side is necessary for the parser.

# Even if strtod(3) parses "INF" as +Infinity, make does not accept this
# since it is not really a number; see TryParseNumber.
.if !(${:UINF} > 1e100)
.  error
.endif

# Neither is NaN a number; see TryParseNumber.
.if ${:UNaN} > NaN
.  error
.endif

# Since NaN is not parsed as a number, both operands are interpreted
# as strings and are therefore equal.  If they were parsed as numbers,
# they would compare unequal, since NaN is unequal to any and everything,
# including itself.
.if !(${:UNaN} == NaN)
.  error
.endif

all:
	@:;
