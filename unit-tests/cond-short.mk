# $NetBSD: cond-short.mk,v 1.6 2020/07/02 16:37:56 rillig Exp $
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

# "VAR U11" is not evaluated; it was evaluated before 2020-07-02.
# The whole !empty condition is only parsed and then discarded.
VAR=	${VAR${:U11${echo "unexpected VAR U11" 1>&2 :L:sh}}}
VAR13=	${VAR${:U12${echo "unexpected VAR13" 1>&2 :L:sh}}}
.if 0 && !empty(VAR${:U13${echo "unexpected U13 condition" 1>&2 :L:sh}})
.endif

VAR=	${VAR${:U21${echo "unexpected VAR U21" 1>&2 :L:sh}}}
VAR23=	${VAR${:U22${echo   "expected VAR23" 1>&2 :L:sh}}}
.if 1 && !empty(VAR${:U23${echo   "expected U23 condition" 1>&2 :L:sh}})
.endif
VAR=	# empty again, for the following tests

# The :M modifier is only parsed, not evaluated.
# Before 2020-07-02, it was wrongly evaluated.
.if 0 && !empty(VAR:M${:U${echo "unexpected M pattern" 1>&2 :L:sh}})
.endif

.if 1 && !empty(VAR:M${:U${echo   "expected M pattern" 1>&2 :L:sh}})
.endif

.if 0 && !empty(VAR:S,from,${:U${echo "unexpected S modifier" 1>&2 :L:sh}},)
.endif

.if 0 && !empty(VAR:C,from,${:U${echo "unexpected C modifier" 1>&2 :L:sh}},)
.endif

.if 0 && !empty("" == "" :? ${:U${echo "unexpected ? modifier" 1>&2 :L:sh}} :)
.endif

.if 0 && !empty(VAR:old=${:U${echo "unexpected = modifier" 1>&2 :L:sh}})
.endif

.if 0 && !empty(1 2 3:L:@var@${:U${echo "unexpected @ modifier" 1>&2 :L:sh}}@)
.endif

.if 0 && !empty(:U${:!echo "unexpected exclam modifier" 1>&2 !})
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
