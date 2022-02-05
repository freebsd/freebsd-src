# $NetBSD: deptgt-interrupt.mk,v 1.4 2022/01/22 21:50:41 rillig Exp $
#
# Tests for the special target .INTERRUPT in dependency declarations, which
# collects commands to be run when make is interrupted while building another
# target.

all:
	@kill -INT ${.MAKE.PID}

.INTERRUPT:
	@echo 'Ctrl-C'
