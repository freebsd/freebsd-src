# $NetBSD: varmod-defined.mk,v 1.7 2020/10/24 08:46:08 rillig Exp $
#
# Tests for the :D variable modifier, which returns the given string
# if the variable is defined.  It is closely related to the :U modifier.

DEF=	defined
.undef UNDEF

# Since DEF is defined, the value of the expression is "value", not
# "defined".
#
.if ${DEF:Dvalue} != "value"
.  error
.endif

# Since UNDEF is not defined, the "value" is ignored.  Instead of leaving the
# expression undefined, it is set to "", exactly to allow the expression to
# be used in .if conditions.  In this place, other undefined expressions
# would generate an error message.
# XXX: Ideally the error message would be "undefined variable", but as of
# 2020-08-25 it is "Malformed conditional".
#
.if ${UNDEF:Dvalue} != ""
.  error
.endif

# The modifier text may contain plain text as well as expressions.
#
.if ${DEF:D<${DEF}>} != "<defined>"
.  error
.endif

# Special characters that would be interpreted differently can be escaped.
# These are '}' (the closing character of the expression), ':', '$' and '\'.
# Any other backslash sequences are preserved.
#
# The escaping rules for string literals in conditions are completely
# different though. There, any character may be escaped using a backslash.
#
.if ${DEF:D \} \: \$ \\ \) \n } != " } : \$ \\ \\) \\n "
.  error
.endif

# Like in several other places in variable expressions, when
# ApplyModifier_Defined calls Var_Parse, double dollars lead to a parse
# error that is silently ignored.  This makes all dollar signs disappear,
# except for the last, which is a well-formed variable expression.
#
.if ${DEF:D$$$$$${DEF}} != "defined"
.  error
.endif

# Any other text is written without any further escaping.  In contrast
# to the :M modifier, parentheses and braces do not need to be nested.
# Instead, the :D modifier is implemented sanely by parsing nested
# expressions as such, without trying any shortcuts. See ApplyModifier_Match
# for an inferior variant.
#
.if ${DEF:D!&((((} != "!&(((("
.  error
.endif

# The :D modifier is often used in combination with the :U modifier.
# It does not matter in which order the :D and :U modifiers appear.
.if ${UNDEF:Dyes:Uno} != no
.  error
.endif
.if ${UNDEF:Uno:Dyes} != no
.  error
.endif
.if ${DEF:Dyes:Uno} != yes
.  error
.endif
.if ${DEF:Uno:Dyes} != yes
.  error
.endif

# Since the variable with the empty name is never defined, the :D modifier
# can be used to add comments in the middle of an expression.  That
# expression always evaluates to an empty string.
.if ${:D This is a comment. } != ""
.  error
.endif

# TODO: Add more tests for parsing the plain text part, to cover each branch
# of ApplyModifier_Defined.

all:
	@:;
