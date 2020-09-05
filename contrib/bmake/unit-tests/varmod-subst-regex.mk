# $NetBSD: varmod-subst-regex.mk,v 1.3 2020/08/28 17:15:04 rillig Exp $
#
# Tests for the :C,from,to, variable modifier.

all: mod-regex-compile-error
all: mod-regex-limits
all: mod-regex-errors

# The variable expression expands to 4 words.  Of these words, none matches
# the regular expression "a b" since these words don't contain any
# whitespace.
.if ${:Ua b b c:C,a b,,} != "a b b c"
.error
.endif

# Using the '1' modifier does not change anything.  The '1' modifier just
# means to apply at most 1 replacement in the whole variable expression.
.if ${:Ua b b c:C,a b,,1} != "a b b c"
.error
.endif

# The 'W' modifier treats the whole variable value as a single big word,
# containing whitespace.  This big word matches the regular expression,
# therefore it gets replaced.  Whitespace is preserved after replacing.
.if ${:Ua b b c:C,a b,,W} != " b c"
.error
.endif

# The 'g' modifier does not have any effect here since each of the words
# contains the character 'b' a single time.
.if ${:Ua b b c:C,b,,g} != "a c"
.error
.endif

# The first :C modifier has the 'W' modifier, which makes the whole
# expression a single word.  The 'g' modifier then replaces all occurrences
# of "1 2" with "___".  The 'W' modifier only applies to this single :C
# modifier.  This is demonstrated by the :C modifier that follows.  If the
# 'W' modifier would be preserved, only a single underscore would have been
# replaced with an 'x'.
.if ${:U1 2 3 1 2 3:C,1 2,___,Wg:C,_,x,} != "x__ 3 x__ 3"
.error
.endif

# The regular expression does not match in the first word.
# It matches once in the second word, and the \0\0 doubles that word.
# In the third word, the regular expression matches as early as possible,
# and since the matches must not overlap, the next possible match would
# start at the 6, but at that point, there is only one character left,
# and that cannot match the regular expression "..".  Therefore only the
# "45" is doubled in the result.
.if ${:U1 23 456:C,..,\0\0,} != "1 2323 45456"
.error
.endif

# The modifier '1' applies the replacement at most once, across the whole
# variable value, no matter whether it is a single big word or many small
# words.
#
# Up to 2020-08-28, the manual page said that the modifiers '1' and 'g'
# were orthogonal, which was wrong.
.if ${:U12345 12345:C,.,\0\0,1} != "112345 12345"
.error
.endif

# Multiple asterisks form an invalid regular expression.  This produces an
# error message and (as of 2020-08-28) stops parsing in the middle of the
# variable expression.  The unparsed part of the expression is then copied
# verbatim to the output, which is unexpected and can lead to strange shell
# commands being run.
mod-regex-compile-error:
	@echo $@: ${:Uword1 word2:C,****,____,g:C,word,____,:Q}.

# These tests generate error messages but as of 2020-08-28 just continue
# parsing and execution as if nothing bad had happened.
mod-regex-limits:
	@echo $@:11-missing:${:U1 23 456:C,..,\1\1,:Q}
	@echo $@:11-ok:${:U1 23 456:C,(.).,\1\1,:Q}
	@echo $@:22-missing:${:U1 23 456:C,..,\2\2,:Q}
	@echo $@:22-missing:${:U1 23 456:C,(.).,\2\2,:Q}
	@echo $@:22-ok:${:U1 23 456:C,(.)(.),\2\2,:Q}
	# The :C modifier only handles single-digit capturing groups,
	# which is more than enough for daily use.
	@echo $@:capture:${:UabcdefghijABCDEFGHIJrest:C,(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.),\9\8\7\6\5\4\3\2\1\0\10\11\12,}

mod-regex-errors:
	@echo $@: ${UNDEF:Uvalue:C,[,,}
