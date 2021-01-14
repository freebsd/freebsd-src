# $NetBSD: directive-else.mk,v 1.7 2020/12/14 22:17:11 rillig Exp $
#
# Tests for the .else directive.
#
# Since 2020-11-13, an '.else' followed by extraneous text generates a parse
# error in -dL (lint) mode.
#
# Since 2020-12-15, an '.else' followed by extraneous text always generates
# a parse error.

.if 0
.  warning must not be reached
# The .else directive does not take any arguments.
.else 123
.  info ok
.endif

.if 1
.  info ok
# The .else directive does not take any arguments.
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
.if 0
.else ${:U}
.endif

all:
	@:;
