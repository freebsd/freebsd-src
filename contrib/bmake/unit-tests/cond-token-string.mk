# $NetBSD: cond-token-string.mk,v 1.4 2021/01/21 00:38:28 rillig Exp $
#
# Tests for quoted string literals in .if conditions.
#
# See also:
#	cond-token-plain.mk
#		Covers string literals without quotes (called "bare words").

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

.MAKEFLAGS: -dc

# A string in quotes is checked whether it is not empty.
.if "UNDEF"
.  info The string literal "UNDEF" is not empty.
.else
.  error
.endif

# A space is not empty as well.
# This differs from many other places where whitespace is trimmed.
.if " "
.  info The string literal " " is not empty, even though it consists of $\
	whitespace only.
.else
.  error
.endif

.if "${UNDEF}"
.  error
.else
.  info An undefined variable in quotes expands to an empty string, which $\
	then evaluates to false.
.endif

.if "${:Uvalue}"
.  info A nonempty variable expression evaluates to true.
.else
.  error
.endif

.if "${:U}"
.  error
.else
.  info An empty variable evaluates to false.
.endif

.MAKEFLAGS: -d0

all:
	@:;
