# $NetBSD: varmod-loop-delete.mk,v 1.3 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the variable modifier ':@', which as a side effect allows to
# delete an arbitrary variable.

# A side effect of the modifier ':@' is that the loop variable is created as
# an actual variable in the current evaluation scope (Command/Global/target),
# and at the end of the loop, this variable is deleted.  Since var.c 1.204
# from 2016-02-18 and before var.c 1.963 from 2021-12-05, a variable could be
# deleted while it was in use, leading to a use-after-free bug.
#
# See Var_Parse, comment 'the value of the variable must not change'.

# Set up the variable that deletes itself when it is evaluated.
VAR=	${:U:@VAR@@} rest of the value

# In an assignment, the scope is 'Global'.  Since the variable 'VAR' is
# defined in the global scope, it deletes itself.
# expect+1: Cannot delete variable "VAR" while it is used
EVAL:=	${VAR}
.if ${EVAL} != " rest of the value"
.  error
.endif

VAR=	${:U:@VAR@@} rest of the value
all: .PHONY
	# In the command that is associated with a target, the scope is the
	# one from the target.  That scope only contains a few variables like
	# '.TARGET', '.ALLSRC', '.IMPSRC'.  Make does not expect that these
	# variables get modified from the outside.
	#
	# There is no variable named 'VAR' in the local scope, so nothing
	# happens.
	: $@: '${VAR}'
