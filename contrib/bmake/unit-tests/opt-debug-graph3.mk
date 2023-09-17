# $NetBSD: opt-debug-graph3.mk,v 1.3 2021/02/02 17:47:56 rillig Exp $
#
# Tests for the -dg3 command line option, which prints the input
# graph before exiting on error.
#
# Before compat.c 1.222 from 2021-02-02, there was no debug output despite
# the error.

.MAKEFLAGS: -dg3

.MAIN: all

made-target: .PHONY
	: 'Making $@.'

error-target: .PHONY
	false

aborted-target: .PHONY aborted-target-dependency
aborted-target-dependency: .PHONY
	false

all: made-target error-target aborted-target
