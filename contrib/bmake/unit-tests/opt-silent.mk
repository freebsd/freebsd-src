# $NetBSD: opt-silent.mk,v 1.3 2022/01/23 16:09:38 rillig Exp $
#
# Tests for the -s command line option.

.MAKEFLAGS: -s

# No matter whether a command is prefixed by '@' or not, it is not echoed.
all:
	echo 'message'
	@echo 'silent message'
