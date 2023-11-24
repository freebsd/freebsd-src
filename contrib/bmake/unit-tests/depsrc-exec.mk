# $NetBSD: depsrc-exec.mk,v 1.3 2020/09/05 15:57:12 rillig Exp $
#
# Tests for the special source .EXEC in dependency declarations,
# which always executes the commands, even if the target is up to date.
# The target itself is considered up to date.
#
# TODO: Describe possible use cases for .EXEC.

all: ${MAKEFILE} ${MAKEFILE:H}/depsrc.mk

${MAKEFILE}: .EXEC
	: ${.TARGET:T}: This is always executed.

${MAKEFILE:H}/depsrc.mk:
	: This is not executed.
	+: ${.TARGET:T}: This is not executed as well.
