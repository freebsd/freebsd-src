# $NetBSD: varmod-shell.mk,v 1.11 2024/08/29 20:20:37 rillig Exp $
#
# Tests for the ':!cmd!' variable modifier, which runs the shell command
# given by the variable modifier and returns its output.
#
# This modifier has been added on 2000-04-29.
#
# See also:
#	ApplyModifier_ShellCommand

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
# command that was executed.
# expect+1: warning: Command "echo word; (exit 13)" exited with status 13
.if ${:!echo word; (exit 13)!} != "word"
.  error
.endif
# expect+1: warning: Command "echo word; (exit 13)" exited with status 13
.if ${:Uprevious value:!echo word; (exit 13)!} != "word"
.  error
.endif


.MAKEFLAGS: -dv			# to see the "Capturing" debug output
# expect+1: warning: Command "echo word; (exit 13)" exited with status 13
_:=	${:!echo word; ${:U(exit 13)}!}
.MAKEFLAGS: -d0


all:
