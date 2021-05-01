# $NetBSD: dep-wildcards.mk,v 1.3 2020/09/08 05:33:05 rillig Exp $
#
# Tests for wildcards such as *.c in dependency declarations.

all: ${.PARSEDIR}/dep-*.mk
	# The :T is necessary to run this test from another directory.
	# The :O is necessary since the result of the dependency resolution
	# does not order the directory entries itself.
	@printf '%s\n' ${.ALLSRC:T:O}
