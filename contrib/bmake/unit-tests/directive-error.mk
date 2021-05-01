# $NetBSD: directive-error.mk,v 1.5 2021/01/27 00:02:38 rillig Exp $
#
# Tests for the .error directive, which prints an error message and exits
# immediately, unlike other "fatal" parse errors, which continue to parse
# until the end of the current top-level makefile.
#
# See also:
#	opt-warnings-as-errors.mk

# Before parse.c 1.532 from 2021-01-27, the ".error" issued an irrelevant
# message saying "parsing warnings being treated as errors".
.MAKEFLAGS: -W
.error message
