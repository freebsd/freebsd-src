# $NetBSD: deptgt-default.mk,v 1.3 2021/12/01 23:56:29 rillig Exp $
#
# Tests for the special target .DEFAULT in dependency declarations, which
# attaches its associated commands to all targets that don't specify any way
# to create them.

all: test-default not-a-target

test-default: .PHONY

has-commands: .PHONY
	@echo 'Making ${.TARGET} from ${.IMPSRC}.'

.DEFAULT: dependency-is-ignored
	@echo "Default command is making '${.TARGET}' from '${.IMPSRC}'."

all:
