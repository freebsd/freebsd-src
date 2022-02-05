# $NetBSD: varmod-sun-shell.mk,v 1.2 2022/01/10 20:32:29 rillig Exp $
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

# If the command exits with non-zero, an error message is printed.
# XXX: Processing continues as usual though.
.if ${echo word; false:L:sh} != "word"
.  error
.endif


.MAKEFLAGS: -dv			# to see the actual command
_:=	${echo word; ${:Ufalse}:L:sh}
.MAKEFLAGS: -d0

all:
