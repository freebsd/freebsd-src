# $NetBSD: varname-dot-make-ppid.mk,v 1.3 2022/01/23 21:48:59 rillig Exp $
#
# Tests for the special .MAKE.PPID variable, which contains the process ID of
# make's parent process.

# The parent process ID must be a positive integer.
.if ${.MAKE.PPID:C,[0-9],,g} != ""
.  error
.elif !(${.MAKE.PPID} > 0)
.  error
.endif

# Ensure that the process exists.
_!=	kill -0 ${.MAKE.PPID}

# The parent process ID must be different from the process ID.  If they were
# the same, make would run as process 1, which is not a good idea because make
# is not prepared to clean up after other processes.
.if ${.MAKE.PPID} == ${.MAKE.PID}
.  error
.endif

all: .PHONY
