# $NetBSD: varname-dot-includes.mk,v 1.2 2020/09/13 16:53:19 rillig Exp $
#
# Tests for the special .INCLUDES variable, which is not documented in the
# manual page.
#
# It is yet unclear in which situations this feature is useful.

.SUFFIXES: .h

.PATH.h: . ..

.INCLUDES: .h

# The .INCLUDES variable is not yet available.
.if defined(.INCLUDES)
.  error
.endif

all:
	@echo .INCLUDES=${.INCLUDES:Q}
