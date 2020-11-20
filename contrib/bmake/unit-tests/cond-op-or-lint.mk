# $NetBSD: cond-op-or-lint.mk,v 1.1 2020/11/08 23:54:28 rillig Exp $
#
# Tests for the || operator in .if conditions, in lint mode.

.MAKEFLAGS: -dL

# The '|' operator is not allowed in lint mode.
# It is not used in practice anyway.
.if 0 | 0
.  error
.else
.  error
.endif
