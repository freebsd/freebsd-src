# $NetBSD: depsrc-meta.mk,v 1.6 2022/01/26 22:47:03 rillig Exp $
#
# Tests for the special source .META in dependency declarations.

# TODO: Implementation
# TODO: Explanation

.MAIN: all

.if make(actual-test)
.MAKEFLAGS: -dM
.MAKE.MODE=	meta curDirOk=true
.endif

actual-test: depsrc-meta-target
depsrc-meta-target: .META
	@> ${.TARGET}-file
	@rm -f ${.TARGET}-file

check-results:
	@echo 'Targets from meta mode${.MAKE.JOBS:D in jobs mode}:'
	@awk '/^TARGET/ { print "| " $$0 }' depsrc-meta-target.meta
	@rm depsrc-meta-target.meta

all:
	@${MAKE} -r -f ${MAKEFILE} actual-test
	@${MAKE} -r -f ${MAKEFILE} check-results

	@${MAKE} -r -f ${MAKEFILE} actual-test -j1
	@${MAKE} -r -f ${MAKEFILE} check-results -j1
