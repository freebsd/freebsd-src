# $NetBSD: opt-debug-curdir.mk,v 1.2 2022/01/23 16:09:38 rillig Exp $
#
# Tests for the -dC command line option, which does nothing, as of 2020-09-05,
# as the string "DEBUG(CWD" does not occur in the source code.

.MAKEFLAGS: -dC

all: .PHONY
