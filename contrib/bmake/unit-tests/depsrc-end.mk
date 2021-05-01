# $NetBSD: depsrc-end.mk,v 1.1 2020/10/23 19:23:01 rillig Exp $
#
# Demonstrate the edge case that .BEGIN depends on .END, which sounds a bit
# paradox but works since these special nodes are not in the dependency
# hierarchy where the cycles are detected.

.BEGIN:
	: 'Making ${.TARGET}.'
.END:
	: 'Making ${.TARGET}.'
all:
	: 'Making ${.TARGET}.'

.BEGIN: .END
