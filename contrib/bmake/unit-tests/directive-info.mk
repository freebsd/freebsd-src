# $NetBSD: directive-info.mk,v 1.8 2020/12/19 22:33:11 rillig Exp $
#
# Tests for the .info directive.
#
# Until parse.c 1.502 from 2020-12-19, a missing argument to the directive
# produced the wrong error message "Unknown directive".  Since parse.c 1.503
# from 2020-12-19, the correct "Missing argument" is produced.

# TODO: Implementation

.info begin .info tests
.inf				# misspelled
.info				# "Missing argument"
.info message
.info		indented message
.information
.information message		# Accepted before 2020-12-13 01:07:54.
.info.man:			# not a message, but possibly a suffix rule

# Even if lines would have trailing whitespace, this would be trimmed by
# ParseGetLine.
.info
.info				# comment

.info: message			# This is a dependency declaration.
.info-message			# This is an unknown directive.
.info no-target: no-source	# This is a .info directive, not a dependency.
# See directive.mk for more tests of this kind.

# Since at least 2002-01-01, the line number that is used in error messages
# and the .info directives is the number of completely read lines.  For the
# following multi-line directive, this means that the reported line number is
# the one of the last line, not the first line.
.info expect line 30 for\
	multi$\
	-line message

all:
	@:;
