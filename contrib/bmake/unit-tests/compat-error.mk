# $NetBSD: compat-error.mk,v 1.3 2020/12/13 19:33:53 rillig Exp $
#
# Test detailed error handling in compat mode.
#
# Until 2020-12-13, .ERROR_TARGET was success3, which was wrong.
# Since compat.c 1.215 from 2020-12-13, it is 'fail1', which is the first
# failed top-level target.  XXX: Even better would be if .ERROR_TARGET were
# the smallest target that caused the build to fail, even if it were a
# sub-sub-sub-dependency of a top-level target.
#
# XXX: As of 2020-12-13, .ERROR_CMD is empty, which is wrong.
#
# See also:
#	Compat_Run
#
#	The commit that added the NULL command to gn->commands:
#		CVS: 1994.06.06.22.45.??
#		Git: 26a8972fd7f982502c5fbfdabd34578b99d77ca5
#		1994: Lst_Replace (cmdNode, (ClientData) NULL);
#		2020: LstNode_SetNull(cmdNode);
#
#	The commit that skipped NULL commands for .ERROR_CMD:
#		CVS: 2016.08.11.19.53.??
#		Git: 58b23478b7353d46457089e726b07a49197388e4

.MAKEFLAGS: success1 fail1 success2 fail2 success3

success1 success2 success3:
	: Making ${.TARGET} out of nothing.

fail1 fail2:
	: Making ${.TARGET} out of nothing.
	false '${.TARGET}' '$${.TARGET}' '$$$${.TARGET}'

.ERROR:
	@echo ${.TARGET} target: '<'${.ERROR_TARGET:Q}'>'
	@echo ${.TARGET} command: '<'${.ERROR_CMD:Q}'>'
