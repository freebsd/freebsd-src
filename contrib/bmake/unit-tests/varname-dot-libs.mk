# $NetBSD: varname-dot-libs.mk,v 1.1 2020/08/28 03:51:06 rillig Exp $
#
# Tests for the special .LIBS variable, which is not documented in the
# manual page.
#
# It is yet unclear in which situations this feature is useful.

.SUFFIXES: .a

.PATH.a: . ..

.LIBS: .a

# The .LIBS variable is not yet available.
.if defined(${.LIBS:Q})
.error
.endif

all:
	@echo .LIBS=${.LIBS:Q}
