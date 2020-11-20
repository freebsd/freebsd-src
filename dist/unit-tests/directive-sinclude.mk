# $NetBSD: directive-sinclude.mk,v 1.2 2020/11/15 20:20:58 rillig Exp $
#
# Tests for the .sinclude directive, which includes another file,
# silently skipping it if it cannot be opened.
#
# The 'silently skipping' only applies to the case where the file cannot be
# opened.  Parse errors and other errors are handled the same way as in the
# other .include directives.

# TODO: Implementation

all:
	@:;
