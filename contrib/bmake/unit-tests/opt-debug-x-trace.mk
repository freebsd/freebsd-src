# $NetBSD: opt-debug-x-trace.mk,v 1.2 2022/01/23 16:09:38 rillig Exp $
#
# Tests for the -dx command line option, which runs shell commands with
# the -x option, thereby printing the actual commands as they are
# executed.

.MAKEFLAGS: -dx

# expect: + echo 'Counting 1 2 3 4 5 6 7'
# expect: Counting 1 2 3 4 5 6 7
all: .PHONY
	@echo 'Counting ${:U:range=7}'
