# $NetBSD: varmod-match.mk,v 1.8 2022/03/27 18:39:01 rillig Exp $
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

# Before 2020-06-13, this expression took quite a long time in Str_Match,
# calling itself 601080390 times for 16 asterisks.
.if ${:U****************:M****************b}
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
# Without the 'a' in the below expressions, the backslash would end a word and
# thus influence how the string is split into words.
.if ${ ${:U\\a} ${:U\\\\a} :L:M[\]a} != "\\a"
.  error
.endif

#.MAKEFLAGS: -dcv
#
# Incomplete patterns:
#	[	matches TODO
#	[x	matches TODO
#	[^	matches TODO
#	[-	matches TODO
#	[xy	matches TODO
#	[^x	matches TODO
#	[\	matches TODO
#
#	[x-	matches exactly 'x', doesn't match 'x-'
#	[^x-	matches TODO
#	\	matches never


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
