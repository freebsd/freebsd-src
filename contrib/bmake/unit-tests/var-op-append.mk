# $NetBSD: var-op-append.mk,v 1.12 2023/11/02 05:46:26 rillig Exp $
#
# Tests for the '+=' variable assignment operator, which appends to a
# variable, creating it if necessary.
#
# See also
#	var-op.mk
#
# Standards
#	The '+=' variable assignment operator is planned to be added in
#	POSIX.1-202x.
#
#	This implementation does not support the immediate-expansion macros
#	specified in POSIX.1-202x.  All variables are delayed-expansion.
#
# History
#	The '+=' variable assignment operator was added before 1993-03-21.

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
# See Parse_Var, AdjustVarassignOp.
C++=	value
.if ${C+} != "value" || defined(C++)
.  error
.endif

# Before var.c 1.793 from 2021-02-03, the variable name of a newly created
# variable was expanded two times in a row, which was unexpected but
# irrelevant in practice since variable names containing dollars lead to
# strange side effects in several other places as well.
.MAKEFLAGS: -dv
VAR.${:U\$\$\$\$\$\$\$\$}+=	dollars
.MAKEFLAGS: -d0
.if ${VAR.${:U\$\$\$\$\$\$\$\$}} != "dollars"
.  error
.endif


# Appending to an environment variable in the global scope creates a global
# variable of the same name, taking its initial value from the environment
# variable.  After the assignment, the environment variable is left as-is,
# the value of the global variable is not synced back to the environment
# variable.
export ENV_PLUS_GLOBAL=from-env-value
ENV_PLUS_GLOBAL+=	appended-value
.if ${ENV_PLUS_GLOBAL} != "from-env-value appended-value"
.  error
.endif
EXPORTED!=	echo "$$ENV_PLUS_GLOBAL"
.if ${EXPORTED} != "from-env-value"
.  error
.endif

# Appending to an environment variable in the command line scope ignores the
# environment variable.
export ENV_PLUS_COMMAND=from-env-value
.MAKEFLAGS: ENV_PLUS_COMMAND+=appended-command
.if ${ENV_PLUS_COMMAND} != "appended-command"
.  error ${ENV_PLUS_COMMAND}
.endif
EXPORTED!=	echo "$$ENV_PLUS_GLOBAL"
.if ${EXPORTED} != "from-env-value"
.  error
.endif


all:
