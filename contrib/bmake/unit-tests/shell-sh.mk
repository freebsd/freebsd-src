# $NetBSD: shell-sh.mk,v 1.2 2023/12/24 16:48:30 sjg Exp $
#
# Tests for using a bourne shell for running the commands.
# This is the default shell, so there's nothing surprising.

.SHELL: name="sh"

all:
	: normal
	@: hidden
	+: always
	-: ignore errors
