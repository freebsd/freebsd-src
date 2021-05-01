# $NetBSD: varmod-range.mk,v 1.7 2020/11/01 14:36:25 rillig Exp $
#
# Tests for the :range variable modifier, which generates sequences
# of integers from the given range.
#
# See also:
#	modword.mk

# The :range modifier generates a sequence of integers, one number per
# word of the variable expression's value.
.if ${a b c:L:range} != "1 2 3"
.  error
.endif

# To preserve spaces in a word, they can be enclosed in quotes, just like
# everywhere else.
.if ${:U first "the second word" third 4 :range} != "1 2 3 4"
.  error
.endif

# The :range modifier takes the number of words from the value of the
# variable expression.  If that expression is undefined, the range is
# undefined as well.  This should not come as a surprise.
.if "${:range}" != ""
.  error
.endif

# The :range modifier can be given a parameter, which makes the generated
# range independent from the value or the name of the variable expression.
#
# XXX: As of 2020-09-27, the :range=... modifier does not turn the undefined
# expression into a defined one.  This looks like an oversight.
.if "${:range=5}" != ""
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
.if "${:U:range=x}Rest" != "Rest"
.  error
.else
.  error
.endif

# The upper limit of the range must always be given in decimal.
# This parse error stops at the 'x', trying to parse it as a variable
# modifier.
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
.if "${a b c:L:rang}Rest" != "Rest"
.  error
.else
.  error
.endif

# misspelled modifier name
.if "${a b c:L:rango}Rest" != "Rest"
.  error
.else
.  error
.endif

# modifier name too long
.if "${a b c:L:ranger}Rest" != "Rest"
.  error
.else
.  error
.endif

all:
