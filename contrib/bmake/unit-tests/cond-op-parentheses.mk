# $NetBSD: cond-op-parentheses.mk,v 1.9 2025/06/28 22:39:28 rillig Exp $
#
# Tests for parentheses in .if conditions, which group expressions to override
# the precedence of the operators '!', '&&' and '||'.  Parentheses cannot be
# used to form arithmetic expressions such as '(3+4)' though.

# Contrary to the C family of programming languages, the outermost condition
# does not have to be enclosed in parentheses.
.if defined(VAR)
.  error
.elif !1
.  error
.endif

# Parentheses cannot enclose numbers as there is no need for it.  Make does
# not implement any arithmetic functions in its condition parser.  If
# absolutely necessary, use expr(1).
#
# XXX: It's inconsistent that the right operand has unbalanced parentheses.
#
# expect+1: Comparison with ">" requires both operands "3" and "(2" to be numeric
.if 3 > (2)
.endif
# expect+1: Malformed conditional "(3) > 2"
.if (3) > 2
.endif

# Test for deeply nested conditions.
.if	((((((((((((((((((((((((((((((((((((((((((((((((((((((((	\
	((((((((((((((((((((((((((((((((((((((((((((((((((((((((	\
	1								\
	))))))))))))))))))))))))))))))))))))))))))))))))))))))))	\
	))))))))))))))))))))))))))))))))))))))))))))))))))))))))
# Parentheses can be nested at least to depth 112.  There is nothing special
# about this number though, much higher numbers work as well, at least on
# NetBSD.  The actual limit depends on the allowed call stack depth for C code
# of the platform.  Anyway, 112 should be enough for all practical purposes.
.else
.  error
.endif

# An unbalanced opening parenthesis is a parse error.
# expect+1: Malformed conditional "("
.if (
.  error
.else
.  error
.endif

# An unbalanced closing parenthesis is a parse error.
#
# Before cond.c 1.237 from 2021-01-19, CondParser_Term returned TOK_RPAREN
# even though the documentation of that function promised to only ever return
# TOK_TRUE, TOK_FALSE or TOK_ERROR.  In cond.c 1.241, the return type of that
# function was changed to a properly restricted enum type, to prevent this bug
# from occurring again.
# expect+1: Malformed conditional ")"
.if )
.  error
.else
.  error
.endif

all:
	@:;
