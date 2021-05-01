# $NetBSD: varmod-to-separator.mk,v 1.7 2020/11/15 20:20:58 rillig Exp $
#
# Tests for the :ts variable modifier, which joins the words of the variable
# using an arbitrary character as word separator.

WORDS=	one two three four five six

# The words are separated by a single space, just as usual.
.if ${WORDS:ts } != "one two three four five six"
.  warning Space as separator does not work.
.endif

# The separator can be an arbitrary character, for example a comma.
.if ${WORDS:ts,} != "one,two,three,four,five,six"
.  warning Comma as separator does not work.
.endif

# After the :ts modifier, other modifiers can follow.
.if ${WORDS:ts/:tu} != "ONE/TWO/THREE/FOUR/FIVE/SIX"
.  warning Chaining modifiers does not work.
.endif

# To use the ':' as the separator, just write it normally.
# The first colon is the separator, the second ends the modifier.
.if ${WORDS:ts::tu} != "ONE:TWO:THREE:FOUR:FIVE:SIX"
.  warning Colon as separator does not work.
.endif

# When there is just a colon but no other character, the words are
# "separated" by an empty string, that is, they are all squashed
# together.
.if ${WORDS:ts:tu} != "ONETWOTHREEFOURFIVESIX"
.  warning Colon as separator does not work.
.endif

# Applying the :tu modifier first and then the :ts modifier does not change
# anything since neither of these modifiers is related to how the string is
# split into words.  Beware of separating the words using a single or double
# quote though, or other special characters like dollar or backslash.
#
# This example also demonstrates that the closing brace is not interpreted
# as a separator, but as the closing delimiter of the whole variable
# expression.
.if ${WORDS:tu:ts} != "ONETWOTHREEFOURFIVESIX"
.  warning Colon as separator does not work.
.endif

# The '}' plays the same role as the ':' in the preceding examples.
# Since there is a single character before it, that character is taken as
# the separator.
.if ${WORDS:tu:ts/} != "ONE/TWO/THREE/FOUR/FIVE/SIX"
.  warning Colon as separator does not work.
.endif

# Now it gets interesting and ambiguous:  The separator could either be empty
# since it is followed by a colon.  Or it could be the colon since that
# colon is followed by the closing brace.  It's the latter case.
.if ${WORDS:ts:} != "one:two:three:four:five:six"
.  warning Colon followed by closing brace does not work.
.endif

# As in the ${WORDS:tu:ts} example above, the separator is empty.
.if ${WORDS:ts} != "onetwothreefourfivesix"
.  warning Empty separator before closing brace does not work.
.endif

# The :ts modifier can be followed by other modifiers.
.if ${WORDS:ts:S/two/2/} != "one2threefourfivesix"
.  warning Separator followed by :S modifier does not work.
.endif

# The :ts modifier can follow other modifiers.
.if ${WORDS:S/two/2/:ts} != "one2threefourfivesix"
.  warning :S modifier followed by :ts modifier does not work.
.endif

# The :ts modifier with an actual separator can be followed by other
# modifiers.
.if ${WORDS:ts/:S/two/2/} != "one/2/three/four/five/six"
.  warning The :ts modifier followed by an :S modifier does not work.
.endif

# The separator can be \n, which is a newline.
.if ${WORDS:[1..3]:ts\n} != "one${.newline}two${.newline}three"
.  warning The separator \n does not produce a newline.
.endif

# The separator can be \t, which is a tab.
.if ${WORDS:[1..3]:ts\t} != "one	two	three"
.  warning The separator \t does not produce a tab.
.endif

# The separator can be given as octal number.
.if ${WORDS:[1..3]:ts\012:tu} != "ONE${.newline}TWO${.newline}THREE"
.  warning The separator \012 is not interpreted in octal ASCII.
.endif

# The octal number can have as many digits as it wants.
.if ${WORDS:[1..2]:ts\000000000000000000000000012:tu} != "ONE${.newline}TWO"
.  warning The separator \012 cannot have many leading zeroes.
.endif

# The value of the separator character must not be outside the value space
# for an unsigned character though.
#
# Since 2020-11-01, these out-of-bounds values are rejected.
.if ${WORDS:[1..3]:ts\400:tu}
.  warning The separator \400 is accepted even though it is out of bounds.
.else
.  warning The separator \400 is accepted even though it is out of bounds.
.endif

# The separator can be given as hexadecimal number.
.if ${WORDS:[1..3]:ts\xa:tu} != "ONE${.newline}TWO${.newline}THREE"
.  warning The separator \xa is not interpreted in hexadecimal ASCII.
.endif

# The hexadecimal number must be in the range of an unsigned char.
#
# Since 2020-11-01, these out-of-bounds values are rejected.
.if ${WORDS:[1..3]:ts\x100:tu}
.  warning The separator \x100 is accepted even though it is out of bounds.
.else
.  warning The separator \x100 is accepted even though it is out of bounds.
.endif

# Negative numbers are not allowed for the separator character.
.if ${WORDS:[1..3]:ts\-300:tu}
.  warning The separator \-300 is accepted even though it is negative.
.else
.  warning The separator \-300 is accepted even though it is negative.
.endif

# The character number is interpreted as octal number by default.
# The digit '8' is not an octal digit though.
.if ${1 2 3:L:ts\8:tu}
.  warning The separator \8 is accepted even though it is not octal.
.else
.  warning The separator \8 is accepted even though it is not octal.
.endif

# Trailing characters after the octal character number are rejected.
.if ${1 2 3:L:ts\100L}
.  warning The separator \100L is accepted even though it contains an 'L'.
.else
.  warning The separator \100L is accepted even though it contains an 'L'.
.endif

# Trailing characters after the hexadecimal character number are rejected.
.if ${1 2 3:L:ts\x40g}
.  warning The separator \x40g is accepted even though it contains a 'g'.
.else
.  warning The separator \x40g is accepted even though it contains a 'g'.
.endif


# In the :t modifier, the :t must be followed by any of A, l, s, u.
.if ${WORDS:tx} != "anything"
.  info This line is not reached because of the malformed condition.
.  info If this line were reached, it would be visible in the -dcpv log.
.endif

# After the backslash, only n, t, an octal number, or x and a hexadecimal
# number are allowed.
.if ${WORDS:t\X} != "anything"
.  info This line is not reached.
.endif

# TODO: This modifier used to accept decimal numbers as well, in the form
# ':ts\120'.  When has this been changed to octal, and what happens now
# for ':ts\90' ('Z' in decimal ASCII, undefined in octal)?

# TODO: :ts\x1F600

all:
