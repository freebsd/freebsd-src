# $NetBSD: cond-op-not.mk,v 1.3 2020/08/28 14:48:37 rillig Exp $
#
# Tests for the ! operator in .if conditions.

# The exclamation mark negates its operand.
.if !1
.error
.endif

# Exclamation marks can be chained.
# This doesn't happen in practice though.
.if !!!1
.error
.endif

# The ! binds more tightly than the &&.
.if !!0 && 1
.error
.endif

all:
	@:;
