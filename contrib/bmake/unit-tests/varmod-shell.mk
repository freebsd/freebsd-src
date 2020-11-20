# $NetBSD: varmod-shell.mk,v 1.5 2020/11/17 20:11:02 rillig Exp $
#
# Tests for the :sh variable modifier, which runs the shell command
# given by the variable value and returns its output.
#
# This modifier has been added on 2000-04-29.
#
# See also:
#	ApplyModifier_ShellCommand

# TODO: Implementation

# The command to be run is enclosed between exclamation marks.
# The previous value of the expression is irrelevant for this modifier.
# The :!cmd! modifier turns an undefined expression into a defined one.
.if ${:!echo word!} != "word"
.  error
.endif

# If the command exits with non-zero, an error message is printed.
# XXX: Processing continues as usual though.
#
# Between 2000-04-29 and 2020-11-17, the error message mentioned the previous
# value of the expression (which is usually an empty string) instead of the
# command that was executed.  It's strange that such a simple bug could
# survive such a long time.
.if ${:!echo word; false!} != "word"
.  error
.endif
.if ${:Uprevious value:!echo word; false!} != "word"
.  error
.endif

all:
	@:;
