# $NetBSD: opt-debug-lint.mk,v 1.11 2020/10/24 08:50:17 rillig Exp $
#
# Tests for the -dL command line option, which runs additional checks
# to catch common mistakes, such as unclosed variable expressions.

.MAKEFLAGS: -dL

# Since 2020-09-13, undefined variables that are used on the left-hand side
# of a condition at parse time get a proper error message.  Before, the
# error message was "Malformed conditional" only, which was wrong and
# misleading.  The form of the condition is totally fine, it's the evaluation
# that fails.
#
# Since 2020-09-13, the "Malformed conditional" error message is not printed
# anymore.
#
# See also:
#	cond-undef-lint.mk
.if $X
.  error
.endif

# The dynamic variables like .TARGET are treated specially.  It does not make
# sense to expand them in the global scope since they will never be defined
# there under normal circumstances.  Therefore they expand to a string that
# will later be expanded correctly, when the variable is evaluated again in
# the scope of an actual target.
#
# Even though the "@" variable is not defined at this point, this is not an
# error.  In all practical cases, this is no problem.  This particular test
# case is made up and unrealistic.
.if $@ != "\$(.TARGET)"
.  error
.endif

# Since 2020-09-13, Var_Parse properly reports errors for undefined variables,
# but only in lint mode.  Before, it had only silently returned var_Error,
# hoping for the caller to print an error message.  This resulted in the
# well-known "Malformed conditional" error message, even though the
# conditional was well-formed and the only error was an undefined variable.
.if ${UNDEF}
.  error
.endif

# Since 2020-09-14, dependency lines may contain undefined variables.
# Before, undefined variables were forbidden, but this distinction was not
# observable from the outside of the function Var_Parse.
${UNDEF}: ${UNDEF}

# In a condition that has a defined(UNDEF) guard, all guarded conditions
# may assume that the variable is defined since they will only be evaluated
# if the variable is indeed defined.  Otherwise they are only parsed, and
# for parsing it doesn't make a difference whether the variable is defined
# or not.
.if defined(UNDEF) && exists(${UNDEF})
.  error
.endif

# Since 2020-10-03, in lint mode the variable modifier must be separated
# by colons.  See varparse-mod.mk.
.if ${value:LPL} != "value"
.  error
.endif

# Since 2020-10-03, in lint mode the variable modifier must be separated
# by colons.  See varparse-mod.mk.
.if ${value:${:UL}PL} != "LPL}"		# FIXME: "LPL}" is unexpected here.
.  error ${value:${:UL}PL}
.endif

all:
	@:;
