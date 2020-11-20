# $NetBSD: cond-op-parentheses.mk,v 1.3 2020/11/15 14:58:14 rillig Exp $
#
# Tests for parentheses in .if conditions.

# TODO: Implementation

# Test for deeply nested conditions.
.if	((((((((((((((((((((((((((((((((((((((((((((((((((((((((	\
	((((((((((((((((((((((((((((((((((((((((((((((((((((((((	\
	1								\
	))))))))))))))))))))))))))))))))))))))))))))))))))))))))	\
	))))))))))))))))))))))))))))))))))))))))))))))))))))))))
.  info Parentheses can be nested at least to depth 112.
.else
.  error
.endif

all:
	@:;
