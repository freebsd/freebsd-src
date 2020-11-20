# $NetBSD: sh-meta-chars.mk,v 1.3 2020/11/15 20:20:58 rillig Exp $
#
# Tests for running shell commands that contain meta-characters.
#
# These meta-characters decide whether the command is run by the shell
# or executed directly via execv, but only in compatibility mode, not
# in jobs mode, and only if MAKE_NATIVE is defined during compilation.
#
# See also:
#	Compat_RunCommand, useShell

# TODO: Implementation

all:
	@:;
