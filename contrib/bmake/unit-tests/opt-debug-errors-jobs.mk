# $NetBSD: opt-debug-errors-jobs.mk,v 1.1 2021/04/27 16:20:06 rillig Exp $
#
# Tests for the -de command line option, which adds debug logging for
# failed commands and targets; since 2021-04-27 also in jobs mode.

.MAKEFLAGS: -de -j1

all: fail-spaces
all: fail-escaped-space
all: fail-newline
all: fail-multiline
all: fail-multiline-intention

fail-spaces:
	echo '3   spaces'; false

fail-escaped-space:
	echo \  indented; false

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
# irrelevant.  Since "usually" is not "always", these space characters are
# not merged into a single space.
fail-multiline-intention:
	echo	'word1'							\
		'word2'; false
