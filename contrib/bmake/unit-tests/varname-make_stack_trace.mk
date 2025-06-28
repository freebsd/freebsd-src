# $NetBSD: varname-make_stack_trace.mk,v 1.1 2025/06/13 03:51:18 rillig Exp $
#
# Tests for the MAKE_STACK_TRACE environment variable, which controls whether
# to print inter-process stack traces that are useful to narrow down where an
# erroneous expression comes from.
#
# While inter-process stack traces are useful to narrow down errors, they are
# disabled by default since the stack trace is stored in an environment
# variable and a stack trace can grow large depending on the shell commands in
# the sub-make processes.  The space used for the stack traces would compete
# with the space for the command line arguments, and long command lines are
# already written to a temporary file by Cmd_Exec to not overwhelm this space.

all: .PHONY
	@${MAKE} -f ${MAKEFILE} disabled-compat || :
	@${MAKE} -f ${MAKEFILE} -j1 disabled-parallel || :
	@MAKE_STACK_TRACE=yes ${MAKE} -f ${MAKEFILE} enabled-compat || :
	@MAKE_STACK_TRACE=yes ${MAKE} -f ${MAKEFILE} -j1 enabled-parallel || :

# expect-not: in target "disabled-compat"
disabled-compat: .PHONY
	@${MAKE} -f ${MAKEFILE} provoke-error

# expect-not: in target "disabled-parallel"
disabled-parallel: .PHONY
	@${MAKE} -f ${MAKEFILE} provoke-error

# expect: in target "enabled-compat"
enabled-compat: .PHONY
	@${MAKE} -f ${MAKEFILE} provoke-error

# expect: in target "enabled-parallel"
enabled-parallel: .PHONY
	@${MAKE} -f ${MAKEFILE} provoke-error

provoke-error: .PHONY
	@echo ${:Z}
