# $NetBSD: cond-cmp-unary.mk,v 1.2 2020/11/11 07:30:11 rillig Exp $
#
# Tests for unary comparisons in .if conditions, that is, comparisons with
# a single operand.  If the operand is a number, it is compared to zero,
# if it is a string, it is tested for emptiness.

# The number 0 evaluates to false.
.if 0
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
# XXX: As of 2020-11-11, this empty string is interpreted "as a number" in
# EvalNotEmpty, which is plain wrong.  The bug is in TryParseNumber.
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
# XXX: As of 2020-11-11, the implementation in EvalNotEmpty does not skip
# whitespace before testing for the end.  This was probably an oversight in
# a commit from 1992-04-15 saying "A variable is empty when it just contains
# spaces".
.if ${:U   }
.  info This is only reached because of a bug in EvalNotEmpty.
.else
.  error
.endif

all: # nothing
