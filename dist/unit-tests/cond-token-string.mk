# $NetBSD: cond-token-string.mk,v 1.3 2020/11/10 22:23:37 rillig Exp $
#
# Tests for quoted and unquoted string literals in .if conditions.

# TODO: Implementation

# Cover the code in CondParser_String that frees the memory after parsing
# a variable expression based on an undefined variable.
.if "" != "${:Uvalue:Z}"
.  error
.else
.  error
.endif

.if x${:Uvalue}
.  error
.else
.  info xvalue is not defined.
.endif

# The 'x' produces a "Malformed conditional" since the left-hand side of a
# comparison in an .if directive must be either a variable expression, a
# quoted string literal or a number that starts with a digit.
.if x${:Uvalue} == ""
.  error
.else
.  error
.endif

# In plain words, a '\' can be used to escape any character, just as in
# double-quoted string literals.  See CondParser_String.
.if \x${:Uvalue} == "xvalue"
.  info Expected.
.else
.  error
.endif

all:
	@:;
