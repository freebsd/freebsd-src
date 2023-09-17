# $NetBSD: depsrc-silent.mk,v 1.3 2020/08/29 17:34:21 rillig Exp $
#
# Tests for the special source .SILENT in dependency declarations,
# which hides the commands, no matter whether they are prefixed with
# '@' or not.

# Without the .SILENT, the commands 'echo one' and 'echo two' would be
# written to stdout.
all: .SILENT
	echo one
	echo two
	@echo three
