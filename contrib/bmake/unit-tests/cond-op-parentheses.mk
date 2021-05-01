# $NetBSD: cond-op-parentheses.mk,v 1.4 2021/01/19 17:49:13 rillig Exp $
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

# An unbalanced opening parenthesis is a parse error.
.if (
.  error
.else
.  error
.endif

# An unbalanced closing parenthesis is a parse error.
#
# As of 2021-01-19, CondParser_Term returned TOK_RPAREN even though this
# function promised to only ever return TOK_TRUE, TOK_FALSE or TOK_ERROR.
.if )
.  error
.else
.  error
.endif

all:
	@:;
