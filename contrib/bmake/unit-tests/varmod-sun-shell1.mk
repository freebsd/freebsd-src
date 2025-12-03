# $NetBSD: varmod-sun-shell1.mk,v 1.1 2025/11/12 22:14:08 sjg Exp $
#
# Tests for the :sh1 variable modifier, which runs the shell command
# given by the variable value only on first reference and caches its output.
#
# This modifier has been added on 2025-11-11
#
# See also:
#	ApplyModifier_SunShell1

ANSWER= echo 42; (exit 13)
THE_ANSWER= ${ANSWER:sh1}

# first reference will warn
.MAKEFLAGS: -dv			# to see the "Capturing" debug output
# expect+1: warning: Command "echo 42; (exit 13)" exited with status 13
_:=	${THE_ANSWER}
.MAKEFLAGS: -d0

# subsequent references will not, since we do not execute a command
.if ${THE_ANSWER} != "42"
.  error
.endif

all:
