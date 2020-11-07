# $NetBSD: cond-short.mk,v 1.11 2020/10/24 08:50:17 rillig Exp $
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

# Irrelevant assignment modifiers are skipped as well.
.if 0 && ${1 2 3:L:@i@${FIRST::?=$i}@}
.endif
.if 0 && ${1 2 3:L:@i@${LAST::=$i}@}
.endif
.if 0 && ${1 2 3:L:@i@${APPENDED::+=$i}@}
.endif
.if 0 && ${echo.1 echo.2 echo.3:L:@i@${RAN::!=${i:C,.*,&; & 1>\&2,:S,., ,g}}@}
.endif
.if defined(FIRST) || defined(LAST) || defined(APPENDED) || defined(RAN)
.  warning first=${FIRST} last=${LAST} appended=${APPENDED} ran=${RAN}
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

# make sure these do not cause complaint
#.MAKEFLAGS: -dc

V42=	42
iV1=	${V42}
iV2=	${V66}

.if defined(V42) && ${V42} > 0
x=	Ok
.else
x=	Fail
.endif
x!=	echo 'defined(V42) && ${V42} > 0: $x' >&2; echo

# this one throws both String comparison operator and
# Malformed conditional with cond.c 1.78
# indirect iV2 would expand to "" and treated as 0
.if defined(V66) && ( ${iV2} < ${V42} )
x=	Fail
.else
x=	Ok
.endif
x!=	echo 'defined(V66) && ( "${iV2}" < ${V42} ): $x' >&2; echo

# next two thow String comparison operator with cond.c 1.78
# indirect iV1 would expand to 42
.if 1 || ${iV1} < ${V42}
x=	Ok
.else
x=	Fail
.endif
x!=	echo '1 || ${iV1} < ${V42}: $x' >&2; echo

.if 1 || ${iV2:U2} < ${V42}
x=	Ok
.else
x=	Fail
.endif
x!=	echo '1 || ${iV2:U2} < ${V42}: $x' >&2; echo

# the same expressions are fine when the lhs is expanded
# ${iV1} expands to 42
.if 0 || ${iV1} <= ${V42}
x=	Ok
.else
x=	Fail
.endif
x!=	echo '0 || ${iV1} <= ${V42}: $x' >&2; echo

# ${iV2:U2} expands to 2
.if 0 || ${iV2:U2} < ${V42}
x=	Ok
.else
x=	Fail
.endif
x!=	echo '0 || ${iV2:U2} < ${V42}: $x' >&2; echo

all:
	@:;:
