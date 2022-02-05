# $NetBSD: varname-dot-make-jobs.mk,v 1.3 2022/01/26 22:47:03 rillig Exp $
#
# Tests for the special .MAKE.JOBS variable, which is defined in jobs mode
# only.  There it contains the number of jobs that may run in parallel.

.MAIN: all

echo: .PHONY
	@echo ${.MAKE.JOBS:Uundefined}

all:
	@${MAKE} -r -f ${MAKEFILE} echo
	@${MAKE} -r -f ${MAKEFILE} echo -j1
	@${MAKE} -r -f ${MAKEFILE} echo -j5
	@${MAKE} -r -f ${MAKEFILE} echo -j20
	@${MAKE} -r -f ${MAKEFILE} echo -j00000000000000000000000000000001

# expect: undefined
# expect: 1
# expect: 5
# expect: 20
# The value of .MAKE.JOBS is the exact text given in the command line, not the
# canonical number.  This doesn't have practical consequences though.
# expect: 00000000000000000000000000000001
