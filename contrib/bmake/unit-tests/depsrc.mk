# $NetBSD: depsrc.mk,v 1.3 2020/11/15 20:20:58 rillig Exp $
#
# Tests for special sources (those starting with a dot, followed by
# uppercase letters) in dependency declarations, such as .PHONY.

# TODO: Implementation

# TODO: Test 'target: ${:U.SILENT}'

all:
	@:;
