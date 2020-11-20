# $NetBSD: deptgt-interrupt.mk,v 1.3 2020/11/15 20:20:58 rillig Exp $
#
# Tests for the special target .INTERRUPT in dependency declarations, which
# collects commands to be run when make is interrupted while building another
# target.

# TODO: Implementation

all:
	@:;
