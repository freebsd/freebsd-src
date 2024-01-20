# $NetBSD: cond-op-and.mk,v 1.9 2023/12/17 09:44:00 rillig Exp $
#
# Tests for the && operator in .if conditions.

.if 0 && 0
.  error
.endif

.if 1 && 0
.  error
.endif

.if 0 && 1
.  error
.endif

.if !(1 && 1)
.  error
.endif


# The right-hand side is not evaluated since the left-hand side is already
# false.
.if 0 && ${UNDEF}
.endif

# When an outer condition makes the inner '&&' condition irrelevant, neither
# of its operands must be evaluated.
#
.if 1 || (${UNDEF} && ${UNDEF})
.endif

# Test combinations of outer '||' with inner '&&', to ensure that the operands
# of the inner '&&' are only evaluated if necessary.
DEF=	defined
# expect+1: Malformed conditional (0 || (${DEF} && ${UNDEF}))
.if 0 || (${DEF} && ${UNDEF})
.endif
.if 0 || (!${DEF} && ${UNDEF})
.endif
# expect+1: Malformed conditional (0 || (${UNDEF} && ${UNDEF}))
.if 0 || (${UNDEF} && ${UNDEF})
.endif
# expect+1: Malformed conditional (0 || (!${UNDEF} && ${UNDEF}))
.if 0 || (!${UNDEF} && ${UNDEF})
.endif
.if 1 || (${DEF} && ${UNDEF})
.endif
.if 1 || (!${DEF} && ${UNDEF})
.endif
.if 1 || (${UNDEF} && ${UNDEF})
.endif
.if 1 || (!${UNDEF} && ${UNDEF})
.endif


# The && operator may be abbreviated as &.  This is not widely known though
# and is also not documented in the manual page.

.if 0 & 0
.  error
.endif
.if 1 & 0
.  error
.endif
.if 0 & 1
.  error
.endif
.if !(1 & 1)
.  error
.endif

# There is no operator &&&.
# expect+1: Malformed conditional (0 &&& 0)
.if 0 &&& 0
.  error
.endif

# The '&&' operator must be preceded by whitespace, otherwise it becomes part
# of the preceding bare word.  The condition is parsed as '"1&&" != "" && 1'.
.if 1&& && 1
.else
.  error
.endif
