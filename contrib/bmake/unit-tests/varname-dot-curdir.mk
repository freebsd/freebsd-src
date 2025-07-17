# $NetBSD: varname-dot-curdir.mk,v 1.7 2020/10/08 19:09:08 rillig Exp $
#
# Tests for the special .CURDIR variable, which is initially set to the
# canonical path of the current working directory, when make started.

# In all normal situations, the current directory exists, and its name can
# be resolved.  If not, make fails at startup.
#
# It would be possible on some systems to remove the current directory, even
# while a process runs in it, but this is so unrealistic that it's no worth
# testing.
.if !exists(${.CURDIR})
.  error
.endif
.if !exists(${.CURDIR}/)
.  error
.endif
.if !exists(${.CURDIR}/.)
.  error
.endif
.if !exists(${.CURDIR}/..)
.  error
.endif

# Until 2020-10-04, assigning the result of a shell assignment to .CURDIR
# tried to add the shell command ("echo /") to the .PATH instead of the
# output of the shell command ("/").  Since "echo /" does not exist, the
# .PATH was left unmodified.  See VarAssign_Eval.
#
# Since 2020-10-04, the output of the shell command is added to .PATH.
.CURDIR!=	echo /
.if ${.PATH:M/} != "/"
.  error
.endif

# A normal assignment works fine, as does a substitution assignment.
# Appending to .CURDIR does not make sense, therefore it doesn't matter that
# this code path is buggy as well.
.CURDIR=	/
.if ${.PATH:M/} != "/"
.  error
.endif

all:
	@:;
