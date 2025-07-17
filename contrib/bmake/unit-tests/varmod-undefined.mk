# $NetBSD: varmod-undefined.mk,v 1.11 2024/06/03 02:46:29 sjg Exp $
#
# Tests for the :U variable modifier, which returns the given string
# if the variable is undefined.
#
# See also:
#	directive-for.mk
#	varmod-defined.mk

# this test depends on
.MAKE.SAVE_DOLLARS= yes

# The pattern ${:Uword} is heavily used when expanding .for loops.
#
# This is how an expanded .for loop looks like.
# .for word in one
# .  if ${word} != one
.if ${:Uone} != one
# .    error ${word}
.  error ${:Uone}
# .  endif
.endif
# .endfor

# The expressions in the text of the :U modifier may be arbitrarily
# nested.

.if ${:U${:Unested}${${${:Udeeply}}}} != nested
.  error
.endif

# The nested expressions may contain braces, and these braces don't
# need to match pairwise.  In the following example, the :S modifier uses '{'
# as delimiter, which confuses both editors and humans because the opening
# and closing braces don't match anymore.  It's syntactically valid though.
# For more similar examples, see varmod-subst.mk, mod-subst-delimiter.

.if ${:U${:Uvalue:S{a{X{}} != vXlue
.  error
.endif

# The escaping rules for the :U modifier (left-hand side) and condition
# string literals (right-hand side) are completely different.
#
# In the :U modifier, the backslash only escapes very few characters, all
# other backslashes are retained.
#
# In condition string literals, the backslash always escapes the following
# character, no matter whether it would be necessary or not.
#
# In both contexts, \n is an escaped letter n, not a newline; that's what
# the .newline variable is for.
#
# Whitespace at the edges is preserved, on both sides of the comparison.
#
.if ${:U \: \} \$ \\ \a \b \n } != " : } \$ \\ \\a \\b \\n "
.  error
.endif
# An expression enclosed in quotes may be based on an undefined variable.
.if "${:U \: \} \$ \\ \a \b \n }" != " : } \$ \\ \\a \\b \\n "
.  error
.endif

# Even after the :U modifier has been applied, the expression still remembers
# that it originated from an undefined variable, and the :U modifier can
# be used to overwrite the value of the expression.
#
.if ${UNDEF:Uvalue:S,a,X,} != "vXlue"
.  error
.elif ${UNDEF:Uvalue:S,a,X,:Uwas undefined} != "was undefined"
.  error
.endif


# VARE_PARSE
.if 0 && ${:U . \: \} \$ \\ ${EXPR}}
.  error
.endif

# VARE_EVAL_KEEP_DOLLAR_AND_UNDEFINED
SUBST:=		${:U . \: \} \$ \\ ${EXPR}}
${:U }=		<space>
EXPR=		<expr>
.if ${SUBST} != " . : } <space>\\ "
.  error
.endif

8_DOLLAR=	$$$$$$$$
.if ${8_DOLLAR} != "\$\$\$\$"
.  error
.endif
.if ${:U${8_DOLLAR}} != "\$\$\$\$"
.  error
.endif
.if ${x:L:@_@${8_DOLLAR}@} != "\$\$\$\$"
.  error
.endif
EXPR:=		${8_DOLLAR}
.if ${EXPR} != "\$\$\$\$"
.  error
.endif
EXPR:=		${:U${8_DOLLAR}}
.if ${EXPR} != "\$\$\$\$"
.  error
.endif
# VARE_EVAL_KEEP_UNDEFINED
EXPR:=		${x:L:@_@${8_DOLLAR}@}
.if ${EXPR} != "\$\$"
.  error
.endif


all: .PHONY
