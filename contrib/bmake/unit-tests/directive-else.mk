# $NetBSD: directive-else.mk,v 1.6 2020/11/13 09:01:59 rillig Exp $
#
# Tests for the .else directive.

.MAKEFLAGS: -dL			# To enable the check for ".else <cond>"

# The .else directive does not take any arguments.
# As of 2020-08-29, make doesn't warn about this.
.if 0
.  warning must not be reached
.else 123
.  info ok
.endif

.if 1
.  info ok
.else 123
.  warning must not be reached
.endif

# An .else without a corresponding .if is an error.
.else

# Accidental extra .else directives are detected too.
.if 0
.  warning must not be reached
.else
.  info ok
.else
.  info After an extra .else, everything is skipped.
.endif

# An .else may have a comment.  This comment does not count as an argument,
# therefore no parse error.
.if 0
.else # comment
.endif

# A variable expression does count as an argument, even if it is empty.
# XXX: This should be a parse error.
.if 0
.else ${:U}
.endif

all:
	@:;
