# $NetBSD: cond-op-not.mk,v 1.6 2020/11/15 14:58:14 rillig Exp $
#
# Tests for the ! operator in .if conditions, which negates its argument.

# The exclamation mark negates its operand.
.if !1
.  error
.endif

# Exclamation marks can be chained.
# This doesn't happen in practice though.
.if !!!1
.  error
.endif

# The ! binds more tightly than the &&.
.if !!0 && 1
.  error
.endif

# The operator '==' binds more tightly than '!'.
# This is unusual since most other programming languages define the precedence
# to be the other way round.
.if !${:Uexpression} == "expression"
.  error
.endif

.if !${:U}
.  info Not empty evaluates to true.
.else
.  info Not empty evaluates to false.
.endif

.if !${:U }
.  info Not space evaluates to true.
.else
.  info Not space evaluates to false.
.endif

.if !${:U0}
.  info Not 0 evaluates to true.
.else
.  info Not 0 evaluates to false.
.endif

.if !${:U1}
.  info Not 1 evaluates to true.
.else
.  info Not 1 evaluates to false.
.endif

.if !${:Uword}
.  info Not word evaluates to true.
.else
.  info Not word evaluates to false.
.endif

all:
	@:;
