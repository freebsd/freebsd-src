# $NetBSD: depsrc-make.mk,v 1.4 2020/11/15 20:20:58 rillig Exp $
#
# Tests for the special source .MAKE in dependency declarations, which
# executes the commands of the target even if the -n or -t command line
# options are given.

# TODO: Add a test for the -t command line option.

.MAKEFLAGS: -n

all: this-is-made
all: this-is-not-made

this-is-made: .MAKE
	@echo ${.TARGET} is made.

this-is-not-made:
	@echo ${.TARGET} is just echoed.
