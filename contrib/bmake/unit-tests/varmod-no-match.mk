# $NetBSD: varmod-no-match.mk,v 1.3 2023/02/26 06:08:06 rillig Exp $
#
# Tests for the expression modifier ':N', which filters words that do not
# match the given pattern.


# Keep all words except for 'two'.
.if ${:U one two three :Ntwo} != "one three"
.  error
.endif

# Keep all words except those starting with 't'.
# See varmod-match.mk for the details of pattern matching.
.if ${:U one two three four six :Nt*} != "one four six"
.  error
.endif


# Idiom: normalize whitespace
#
# The modifier ':N' can be used with an empty pattern.  As that pattern never
# matches a word, the only effect is that the string is split into words and
# then joined again, thereby normalizing whitespace around and between the
# words.  And even though the 'N' in ':N' might serve as a mnemonic for
# "normalize whitespace", this idiom is not used in practice, resorting to the
# much more common ':M*' to "select all words" instead.
.if ${:U :N} != ""
.  error
.endif
.if ${:U one    two three :N} != "one two three"
.  error
.endif
.if ${:U one    two three :M*} != "one two three"
.  error
.endif


# Idiom: single-word expression equals any of several words or patterns
#
# If an expression is guaranteed to consist of a single word, the modifier
# ':N' can be chained to compare the expression to several words or even
# patterns in a sequence.  If one of the patterns matches, the final
# expression will be the empty string.
#
.if ${:U word :None:Ntwo:Nthree} != ""
#  good
.else
.  error
.endif
.if ${:U two :None:Ntwo:Nthree} != ""
.  error
.else
#  good
.endif
#
# The modifier ':N' is seldom used in general since positive matches with ':M'
# are easier to grasp.  Chaining the ':N' modifier is even more difficult to
# grasp due to the many negations involved.
#
# The final '!= ""' adds to the confusion because at first glance, the
# condition may look like '${VAR} != ""', which for a single-word variable is
# always true.
#
# The '!= ""' can be omitted if the expression cannot have the numeric value
# 0, which is common in practice.  In that form, each ':N' can be pronounced
# as 'neither' or 'nor', which makes the expression sound more natural.
#
.if ${:U word :None:Ntwo:Nthree}
#  good
.else
.  error
.endif
.if ${:U two :None:Ntwo:Nthree}
.  error
.else
#  good
.endif
#
# Replacing the '${...} != ""' with '!empty(...)' doesn't improve the
# situation as the '!' adds another level of negations, and the word 'empty'
# is a negation on its own, thereby creating a triple negation.  Furthermore,
# due to the '!empty', the expression to be evaluated no longer starts with
# '$' and is thus more difficult to spot quickly.
#
.if !empty(:U word :None:Ntwo:Nthree)
#  good
.else
.  error
.endif
.if !empty(:U two :None:Ntwo:Nthree)
.  error
.else
#  good
.endif


all:
