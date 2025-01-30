# $NetBSD: varmod-assign-shell.mk,v 1.11 2025/01/11 21:21:33 rillig Exp $
#
# Tests for the variable modifier '::!=', which assigns the output of a shell
# command to the variable, but only if the command exited successfully.  This
# is different from the other places that capture the output of an external
# command (variable assignment operator '!=', expression modifier ':sh',
# expression modifier ':!...!'), which also use the output when the shell
# command fails or crashes.
#
# The variable modifier '::!=' and its close relatives have been around since
# var.c 1.45 from 2000-06-01.
#
# Before 2020.08.25.21.16.53, the variable modifier '::!=' had a bug for
# unsuccessful commands, it put the previous value of the variable into the
# error message instead of the command that was executed.  That's where the
# counterintuitive error message 'make: "previous" returned non-zero status'
# comes from.

DIRECT=		previous
# expect+1: warning: Command "echo output; (exit 13)" exited with status 13
DIRECT!=	echo output; (exit 13)

ASSIGNED=	previous
.MAKEFLAGS: -dv			# to see the "Capturing" debug output
# expect+1: warning: Command "echo output; (exit 13)" exited with status 13
_:=		${ASSIGNED::!=echo output; ${:U(exit 13)}}
.MAKEFLAGS: -d0

all:
	@echo DIRECT=${DIRECT:Q}
	@echo ASSIGNED=${ASSIGNED:Q}
