# $NetBSD: varmod-sun-shell.mk,v 1.6 2024/08/29 20:20:37 rillig Exp $
#
# Tests for the :sh variable modifier, which runs the shell command
# given by the variable value and returns its output.
#
# This modifier has been added on 1996-05-29.
#
# See also:
#	ApplyModifier_SunShell

.if ${echo word:L:sh} != "word"
.  error
.endif

# If the command exits with non-zero, a warning is printed.
# expect+1: warning: Command "echo word; (exit 13)" exited with status 13
.if ${echo word; (exit 13):L:sh} != "word"
.  error
.endif


.MAKEFLAGS: -dv			# to see the "Capturing" debug output
# expect+1: warning: Command "echo word; (exit 13)" exited with status 13
_:=	${echo word; ${:U(exit 13)}:L:sh}
.MAKEFLAGS: -d0


all:
