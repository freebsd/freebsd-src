# $NetBSD: cond-token-string.mk,v 1.6 2022/05/08 06:57:00 rillig Exp $
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

# A non-empty string evaluates to true, no matter if it's a literal string or
# if it contains variable expressions.  The parentheses are not necessary for
# the parser, in this case their only purpose is to make the code harder to
# read for humans.
VAR=	value
.if ("${VAR}")
.else
.  error
.endif

# In the conditions in .if directives, the left-hand side of a comparison must
# be enclosed in quotes.  The right-hand side does not need to be enclosed in
# quotes.
.if "quoted" == quoted
.else
.  error
.endif

.MAKEFLAGS: -d0

all: .PHONY
