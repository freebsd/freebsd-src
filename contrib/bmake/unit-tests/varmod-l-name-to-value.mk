# $NetBSD: varmod-l-name-to-value.mk,v 1.3 2020/08/25 22:25:05 rillig Exp $
#
# Tests for the :L modifier, which returns the variable name as the new value.

# The empty variable name leads to an empty string.
.if ${:L} != ""
.error
.endif

# The variable name is converted into an expression with the variable name
# "VARNAME" and the value "VARNAME".
.if ${VARNAME:L} != "VARNAME"
.error
.endif

# The value of the expression can be modified afterwards.
.if ${VARNAME:L:S,VAR,,} != "NAME"
.error
.endif

# The name of the expression is still the same as before. Using the :L
# modifier, it can be restored.
#
# Hmmm, this can be used as a double storage or a backup mechanism.
# Probably unintended, but maybe useful.
.if ${VARNAME:L:S,VAR,,:L} != "VARNAME"
.error
.endif

all:
	@:;
