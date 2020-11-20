# $NetBSD: depsrc-wait.mk,v 1.3 2020/09/07 18:40:32 rillig Exp $
#
# Tests for the special source .WAIT in dependency declarations,
# which adds a sequence point between the nodes to its left and the nodes
# to its right.

# Even though the build could run massively parallel, the .WAIT imposes a
# strict ordering in this example, which forces the targets to be made in
# exactly this order.
.MAKEFLAGS: -j8

# This is the example from the manual page.
.PHONY: x a b b1
x: a .WAIT b
	echo x
a:
	echo a
b: b1
	echo b
b1:
	echo b1
