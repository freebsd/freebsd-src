# $NetBSD: varmod-quote-dollar.mk,v 1.2 2020/08/16 14:25:16 rillig Exp $
#
# Tests for the :q variable modifier, which quotes the string for the shell
# and doubles dollar signs, to prevent them from being interpreted by a
# child process of make.

# TODO: Implementation

all:
	@:;
