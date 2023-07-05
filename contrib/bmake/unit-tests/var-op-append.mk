# $NetBSD: var-op-append.mk,v 1.10 2023/06/21 07:30:50 rillig Exp $
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
# See Parse_Var
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

all:
