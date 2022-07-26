# $NetBSD: compat-error.mk,v 1.5 2022/05/08 06:51:27 rillig Exp $
#
# Test detailed error handling in compat mode.
#
# Make several targets that alternately succeed and fail.
#
# The first failing top-level target is recorded in '.ERROR_TARGET'.  While
# this information may give a hint as to which target failed, it would be more
# useful at that point to know the actual target that failed, or the complete
# chain from root cause to top-level target.
#
# Historic bugs
#	Before compat.c 1.215 from 2020-12-13, '.ERROR_TARGET' was 'success3',
#	which was obviously wrong.
#
# Bugs
#	As of 2020-12-13, '.ERROR_CMD' is empty, which does not provide any
#	insight into the command that actually failed.
#
# See also:
#	Compat_MakeAll
#
#	The commit that added the NULL command to gn->commands:
#		CVS: 1994.06.06.22.45.??
#		Git: 26a8972fd7f982502c5fbfdabd34578b99d77ca5
#		1994: Lst_Replace (cmdNode, (ClientData) NULL);
#		2020: LstNode_SetNull(cmdNode);
#
#	The commit that skipped NULL commands for .ERROR_CMD:
#		CVS: 2016.08.11.19.53.17
#		Git: 58b23478b7353d46457089e726b07a49197388e4

.MAKEFLAGS: -k success1 fail1 success2 fail2 success3

success1 success2 success3:
	: Making ${.TARGET} out of nothing.

fail1 fail2:
	: Making ${.TARGET} out of nothing.
	false '${.TARGET}' '$${.TARGET}' '$$$${.TARGET}'

.ERROR:
	@echo ${.TARGET} target: '<'${.ERROR_TARGET:Q}'>'
	@echo ${.TARGET} command: '<'${.ERROR_CMD:Q}'>'
