# $NetBSD: deptgt-ignore.mk,v 1.3 2020/11/15 20:20:58 rillig Exp $
#
# Tests for the special target .IGNORE in dependency declarations, which
# does not stop if a command from this target exits with a non-zero status.

# TODO: Implementation

all:
	@:;
