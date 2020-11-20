# $NetBSD: directive-ifmake.mk,v 1.8 2020/11/15 20:20:58 rillig Exp $
#
# Tests for the .ifmake directive, which provides a shortcut for asking
# whether a certain target is requested to be made from the command line.
#
# TODO: Describe why the shortcut may be useful (if it's useful at all),
# instead of sticking to the simple '.if' only.

# The targets 'first' and 'second' are passed in on the command line.

# This is the most basic form.
.ifmake first
.  info ok: positive condition works
.else
.  warning positive condition fails
.endif

# The not operator works as expected.
# An alternative interpretation were that this condition is asking whether
# the target "!first" was requested.  To distinguish this, see the next test.
.ifmake !first
.  warning unexpected
.else
.  info ok: negation works
.endif

# See if the exclamation mark really means "not", or if it is just part of
# the target name.  Since it means 'not', the two exclamation marks are
# effectively ignored, and 'first' is indeed a requested target.  If the
# exclamation mark were part of the name instead, the name would be '!!first',
# and such a target was not requested to be made.
.ifmake !!first
.  info ok: double negation works
.else
.  warning double negation fails
.endif

# Multiple targets can be combined using the && and || operators.
.ifmake first && second
.  info ok: both mentioned
.else
.  warning && does not work as expected
.endif

# Negation also works in complex conditions.
.ifmake first && !unmentioned
.  info ok: only those mentioned
.else
.  warning && with ! does not work as expected
.endif

# Using the .MAKEFLAGS special dependency target, arbitrary command
# line options can be added at parse time.  This means that it is
# possible to extend the targets to be made.
.MAKEFLAGS: late-target
.ifmake late-target
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
.  info ok
.else
.  error
.endif


first second unmentioned late-target:
	: $@
