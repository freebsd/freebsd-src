# $NetBSD: opt-debug-errors.mk,v 1.2 2020/09/06 04:35:03 rillig Exp $
#
# Tests for the -de command line option, which adds debug logging for
# failed commands and targets.

.MAKEFLAGS: -de

all: fail-spaces
all: fail-escaped-space
all: fail-newline
all: fail-multiline
all: fail-multiline-intention

# XXX: The debug output folds the spaces, showing '3 spaces' instead of
# the correct '3   spaces'.
fail-spaces:
	echo '3   spaces'; false

# XXX: The debug output folds the spaces, showing 'echo \ indented' instead
# of the correct 'echo \  indented'.
fail-escaped-space:
	echo \  indented; false

# XXX: A newline is turned into an ordinary space in the debug log.
fail-newline:
	echo 'line1${.newline}line2'; false

# The line continuations in multiline commands are turned into an ordinary
# space before the command is actually run.
fail-multiline:
	echo 'line1\
		line2'; false

# It is a common style to align the continuation backslashes at the right
# of the lines, usually at column 73.  All spaces before the continuation
# backslash are preserved and are usually outside a shell word and thus
# irrelevant.  Having these spaces collapsed makes sense to show the command
# in its condensed form.
#
fail-multiline-intention:
	echo	'word1'							\
		'word2'; false
