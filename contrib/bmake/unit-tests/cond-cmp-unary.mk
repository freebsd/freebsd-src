# $NetBSD: cond-cmp-unary.mk,v 1.5 2023/06/01 20:56:35 rillig Exp $
#
# Tests for unary comparisons in .if conditions, that is, comparisons with
# a single operand.  If the operand is a number, it is compared to zero,
# if it is a string, it is tested for emptiness.

# The number 0 in all its various representations evaluates to false.
.if 0 || 0.0 || 0e0 || 0.0e0 || 0.0e10
.  error
.endif

# Any other number evaluates to true.
.if !12345
.  error
.endif

# The empty string evaluates to false.
.if ""
.  error
.endif

# Any other string evaluates to true.
.if !"0"
.  error
.endif

# The empty string may come from a variable expression.
#
# XXX: As of 2023-06-01, this empty string is interpreted "as a number" in
# EvalTruthy, which is plain wrong.  The bug is in TryParseNumber.
.if ${:U}
.  error
.endif

# A variable expression that is not surrounded by quotes is interpreted
# as a number if possible, otherwise as a string.
.if ${:U0}
.  error
.endif

# A non-zero number from a variable expression evaluates to true.
.if !${:U12345}
.  error
.endif

# A string of whitespace should evaluate to false.
#
# XXX: As of 2023-06-01, the implementation in EvalTruthy does not skip
# whitespace before testing for the end.  This was probably an oversight in
# a commit from 1992-04-15 saying "A variable is empty when it just contains
# spaces".
.if ${:U   }
# expect+1: This is only reached because of a bug in EvalTruthy.
.  info This is only reached because of a bug in EvalTruthy.
.else
.  error
.endif

# The condition '${VAR:M*}' is almost equivalent to '${VAR:M*} != ""'.  The
# only case where they differ is for a single word whose numeric value is zero.
.if ${:U0:M*}
.  error
.endif
.if ${:U0:M*} == ""
.  error
.endif
# Multiple words cannot be parsed as a single number, thus evaluating to true.
.if !${:U0 0:M*}
.  error
.endif
.if ${:U0 0:M*} == ""
.  error
.endif

all: # nothing
