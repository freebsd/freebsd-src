# $NetBSD: directive-ifmake.mk,v 1.11 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the .ifmake directive, which provides a shortcut for asking
# whether a certain target is requested to be made from the command line.
#
# TODO: Describe why the shortcut may be useful (if it's useful at all),
# instead of using the more general '.if make(target)'.

.MAKEFLAGS: first second

# This is the most basic form.
.ifmake first
# expect+1: ok: positive condition works
.  info ok: positive condition works
.else
.  warning positive condition fails
.endif

# The '!' is interpreted as 'not'.  A possible alternative interpretation of
# this condition is whether the target named "!first" was requested.  To
# distinguish these cases, see the next test.
.ifmake !first
.  warning unexpected
.else
# expect+1: ok: negation works
.  info ok: negation works
.endif

# See if the exclamation mark really means "not", or if it is just part of
# the target name.  Since it means 'not', the two exclamation marks are
# effectively ignored, and 'first' is indeed a requested target.  If the
# exclamation mark were part of the name instead, the name would be '!!first',
# and such a target was not requested to be made.
.ifmake !!first
# expect+1: ok: double negation works
.  info ok: double negation works
.else
.  warning double negation fails
.endif

# Multiple targets can be combined using the && and || operators.
.ifmake first && second
# expect+1: ok: both mentioned
.  info ok: both mentioned
.else
.  warning && does not work as expected
.endif

# Negation also works in complex conditions.
.ifmake first && !unmentioned
# expect+1: ok: only those mentioned
.  info ok: only those mentioned
.else
.  warning && with ! does not work as expected
.endif

# Using the .MAKEFLAGS special dependency target, arbitrary command
# line options can be added at parse time.  This means that it is
# possible to extend the targets to be made.
.MAKEFLAGS: late-target
.ifmake late-target
# expect+1: Targets can even be added at parse time.
.  info Targets can even be added at parse time.
.else
.  info No, targets cannot be added at parse time anymore.
.endif

# Numbers are interpreted as numbers, no matter whether the directive is
# a plain .if or an .ifmake.
.ifmake 0
.  error
.endif
.ifmake 1
.else
.  error
.endif

# A condition that consists of a variable expression only (without any
# comparison operator) can be used with .if and the other .ifxxx directives.
.ifmake ${:Ufirst}
# expect+1: ok
.  info ok
.else
.  error
.endif


# As an edge case, a target can actually be named "!first" on the command
# line.  There is no way to define a target of this name though since in a
# dependency line, a plain '!' is interpreted as a dependency operator.

.MAKEFLAGS: !edge
.ifmake edge
.  error
.endif

# The '\!edge' in the following condition is parsed as a bare word.  For such
# a bare word, there is no escaping mechanism so the backslash passes through.
# Since the condition function 'make' accepts a pattern instead of a plain
# target name, the '\' is finally discarded in Str_Match.
.ifmake \!edge
.else
.  error
.endif

# In a dependency line, a plain '!' is interpreted as a dependency operator
# (the other two are ':' and '::').  If the '!' is escaped by a '\', as
# implemented in ParseDependencyTargetWord, the additional backslash is never
# removed though.  The target name thus becomes '\!edge' instead of the
# intended '!edge'.  Defining a target whose name contains a '!' will either
# require additional tricks, or it may even be impossible.

first second unmentioned late-target \!edge:
	: $@
