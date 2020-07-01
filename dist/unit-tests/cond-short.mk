# $NetBSD: cond-short.mk,v 1.2 2020/06/28 11:06:26 rillig Exp $
#
# Demonstrates that in conditions, the right-hand side of an && or ||
# is only evaluated if it can actually influence the result.
#
# Between 2015-10-11 and 2020-06-28, the right-hand side of an && or ||
# operator was always evaluated, which was wrong.
#

# The && operator.

.if 0 && ${echo "unexpected and" 1>&2 :L:sh}
.endif

.if 1 && ${echo "expected and" 1>&2 :L:sh}
.endif

.if 0 && exists(nonexistent${echo "unexpected and exists" 1>&2 :L:sh})
.endif

.if 1 && exists(nonexistent${echo "expected and exists" 1>&2 :L:sh})
.endif

.if 0 && empty(${echo "unexpected and empty" 1>&2 :L:sh})
.endif

.if 1 && empty(${echo "expected and empty" 1>&2 :L:sh})
.endif

# The || operator.

.if 1 || ${echo "unexpected or" 1>&2 :L:sh}
.endif

.if 0 || ${echo "expected or" 1>&2 :L:sh}
.endif

.if 1 || exists(nonexistent${echo "unexpected or exists" 1>&2 :L:sh})
.endif

.if 0 || exists(nonexistent${echo "expected or exists" 1>&2 :L:sh})
.endif

.if 1 || empty(${echo "unexpected or empty" 1>&2 :L:sh})
.endif

.if 0 || empty(${echo "expected or empty" 1>&2 :L:sh})
.endif

# Unreachable nested conditions are skipped completely as well.

.if 0
.  if ${echo "unexpected nested and" 1>&2 :L:sh}
.  endif
.endif

.if 1
.elif ${echo "unexpected nested or" 1>&2 :L:sh}
.endif

all:
	@:;:
