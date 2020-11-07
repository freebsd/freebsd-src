# $NetBSD: cond-undef-lint.mk,v 1.2 2020/09/14 07:13:29 rillig Exp $
#
# Tests for defined and undefined variables in .if conditions, in lint mode.
#
# As of 2020-09-14, lint mode contains experimental code for printing
# accurate error messages in case of undefined variables, instead of the
# wrong "Malformed condition".
#
# See also:
#	opt-debug-lint.mk

.MAKEFLAGS: -dL

# DEF is defined, UNDEF is not.
DEF=		defined

# An expression based on a defined variable is fine.
.if !${DEF}
.  error
.endif

# Since the condition fails to evaluate, neither of the branches is taken.
.if ${UNDEF}
.  error
.else
.  error
.endif

# The variable name depends on the undefined variable, which is probably a
# mistake.  The variable UNDEF, as used here, can be easily turned into
# an expression that is always defined, using the :U modifier.
#
# The outer expression does not generate an error message since there was
# already an error evaluating this variable's name.
#
# TODO: Suppress the error message "Variable VAR. is undefined".  That part
# of the expression must not be evaluated at all.
.if ${VAR.${UNDEF}}
.  error
.else
.  error
.endif

# The variable VAR.defined is not defined and thus generates an error message.
.if ${VAR.${DEF}}
.  error
.else
.  error
.endif


# Variables that are referenced indirectly may be undefined in a condition.
#
# A practical example for this is CFLAGS, which consists of CWARNS, COPTS
# and a few others.  Just because these nested variables are not defined,
# this does not make the condition invalid.
#
# The crucial point is that at the point where the variable appears in the
# condition, there is no way to influence the definedness of the nested
# variables.  In particular, there is no modifier that would turn undefined
# nested variables into empty strings, as an equivalent to the :U modifier.
INDIRECT=	${NESTED_UNDEF} ${NESTED_DEF}
NESTED_DEF=	nested-defined

# Since NESTED_UNDEF is not controllable at this point, it must not generate
# an error message, and it doesn't do so, since 2020-09-14.
.if !${INDIRECT}
.  error
.endif
