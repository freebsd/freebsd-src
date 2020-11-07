# $NetBSD: varmod-remember.mk,v 1.3 2020/08/23 15:18:43 rillig Exp $
#
# Tests for the :_ modifier, which saves the current variable value
# in the _ variable or another, to be used later again.

# In the parameterized form, having the variable name on the right side of
# the = assignment operator is confusing.  In almost all other situations
# the variable name is on the left-hand side of the = operator.  Luckily
# this modifier is only rarely needed.
all:
	@echo ${1 2 3:L:_:@var@${_}@}
	@echo ${1 2 3:L:@var@${var:_=SAVED:}@}, SAVED=${SAVED}
