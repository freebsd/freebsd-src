# $NetBSD: cond-op-not.mk,v 1.10 2025/06/28 22:39:28 rillig Exp $
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
# expect+1: Not empty evaluates to true.
.  info Not empty evaluates to true.
.else
.  info Not empty evaluates to false.
.endif

.if !${:U }
.  info Not space evaluates to true.
.else
# expect+1: Not space evaluates to false.
.  info Not space evaluates to false.
.endif

.if !${:U0}
# expect+1: Not 0 evaluates to true.
.  info Not 0 evaluates to true.
.else
.  info Not 0 evaluates to false.
.endif

.if !${:U1}
.  info Not 1 evaluates to true.
.else
# expect+1: Not 1 evaluates to false.
.  info Not 1 evaluates to false.
.endif

.if !${:Uword}
.  info Not word evaluates to true.
.else
# expect+1: Not word evaluates to false.
.  info Not word evaluates to false.
.endif

# A single exclamation mark is a parse error.
# expect+1: Malformed conditional "!"
.if !
.  error
.else
.  error
.endif

all:
	@:;
