# $NetBSD: depsrc-wait.mk,v 1.4 2022/05/07 17:49:47 rillig Exp $
#
# Tests for the special source .WAIT in dependency declarations,
# which adds a sequence point between the nodes to its left and the nodes
# to its right.

all: .PHONY
	@${MAKE} -r -f ${MAKEFILE} x
	@${MAKE} -r -f ${MAKEFILE} three-by-three


.if make(x)
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
.endif


# There are 3 groups of 3 targets, with .WAIT barriers in between.  Each of
# these groups has to be made completely before starting the next group.
# See Makefile, POSTPROC for the postprocessing that takes place.
.if make(three-by-three)
.MAKEFLAGS: -j5
.MAKE.MODE+=	randomize-targets

three-by-three: .WAIT 3a1 3a2 3a3 .WAIT 3b1 3b2 3b3 .WAIT 3c1 3c2 3c3 .WAIT
3a1 3a2 3a3 3b1 3b2 3b3 3c1 3c2 3c3:
	: Making ${.TARGET}
.endif
