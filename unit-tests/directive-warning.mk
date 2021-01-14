# $NetBSD: directive-warning.mk,v 1.6 2020/12/19 22:33:11 rillig Exp $
#
# Tests for the .warning directive.
#
# Until parse.c 1.502 from 2020-12-19, a missing argument to the directive
# produced the wrong error message "Unknown directive".  Since parse.c 1.503
# from 2020-12-19, the correct "Missing argument" is produced.

# TODO: Implementation

.warn				# misspelled
.warn message			# misspelled
.warnin				# misspelled
.warnin	message			# misspelled
.warning			# "Missing argument"
.warning message		# ok
.warnings			# misspelled
.warnings messages		# Accepted before 2020-12-13 01:07:54.

all:
	@:;
