# $NetBSD: directive-dinclude.mk,v 1.2 2022/01/23 21:48:59 rillig Exp $
#
# Tests for the .dinclude directive, which includes another file,
# silently skipping it if it cannot be opened.  This is primarily used for
# including '.depend' files, that's where the 'd' comes from.
#
# The 'silently skipping' only applies to the case where the file cannot be
# opened.  Parse errors and other errors are handled the same way as in the
# other .include directives.

# No complaint that there is no such file.
.dinclude "${.CURDIR}/directive-dinclude-nonexistent.inc"

# No complaint either, even though the operating system error is ENOTDIR, not
# ENOENT.
.dinclude "${MAKEFILE}/subdir"

# Errors that are not related to opening the file are still reported.
# expect: make: "directive-dinclude-error.inc" line 1: Invalid line type
_!=	echo 'syntax error' > directive-dinclude-error.inc
.dinclude "${.CURDIR}/directive-dinclude-error.inc"
_!=	rm directive-dinclude-error.inc

all: .PHONY
