# $NetBSD: cond-short.mk,v 1.24 2025/06/28 22:39:28 rillig Exp $
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
# possible to skip evaluation of irrelevant expressions and only
# parse them.  They were still evaluated though, the only difference to
# relevant expressions was that in the irrelevant
# expressions, undefined variables were allowed.  This allowed for conditions
# like 'defined(VAR) && ${VAR:S,from,to,} != ""', which no longer produced an
# error message 'Malformed conditional', but the irrelevant expression was
# still evaluated.
#
# Since the initial commit on 1993-03-21, the manual page has been saying that
# make 'will only evaluate a conditional as far as is necessary to determine',
# but that was wrong.  The code in cond.c 1.1 from 1993-03-21 looks good since
# it calls Var_Parse(condExpr, VAR_CMD, doEval,&varSpecLen,&doFree), but the
# definition of Var_Parse did not call the third parameter 'doEval', as would
# be expected, but instead 'err', accompanied by the comment 'TRUE if
# undefined variables are an error'.  This subtle difference between 'do not
# evaluate at all' and 'allow undefined variables' led to the unexpected
# evaluation.
#
# See also:
#	var-eval-short.mk, for short-circuited variable modifiers

# The && operator:

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

# The || operator:

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

# Unreachable nested conditions are skipped completely as well.  These skipped
# lines may even contain syntax errors.  This allows to skip syntactically
# incompatible new features in older versions of make.

.if 0
.  if ${echo "unexpected nested and" 1>&2 :L:sh}
.  endif
.endif

.if 1
.elif ${echo "unexpected nested or" 1>&2 :L:sh}
.endif


NUMBER=		42
INDIR_NUMBER=	${NUMBER}
INDIR_UNDEF=	${UNDEF}

.if defined(NUMBER) && ${NUMBER} > 0
.else
.  error
.endif

# Starting with var.c 1.226 from from 2020-07-02, the following condition
# triggered a warning: "String comparison operator should be either == or !=".
#
# The left-hand side of the '&&' evaluated to false, which should have made
# the right-hand side irrelevant.
#
# On the right-hand side of the '&&', the expression ${INDIR_UNDEF} was
# defined and had the value '${UNDEF}', but the nested variable UNDEF was
# undefined.  The right hand side "${INDIR_UNDEF}" still needed to be parsed,
# and in parse-only mode, the "value" of the parsed expression was the
# uninterpreted variable value, in this case '${UNDEF}'.  And even though the
# right hand side of the '&&' should have been irrelevant, the two sides of
# the comparison were still parsed and evaluated.  Comparing these two values
# numerically was not possible since the string '${UNDEF}' is not a number,
# so the comparison fell back to string comparison, which then complained
# about the '>' operator.
#
# This was fixed in cond.c 1.79 from 2020-07-09 by not evaluating irrelevant
# comparisons.  Instead, they are only parsed and then discarded.
#
# At that time, there was not enough debug logging to see the details in the
# -dA log.  To actually see it, add debug logging at the beginning and end of
# Var_Parse.
.if defined(UNDEF) && ${INDIR_UNDEF} < ${NUMBER}
.  error
.endif
# Adding a ':U' modifier to the irrelevant expression didn't help, as that
# expression was only parsed, not evaluated.  The resulting literal string
# '${INDIR_UNDEF:U2}' was not numeric either, for the same reason as above.
.if defined(UNDEF) && ${INDIR_UNDEF:U2} < ${NUMBER}
.  error
.endif


# Since cond.c 1.76 from 2020.06.28 and before var.c 1.225 from 2020.07.01,
# the following snippet resulted in the error message 'Variable VAR is
# recursive'.  The condition '0' evaluated to false, which made the right-hand
# side of the '&&' irrelevant.  Back then, irrelevant condition parts were
# still evaluated, but in "irrelevant mode", which allowed undefined variables
# to occur in expressions.  In this mode, the variable name 'VAR' was
# unnecessarily evaluated, resulting in the expression '${VAR${:U1}}'.  In
# this expression, the variable name was 'VAR${:U1}', and of this variable
# name, only the fixed part 'VAR' was evaluated, without the part '${:U1}'.
# This partial evaluation led to the wrong error message about 'VAR' being
# recursive.
VAR=	${VAR${:U1}}
.if 0 && !empty(VAR)
.endif


# Enclosing the expression in double quotes changes how that expression is
# evaluated.  In irrelevant expressions that are enclosed in double quotes,
# expressions based on undefined variables are allowed and evaluate to an
# empty string.
#
# The manual page stated from at least 1993 on that irrelevant conditions were
# not evaluated, but that was wrong.  These conditions were evaluated, the
# only difference was that undefined variables in them didn't trigger an
# error.  Since numeric conditions are quite rare, this subtle difference
# didn't catch much attention, as most other conditions such as pattern
# matches or equality comparisons worked fine and never produced error
# messages.
.if defined(UNDEF) && "${INDIR_UNDEF}" < ${NUMBER}
.  error
.endif

# Since the condition is relevant, the indirect undefined variable is
# evaluated as usual, resolving nested undefined expressions to an empty
# string.
#
# Comparing an empty string numerically is not possible, however, make has an
# ugly hack in TryParseNumber that treats an empty string as a valid numerical
# value, thus hiding bugs in the makefile.
.if ${INDIR_UNDEF} < ${NUMBER}
#  only due to the ugly hack
.else
.  error
.endif

# Due to the quotes around the left-hand side of the '<', the operand is
# marked as a string, thus preventing a numerical comparison.
#
# expect+1: Comparison with "<" requires both operands "" and "42" to be numeric
.if "${INDIR_UNDEF}" < ${NUMBER}
.  info yes
.else
.  info no
.endif

# The right-hand side of '||' is irrelevant and thus not evaluated.
.if 1 || ${INDIR_NUMBER} < ${NUMBER}
.else
.  error
.endif

# The right-hand side of '||' is relevant and thus evaluated normally.
.if 0 || ${INDIR_NUMBER} < ${NUMBER}
.  error
.endif

# The right-hand side of '||' evaluates to an empty string, as the variable
# 'INDIR_UNDEF' is defined, therefore the modifier ':U2' has no effect.
# Comparing an empty string numerically is not possible, however, make has an
# ugly hack in TryParseNumber that treats an empty string as a valid numerical
# value, thus hiding bugs in the makefile.
.if 0 || ${INDIR_UNDEF:U2} < ${NUMBER}
#  only due to the ugly hack
.else
.  error
.endif


# The right-hand side of the '&&' is irrelevant since the left-hand side
# already evaluates to false.  Before cond.c 1.79 from 2020-07-09, it was
# expanded nevertheless, although with a small modification:  undefined
# variables may be used in these expressions without generating an error.
.if defined(UNDEF) && ${UNDEF} != "undefined"
.  error
.endif


# Ensure that irrelevant conditions do not influence the result of the whole
# condition.  As of cond.c 1.302 from 2021-12-11, an irrelevant function call
# evaluated to true (see CondParser_FuncCall and CondParser_FuncCallEmpty), an
# irrelevant comparison evaluated to false (see CondParser_Comparison).
#
# An irrelevant true bubbles up to the outermost CondParser_And, where it is
# ignored.  An irrelevant false bubbles up to the outermost CondParser_Or,
# where it is ignored.
#
# If the condition parser should ever be restructured, the bubbling up of the
# irrelevant evaluation results might show up accidentally.  Prevent this.
DEF=	defined
.undef UNDEF

.if 0 && defined(DEF)
.  error
.endif

.if 1 && defined(DEF)
.else
.  error
.endif

.if 0 && defined(UNDEF)
.  error
.endif

.if 1 && defined(UNDEF)
.  error
.endif

.if 0 || defined(DEF)
.else
.  error
.endif

.if 1 || defined(DEF)
.else
.  error
.endif

.if 0 || defined(UNDEF)
.  error
.endif

.if 1 || defined(UNDEF)
.else
.  error
.endif


all:
