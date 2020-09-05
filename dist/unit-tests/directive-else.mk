# $NetBSD: directive-else.mk,v 1.3 2020/08/29 18:50:25 rillig Exp $
#
# Tests for the .else directive.

# The .else directive does not take any arguments.
# As of 2020-08-29, make doesn't warn about this.
.if 0
.warning must not be reached
.else 123
.info ok
.endif

.if 1
.info ok
.else 123
.warning must not be reached
.endif

# An .else without a corresponding .if is an error.
.else

# Accidental extra .else directives are detected too.
.if 0
.warning must not be reached
.else
.info ok
.else
.info After an extra .else, everything is skipped.
.endif

all:
	@:;
