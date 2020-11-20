# $NetBSD: var-op-append.mk,v 1.7 2020/10/30 20:36:33 rillig Exp $
#
# Tests for the += variable assignment operator, which appends to a variable,
# creating it if necessary.

# Appending to an undefined variable is possible.
# The variable is created, and no extra space is added before the value.
VAR+=	one
.if ${VAR} != "one"
.  error
.endif

# Appending to an existing variable adds a single space and the value.
VAR+=	two
.if ${VAR} != "one two"
.  error
.endif

# Appending an empty string nevertheless adds a single space.
VAR+=	# empty
.if ${VAR} != "one two "
.  error
.endif

# Variable names may contain '+', and this character is also part of the
# '+=' assignment operator.  As far as possible, the '+' is interpreted as
# part of the assignment operator.
#
# See Parse_DoVar
C++=	value
.if ${C+} != "value" || defined(C++)
.  error
.endif

# Try out how often the variable name is expanded when appending to a
# nonexistent variable.
# As of 2020-10-30, that's two times.
# XXX: That's one time too often.
# See Var_Append, the call to Var_Set.
.MAKEFLAGS: -dv
VAR.${:U\$\$\$\$\$\$\$\$}+=	dollars
.MAKEFLAGS: -d0
.if ${VAR.${:U\$\$\$\$}} != "dollars"
.  error
.endif

all:
	@:;
