# $NetBSD: directive-hyphen-include.mk,v 1.4 2025/03/30 09:51:50 rillig Exp $
#
# Tests for the .-include directive, which includes another file,
# silently skipping it if it cannot be opened.
#
# The 'silently skipping' only applies to the case where the file cannot be
# opened.  Parse errors and other errors are handled the same way as in the
# other .include directives.

# No complaint that there is no such file.
.-include "${.CURDIR}/directive-hyphen-include-nonexistent.inc"

# No complaint either, even though the operating system error is ENOTDIR, not
# ENOENT.
.-include "${MAKEFILE}/subdir"

# Errors that are not related to opening the file are still reported.
# expect: make: directive-hyphen-include-error.inc:1: Invalid line 'syntax error'
_!=	echo 'syntax error' > directive-hyphen-include-error.inc
.-include "${.CURDIR}/directive-hyphen-include-error.inc"
_!=	rm directive-hyphen-include-error.inc

all: .PHONY
