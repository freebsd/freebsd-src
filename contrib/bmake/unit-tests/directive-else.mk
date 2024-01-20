# $NetBSD: directive-else.mk,v 1.9 2023/11/19 21:47:52 rillig Exp $
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
# expect+1: The .else directive does not take arguments
.else 123
# expect+1: ok
.  info ok
.endif

.if 1
# expect+1: ok
.  info ok
# expect+1: The .else directive does not take arguments
.else 123
.  warning must not be reached
.endif

# An .else without a corresponding .if is an error.
# expect+1: if-less else
.else

# Accidental extra .else directives are detected too.
.if 0
.  warning must not be reached
.else
# expect+1: ok
.  info ok
# expect+1: warning: extra else
.else
.  info After an extra .else, everything is skipped.
.endif

# An .else may have a comment.  This comment does not count as an argument,
# therefore no parse error.
.if 0
.else # comment
.endif

# An expression does count as an argument, even if it is empty.
.if 0
# expect+1: The .else directive does not take arguments
.else ${:U}
.endif

all:
	@:;
