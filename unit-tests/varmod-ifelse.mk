# $NetBSD: varmod-ifelse.mk,v 1.9 2021/01/25 19:05:39 rillig Exp $
#
# Tests for the ${cond:?then:else} variable modifier, which evaluates either
# the then-expression or the else-expression, depending on the condition.
#
# The modifier was added on 1998-04-01.
#
# Until 2015-10-11, the modifier always evaluated both the "then" and the
# "else" expressions.

# TODO: Implementation

# The variable name of the expression is expanded and then taken as the
# condition.  In this case it becomes:
#
#	variable expression == "variable expression"
#
# This confuses the parser, which expects an operator instead of the bare
# word "expression".  If the name were expanded lazily, everything would be
# fine since the condition would be:
#
#	${:Uvariable expression} == "literal"
#
# Evaluating the variable name lazily would require additional code in
# Var_Parse and ParseVarname, it would be more useful and predictable
# though.
.if ${${:Uvariable expression} == "literal":?bad:bad}
.  error
.else
.  error
.endif

# In a variable assignment, undefined variables are not an error.
# Because of the early expansion, the whole condition evaluates to
# ' == ""' though, which cannot be parsed because the left-hand side looks
# empty.
COND:=	${${UNDEF} == "":?bad-assign:bad-assign}

# In a condition, undefined variables generate a "Malformed conditional"
# error.  That error message is wrong though.  In lint mode, the correct
# "Undefined variable" error message is generated.
# The difference to the ':=' variable assignment is the additional
# "Malformed conditional" error message.
.if ${${UNDEF} == "":?bad-cond:bad-cond}
.  error
.else
.  error
.endif

# When the :? is parsed, it is greedy.  The else branch spans all the
# text, up until the closing character '}', even if the text looks like
# another modifier.
.if ${1:?then:else:Q} != "then"
.  error
.endif
.if ${0:?then:else:Q} != "else:Q"
.  error
.endif

# This line generates 2 error messages.  The first comes from evaluating the
# malformed conditional "1 == == 2", which is reported as "Bad conditional
# expression" by ApplyModifier_IfElse.  The variable expression containing that
# conditional therefore returns a parse error from Var_Parse, and this parse
# error propagates to CondEvalExpression, where the "Malformed conditional"
# comes from.
.if ${1 == == 2:?yes:no} != ""
.  error
.else
.  error
.endif

# If the "Bad conditional expression" appears in a quoted string literal, the
# error message "Malformed conditional" is not printed, leaving only the "Bad
# conditional expression".
#
# XXX: The left-hand side is enclosed in quotes.  This results in Var_Parse
# being called without VARE_UNDEFERR being set.  When ApplyModifier_IfElse
# returns AMR_CLEANUP as result, Var_Parse returns varUndefined since the
# value of the variable expression is still undefined.  CondParser_String is
# then supposed to do proper error handling, but since varUndefined is local
# to var.c, it cannot distinguish this return value from an ordinary empty
# string.  The left-hand side of the comparison is therefore just an empty
# string, which is obviously equal to the empty string on the right-hand side.
#
# XXX: The debug log for -dc shows a comparison between 1.0 and 0.0.  The
# condition should be detected as being malformed before any comparison is
# done since there is no well-formed comparison in the condition at all.
.MAKEFLAGS: -dc
.if "${1 == == 2:?yes:no}" != ""
.  error
.else
.  warning Oops, the parse error should have been propagated.
.endif
.MAKEFLAGS: -d0

# As of 2020-12-10, the variable "name" is first expanded, and the result of
# this expansion is then taken as the condition.  To force the variable
# expression in the condition to be evaluated at exactly the right point,
# the '$' of the intended '${VAR}' escapes from the parser in form of the
# expression ${:U\$}.  Because of this escaping, the variable "name" and thus
# the condition ends up as "${VAR} == value", just as intended.
#
# This hack does not work for variables from .for loops since these are
# expanded at parse time to their corresponding ${:Uvalue} expressions.
# Making the '$' of the '${VAR}' expression indirect hides this expression
# from the parser of the .for loop body.  See ForLoop_SubstVarLong.
.MAKEFLAGS: -dc
VAR=	value
.if ${ ${:U\$}{VAR} == value :?ok:bad} != "ok"
.  error
.endif
.MAKEFLAGS: -d0

all:
	@:;
