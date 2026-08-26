# $NetBSD: varname-make_stack_trace.mk,v 1.8 2026/03/10 05:02:00 sjg Exp $
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

_make ?= .make${.MAKE.PID}
.export _make

all: .PHONY
	@${MAKE} -f ${MAKEFILE} disabled-compat || :
	@${MAKE} -f ${MAKEFILE} -j1 disabled-parallel || :
	@MAKE_STACK_TRACE=yes ${MAKE} -f ${MAKEFILE} enabled-compat || :
	@MAKE_STACK_TRACE=yes ${MAKE} -f ${MAKEFILE} -j1 enabled-parallel || :
	@MAKE_STACK_TRACE=yes ${MAKE} -f ${MAKEFILE} -j1 multi-stage-1
	@rm -f ${_make}

# expect-not-matches: in target "disabled%-compat"
disabled-compat: .PHONY
	@${MAKE} -f ${MAKEFILE} provoke-error

# expect-not-matches: in target "disabled%-parallel"
disabled-parallel: .PHONY
	@${MAKE} -f ${MAKEFILE} provoke-error

# expect: in target "enabled-compat" from varname-make_stack_trace.mk:35
enabled-compat: .PHONY
	@${MAKE} -f ${MAKEFILE} provoke-error

# expect: in target "enabled-parallel" from varname-make_stack_trace.mk:39
enabled-parallel: .PHONY
	@${MAKE} -f ${MAKEFILE} provoke-error

provoke-error: .PHONY
	@echo ${:Z}

# The stack trace must be printed exactly once.
# expect: in target "multi-stage-4" from varname-make_stack_trace.mk:56
# expect: in target "multi-stage-1" from varname-make_stack_trace.mk:50
# expect-not-matches: in target "multi%-stage%-4"
# expect-not-matches: in target "multi%-stage%-1"
multi-stage-1: .PHONY ${_make}
	@${MAKE} -f ${MAKEFILE} -j1 multi-stage-2
multi-stage-2: .PHONY
	@${MAKE} -f ${MAKEFILE} -j1 multi-stage-3
multi-stage-3: .PHONY
	@${MAKE} -f ${MAKEFILE} -j1 multi-stage-4
multi-stage-4: .PHONY
	@./${_make} -f ${MAKEFILE} -j1 multi-stage-5
multi-stage-5: .PHONY

${_make}:
	@ln -s ${MAKE} ${.TARGET}

# for FreeBSD and similar make sure we get the expected errors.
.MAKE.ALWAYS_PASS_JOB_QUEUE= no
