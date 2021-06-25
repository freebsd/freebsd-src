# $NetBSD: varmod-remember.mk,v 1.6 2021/03/14 17:27:27 rillig Exp $
#
# Tests for the :_ modifier, which saves the current variable value
# in the _ variable or another, to be used later again.

.if ${1 2 3:L:_:@var@${_}@} != "1 2 3 1 2 3 1 2 3"
.  error
.endif

# In the parameterized form, having the variable name on the right side of
# the = assignment operator is confusing.  In almost all other situations
# the variable name is on the left-hand side of the = operator.  Luckily
# this modifier is only rarely needed.
.if ${1 2 3:L:@var@${var:_=SAVED:}@} != "1 2 3"
.  error
.elif ${SAVED} != "3"
.  error
.endif

# The ':_' modifier takes a variable name as optional argument.  This variable
# name can refer to other variables, though this was rather an implementation
# oversight than an intended feature.  The variable name stops at the first
# '}' or ')' and thus cannot use the usual form ${VARNAME} of long variable
# names.
#
# Because of all these edge-casey conditions, this "feature" has been removed
# in var.c 1.867 from 2021-03-14.
S=	INDIRECT_VARNAME
.if ${value:L:@var@${var:_=$S}@} != "value"
.  error
.elif defined(INDIRECT_VARNAME)
.  error
.endif

all:
