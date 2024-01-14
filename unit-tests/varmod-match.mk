# $NetBSD: varmod-match.mk,v 1.20 2023/12/17 23:19:02 rillig Exp $
#
# Tests for the ':M' modifier, which keeps only those words that match the
# given pattern.
#
# Table of contents
#
# 1. Pattern characters '*', '?' and '\'
# 2. Character lists and character ranges
# 3. Parsing and escaping
# 4. Interaction with other modifiers
# 5. Performance
# 6. Error handling
# 7. Historical bugs
#
# See ApplyModifier_Match, ParseModifier_Match, ModifyWord_Match and
# Str_Match.


# 1. Pattern characters '*', '?' and '\'
#
#	*	matches 0 or more characters
#	?	matches 1 character
#	\x	matches the character 'x'

# The pattern is anchored both at the beginning and at the end of the word.
# Since the pattern 'e' does not contain any pattern matching characters, it
# matches exactly the word 'e', twice.
.if ${a c e aa cc ee e f g:L:Me} != "e e"
.  error
.endif

# The pattern character '?' matches exactly 1 character, the pattern character
# '*' matches 0 or more characters.  The whole pattern matches all words that
# start with 's' and have 3 or more characters.
.if ${One Two Three Four five six seven:L:Ms??*} != "six seven"
.  error
.endif

# Ensure that a pattern without placeholders only matches itself.
.if ${a aa aaa b ba baa bab:L:Ma} != "a"
.  error
.endif

# Ensure that a pattern that ends with '*' is properly anchored at the
# beginning.
.if ${a aa aaa b ba baa bab:L:Ma*} != "a aa aaa"
.  error
.endif

# Ensure that a pattern that starts with '*' is properly anchored at the end.
.if ${a aa aaa b ba baa bab:L:M*a} != "a aa aaa ba baa"
.  error
.endif

# Test the fast code path for '*' followed by a regular character.
.if ${:U file.c file.*c file.h file\.c :M*.c} != "file.c file\\.c"
.  error
.endif
# Ensure that the fast code path correctly handles the backslash.
.if ${:U file.c file.*c file.h file\.c :M*\.c} != "file.c file\\.c"
.  error
.endif
# Ensure that the fast code path correctly handles '\*'.
.if ${:U file.c file.*c file.h file\.c :M*\*c} != "file.*c"
.  error
.endif
# Ensure that the partial match '.c' doesn't confuse the fast code path.
.if ${:U file.c.cc file.cc.cc file.cc.c :M*.cc} != "file.c.cc file.cc.cc"
.  error
.endif
# Ensure that the substring '.cc' doesn't confuse the fast code path for '.c'.
.if ${:U file.c.cc file.cc.cc file.cc.c :M*.c} != "file.cc.c"
.  error
.endif


# 2. Character lists and character ranges
#
#	[...]	matches 1 character from the listed characters
#	[^...]	matches 1 character from the unlisted characters
#	[a-z]	matches 1 character from the range 'a' to 'z'
#	[z-a]	matches 1 character from the range 'a' to 'z'

# Only keep words that start with an uppercase letter.
.if ${One Two Three Four five six seven:L:M[A-Z]*} != "One Two Three Four"
.  error
.endif

# Only keep words that start with a character other than an uppercase letter.
.if ${One Two Three Four five six seven:L:M[^A-Z]*} != "five six seven"
.  error
.endif

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

#	[\]	matches a single backslash; no escaping takes place in
#		character ranges
# Without the 'b' in the below words, the backslash would end a word and thus
# influence how the string is split into words.
WORDS=		a\b a[\]b ab a\\b
.if ${WORDS:Ma[\]b} != "a\\b"
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

# Only keep words that don't start with s and at the same time end with
# either of [ex].
#
# This test case ensures that the negation from the first character list
# '[^s]' does not propagate to the second character list '[ex]'.
.if ${One Two Three Four five six seven:L:M[^s]*[ex]} != "One Three five"
.  error
.endif


# 3. Parsing and escaping
#
#	*	matches 0 or more characters
#	?	matches 1 character
#	\	outside a character list, escapes the following character
#	[	starts a character list for matching 1 character
#	]	ends a character list for matching 1 character
#	-	in a character list, forms a character range
#	^	at the beginning of a character list, negates the list
#	(	while parsing the pattern, starts a nesting level
#	)	while parsing the pattern, ends a nesting level
#	{	while parsing the pattern, starts a nesting level
#	}	while parsing the pattern, ends a nesting level
#	:	while parsing the pattern, terminates the pattern
#	$	while parsing the pattern, starts a nested expression
#	#	in a line except a shell command, starts a comment

# The pattern can come from an expression.  For single-letter
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

# To match a dollar sign in a word, double it.
#
# This is different from the :S and :C modifiers, where a '$' has to be
# escaped as '\$'.
.if ${:Ua \$ sign:M*$$*} != "\$"
.  error
.endif

# In the :M modifier, '\$' does not escape a dollar.  Instead it is
# interpreted as a backslash followed by whatever expression the
# '$' starts.
#
# This differs from the :S, :C and several other modifiers.
${:U*}=		asterisk
.if ${:Ua \$ sign any-asterisk:M*\$*} != "any-asterisk"
.  error
.endif

# TODO: ${VAR:M(((}}}}
# TODO: ${VAR:M{{{)))}
# TODO: ${VAR:M${UNBALANCED}}
# TODO: ${VAR:M${:U(((\}\}\}}}


# 4. Interaction with other modifiers

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


# 5. Performance

# Before 2020-06-13, this expression called Str_Match 601,080,390 times.
# Since 2020-06-13, this expression calls Str_Match 1 time.
.if ${:U****************:M****************b}
.endif

# Before 2023-06-22, this expression called Str_Match 2,621,112 times.
# Adding another '*?' to the pattern called Str_Match 20,630,572 times.
# Adding another '*?' to the pattern called Str_Match 136,405,672 times.
# Adding another '*?' to the pattern called Str_Match 773,168,722 times.
# Adding another '*?' to the pattern called Str_Match 3,815,481,072 times.
# Since 2023-06-22, Str_Match no longer backtracks.
.if ${:U..................................................b:M*?*?*?*?*?a}
.endif


# 6. Error handling

#	[	Incomplete empty character list, never matches.
WORDS=		a a[
# expect+1: warning: Unfinished character list in pattern 'a[' of modifier ':M'
.if ${WORDS:Ma[} != ""
.  error
.endif

#	[^	Incomplete negated empty character list, matches any single
#		character.
WORDS=		a a[ aX
# expect+1: warning: Unfinished character list in pattern 'a[^' of modifier ':M'
.if ${WORDS:Ma[^} != "a[ aX"
.  error
.endif

#	[-x1-3	Incomplete character list, matches those elements that can be
#		parsed without lookahead.
WORDS=		- + x xx 0 1 2 3 4 [x1-3
# expect+1: warning: Unfinished character list in pattern '[-x1-3' of modifier ':M'
.if ${WORDS:M[-x1-3} != "- x 1 2 3"
.  error
.endif

#	*[-x1-3	Incomplete character list after a wildcard, matches those
#		words that end with one of the characters from the list.
WORDS=		- + x xx 0 1 2 3 4 00 01 10 11 000 001 010 011 100 101 110 111 [x1-3
# expect+1: warning: Unfinished character list in pattern '*[-x1-3' of modifier ':M'
.if ${WORDS:M*[-x1-3} != "- x xx 1 2 3 01 11 001 011 101 111 [x1-3"
.  warning ${WORDS:M*[-x1-3}
.endif

#	[^-x1-3
#		Incomplete negated character list, matches any character
#		except those elements that can be parsed without lookahead.
WORDS=		- + x xx 0 1 2 3 4 [x1-3
# expect+1: warning: Unfinished character list in pattern '[^-x1-3' of modifier ':M'
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
PATTERN=	${:U?[\\}
# expect+1: warning: Unfinished character list in pattern '?[\' of modifier ':M'
.if ${WORDS:M${PATTERN}} != "\\\\ x\\"
.  error
.endif

#	[x-	Incomplete character list containing an incomplete character
#		range, matches only the 'x'.
WORDS=		[x- x x- y
# expect+1: warning: Unfinished character range in pattern '[x-' of modifier ':M'
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
# expect+1: warning: Unfinished character range in pattern '[^x-' of modifier ':M'
.if ${WORDS:M[^x-} != "[x- y yyyyy"
.  error
.endif

#	[:]	matches never since the ':' starts the next modifier
# expect+3: warning: Unfinished character list in pattern '[' of modifier ':M'
# expect+2: Unknown modifier "]"
# expect+1: Malformed conditional (${ ${:U\:} ${:U\:\:} :L:M[:]} != ":")
.if ${ ${:U\:} ${:U\:\:} :L:M[:]} != ":"
.  error
.else
.  error
.endif


# 7. Historical bugs

# Before var.c 1.1031 from 2022-08-24, the following expressions caused an
# out-of-bounds read beyond the indirect ':M' modifiers.
.if ${:U:${:UM\\}}		# The ':M' pattern need not be unescaped, the
.  error			# resulting pattern is '\', it never matches
.endif				# anything.
.if ${:U:${:UM\\\:\\}}		# The ':M' pattern must be unescaped, the
.  error			# resulting pattern is ':\', it never matches
.endif				# anything.
