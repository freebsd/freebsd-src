# $NetBSD: directive-ifdef.mk,v 1.3 2020/11/08 22:38:28 rillig Exp $
#
# Tests for the .ifdef directive.

# TODO: Implementation

DEFINED=	defined

# It looks redundant to have a call to defined() in an .ifdef, but it's
# possible.  The .ifdef only affects plain symbols, not function calls.
.ifdef defined(DEFINED)
.  info Function calls in .ifdef are possible.
.else
.  error
.endif

all:
	@:;
