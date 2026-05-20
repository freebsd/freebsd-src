# $NetBSD: varmod-loop-delete.mk,v 1.9 2026/02/09 22:04:54 rillig Exp $
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

all: .PHONY
	${MAKE} -f ${MAKEFILE} delete-active-variable || true
	@echo
	${MAKE} -f ${MAKEFILE} delete-active-variable-in-target || true
	@echo
	# Disabled since the details of the crash depend on the execution
	# environment.
	#${MAKE} -f ${MAKEFILE} use-after-free

delete-active-variable: .PHONY
.if make(delete-active-variable)
# Set up the variable that deletes itself when it is evaluated.
VAR=	${:U:@VAR@@} rest of the value

# In an assignment, the scope is 'Global'.  Since the variable 'VAR' is
# defined in the global scope, it deletes itself.
# expect+1: Cannot delete variable "VAR" while it is used
EVAL:=	${VAR}
.  if ${EVAL} != " rest of the value"
.    error
.  endif
.endif

delete-active-variable-in-target: .PHONY
.if make(delete-active-variable-in-target)
VAR=	${:U:@VAR@@} rest of the value

delete-active-variable-in-target:
	# In the command that is associated with a target, the scope is the
	# one from the target.  That scope only contains a few variables like
	# '.TARGET', '.ALLSRC', '.IMPSRC'.  Make does not expect that these
	# variables get modified from the outside.
	#
	# There is no variable named 'VAR' in the local scope, so nothing
	# happens.
	: $@: '${VAR}'
# expect: : delete-active-variable-in-target: ' rest of the value'
.endif


# On NetBSD 11.99.x with jemalloc and MALLOC_CONF=junk:true, the output is:
#	make: varmod-loop-delete.mk:72: Unknown modifier ":Z2"
#	        while evaluating "${:U 333 :@v@...${:Z1}@:Z2}" with value "...${:Z1}"
#	        while evaluating variable "INNER.1" with value "${:U 333 :@v@...${:Z1}@:Z2}"
#	        while evaluating variable "ZZZZZZZZZZZZ...${:Z1}" with value "ZZZZZZZZ"
#	        while evaluating "${:U 111 222 :@v@${v:S,^,${INNER.1},}@}" with value " 111 222 "
#	        while evaluating variable "OUTER" with value "${:U 111 222 :@v@${v:S,^,${INNER.1},}@}"
#	        in make in directory "<curdir>"
#	Modifier part: ""
#	ModifyWords: split "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ" into 1 word
#	Result of ${ZZZZZZZZZZZZ:S,^,${INNER.1},} is "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ" (eval-keep-undefined, regular)
#	Segmentation fault (core dumped)
use-after-free: .PHONY
.if make(use-after-free)
OUTER=		${:U 111 222 :@v@${v:S,^,${INNER.1},}@}
INNER.1=	${:U 333 :@v@...${:Z1}@:Z2}

.MAKEFLAGS: -dcpv
_:= ${OUTER}
.MAKEFLAGS: -d0
.endif
