# $NetBSD: cond-op-or.mk,v 1.9 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the || operator in .if conditions.

.if 0 || 0
.  error
.endif

.if !(1 || 0)
.  error
.endif

.if !(0 || 1)
.  error
.endif

.if !(1 || 1)
.  error
.endif


# The right-hand side is not evaluated since the left-hand side is already
# true.
.if 1 || ${UNDEF}
.endif

# When an outer condition makes the inner '||' condition irrelevant, neither
# of its operands must be evaluated.  This had been wrong in cond.c 1.283 from
# 2021-12-09 and was reverted in cond.c 1.284 an hour later.
.if 0 && (!defined(UNDEF) || ${UNDEF})
.endif

# Test combinations of outer '&&' with inner '||', to ensure that the operands
# of the inner '||' is only evaluated if necessary.
DEF=	defined
.if 0 && (${DEF} || ${UNDEF})
.endif
.if 0 && (!${DEF} || ${UNDEF})
.endif
.if 0 && (${UNDEF} || ${UNDEF})
.endif
.if 0 && (!${UNDEF} || ${UNDEF})
.endif
.if 1 && (${DEF} || ${UNDEF})
.endif
# expect+1: Malformed conditional (1 && (!${DEF} || ${UNDEF})
.if 1 && (!${DEF} || ${UNDEF})
.endif
# expect+1: Malformed conditional (1 && (${UNDEF} || ${UNDEF})
.if 1 && (${UNDEF} || ${UNDEF})
.endif
# expect+1: Malformed conditional (1 && (!${UNDEF} || ${UNDEF})
.if 1 && (!${UNDEF} || ${UNDEF})
.endif


# The || operator may be abbreviated as |.  This is not widely known though
# and is also not documented in the manual page.

.if 0 | 0
.  error
.endif
.if !(1 | 0)
.  error
.endif
.if !(0 | 1)
.  error
.endif
.if !(1 | 1)
.  error
.endif

# There is no operator |||.
# expect+1: Malformed conditional (0 ||| 0)
.if 0 ||| 0
.  error
.endif

all:
	@:;
