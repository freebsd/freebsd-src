# $NetBSD: opt-debug-lint.mk,v 1.14 2021/03/14 10:57:12 rillig Exp $
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

# Between 2020-10-03 and var.c 1.752 from 2020-12-20, in lint mode the
# variable modifier had to be separated by colons.  This was wrong though
# since make always fell back trying to parse the indirect modifier as a
# SysV modifier.
.if ${value:${:UL}PL} != "LPL}"		# FIXME: "LPL}" is unexpected here.
.  error ${value:${:UL}PL}
.endif

# Typically, an indirect modifier is followed by a colon or the closing
# brace.  This one isn't, therefore make falls back to parsing it as the SysV
# modifier ":lue=lid".
.if ${value:L:${:Ulue}=${:Ulid}} != "valid"
.  error
.endif

# In lint mode, the whole variable text is evaluated to check for unclosed
# expressions and unknown operators.  During this check, the subexpression
# '${:U2}' is not expanded, instead it is copied verbatim into the regular
# expression, leading to '.*=.{1,${:U2}}$'.
#
# Before var.c 1.856 from 2021-03-14, this regular expression was then
# compiled even though that was not necessary for checking the syntax at the
# level of variable expressions.  The unexpanded '$' then resulted in a wrong
# error message.
#
# This only happened in lint mode since in default mode the early check for
# unclosed expressions and unknown modifiers is skipped.
#
# See VarCheckSyntax, ApplyModifier_Regex.
#
VARMOD_REGEX=	${:UA=111 B=222 C=33:C/.*=.{1,${:U2}}$//g}
