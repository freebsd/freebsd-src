# $NetBSD: opt-debug-errors-jobs.mk,v 1.2 2021/11/27 23:56:11 rillig Exp $
#
# Tests for the -de command line option, which adds debug logging for
# failed commands and targets; since 2021-04-27 also in jobs mode.

.MAKEFLAGS: -de -j1

all: fail-spaces
all: fail-escaped-space
all: fail-newline
all: fail-multiline
all: fail-multiline-intention
all: fail-vars

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

# In makefiles that rely heavily on abstracted variables, it is not possible
# to determine the actual command from the unexpanded command alone. To help
# debugging these issues (for example in NetBSD's build.sh), output the
# expanded command as well whenever it differs from the unexpanded command.
# Since 2021-11-28.
COMPILE_C=	false c-compiler
COMPILE_C_DEFS=	macro="several words"
COMPILE_C_FLAGS=flag1 ${COMPILE_C_DEFS:@def@-${def}@}
fail-vars:
	@${COMPILE_C} ${COMPILE_C_FLAGS}
