# $NetBSD: directive-sinclude.mk,v 1.5 2023/08/19 10:52:14 rillig Exp $
#
# Tests for the .sinclude directive, which includes another file,
# silently skipping it if it cannot be opened.
#
# The 'silently skipping' only applies to the case where the file cannot be
# opened.  Parse errors and other errors are handled the same way as in the
# other .include directives.

# No complaint that there is no such file.
.sinclude "${.CURDIR}/directive-include-nonexistent.inc"

# No complaint either, even though the operating system error is ENOTDIR, not
# ENOENT.
.sinclude "${MAKEFILE}/subdir"

# Errors that are not related to opening the file are still reported.
# expect: make: "directive-include-error.inc" line 1: Invalid line 'syntax error'
_!=	echo 'syntax error' > directive-include-error.inc
.sinclude "${.CURDIR}/directive-include-error.inc"
_!=	rm directive-include-error.inc

all: .PHONY
