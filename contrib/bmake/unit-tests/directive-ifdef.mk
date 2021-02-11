# $NetBSD: directive-ifdef.mk,v 1.4 2021/01/21 23:03:41 rillig Exp $
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

# String literals are handled the same in all variants of the .if directive.
# They evaluate to true if they are not empty.  Whitespace counts as non-empty
# as well.
.ifdef ""
.  error
.else
.  info String literals are tested for emptiness.
.endif

.ifdef " "
.  info String literals are tested for emptiness.  Whitespace is non-empty.
.else
.  error
.endif

all:
	@:;
