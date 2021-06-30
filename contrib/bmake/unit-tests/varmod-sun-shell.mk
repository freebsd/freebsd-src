# $NetBSD: varmod-sun-shell.mk,v 1.1 2021/02/14 20:16:17 rillig Exp $
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

all:
