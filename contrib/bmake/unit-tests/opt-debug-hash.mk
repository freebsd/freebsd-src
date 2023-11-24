# $NetBSD: opt-debug-hash.mk,v 1.4 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the -dh command line option, which adds debug logging for
# hash tables.  Even more detailed logging is available by compiling
# make with -DDEBUG_HASH_LOOKUP.

.MAKEFLAGS: -dh

# Force a parse error, to demonstrate the newline character in the diagnostic
# that had been missing before parse.c 1.655 from 2022-01-22.
# expect+1: Missing argument for ".error"
.error
