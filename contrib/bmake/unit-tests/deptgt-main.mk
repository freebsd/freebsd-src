# $NetBSD: deptgt-main.mk,v 1.3 2020/11/15 20:20:58 rillig Exp $
#
# Tests for the special target .MAIN in dependency declarations, which defines
# the main target.  This main target is built if no target has been specified
# on the command line or via MAKEFLAGS.

# TODO: Implementation

all:
	@:;
