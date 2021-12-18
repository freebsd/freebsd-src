# $NetBSD: cond-op-and.mk,v 1.6 2021/12/10 19:14:35 rillig Exp $
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
.if 0 || (${DEF} && ${UNDEF})
.endif
.if 0 || (!${DEF} && ${UNDEF})
.endif
.if 0 || (${UNDEF} && ${UNDEF})
.endif
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
.if 0 &&& 0
.  error
.endif

all:
	@:;
