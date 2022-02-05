# $NetBSD: directive-warning.mk,v 1.7 2022/01/23 16:09:38 rillig Exp $
#
# Tests for the .warning directive.
#
# Until parse.c 1.502 from 2020-12-19, a missing argument to the directive
# produced the wrong error message "Unknown directive".  Since parse.c 1.503
# from 2020-12-19, the correct "Missing argument" is produced.

.warn				# misspelled
.warn message			# misspelled
.warnin				# misspelled
.warnin	message			# misspelled
.warning			# "Missing argument"
.warning message		# expect+0: message
.warnings			# misspelled
.warnings messages		# Accepted before 2020-12-13 01:07:54.

all: .PHONY
