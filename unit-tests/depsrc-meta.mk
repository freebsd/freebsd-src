# $NetBSD: depsrc-meta.mk,v 1.4 2020/11/27 08:39:07 rillig Exp $
#
# Tests for the special source .META in dependency declarations.

# TODO: Implementation
# TODO: Explanation

.if make(actual-test)

.MAKEFLAGS: -dM
.MAKE.MODE=	meta curDirOk=true

actual-test: depsrc-meta-target
depsrc-meta-target: .META
	@> ${.TARGET}-file
	@rm -f ${.TARGET}-file

.elif make(check-results)

check-results:
	@echo 'Targets from meta mode:'
	@awk '/^TARGET/ { print "| " $$0 }' depsrc-meta-target.meta
	@rm depsrc-meta-target.meta

.else

all:
	@${MAKE} -f ${MAKEFILE} actual-test
	@${MAKE} -f ${MAKEFILE} check-results

.endif
