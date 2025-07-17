# $NetBSD: varmod-defined.mk,v 1.16 2023/11/19 21:47:52 rillig Exp $
#
# Tests for the :D variable modifier, which returns the given string
# if the variable is defined.  It is closely related to the :U modifier.

# Force the test results to be independent of the default value of this
# setting, which is 'yes' for NetBSD's usr.bin/make but 'no' for the bmake
# distribution and pkgsrc/devel/bmake.
.MAKE.SAVE_DOLLARS=	yes

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

# Like in several other places in expressions, when
# ApplyModifier_Defined calls Var_Parse, double dollars lead to a parse
# error that is silently ignored.  This makes all dollar signs disappear,
# except for the last, which is a well-formed expression.
#
.if ${DEF:D$$$$$${DEF}} != "defined"
.  error
.endif

# Any other text is written without any further escaping.  In contrast
# to the :M modifier, parentheses and braces do not need to be nested.
# Instead, the :D modifier is implemented sanely by parsing nested
# expressions as such, without trying any shortcuts. See ParseModifier_Match
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

# The :D and :U modifiers behave differently from the :@var@ modifier in
# that they preserve dollars in a ':=' assignment.  This is because
# ApplyModifier_Defined passes the emode unmodified to Var_Parse, unlike
# ApplyModifier_Loop, which uses ParseModifierPart, which in turn removes
# the keepDollar flag from emode.
#
# XXX: This inconsistency is documented nowhere.
.MAKEFLAGS: -dv
8_DOLLARS=	$$$$$$$$
VAR:=		${8_DOLLARS}
VAR:=		${VAR:D${8_DOLLARS}}
VAR:=		${VAR:@var@${8_DOLLARS}@}
.MAKEFLAGS: -d0


# Before var.c 1.1030 from 2022-08-24, the following expression caused an
# out-of-bounds read when parsing the indirect ':U' modifier.
M_U_backslash:=	${:UU\\}
.if ${:${M_U_backslash}} != "\\"
.  error
.endif


all: .PHONY
