# $NetBSD: varmod-match-escape.mk,v 1.1 2020/08/16 20:03:53 rillig Exp $
#
# As of 2020-08-01, the :M and :N modifiers interpret backslashes differently,
# depending on whether there was a variable expression somewhere before the
# first backslash or not.  See ApplyModifier_Match, "copy = TRUE".
#
# Apart from the different and possibly confusing debug output, there is no
# difference in behavior.  When parsing the modifier text, only \{, \} and \:
# are unescaped, and in the pattern matching these have the same meaning as
# their plain variants '{', '}' and ':'.  In the pattern matching from
# Str_Match, only \*, \? or \[ would make a noticeable difference.
SPECIALS=	\: : \\ * \*
RELEVANT=	yes
.if ${SPECIALS:M${:U}\:} != ${SPECIALS:M\:${:U}}
.warning unexpected
.endif
RELEVANT=	no

all:
	@:;
