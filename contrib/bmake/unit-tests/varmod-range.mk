# $NetBSD: varmod-range.mk,v 1.18 2025/04/04 18:57:01 rillig Exp $
#
# Tests for the :range variable modifier, which generates sequences
# of integers from the given range.
#
# See also:
#	modword.mk

# The :range modifier generates a sequence of integers, one number per
# word of the expression's value.
.if ${a b c:L:range} != "1 2 3"
.  error
.endif

# To preserve spaces in a word, they can be enclosed in quotes, just like
# everywhere else.
.if ${:U first "the second word" third 4 :range} != "1 2 3 4"
.  error
.endif

# The :range modifier takes the number of words from the value of the
# expression.  If that expression is undefined, the range is
# undefined as well.  This should not come as a surprise.
.if "${:range}" != ""
.  error
.endif

# An empty expression results in a sequence of a single number, even though
# the expression contains 0 words.
.if ${:U:range} != "1"
.  error
.endif

# The :range modifier can be given a parameter, which makes the generated
# range independent from the value or the name of the expression.
.if "${:range=5}" != ""
.  error
.endif
# XXX: As of 2023-12-17, the ':range=n' modifier does not turn the undefined
# expression into a defined one, even though it does not depend on the value
# of the expression.  This looks like an oversight.
# expect+1: Variable "" is undefined
.if ${:range=5} != ""
.  error
.else
.  error
.endif

# Negative ranges don't make sense.
# As of 2020-11-01, they are accepted though, using up all available memory.
#.if "${:range=-1}"
#.  error
#.else
#.  error
#.endif

# The :range modifier requires a number as parameter.
#
# Until 2020-11-01, the parser tried to read the 'x' as a number, failed and
# stopped there.  It then tried to parse the next modifier at that point,
# which failed with the message "Unknown modifier".
#
# Since 2020-11-01, the parser issues a more precise "Invalid number" error
# instead.
# expect+1: Invalid number "x}Rest" != "Rest"" for ':range' modifier
.if "${:U:range=x}Rest" != "Rest"
.  error
.else
.  error
.endif

# The upper limit of the range must always be given in decimal.
# This parse error stops at the 'x', trying to parse it as a variable
# modifier.
# expect+1: Unknown modifier ":x0"
.if "${:U:range=0x0}Rest" != "Rest"
.  error
.else
.  error
.endif

# As of 2020-11-01, numeric overflow is not detected.
# Since strtoul returns ULONG_MAX in such a case, it is interpreted as a
# very large number, consuming all available memory.
#.if "${:U:range=18446744073709551619}Rest" != "Rest"
#.  error
#.else
#.  error
#.endif

# modifier name too short
# expect+1: Unknown modifier ":rang"
.if "${a b c:L:rang}Rest" != "Rest"
.  error
.else
.  error
.endif

# misspelled modifier name
# expect+1: Unknown modifier ":rango"
.if "${a b c:L:rango}Rest" != "Rest"
.  error
.else
.  error
.endif

# modifier name too long
# expect+1: Unknown modifier ":ranger"
.if "${a b c:L:ranger}Rest" != "Rest"
.  error
.else
.  error
.endif

all:
