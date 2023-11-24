# $NetBSD: dep-op-missing.mk,v 1.1 2021/12/14 00:02:57 rillig Exp $
#
# Test for a missing dependency operator, in a line with trailing whitespace.

# Before parse.c 1.578 from 2021-12-14, there was some unreachable error
# handling code in ParseDependencyOp.  This test tried to reach it and failed.
# To reach that point, there would have to be trailing whitespace in the line,
# but that is removed in an early stage of parsing.

all: .PHONY
	@printf 'target ' > dep-op-missing.tmp
	@${MAKE} -r -f dep-op-missing.tmp || exit 0
	@rm dep-op-missing.tmp
