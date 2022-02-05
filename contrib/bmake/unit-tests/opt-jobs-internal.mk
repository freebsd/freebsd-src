# $NetBSD: opt-jobs-internal.mk,v 1.3 2022/01/23 16:09:38 rillig Exp $
#
# Tests for the (intentionally undocumented) -J command line option.
#
# Only test the error handling here, the happy path is covered in other tests
# as a side effect.

# expect: make: internal error -- J option malformed (garbage)
.MAKEFLAGS: -Jgarbage
