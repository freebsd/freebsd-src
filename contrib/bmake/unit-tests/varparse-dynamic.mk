# $NetBSD: varparse-dynamic.mk,v 1.1 2020/07/26 22:15:36 rillig Exp $

# Before 2020-07-27, there was an off-by-one error in Var_Parse that skipped
# the last character in the variable name.
# To trigger the bug, the variable must not be defined.
.if ${.TARGET}			# exact match, may be undefined
.endif
.if ${.TARGEX}			# 1 character difference, must be defined
.endif
.if ${.TARGXX}			# 2 characters difference, must be defined
.endif

all:
	@:
