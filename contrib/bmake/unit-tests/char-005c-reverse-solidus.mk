# $NetBSD: char-005c-reverse-solidus.mk,v 1.2 2025/06/29 11:27:21 rillig Exp $
#
# Tests for the character U+005C "REVERSE SOLIDUS".
#
# See also:
#	TODO
#	TODO
#	TODO

# TODO: Where is this character used normally?
# TODO: What are the edge cases?

# TODO: escape '#' in lines
# TODO: escape '#' in comments
# TODO: escape ':' in modifiers
# TODO: escape any character in condition strings

# begin https://gnats.netbsd.org/46139

# Too see the details of parsing, uncomment the following line.
#.MAKEFLAGS: -dcpv

# This backslash is treated as a line continuation.
# It does not end up in the variable value.
LINE_CONTINUATION=foo\
# This line is still part of the variable assignment
.if ${LINE_CONTINUATION:C,[^a-z],<>,gW} != "foo"
.  error
.endif

# The variable value contains two backslashes.
TWO_BACKSLASHES_AT_EOL=foo\\
.if ${TWO_BACKSLASHES_AT_EOL:C,[^a-z],<>,gW} != "foo<><>"
.  error
.endif

TRAILING_WHITESPACE=foo\ # trailing space
.if ${TRAILING_WHITESPACE:C,[^a-z],<>,gW} != "foo<><>"
.  error
.endif

# The simplest was to produce a single backslash is the :U modifier.
BACKSLASH=	${:U\\}
.if ${BACKSLASH} != "\\"
.  error
.endif
BACKSLASH_C=	${:U1:C,.,\\,}
.if ${BACKSLASH_C} != "\\"
.  error
.endif

# expect+5: Unclosed expression, expecting "}" for modifier "Mx\}"
# At the point where the unclosed expression is detected, the ":M" modifier
# has been applied to the expression.  Its pattern is "x}", which doesn't
# match the single backslash.
# expect: while evaluating variable "BACKSLASH" with value ""
.if ${BACKSLASH:Mx\}
.  error
.else
.  error
.endif

# expect+1: Unclosed expression, expecting "}" for modifier "Mx\\}"
.if ${BACKSLASH:Mx\\}
.  error
.else
.  error
.endif

# expect+1: Unclosed expression, expecting "}" for modifier "Mx\\\\\\\\}"
.if ${BACKSLASH:Mx\\\\\\\\}
.  error
.else
.  error
.endif

# Adding more text after the backslash adds to the pattern, as the backslash
# serves to escape the ":" that is otherwise used to separate the modifiers.
# The result is a single ":M" modifier with the pattern "x:Nzzz".
.if ${BACKSLASH:Mx\:Nzzz} != ""
.  error
.endif

# The pattern ends up as "x\:Nzzz".  Only the sequence "\:" is unescaped, all
# others, including "\\", are left as-is.
.if ${BACKSLASH:Mx\\:Nzzz} != ""
.  error
.endif

# The pattern for the ":M" modifier ends up as "x\\\\\\\:Nzzz".  Only the
# sequence "\:" is unescaped, all others, including "\\", are left as-is.
.if ${BACKSLASH:Mx\\\\\\\\:Nzzz} != ""
.  error
.endif

# The ":M" modifier is parsed differently than the other modifiers.  To
# circumvent the peculiarities of that parser, the pattern can be passed via
# an expression.  There, the usual escaping rules for modifiers apply.
# expect+1: Unfinished backslash at the end in pattern "\" of modifier ":M"
.if ${BACKSLASH:M${BACKSLASH}} != "\\"
.  error
.else
.  error
.endif

# Trying to bypass the parser by using a direct expression doesn't work, as
# the parser for the ":M" modifier does not parse the subexpression like in
# all other places, but instead counts the braces and tries to decode the
# escaping, which fails in this case.
# expect+1: Unclosed expression, expecting "}" for modifier "M${:U\\\\}} != "\\""
.if ${BACKSLASH:M${:U\\\\}} != "\\"
.  error
.else
.  error
.endif

# Matching a backslash with the pattern matching characters works.
.if ${BACKSLASH:M?} != "\\"
.  error
.endif
.if ${BACKSLASH:M*} != "\\"
.  error
.endif
.if ${BACKSLASH:M[Z-a]} != "\\"
.  error
.endif
.if ${BACKSLASH:M[\\]} != "\\"
.  error
.endif

# end https://gnats.netbsd.org/46139
