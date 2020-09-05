# $NetBSD: opt-debug-g1.mk,v 1.1 2020/08/27 19:00:17 rillig Exp $
#
# Tests for the -dg1 command line option, which prints the input
# graph before making anything.

all: made-target made-target-no-sources

made-target: made-source

made-source:

made-target-no-sources:

unmade-target: unmade-sources

unmade-target-no-sources:

all:
	@:;
