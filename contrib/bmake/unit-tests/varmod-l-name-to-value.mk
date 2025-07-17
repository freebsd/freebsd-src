# $NetBSD: varmod-l-name-to-value.mk,v 1.8 2023/11/19 21:47:52 rillig Exp $
#
# Tests for the :L modifier, which returns the variable name as the new value.

# The empty variable name leads to an empty string.
.if ${:L} != ""
.  error
.endif

# The variable name is converted into an expression with the variable name
# "VARNAME" and the value "VARNAME".
.if ${VARNAME:L} != "VARNAME"
.  error
.endif

# The value of the expression can be modified afterwards.
.if ${VARNAME:L:S,VAR,,} != "NAME"
.  error
.endif

# The name of the expression is still the same as before. Using the :L
# modifier, it can be restored.
#
# Hmmm, this can be used as a double storage or a backup mechanism.
# Probably unintended, but maybe useful.
.if ${VARNAME:L:S,VAR,,:L} != "VARNAME"
.  error
.endif

# Between 2020-09-22 (var.c 1.527) and 2020-09-30 (var.c 1.553), there was
# a bug in the evaluation of expressions.  Indirect modifiers like
# the below :L did not update the definedness of the enclosing expression.
# This resulted in a wrong "Malformed conditional".
.if ${value:${:UL}} == ""
.endif

# As of 2020-10-02, the :L modifier does not ensure that it is followed by
# a delimiter, that is, a ':' or endc.  Neither does the :P modifier.
.if ${value:LLLLLLPL} != "value"
.  error
.endif

all:
	@:;
