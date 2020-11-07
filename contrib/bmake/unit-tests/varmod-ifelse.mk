# $NetBSD: varmod-ifelse.mk,v 1.5 2020/10/23 14:24:51 rillig Exp $
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

all:
	@:;
