# $NetBSD: directive-warning.mk,v 1.10 2025/07/01 04:24:20 rillig Exp $
#
# Tests for the .warning directive.
#
# Until parse.c 1.502 from 2020-12-19, a missing argument to the directive
# produced the wrong error message "Unknown directive".  Since parse.c 1.503
# from 2020-12-19, the correct "Missing argument" is produced.

# expect+1: Unknown directive "warn"
.warn				# misspelled
# expect+1: Unknown directive "warn"
.warn message			# misspelled
# expect+1: Unknown directive "warnin"
.warnin				# misspelled
# expect+1: Unknown directive "warnin"
.warnin	message			# misspelled
# expect+1: Missing argument for ".warning"
.warning			# "Missing argument"
# expect+1: warning: message
.warning message
# expect+1: Unknown directive "warnings"
.warnings			# misspelled
# expect+1: Unknown directive "warnings"
.warnings messages		# Accepted before 2020-12-13 01:07:54.

all: .PHONY
