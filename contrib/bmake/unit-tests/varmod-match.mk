# $NetBSD: varmod-match.mk,v 1.5 2020/09/13 05:36:26 rillig Exp $
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

all:
	@:;
