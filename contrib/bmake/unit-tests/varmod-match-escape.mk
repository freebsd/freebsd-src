# $NetBSD: varmod-match-escape.mk,v 1.10 2023/06/23 04:56:54 rillig Exp $
#
# As of 2020-08-01, the :M and :N modifiers interpret backslashes differently,
# depending on whether there was a variable expression somewhere before the
# first backslash or not.  See ApplyModifier_Match, "copy = true".
#
# Apart from the different and possibly confusing debug output, there is no
# difference in behavior.  When parsing the modifier text, only \{, \} and \:
# are unescaped, and in the pattern matching these have the same meaning as
# their plain variants '{', '}' and ':'.  In the pattern matching from
# Str_Match, only \*, \? or \[ would make a noticeable difference.

.MAKEFLAGS: -dcv

SPECIALS=	\: : \\ * \*
.if ${SPECIALS:M${:U}\:} != ${SPECIALS:M\:${:U}}
.  warning unexpected
.endif

# And now both cases combined: A single modifier with both an escaped ':'
# as well as a variable expression that expands to a ':'.
#
# XXX: As of 2020-11-01, when an escaped ':' occurs before the variable
# expression, the whole modifier text is subject to unescaping '\:' to ':',
# before the variable expression is expanded.  This means that the '\:' in
# the variable expression is expanded as well, turning ${:U\:} into a simple
# ${:U:}, which silently expands to an empty string, instead of generating
# an error message.
#
# XXX: As of 2020-11-01, the modifier on the right-hand side of the
# comparison is parsed differently though.  First, the variable expression
# is parsed, resulting in ':' and needSubst=true.  After that, the escaped
# ':' is seen, and this time, copy=true is not executed but stays copy=false.
# Therefore the escaped ':' is kept as-is, and the final pattern becomes
# ':\:'.
#
# If ApplyModifier_Match had used the same parsing algorithm as Var_Subst,
# both patterns would end up as '::'.
#
VALUES=		: :: :\:
.if ${VALUES:M\:${:U\:}} != ${VALUES:M${:U\:}\:}
# expect+1: warning: XXX: Oops
.  warning XXX: Oops
.endif

.MAKEFLAGS: -d0

# XXX: As of 2020-11-01, unlike all other variable modifiers, a '$' in the
# :M and :N modifiers is written as '$$', not as '\$'.  This is confusing,
# undocumented and hopefully not used in practice.
.if ${:U\$:M$$} != "\$"
.  error
.endif

# XXX: As of 2020-11-01, unlike all other variable modifiers, '\$' is not
# parsed as an escaped '$'.  Instead, ApplyModifier_Match first scans for
# the ':' at the end of the modifier, which results in the pattern '\$'.
# No unescaping takes place since the pattern neither contained '\:' nor
# '\{' nor '\}'.  But the text is expanded, and a lonely '$' at the end
# is silently discarded.  The resulting expanded pattern is thus '\', that
# is a single backslash.
.if ${:U\$:M\$} != ""
.  error
.endif

# In lint mode, the case of a lonely '$' is covered with an error message.
.MAKEFLAGS: -dL
# expect+1: Dollar followed by nothing
.if ${:U\$:M\$} != ""
.  error
.endif

# The control flow of the pattern parser depends on the actual string that
# is being matched.  There needs to be either a test that shows a difference
# in behavior, or a proof that the behavior does not depend on the actual
# string.
#
# TODO: Str_Match("a-z]", "[a-z]")
# TODO: Str_Match("012", "[0-]]")
# TODO: Str_Match("[", "[[]")
# TODO: Str_Match("]", "[]")
# TODO: Str_Match("]", "[[-]]")

# Demonstrate an inconsistency between positive and negative character lists
# when the range ends with the character ']'.
#
# 'A' begins the range, 'B' is in the middle of the range, ']' ends the range,
# 'a' is outside the range.
WORDS=		A A] A]] B B] B]] ] ]] ]]] a a] a]]
# The ']' is part of the character range and at the same time ends the
# character list.
EXP.[A-]=	A B ]
# The first ']' is part of the character range and at the same time ends the
# character list.
EXP.[A-]]=	A] B] ]]
# The first ']' is part of the character range and at the same time ends the
# character list.
EXP.[A-]]]=	A]] B]] ]]]
# For negative character lists, the ']' ends the character range but does not
# end the character list.
# XXX: This is unnecessarily inconsistent but irrelevant in practice as there
# is no practical need for a character range that ends at ']'.
EXP.[^A-]=	a
EXP.[^A-]]=	a
EXP.[^A-]]]=	a]

.for pattern in [A-] [A-]] [A-]]] [^A-] [^A-]] [^A-]]]
# expect+2: warning: Unfinished character list in pattern '[A-]' of modifier ':M'
# expect+1: warning: Unfinished character list in pattern '[^A-]' of modifier ':M'
.  if ${WORDS:M${pattern}} != ${EXP.${pattern}}
.    warning ${pattern}: ${WORDS:M${pattern}} != ${EXP.${pattern}}
.  endif
.endfor

# In brackets, the backslash is just an ordinary character.
# Outside brackets, it is an escape character for a few special characters.
# TODO: Str_Match("\\", "[\\-]]")
# TODO: Str_Match("-]", "[\\-]]")

all:
	@:;
