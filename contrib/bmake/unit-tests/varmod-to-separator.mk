# $NetBSD: varmod-to-separator.mk,v 1.10 2022/01/23 21:48:59 rillig Exp $
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

# After the modifier ':ts/', the expression value is a single word since all
# spaces have been replaced with '/'.  This single word does not start with
# 'two', which makes the modifier ':S' a no-op.
.if ${WORDS:ts/:S/^two/2/} != "one/two/three/four/five/six"
.  error
.endif

# After the :ts modifier, the whole string is interpreted as a single
# word since all spaces have been replaced with x.  Because of this single
# word, only the first 'b' is replaced with 'B'.
.if ${aa bb aa bb aa bb:L:tsx:S,b,B,} != "aaxBbxaaxbbxaaxbb"
.  error
.endif

# The :ts modifier also applies to word separators that are added
# afterwards.  First, the modifier ':tsx' joins the 3 words, then the modifier
# ':S' replaces the 2 'b's with spaces.  These spaces are part of the word,
# so when the words are joined at the end of the modifier ':S', there is only
# a single word, and the custom separator from the modifier ':tsx' has no
# effect.
.if ${a ababa c:L:tsx:S,b, ,g} != "axa a axc"
.  error
.endif

# Adding the modifier ':M*' at the end of the above chain splits the
# expression value and then joins it again.  At this point of splitting, the
# newly added spaces are treated as word separators, resulting in 3 words.
# When these 3 words are joined, the separator from the modifier ':tsx' is
# used.
.if ${a ababa c:L:tsx:S,b, ,g:M*} != "axaxaxaxc"
.  error
.endif

# Not all modifiers use the separator from the previous modifier ':ts' though.
# The modifier ':@' always uses a space as word separator instead.  This has
# probably been an oversight during implementation.  For consistency, the
# result should rather be "axaxaxaxc", as in the previous example.
.if ${a ababa c:L:tsx:S,b, ,g:@v@$v@} != "axa a axc"
.  error
.endif

# Adding a final :M* modifier applies the :ts separator again, though.
.if ${a ababa c:L:tsx:S,b, ,g:@v@${v}@:M*} != "axaxaxaxc"
.  error
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
# expect: make: Bad modifier ":tx" for variable "WORDS"
.if ${WORDS:tx}
.  error
.else
.  error
.endif

# The word separator must be can only be a single character.
# expect: make: Bad modifier ":ts\X" for variable "WORDS"
.if ${WORDS:ts\X}
.  error
.else
.  error
.endif

# After the backslash, only n, t, an octal number, or x and a hexadecimal
# number are allowed.
.if ${WORDS:t\X} != "anything"
.  info This line is not reached.
.endif


# Since 2003.07.23.18.06.46 and before 2016.03.07.20.20.35, the modifier ':ts'
# interpreted an "octal escape" as decimal if the first digit was not '0'.
.if ${:Ua b:ts\61} != "a1b"	# decimal would have been "a=b"
.  error
.endif

# Since the character escape is always interpreted as octal, let's see what
# happens for non-octal digits.  From 2003.07.23.18.06.46 to
# 2016.02.27.16.20.06, the result was '1E2', since 2016.03.07.20.20.35 make no
# longer accepts this escape and complains.
# expect: make: Bad modifier ":ts\69" for variable ""
.if ${:Ua b:ts\69}
.  error
.else
.  error
.endif

# Try whether bmake is Unicode-ready.
# expect+2: Invalid character number at "1F60E}"
# expect+1: Malformed conditional (${:Ua b:ts\x1F60E})
.if ${:Ua b:ts\x1F60E}		# U+1F60E "smiling face with sunglasses"
.  error
.else
.  error
.endif
