# $NetBSD: varname-dot-alltargets.mk,v 1.3 2020/08/25 22:51:54 rillig Exp $
#
# Tests for the special .ALLTARGETS variable.

.MAIN: all

TARGETS_1:=	${.ALLTARGETS}

first second: source

TARGETS_2:=	${.ALLTARGETS}

all:
	# Since the tests are run with the -r option, no targets are
	# defined at the beginning.
	@echo ${TARGETS_1}

	# Only first and second are "real" targets.
	# The .ALLTARGETS variable is not about targets though, but
	# about all nodes, therefore source is also included.
	@echo ${TARGETS_2}

	# Interestingly, the .END target is also implicitly defined at
	# this point.
	@echo ${.ALLTARGETS}
