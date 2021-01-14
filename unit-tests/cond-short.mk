# $NetBSD: cond-short.mk,v 1.15 2020/12/01 19:37:23 rillig Exp $
#
# Demonstrates that in conditions, the right-hand side of an && or ||
# is only evaluated if it can actually influence the result.
# This is called 'short-circuit evaluation' and is the usual evaluation
# mode in most programming languages.  A notable exception is Ada, which
# distinguishes between the operators 'And', 'And Then', 'Or', 'Or Else'.
#
# Before 2020-06-28, the right-hand side of an && or || operator was always
# evaluated, which was wrong.  In cond.c 1.69 and var.c 1.197 on 2015-10-11,
# Var_Parse got a new parameter named 'wantit'.  Since then it would have been
# possible to skip evaluation of irrelevant variable expressions and only
# parse them.  They were still evaluated though, the only difference to
# relevant variable expressions was that in the irrelevant variable
# expressions, undefined variables were allowed.

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

# TODO: Rewrite this whole section and check all the conditions and variables.
# Several of the assumptions are probably wrong here.
# TODO: replace 'x=' with '.info' or '.error'.
V42=	42
iV1=	${V42}
iV2=	${V66}

.if defined(V42) && ${V42} > 0
x=	Ok
.else
x=	Fail
.endif
x!=	echo 'defined(V42) && $${V42} > 0: $x' >&2; echo

# With cond.c 1.76 from 2020-07-03, the following condition triggered a
# warning: "String comparison operator should be either == or !=".
# This was because the variable expression ${iV2} was defined, but the
# contained variable V66 was undefined.  The left-hand side of the comparison
# therefore evaluated to the string "${V66}", which is obviously not a number.
#
# This was fixed in cond.c 1.79 from 2020-07-09 by not evaluating irrelevant
# comparisons.  Instead, they are only parsed and then discarded.
#
# At that time, there was not enough debug logging to see the details in the
# -dA log.  To actually see it, add debug logging at the beginning and end of
# Var_Parse.
.if defined(V66) && ( ${iV2} < ${V42} )
x=	Fail
.else
x=	Ok
.endif
# XXX: This condition doesn't match the one above. The quotes are missing
# above.  This is a crucial detail since without quotes, the variable
# expression ${iV2} evaluates to "${V66}", and with quotes, it evaluates to ""
# since undefined variables are allowed and expand to an empty string.
x!=	echo 'defined(V66) && ( "$${iV2}" < $${V42} ): $x' >&2; echo

.if 1 || ${iV1} < ${V42}
x=	Ok
.else
x=	Fail
.endif
x!=	echo '1 || $${iV1} < $${V42}: $x' >&2; echo

# With cond.c 1.76 from 2020-07-03, the following condition triggered a
# warning: "String comparison operator should be either == or !=".
# This was because the variable expression ${iV2} was defined, but the
# contained variable V66 was undefined.  The left-hand side of the comparison
# therefore evaluated to the string "${V66}", which is obviously not a number.
#
# This was fixed in cond.c 1.79 from 2020-07-09 by not evaluating irrelevant
# comparisons.  Instead, they are only parsed and then discarded.
#
# At that time, there was not enough debug logging to see the details in the
# -dA log.  To actually see it, add debug logging at the beginning and end of
# Var_Parse.
.if 1 || ${iV2:U2} < ${V42}
x=	Ok
.else
x=	Fail
.endif
x!=	echo '1 || $${iV2:U2} < $${V42}: $x' >&2; echo

# the same expressions are fine when the lhs is expanded
# ${iV1} expands to 42
.if 0 || ${iV1} <= ${V42}
x=	Ok
.else
x=	Fail
.endif
x!=	echo '0 || $${iV1} <= $${V42}: $x' >&2; echo

# ${iV2:U2} expands to 2
.if 0 || ${iV2:U2} < ${V42}
x=	Ok
.else
x=	Fail
.endif
x!=	echo '0 || $${iV2:U2} < $${V42}: $x' >&2; echo

# The right-hand side of the '&&' is irrelevant since the left-hand side
# already evaluates to false.  Before cond.c 1.79 from 2020-07-09, it was
# expanded nevertheless, although with a small modification:  undefined
# variables may be used in these expressions without generating an error.
.if defined(UNDEF) && ${UNDEF} != "undefined"
.  error
.endif

# TODO: Test each modifier to make sure it is skipped when it is irrelevant
# for the result.  Since this test is already quite long, do that in another
# test.

all:
	@:;:
