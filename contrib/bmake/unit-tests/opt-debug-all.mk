# $NetBSD: opt-debug-all.mk,v 1.1 2020/09/05 06:20:51 rillig Exp $
#
# Tests for the -dA command line option, which enables all debug options
# except for -dL (lint), since that option is not related to debug logging
# but to static analysis.

# TODO: Implementation

all:
	@:;
