# $NetBSD: varparse-undef-partial.mk,v 1.5 2024/01/07 11:39:04 rillig Exp $

# When an undefined variable is expanded in a ':=' assignment, only the
# initial '$' of the expression is skipped by the parser, while
# the remaining expression is evaluated.  In edge cases this can lead to
# a completely different interpretation of the partially expanded text.

LIST=	${DEF} ${UNDEF} ${VAR.${PARAM}} end
DEF=	defined
PARAM=	:Q

# The expression ${VAR.${PARAM}} refers to the variable named "VAR.:Q",
# with the ":Q" being part of the name.  This variable is not defined,
# therefore the initial '$' of that whole expression is skipped by the parser
# (see VarSubstExpr) and the rest of the expression is expanded as usual.
#
# The resulting expression is ${VAR.:Q}, which means that the
# interpretation of the ":Q" has changed from being part of the variable
# name to being a variable modifier.  This is a classical code injection.
EVAL:=	${LIST}
.if ${EVAL} != "defined   end"
.  error ${EVAL}
.endif

# Define the possible outcomes, to see which of them gets expanded.
VAR.=		var-dot without parameter
${:UVAR.\:Q}=	var-dot with parameter :Q

# At this point, the variable "VAR." is defined, therefore the expression
# ${VAR.:Q} is expanded, consisting of the variable name "VAR." and the
# modifier ":Q".
.if ${EVAL} != "defined  var-dot\\ without\\ parameter end"
.  error ${EVAL}
.endif

# In contrast to the previous line, evaluating the original LIST again now
# produces a different result since the variable named "VAR.:Q" is now
# defined.  It is expanded as usual, interpreting the ":Q" as part of the
# variable name, as would be expected from reading the expression.
EVAL:=	${LIST}
.if ${EVAL} != "defined  var-dot with parameter :Q end"
.  error ${EVAL}
.endif

# It's difficult to decide what the best behavior is in this situation.
# Should the whole expression be skipped for now, or should the inner
# subexpressions be expanded already?
#
# Example 1:
# CFLAGS:=	${CFLAGS:N-W*} ${COPTS.${COMPILER}}
#
# The variable COMPILER typically contains an identifier and the variable is
# not modified later.  In this practical case, it does not matter whether the
# expression is expanded early, or whether the whole ${COPTS.${COMPILER}} is
# expanded as soon as the variable COPTS.${COMPILER} becomes defined.  The
# expression ${COMPILER} would be expanded several times, but in this simple
# scenario there would not be any side effects.
#
# TODO: Add a practical example where early/lazy expansion actually makes a
# difference.

all:
	@:
