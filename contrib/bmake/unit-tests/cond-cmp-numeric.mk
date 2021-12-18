# $NetBSD: cond-cmp-numeric.mk,v 1.5 2021/07/29 06:31:18 rillig Exp $
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

# The parsing code in CondParser_Comparison only performs a light check on
# whether the operator is valid, leaving the rest of the work to the
# evaluation functions EvalCompareNum and EvalCompareStr.  Ensure that this
# parse error is properly reported.
#
# XXX: The warning message does not mention the actual operator.
.if 123 ! 123
.  error
.else
.  error
.endif

# Leading spaces are allowed for numbers.
# See EvalCompare and TryParseNumber.
.if ${:U 123} < 124
.else
.  error
.endif

# Trailing spaces are NOT allowed for numbers.
# See EvalCompare and TryParseNumber.
# expect+1: String comparison operator must be either == or !=
.if ${:U123 } < 124
.  error
.else
.  error
.endif

all:
	@:;
