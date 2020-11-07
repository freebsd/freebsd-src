# $NetBSD: deptgt-silent.mk,v 1.3 2020/09/10 21:40:50 rillig Exp $
#
# Tests for the special target .SILENT in dependency declarations.

.SILENT: all

all:
	@echo 'This is not echoed because of the @.'
	# Without the .SILENT, the following command would be echoed.
	echo 'This is not echoed because of the .SILENT.'
