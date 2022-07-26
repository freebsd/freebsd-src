# $NetBSD: varmod-match.mk,v 1.11 2022/06/11 09:15:49 rillig Exp $
#
# Tests for the :M variable modifier, which filters words that match the
# given pattern.
#
# See ApplyModifier_Match and ModifyWord_Match for the implementation.

.MAKEFLAGS: -dc

NUMBERS=	One Two Three Four five six seven

# Only keep words that start with an uppercase letter.
.if ${NUMBERS:M[A-Z]*} != "One Two Three Four"
.  error
.endif

# Only keep words that start with a character other than an uppercase letter.
.if ${NUMBERS:M[^A-Z]*} != "five six seven"
.  error
.endif

# Only keep words that don't start with s and at the same time end with
# either of [ex].
#
# This test case ensures that the negation from the first character class
# does not propagate to the second character class.
.if ${NUMBERS:M[^s]*[ex]} != "One Three five"
.  error
.endif

# Before 2020-06-13, this expression called Str_Match 601,080,390 times.
# Since 2020-06-13, this expression calls Str_Match 1 time.
.if ${:U****************:M****************b}
.endif

# As of 2022-06-11, this expression calls Str_Match 5,242,223 times.
# Adding another '*?' to the pattern calls Str_Match 41,261,143 times.
.if ${:U..................................................b:M*?*?*?*?*?a}
.endif

# To match a dollar sign in a word, double it.
#
# This is different from the :S and :C variable modifiers, where a '$'
# has to be escaped as '\$'.
.if ${:Ua \$ sign:M*$$*} != "\$"
.  error
.endif

# In the :M modifier, '\$' does not escape a dollar.  Instead it is
# interpreted as a backslash followed by whatever expression the
# '$' starts.
#
# This differs from the :S, :C and several other variable modifiers.
${:U*}=		asterisk
.if ${:Ua \$ sign any-asterisk:M*\$*} != "any-asterisk"
.  error
.endif

# TODO: ${VAR:M(((}}}}
# TODO: ${VAR:M{{{)))}
# TODO: ${VAR:M${UNBALANCED}}
# TODO: ${VAR:M${:U(((\}\}\}}}

.MAKEFLAGS: -d0

# Special characters:
#	*	matches 0 or more arbitrary characters
#	?	matches a single arbitrary character
#	\	starts an escape sequence, only outside ranges
#	[	starts a set for matching a single character
#	]	ends a set for matching a single character
#	-	in a set, forms a range of characters
#	^	as the first character in a set, negates the set
#	(	during parsing of the pattern, starts a nesting level
#	)	during parsing of the pattern, ends a nesting level
#	{	during parsing of the pattern, starts a nesting level
#	}	during parsing of the pattern, ends a nesting level
#	:	during parsing of the pattern, finishes the pattern
#	$	during parsing of the pattern, starts a nested expression
#	#	in a line except a shell command, starts a comment
#
# Pattern parts:
#	*	matches 0 or more arbitrary characters
#	?	matches exactly 1 arbitrary character
#	\x	matches exactly the character 'x'
#	[...]	matches exactly 1 character from the set
#	[^...]	matches exactly 1 character outside the set
#	[a-z]	matches exactly 1 character from the range 'a' to 'z'
#

#	[]	matches never
.if ${ ab a[]b a[b a b :L:M[]} != ""
.  error
.endif

#	a[]b	matches never
.if ${ ab a[]b a[b a b [ ] :L:Ma[]b} != ""
.  error
.endif

#	[^]	matches exactly 1 arbitrary character
.if ${ ab a[]b a[b a b [ ] :L:M[^]} != "a b [ ]"
.  error
.endif

#	a[^]b	matches 'a', then exactly 1 arbitrary character, then 'b'
.if ${ ab a[]b a[b a b :L:Ma[^]b} != "a[b"
.  error
.endif

#	[Nn0]	matches exactly 1 character from the set 'N', 'n', '0'
.if ${ a b N n 0 Nn0 [ ] :L:M[Nn0]} != "N n 0"
.  error
.endif

#	[a-c]	matches exactly 1 character from the range 'a' to 'c'
.if ${ A B C a b c d [a-c] [a] :L:M[a-c]} != "a b c"
.  error
.endif

#	[c-a]	matches the same as [a-c]
.if ${ A B C a b c d [a-c] [a] :L:M[c-a]} != "a b c"
.  error
.endif

#	[^a-c67]
#		matches a single character, except for 'a', 'b', 'c', '6' or
#		'7'
.if ${ A B C a b c d 5 6 7 8 [a-c] [a] :L:M[^a-c67]} != "A B C d 5 8"
.  error
.endif

#	[\]	matches a single backslash
WORDS=		a\b a[\]b ab
.if ${WORDS:Ma[\]b} != "a\\b"
.  error
.endif

#	:	terminates the pattern
.if ${ A * :L:M:} != ""
.  error
.endif

#	\:	matches a colon
.if ${ ${:U\: \:\:} :L:M\:} != ":"
.  error
.endif

#	${:U\:}	matches a colon
.if ${ ${:U\:} ${:U\:\:} :L:M${:U\:}} != ":"
.  error
.endif

#	[:]	matches never since the ':' starts the next modifier
# expect+2: Unknown modifier "]"
# expect+1: Malformed conditional (${ ${:U\:} ${:U\:\:} :L:M[:]} != ":")
.if ${ ${:U\:} ${:U\:\:} :L:M[:]} != ":"
.  error
.else
.  error
.endif

#	[\]	matches exactly a backslash; no escaping takes place in
#		character ranges
# Without the 'a' in the below words, the backslash would end a word and thus
# influence how the string is split into words.
WORDS=		1\a 2\\a
.if ${WORDS:M?[\]a} != "1\\a"
.  error
.endif

#	[[-]]	May look like it would match a single '[', '\' or ']', but
#		the inner ']' has two roles: it is the upper bound of the
#		character range as well as the closing character of the
#		character list.  The outer ']' is just a regular character.
WORDS=		[ ] [] \] ]]
.if ${WORDS:M[[-]]} != "[] \\] ]]"
.  error
.endif

#	[b[-]a]
#		Same as for '[[-]]': the character list stops at the first
#		']', and the 'a]' is treated as a literal string.
WORDS=		[a \a ]a []a \]a ]]a [a] \a] ]a] ba]
.if ${WORDS:M[b[-]a]} != "[a] \\a] ]a] ba]"
.  error
.endif

#	[-]	Matches a single '-' since the '-' only becomes part of a
#		character range if it is preceded and followed by another
#		character.
WORDS=		- -]
.if ${WORDS:M[-]} != "-"
.  error
.endif

#	[	Incomplete empty character list, never matches.
WORDS=		a a[
.if ${WORDS:Ma[} != ""
.  error
.endif

#	[^	Incomplete negated empty character list, matches any single
#		character.
WORDS=		a a[ aX
.if ${WORDS:Ma[^} != "a[ aX"
.  error
.endif

#	[-x1-3	Incomplete character list, matches those elements that can be
#		parsed without lookahead.
WORDS=		- + x xx 0 1 2 3 4 [x1-3
.if ${WORDS:M[-x1-3} != "- x 1 2 3"
.  error
.endif

#	[^-x1-3
#		Incomplete negated character list, matches any character
#		except those elements that can be parsed without lookahead.
WORDS=		- + x xx 0 1 2 3 4 [x1-3
.if ${WORDS:M[^-x1-3} != "+ 0 4"
.  error
.endif

#	[\	Incomplete character list containing a single '\'.
#
#		A word can only end with a backslash if the preceding
#		character is a backslash as well; in all other cases the final
#		backslash would escape the following space, making the space
#		part of the word.  Only the very last word of a string can be
#		'\', as there is no following space that could be escaped.
WORDS=		\\ \a ${:Ux\\}
.if ${WORDS:M?[\]} != "\\\\ x\\"
.  error
.endif

#	[x-	Incomplete character list containing an incomplete character
#		range, matches only the 'x'.
WORDS=		[x- x x- y
.if ${WORDS:M[x-} != "x"
.  error
.endif

#	[^x-	Incomplete negated character list containing an incomplete
#		character range; matches each word that does not have an 'x'
#		at the position of the character list.
#
#		XXX: Even matches strings that are longer than a single
#		character.
WORDS=		[x- x x- y yyyyy
.if ${WORDS:M[^x-} != "[x- y yyyyy"
.  error
.endif


# The modifier ':tW' prevents splitting at whitespace.  Even leading and
# trailing whitespace is preserved.
.if ${   plain   string   :L:tW:M*} != "   plain   string   "
.  error
.endif

# Without the modifier ':tW', the string is split into words.  All whitespace
# around and between the words is normalized to a single space.
.if ${   plain    string   :L:M*} != "plain string"
.  error
.endif


# The pattern can come from a variable expression.  For single-letter
# variables, either the short form or the long form can be used, just as
# everywhere else.
PRIMES=	2 3 5 7 11
n=	2
.if ${PRIMES:M$n} != "2"
.  error
.endif
.if ${PRIMES:M${n}} != "2"
.  error
.endif
.if ${PRIMES:M${:U2}} != "2"
.  error
.endif
