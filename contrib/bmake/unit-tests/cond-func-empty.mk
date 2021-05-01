# $NetBSD: cond-func-empty.mk,v 1.11 2020/11/28 14:08:37 rillig Exp $
#
# Tests for the empty() function in .if conditions, which tests a variable
# expression for emptiness.
#
# Note that the argument in the parentheses is indeed a variable name,
# optionally followed by variable modifiers.
#

.undef UNDEF
EMPTY=	# empty
SPACE=	${:U }
WORD=	word

# An undefined variable is empty.
.if !empty(UNDEF)
.  error
.endif

# An undefined variable has the empty string as the value, and the :M
# variable modifier does not change that.
#
.if !empty(UNDEF:M*)
.  error
.endif

# The :S modifier replaces the empty value with an actual word.  The
# expression is now no longer empty, but it is still possible to see whether
# the expression was based on an undefined variable.  The expression has the
# flag VEF_UNDEF.
#
# The expression does not have the flag VEF_DEF though, therefore it is still
# considered undefined.  Yes, indeed, undefined but not empty.  There are a
# few variable modifiers that turn an undefined expression into a defined
# expression, among them :U and :D, but not :S.
#
# XXX: This is hard to explain to someone who doesn't know these
# implementation details.
#
.if !empty(UNDEF:S,^$,value,W)
.  error
.endif

# The :U modifier modifies expressions based on undefined variables
# (VAR_JUNK) by adding the VAR_KEEP flag, which marks the expression
# as "being interesting enough to be further processed".
#
.if empty(UNDEF:S,^$,value,W:Ufallback)
.  error
.endif

# And now to the surprising part.  Applying the following :S modifier to the
# undefined expression makes it non-empty, but the marker VEF_UNDEF is
# preserved nevertheless.  The :U modifier that follows only looks at the
# VEF_UNDEF flag to decide whether the variable is defined or not.  This kind
# of makes sense since the :U modifier tests the _variable_, not the
# _expression_.
#
# But since the variable was undefined to begin with, the fallback value from
# the :U modifier is used in this expression.
#
.if ${UNDEF:S,^$,value,W:Ufallback} != "fallback"
.  error
.endif

# The variable EMPTY is completely empty (0 characters).
.if !empty(EMPTY)
.  error
.endif

# The variable SPACE has a single space, which counts as being empty.
.if !empty(SPACE)
.  error
.endif

# The variable .newline has a single newline, which counts as being empty.
.if !empty(.newline)
.  error
.endif

# The empty variable named "" gets a fallback value of " ", which counts as
# empty.
#
# Contrary to the other functions in conditionals, the trailing space is not
# stripped off, as can be seen in the -dv debug log.  If the space had been
# stripped, it wouldn't make a difference in this case.
#
.if !empty(:U )
.  error
.endif

# Now the variable named " " gets a non-empty value, which demonstrates that
# neither leading nor trailing spaces are trimmed in the argument of the
# function.  If the spaces were trimmed, the variable name would be "" and
# that variable is indeed undefined.  Since ParseEmptyArg calls Var_Parse
# without VARE_UNDEFERR, the value of the undefined variable is returned as
# an empty string.
${:U }=	space
.if empty( )
.  error
.endif

# The value of the following expression is " word", which is not empty.
.if empty(:U word)
.  error
.endif

# The :L modifier creates a variable expression that has the same value as
# its name, which both are "VAR" in this case.  The value is therefore not
# empty.
.if empty(VAR:L)
.  error
.endif

# The variable WORD has the value "word", which does not count as empty.
.if empty(WORD)
.  error
.endif

# The expression ${} for a variable with the empty name always evaluates
# to an empty string (see Var_Parse, varUndefined).
.if !empty()
.  error
.endif

# Ensure that variable expressions that appear as part of the argument are
# properly parsed.  Typical use cases for this are .for loops, which are
# expanded to exactly these ${:U} expressions.
#
# If everything goes well, the argument expands to "WORD", and that variable
# is defined at the beginning of this file.  The surrounding 'W' and 'D'
# ensure that the parser in ParseEmptyArg has the correct position, both
# before and after the call to Var_Parse.
.if empty(W${:UOR}D)
.  error
.endif

# There may be spaces at the outside of the parentheses.
# Spaces inside the parentheses are interpreted as part of the variable name.
.if ! empty ( WORD )
.  error
.endif

${:U WORD }=	variable name with spaces

# Now there is a variable named " WORD ", and it is not empty.
.if empty ( WORD )
.  error
.endif

# Parse error: missing closing parenthesis.
.if empty(WORD
.  error
.else
.  error
.endif

# Between 2020-06-28 and var.c 1.226 from 2020-07-02, this paragraph generated
# a wrong error message "Variable VARNAME is recursive".
#
# The bug was that the !empty() condition was evaluated, even though this was
# not necessary since the defined() condition already evaluated to false.
#
# When evaluating the !empty condition, the variable name was parsed as
# "VARNAME${:U2}", but without expanding any nested variable expression, in
# this case the ${:U2}.  Therefore, the variable name came out as simply
# "VARNAME".  Since this variable name should have been discarded quickly after
# parsing it, this unrealistic variable name should have done no harm.
#
# The variable expression was expanded though, and this was wrong.  The
# expansion was done without the VARE_WANTRES flag (called VARF_WANTRES back
# then) though.  This had the effect that the ${:U1} from the value of VARNAME
# expanded to an empty string.  This in turn created the seemingly recursive
# definition VARNAME=${VARNAME}, and that definition was never meant to be
# expanded.
#
# This was fixed by expanding nested variable expressions in the variable name
# only if the flag VARE_WANTRES is given.
VARNAME=	${VARNAME${:U1}}
.if defined(VARNAME${:U2}) && !empty(VARNAME${:U2})
.endif

all:
	@:;
