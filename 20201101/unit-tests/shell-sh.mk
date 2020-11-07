# $NetBSD: shell-sh.mk,v 1.1 2020/10/03 14:39:36 rillig Exp $
#
# Tests for using a bourne shell for running the commands.
# This is the default shell, so there's nothing surprising.

.SHELL: name="sh" path="sh"

all:
	: normal
	@: hidden
	+: always
	-: ignore errors
