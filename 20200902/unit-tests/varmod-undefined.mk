# $NetBSD: varmod-undefined.mk,v 1.3 2020/08/23 20:49:33 rillig Exp $
#
# Tests for the :U variable modifier, which returns the given string
# if the variable is undefined.
#
# The pattern ${:Uword} is heavily used when expanding .for loops.

# This is how an expanded .for loop looks like.
# .for word in one
# .  if ${word} != one
# .    error ${word}
# .  endif
# .endfor

.if ${:Uone} != one
.  error ${:Uone}
.endif

# The variable expressions in the text of the :U modifier may be arbitrarily
# nested.

.if ${:U${:Unested}${${${:Udeeply}}}} != nested
.error
.endif

# The nested variable expressions may contain braces, and these braces don't
# need to match pairwise.  In the following example, the :S modifier uses '{'
# as delimiter, which confuses both editors and humans because the opening
# and # closing braces don't match anymore.  It's syntactically valid though.
# For more similar examples, see varmod-subst.mk, mod-subst-delimiter.

.if ${:U${:Uvalue:S{a{X{}} != vXlue
.error
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

.if ${:U \: \} \$ \\ \a \b \n } != " : } \$ \\ \\a \\b \\n "
.error
.endif

all:
	@:;
