# $NetBSD: cond-cmp-unary.mk,v 1.1 2020/09/14 06:22:59 rillig Exp $
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

all: # nothing
