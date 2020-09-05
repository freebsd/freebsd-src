# $NetBSD: cond-op-and.mk,v 1.3 2020/08/28 14:48:37 rillig Exp $
#
# Tests for the && operator in .if conditions.

.if 0 && 0
.error
.endif

.if 1 && 0
.error
.endif

.if 0 && 1
.error
.endif

.if !(1 && 1)
.error
.endif

# The right-hand side is not evaluated since the left-hand side is already
# false.
.if 0 && ${UNDEF}
.endif

all:
	@:;
