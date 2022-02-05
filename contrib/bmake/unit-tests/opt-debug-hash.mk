# $NetBSD: opt-debug-hash.mk,v 1.3 2022/01/22 18:59:24 rillig Exp $
#
# Tests for the -dh command line option, which adds debug logging for
# hash tables.  Even more detailed logging is available by compiling
# make with -DDEBUG_HASH_LOOKUP.

.MAKEFLAGS: -dh

# Force a parse error, to demonstrate the newline character in the diagnostic
# that had been missing before parse.c 1.655 from 2022-01-22.
.error
