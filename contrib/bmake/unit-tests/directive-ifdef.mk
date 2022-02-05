# $NetBSD: directive-ifdef.mk,v 1.5 2022/01/23 21:48:59 rillig Exp $
#
# Tests for the .ifdef directive, which evaluates bare words by calling
# 'defined(word)'.

DEFINED=	defined

# There is no variable named 'UNDEF', therefore the condition evaluates to
# false.
.ifdef UNDEF
.  error
.endif

# There is a variable named 'DEFINED', so the condition evaluates to true.
.ifdef DEFINED
.else
.  error
.endif

# Since a bare word is an abbreviation for 'defined(word)', these can be
# used to construct complex conditions.
.ifdef UNDEF && DEFINED
.  error
.endif
.ifdef UNDEF || DEFINED
.else
.  error
.endif

# It looks redundant to have a call to defined() in an .ifdef, but it's
# possible.  The '.ifdef' only affects bare words, not function calls.
.ifdef defined(DEFINED)
.else
.  error
.endif

# String literals are handled the same in all variants of the '.if' directive,
# they evaluate to true if they are not empty, therefore this particular
# example looks confusing and is thus not found in practice.
.ifdef ""
.  error
.else
.endif

# Whitespace counts as non-empty as well.
.ifdef " "
.else
.  error
.endif

all: .PHONY
