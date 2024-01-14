# $NetBSD: cond-func-exists.mk,v 1.7 2023/11/19 21:47:52 rillig Exp $
#
# Tests for the exists() function in .if conditions.

.if !exists(.)
.  error
.endif

# The argument to the function must not be enclosed in quotes.
# Neither double quotes nor single quotes are allowed.
.if exists(".")
.  error
.endif

.if exists('.')
.  error
.endif

# The only way to escape characters that would otherwise influence the parser
# is to enclose them in an expression.  For function arguments,
# neither the backslash nor the dollar sign act as escape character.
.if exists(\.)
.  error
.endif

.if !exists(${:U.})
.  error
.endif

# The argument to the function can have several expressions.
# See cond-func.mk for the characters that cannot be used directly.
.if !exists(${.PARSEDIR}/${.PARSEFILE})
.  error
.endif

# Whitespace is trimmed on both sides of the function argument.
.if !exists(	.	)
.  error
.endif

# The exists function does not really look up the file in the file system,
# instead it uses a cache that is preloaded very early, before parsing the
# first makefile.  At that time, the file did not exist yet.
_!=	> cond-func-exists.just-created
.if exists(cond-func-exists.just-created)
.  error
.endif
_!=	rm cond-func-exists.just-created

all:
	@:;
