# $NetBSD: depsrc-phony.mk,v 1.3 2020/09/05 15:57:12 rillig Exp $
#
# Tests for the special source .PHONY in dependency declarations,
# which executes the commands for the target even if a file of the same
# name exists and would be considered up to date.

# Without the .PHONY, this target would be "up to date".
${MAKEFILE}: .PHONY
	: ${.TARGET:T} is made.
