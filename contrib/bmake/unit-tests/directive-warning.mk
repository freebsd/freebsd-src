# $NetBSD: directive-warning.mk,v 1.3 2020/11/03 17:17:31 rillig Exp $
#
# Tests for the .warning directive.

# TODO: Implementation

.warn				# misspelled
.warn message			# misspelled
.warnin				# misspelled
.warnin	message			# misspelled
.warning			# oops: should be "missing argument"
.warning message		# ok
.warnings			# misspelled
.warnings messages		# oops

all:
	@:;
