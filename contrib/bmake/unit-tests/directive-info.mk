# $NetBSD: directive-info.mk,v 1.4 2020/11/15 11:57:00 rillig Exp $
#
# Tests for the .info directive.

# TODO: Implementation

.info begin .info tests
.inf				# misspelled
.info				# oops: message should be "missing parameter"
.info message
.info		indented message
.information
.information message		# oops: misspelled
.info.man:			# not a message, but possibly a suffix rule

# Even if lines would have trailing whitespace, this would be trimmed by
# ParseGetLine.
.info
.info				# comment

.info: message			# This is a dependency declaration.
.info-message			# This is an unknown directive.
.info no-target: no-source	# This is a .info directive, not a dependency.
# See directive.mk for more tests of this kind.

all:
	@:;
