# $NetBSD: cond-token-var.mk,v 1.3 2020/08/20 19:43:42 rillig Exp $
#
# Tests for variables in .if conditions.

DEF=	defined

# A defined variable may appear on either side of the comparison.
.if ${DEF} == ${DEF}
.info ok
.else
.error
.endif

# A variable that appears on the left-hand side must be defined.
.if ${UNDEF} == ${DEF}
.error
.endif

# A variable that appears on the right-hand side must be defined.
.if ${DEF} == ${UNDEF}
.error
.endif

# A defined variable may appear as an expression of its own.
.if ${DEF}
.endif

# An undefined variable generates a warning.
.if ${UNDEF}
.endif

# The :U modifier turns an undefined variable into an ordinary expression.
.if ${UNDEF:U}
.endif
