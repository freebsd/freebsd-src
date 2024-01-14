# $NetBSD: varparse-mod.mk,v 1.2 2023/11/19 21:47:52 rillig Exp $

# Tests for parsing expressions with modifiers.

# As of 2020-10-02, the below condition does not result in a parse error.
# The condition contains two separate mistakes.  The first mistake is that
# the :!cmd! modifier is missing the closing '!'.  The second mistake is that
# there is a stray '}' at the end of the whole condition.
#
# As of 2020-10-02, the actual parse result of this condition is a single
# expression with 2 modifiers. The first modifier is
# ":!echo "\$VAR"} !".  Afterwards, the parser optionally skips a ':' (at the
# bottom of ApplyModifiers) and continues with the next modifier, in this case
# "= "value"", which is interpreted as a SysV substitution modifier with an
# empty left-hand side, thereby appending the string " "value"" to each word
# of the expression.
#
# As of 2020-10-02, some modifiers ensure that they are followed by either a
# ':' or the closing brace or parenthesis of the expression.  The modifiers
# that don't ensure this are (in order of appearance in ApplyModifier):
#	:@var@replacement@
#	:_
#	:L
#	:P
#	:!cmd!
#	:gmtime=...
#	:localtime=...
#	:M (because '}' and ')' are treated the same)
#	:N (because '}' and ')' are treated the same)
#	:S
#	:C
#	:range=...
# On the other hand, these modifiers ensure that they are followed by a
# delimiter:
#	:D
#	:U
#	:[...]
#	:gmtime (if not followed by '=')
#	:hash (if not followed by '=')
#	:localtime (if not followed by '=')
#	:t
#	:q
#	:Q
#	:T
#	:H
#	:E
#	:R
#	:range (if not followed by '=')
#	:O
#	:u
#	:sh
# These modifiers don't care since they reach until the closing character
# of the expression, which is either ')' or '}':
#	::= (as well as the other assignment modifiers)
#	:?
#
.if ${:!echo "\$VAR"} != "value"}
.endif

all:
	@:
