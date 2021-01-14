# $NetBSD: directive-error.mk,v 1.3 2020/12/13 01:07:54 rillig Exp $
#
# Tests for the .error directive, which prints an error message and exits
# immediately, unlike other "fatal" parse errors, which continue to parse
# until the end of the current top-level makefile.

# TODO: Implementation

all:
	@:;
