# $NetBSD: cond-op-or.mk,v 1.15 2025/06/28 22:39:28 rillig Exp $
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
# of its operands is evaluated.
.if 0 && (!defined(UNDEF) || ${UNDEF})
.endif

# Test combinations of outer '&&' with inner '||', to ensure that the operands
# of the inner '||' are only evaluated if necessary.
DEF=	defined
# expect+1: Variable "UNDEF" is undefined
.if 1 && (!${DEF} || ${UNDEF})
.endif
.if 1 && (${DEF} || ${UNDEF})
.endif
# expect+1: Variable "UNDEF" is undefined
.if 1 && (!${UNDEF} || ${UNDEF})
.endif
# expect+1: Variable "UNDEF" is undefined
.if 1 && (${UNDEF} || ${UNDEF})
.endif
.if 0 && (!${DEF} || ${UNDEF})
.endif
.if 0 && (${DEF} || ${UNDEF})
.endif
.if 0 && (!${UNDEF} || ${UNDEF})
.endif
.if 0 && (${UNDEF} || ${UNDEF})
.endif


# The || operator may be abbreviated as |.  This is not widely known though
# and is also not documented in the manual page.

# expect+1: Unknown operator "|"
.if 0 | 0
.  error
.else
.  error
.endif
# expect+1: Unknown operator "|"
.if !(1 | 0)
.  error
.else
.  error
.endif
# expect+1: Unknown operator "|"
.if !(0 | 1)
.  error
.else
.  error
.endif
# expect+1: Unknown operator "|"
.if !(1 | 1)
.  error
.else
.  error
.endif

# There is no operator '|||'.  The first two '||' form an operator, the third
# '|' forms the next (incomplete) token.
# expect+1: Unknown operator "|"
.if 0 ||| 0
.  error
.else
.  error
.endif

# The '||' operator must be preceded by whitespace, otherwise it becomes part
# of the preceding bare word.  The condition starts with a digit and is thus
# parsed as '"0||" != "" || 0'.
.if 0|| || 0
.else
.  error
.endif
