# $NetBSD: cond-token-plain.mk,v 1.14 2021/12/12 09:36:00 rillig Exp $
#
# Tests for plain tokens (that is, string literals without quotes)
# in .if conditions.  These are also called bare words.

.MAKEFLAGS: -dc

# The word 'value' after the '!=' is a bare word.
.if ${:Uvalue} != value
.  error
.endif

# Using a '#' in a string literal in a condition leads to a malformed
# condition since comment parsing is done in an early phase and removes the
# '#' and everything after it long before the condition parser gets to see it.
#
# XXX: The error message is missing for this malformed condition.
# The right-hand side of the comparison is just a '"', before unescaping.
.if ${:U} != "#hash"
.  error
.endif

# To get a '#' into a condition, it has to be escaped using a backslash.
# This prevents the comment parser from removing it, and in turn, it becomes
# visible to CondParser_String.
.if ${:U\#hash} != "\#hash"
.  error
.endif

# Since 2002-12-30, and still as of 2020-09-11, CondParser_Token handles
# the '#' specially, even though at this point, there should be no need for
# comment handling anymore.  The comments are supposed to be stripped off
# in a very early parsing phase.
#
# See https://gnats.netbsd.org/19596 for example makefiles demonstrating the
# original problems.  At that time, the parser didn't recognize the comment in
# the line '.else # comment3'.  This workaround is not needed anymore since
# comments are stripped in an earlier phase.  See "case '#'" in
# CondParser_Token.
#
# XXX: Missing error message for the malformed condition. The right-hand
# side before unescaping is double-quotes, backslash, backslash.
.if ${:U\\} != "\\#hash"
.  error
.endif

# The right-hand side of a comparison is not parsed as a token, therefore
# the code from CondParser_Token does not apply to it.
# TODO: Explain the consequences.
# TODO: Does this mean that more syntactic variants are allowed here?
.if ${:U\#hash} != \#hash
.  error
.endif

# XXX: What is the purpose of treating an escaped '#' in the following
# condition as a comment?  And why only at the beginning of a token,
# just as in the shell?
.if 0 \# This is treated as a comment, but why?
.  error
.endif

# Ah, ok, this can be used to add an end-of-condition comment.  But does
# anybody really use this?  This is neither documented nor obvious since
# the '#' is escaped.  It's much clearer to write a comment in the line
# above the condition.
.if ${0 \# comment :?yes:no} != no
.  error
.endif
.if ${1 \# comment :?yes:no} != yes
.  error
.endif

# Usually there is whitespace around the comparison operator, but this is
# not required.
.if ${UNDEF:Uundefined}!=undefined
.  error
.endif
.if ${UNDEF:U12345}>12345
.  error
.endif
.if ${UNDEF:U12345}<12345
.  error
.endif
.if (${UNDEF:U0})||0
.  error
.endif

# Only the comparison operator terminates the comparison operand, and it's
# a coincidence that the '!' is both used in the '!=' comparison operator
# as well as for negating a comparison result.
#
# The boolean operators '&' and '|' don't terminate a comparison operand.
.if ${:Uvar}&&name != "var&&name"
.  error
.endif
.if ${:Uvar}||name != "var||name"
.  error
.endif

# A bare word may appear alone in a condition, without any comparison
# operator.  It is implicitly converted into defined(bare).
.if bare
.  error
.else
.  info A bare word is treated like defined(...), and the variable $\
	'bare' is not defined.
.endif

VAR=	defined
.if VAR
.  info A bare word is treated like defined(...).
.else
.  error
.endif

# Bare words may be intermixed with variable expressions.
.if V${:UA}R
.  info ok
.else
.  error
.endif

# In bare words, even undefined variables are allowed.  Without the bare
# words, undefined variables are not allowed.  That feels inconsistent.
.if V${UNDEF}AR
.  info Undefined variables in bare words expand to an empty string.
.else
.  error
.endif

.if 0${:Ux00}
.  error
.else
.  info Numbers can be composed from literals and variable expressions.
.endif

.if 0${:Ux01}
.  info Numbers can be composed from literals and variable expressions.
.else
.  error
.endif

# If the right-hand side is missing, it's a parse error.
.if "" ==
.  error
.else
.  error
.endif

# If the left-hand side is missing, it's a parse error as well, but without
# a specific error message.
.if == ""
.  error
.else
.  error
.endif

# The '\\' is not a line continuation.  Neither is it an unquoted string
# literal.  Instead, it is parsed as a bare word (ParseWord),
# and in that context, the backslash is just an ordinary character. The
# function argument thus stays '\\' (2 backslashes).  This string is passed
# to FuncDefined, and since there is no variable named '\\', the condition
# evaluates to false.
.if \\
.  error
.else
.  info The variable '\\' is not defined.
.endif

${:U\\\\}=	backslash
.if \\
.  info Now the variable '\\' is defined.
.else
.  error
.endif

# Anything that doesn't start with a double quote is considered a "bare word".
# Strangely, a bare word may contain double quotes inside.  Nobody should ever
# depend on this since it may well be unintended.  See CondParser_String.
.if "unquoted\"quoted" != unquoted"quoted
.  error
.endif

# FIXME: In CondParser_String, Var_Parse returns var_Error without a
# corresponding error message.
.if $$$$$$$$ != ""
.  error
.else
.  error
.endif

# In a condition in an .if directive, the left-hand side must not be an
# unquoted string literal.
# expect+1: Malformed conditional (left == right)
.if left == right
.endif
# Before cond.c 1.276 from 2021-09-21, a variable expression containing the
# modifier ':?:' allowed unquoted string literals for the rest of the
# condition.  This was an unintended implementation mistake.
# expect+1: Malformed conditional (${0:?:} || left == right)
.if ${0:?:} || left == right
.endif
# This affected only the comparisons after the expression, so the following
# was still a syntax error.
# expect+1: Malformed conditional (left == right || ${0:?:})
.if left == right || ${0:?:}
.endif

# See cond-token-string.mk for similar tests where the condition is enclosed
# in "quotes".

all:
	@:;
