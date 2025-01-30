# $NetBSD: varmod-subst-regex.mk,v 1.12 2024/07/20 11:05:12 rillig Exp $
#
# Tests for the :C,from,to, variable modifier.

# report unmatched subexpressions
.MAKEFLAGS: -dL

all: mod-regex-compile-error
all: mod-regex-limits-{1,2,3,4,5,6}
all: mod-regex-errors-{1,2}
all: unmatched-subexpression

# The expression expands to 4 words.  Of these words, none matches
# the regular expression "a b" since these words don't contain any
# whitespace.
.if ${:Ua b b c:C,a b,,} != "a b b c"
.  error
.endif

# Using the '1' modifier does not change anything.  The '1' modifier just
# means to apply at most 1 replacement in the whole expression.
.if ${:Ua b b c:C,a b,,1} != "a b b c"
.  error
.endif

# The 'W' modifier treats the whole variable value as a single big word,
# containing whitespace.  This big word matches the regular expression,
# therefore it gets replaced.  Whitespace is preserved after replacing.
.if ${:Ua b b c:C,a b,,W} != " b c"
.  error
.endif

# The 'g' modifier does not have any effect here since each of the words
# contains the character 'b' a single time.
.if ${:Ua b b c:C,b,,g} != "a c"
.  error
.endif

# The first :C modifier has the 'W' modifier, which makes the whole
# expression a single word.  The 'g' modifier then replaces all occurrences
# of "1 2" with "___".  The 'W' modifier only applies to this single :C
# modifier.  This is demonstrated by the :C modifier that follows.  If the
# 'W' modifier would be preserved, only a single underscore would have been
# replaced with an 'x'.
.if ${:U1 2 3 1 2 3:C,1 2,___,Wg:C,_,x,} != "x__ 3 x__ 3"
.  error
.endif

# The regular expression does not match in the first word.
# It matches once in the second word, and the \0\0 doubles that word.
# In the third word, the regular expression matches as early as possible,
# and since the matches must not overlap, the next possible match would
# start at the 6, but at that point, there is only one character left,
# and that cannot match the regular expression "..".  Therefore only the
# "45" is doubled in the third word.
.if ${:U1 23 456:C,..,\0\0,} != "1 2323 45456"
.  error
.endif

# The modifier '1' applies the replacement at most once, across the whole
# expression value, no matter whether it is a single big word or many small
# words.
#
# Up to 2020-08-28, the manual page said that the modifiers '1' and 'g'
# were orthogonal, which was wrong.  It doesn't make sense to specify both
# 'g' and '1' at the same time.
.if ${:U12345 12345:C,.,\0\0,1} != "112345 12345"
.  error
.endif

# A regular expression that matches the empty string applies before every
# single character of the word.
# XXX: Most other places where regular expression are used match at the end
# of the string as well.
.if ${:U1a2b3c:C,a*,*,g} != "*1**2*b*3*c"
.  error
.endif

# A dot in the regular expression matches any character, even a newline.
# In most other contexts where regular expressions are used, a dot matches
# any character except newline.  In make, regcomp is called without
# REG_NEWLINE, thus newline is an ordinary character.
.if ${:U"${.newline}":C,.,.,g} != "..."
.  error
.endif


# Like the ':S' modifier, the ':C' modifier matches on an expression
# that contains no words at all, but only if the regular expression matches an
# empty string, for example, when the regular expression is anchored at the
# beginning or the end of the word.  An unanchored regular expression that
# matches the empty string is uncommon in practice, as it would match before
# each character of the word.
.if "<${:U:S,,unanchored,}> <${:U:C,.?,unanchored,}>" != "<> <unanchored>"
.  error
.endif
.if "<${:U:S,^,prefix,}> <${:U:C,^,prefix,}>" != "<prefix> <prefix>"
.  error
.endif
.if "<${:U:S,$,suffix,}> <${:U:C,$,suffix,}>" != "<suffix> <suffix>"
.  error
.endif
.if "<${:U:S,^$,whole,}> <${:U:C,^$,whole,}>" != "<whole> <whole>"
.  error
.endif
.if "<${:U:S,,unanchored,g}> <${:U:C,.?,unanchored,g}>" != "<> <unanchored>"
.  error
.endif
.if "<${:U:S,^,prefix,g}> <${:U:C,^,prefix,g}>" != "<prefix> <prefix>"
.  error
.endif
.if "<${:U:S,$,suffix,g}> <${:U:C,$,suffix,g}>" != "<suffix> <suffix>"
.  error
.endif
.if "<${:U:S,^$,whole,g}> <${:U:C,^$,whole,g}>" != "<whole> <whole>"
.  error
.endif
.if "<${:U:S,,unanchored,W}> <${:U:C,.?,unanchored,W}>" != "<> <unanchored>"
.  error
.endif
.if "<${:U:S,^,prefix,W}> <${:U:C,^,prefix,W}>" != "<prefix> <prefix>"
.  error
.endif
.if "<${:U:S,$,suffix,W}> <${:U:C,$,suffix,W}>" != "<suffix> <suffix>"
.  error
.endif
.if "<${:U:S,^$,whole,W}> <${:U:C,^$,whole,W}>" != "<whole> <whole>"
.  error
.endif


# Multiple asterisks form an invalid regular expression.  This produces an
# error message and (as of 2020-08-28) stops parsing in the middle of the
# expression.  The unparsed part of the expression is then copied
# verbatim to the output, which is unexpected and can lead to strange shell
# commands being run.
mod-regex-compile-error:
	@echo $@: ${:Uword1 word2:C,****,____,g:C,word,____,:Q}.

# These tests generate error messages but as of 2020-08-28 just continue
# parsing and execution as if nothing bad had happened.
mod-regex-limits-1:
	@echo $@:11-missing:${:U1 23 456:C,..,\1\1,:Q}
mod-regex-limits-2:
	@echo $@:11-ok:${:U1 23 456:C,(.).,\1\1,:Q}
mod-regex-limits-3:
	@echo $@:22-missing:${:U1 23 456:C,..,\2\2,:Q}
mod-regex-limits-4:
	@echo $@:22-missing:${:U1 23 456:C,(.).,\2\2,:Q}
mod-regex-limits-5:
	@echo $@:22-ok:${:U1 23 456:C,(.)(.),\2\2,:Q}
mod-regex-limits-6:
	# The :C modifier only handles single-digit capturing groups,
	# which is enough for all practical use cases.
	@echo $@:capture:${:UabcdefghijABCDEFGHIJrest:C,(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.),\9\8\7\6\5\4\3\2\1\0\10\11\12,}

mod-regex-errors-1:
	@echo $@: ${UNDEF:Uvalue:C,[,,}

mod-regex-errors-2:
	# If the replacement pattern produces a parse error because of an
	# unknown modifier, the parse error is ignored in ParseModifierPart
	# and the faulty expression expands to "".
	@echo $@: ${word:L:C,.*,x${:U:Z}y,W}

# In regular expressions with alternatives, not all capturing groups are
# always set; some may be missing.  Make calls these "unmatched
# subexpressions".
#
# Between var.c 1.16 from 1996-12-24 until before var.c 1.933 from 2021-06-21,
# unmatched subexpressions produced an "error message" but did not have any
# further effect since the "error handling" didn't influence the exit status.
#
# Before 2021-06-21 there was no way to turn off this warning, thus the
# combination of alternative matches and capturing groups was seldom used, if
# at all.
#
# Since var.c 1.933 from 2021-06-21, the error message is only printed in lint
# mode (-dL), but not in default mode.
#
# As an alternative to the change from var.c 1.933 from 2021-06-21, a possible
# mitigation would have been to add a new modifier 'U' to the already existing
# '1Wg' modifiers of the ':C' modifier.  That modifier could have been used in
# the modifier ':C,(a.)|(b.),\1\2,U' to treat unmatched subexpressions as
# empty.  This approach would have created a syntactical ambiguity since the
# modifiers ':S' and ':C' are open-ended (see mod-subst-chain), that is, they
# do not need to be followed by a ':' to separate them from the next modifier.
# Luckily the modifier :U does not make sense after :C, therefore this case
# does not happen in practice.
unmatched-subexpression:
	# In each of the following cases, if the regular expression matches at
	# all, the subexpression \1 matches as well.
	@echo $@.ok: ${:U1 1 2 3 5 8 13 21 34:C,1(.*),one\1,}

	# In the following cases:
	#	* The subexpression \1 is only defined for 1 and 13.
	#	* The subexpression \2 is only defined for 2 and 21.
	#	* If the regular expression does not match at all, the
	#	  replacement string is not analyzed, thus no error messages.
	# In total, there are 5 error messages about unmatched subexpressions.
	@echo $@.1:  ${:U  1:C,1(.*)|2(.*),(\1)(\2),:Q}		# missing \2
	@echo $@.1:  ${:U  1:C,1(.*)|2(.*),(\1)(\2),:Q}		# missing \2
	@echo $@.2:  ${:U  2:C,1(.*)|2(.*),(\1)(\2),:Q}		# missing \1
	@echo $@.3:  ${:U  3:C,1(.*)|2(.*),(\1)(\2),:Q}
	@echo $@.5:  ${:U  5:C,1(.*)|2(.*),(\1)(\2),:Q}
	@echo $@.8:  ${:U  8:C,1(.*)|2(.*),(\1)(\2),:Q}
	@echo $@.13: ${:U 13:C,1(.*)|2(.*),(\1)(\2),:Q}		# missing \2
	@echo $@.21: ${:U 21:C,1(.*)|2(.*),(\1)(\2),:Q}		# missing \1
	@echo $@.34: ${:U 34:C,1(.*)|2(.*),(\1)(\2),:Q}

	# And now all together: 5 error messages for 1, 1, 2, 13, 21.
	@echo $@.all: ${:U1 1 2 3 5 8 13 21 34:C,1(.*)|2(.*),(\1)(\2),:Q}
