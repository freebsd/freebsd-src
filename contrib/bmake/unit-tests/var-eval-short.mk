# $NetBSD: var-eval-short.mk,v 1.17 2025/01/11 20:54:45 rillig Exp $
#
# Tests for each variable modifier to ensure that they only do the minimum
# necessary computations.  If the result of the expression is irrelevant,
# the modifier should only be parsed.  The modifier should not be evaluated,
# but if it is evaluated for simplicity of the code (such as ':ts'), it must
# not have any observable side effects.
#
# See also:
#	var.c, the comment starting with 'The ApplyModifier functions'
#	ParseModifierPart, for evaluating nested expressions
#	cond-short.mk

FAIL=	${:!echo unexpected 1>&2!}

# The following tests only ensure that nested expressions are not evaluated.
# They cannot ensure that any unexpanded text returned from ParseModifierPart
# is ignored as well.  To do that, it is necessary to step through the code of
# each modifier.

# TODO: Test the modifiers in the same order as they occur in ApplyModifier.

.if 0 && ${FAIL}
.endif

.if 0 && ${VAR::=${FAIL}}
.elif defined(VAR)
.  error
.endif

.if 0 && ${${FAIL}:?then:else}
.endif

.if 0 && ${1:?${FAIL}:${FAIL}}
.endif

.if 0 && ${0:?${FAIL}:${FAIL}}
.endif

# Before var.c 1.870 from 2021-03-14, the expression ${FAIL} was evaluated
# after the loop, when undefining the temporary global loop variable.
# Since var.c 1.907 from 2021-04-04, a '$' is no longer allowed in the
# variable name.
# expect+1: In the :@ modifier, the variable name "${FAIL}" must not contain a dollar
.if 0 && ${:Uword:@${FAIL}@expr@}
.endif

.if 0 && ${:Uword:@var@${FAIL}@}
.endif

# Before var.c 1.877 from 2021-03-14, the modifier ':[...]' did not expand
# the nested expression ${FAIL} and then tried to parse the unexpanded text,
# which failed since '$' is not a valid range character.
.if 0 && ${:Uword:[${FAIL}]}
.endif

# Before var.c 1.867 from 2021-03-14, the modifier ':_' defined the variable
# even though the whole expression should have only been parsed, not
# evaluated.
.if 0 && ${:Uword:_=VAR}
.elif defined(VAR)
.  error
.endif

# Before var.c 1.856 from 2021-03-14, the modifier ':C' did not expand the
# nested expression ${FAIL}, which is correct, and then tried to compile the
# unexpanded text as a regular expression, which is unnecessary since the
# right-hand side of the '&&' cannot influence the outcome of the condition.
# Compiling the regular expression then failed both because of the '{FAIL}',
# which is not a valid repetition of the form '{1,5}', and because of the
# '****', which are repeated repetitions as well.
# '${FAIL}'
.if 0 && ${:Uword:C,${FAIL}****,,}
.endif

DEFINED=	# defined
.if 0 && ${DEFINED:D${FAIL}}
.endif

.if 0 && ${:Uword:E}
.endif

# Before var.c 1.1050 from 2023-05-09, the ':gmtime' modifier produced the
# error message 'Invalid time value: ${FAIL}}' since it did not expand its
# argument.
.if 0 && ${:Uword:gmtime=${FAIL}}
.endif

.if 0 && ${:Uword:H}
.endif

.if 0 && ${:Uword:hash}
.endif

.if 0 && ${value:L}
.endif

# Before var.c 1.1050 from 2023-05-09, the ':localtime' modifier produced the
# error message 'Invalid time value: ${FAIL}}' since it did not expand its
# argument.
.if 0 && ${:Uword:localtime=${FAIL}}
.endif

.if 0 && ${:Uword:M${FAIL}}
.endif

.if 0 && ${:Uword:N${FAIL}}
.endif

.if 0 && ${:Uword:O}
.endif

.if 0 && ${:Uword:Ox}
.endif

.if 0 && ${:Uword:P}
.endif

.if 0 && ${:Uword:Q}
.endif

.if 0 && ${:Uword:q}
.endif

.if 0 && ${:Uword:R}
.endif

.if 0 && ${:Uword:range}
.endif

.if 0 && ${:Uword:S,${FAIL},${FAIL},}
.endif

.if 0 && ${:Uword:sh}
.endif

.if 0 && ${:Uword:T}
.endif

.if 0 && ${:Uword:ts/}
.endif

.if 0 && ${:U${FAIL}}
.endif

.if 0 && ${:Uword:u}
.endif

.if 0 && ${:Uword:word=replacement}
.endif

# Before var.c 1.875 from 2021-03-14, Var_Parse returned "${FAIL}else" for the
# irrelevant right-hand side of the condition, even though this was not
# necessary.  Since the return value from Var_Parse is supposed to be ignored
# anyway, and since it is actually ignored in an overly complicated way,
# an empty string suffices.
.MAKEFLAGS: -dcpv
.if 0 && ${0:?${FAIL}then:${FAIL}else}
.endif

# The ':L' is applied before the ':?' modifier, giving the expression a name
# and a value, just to see whether this value gets passed through or whether
# the parse-only mode results in an empty string (only visible in the debug
# log).  As of var.c 1.875 from 2021-03-14, the value of the variable gets
# through, even though an empty string would suffice.
DEFINED=	defined
.if 0 && ${DEFINED:L:?${FAIL}then:${FAIL}else}
.endif
.MAKEFLAGS: -d0

all:
