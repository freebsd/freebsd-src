# $NetBSD: varcmd.mk,v 1.7 2023/04/07 05:54:16 rillig Exp $
#
# Test behaviour of recursive make and vars set on command line.
#
# FIXME: The purpose of this test is unclear.  The test uses six levels of
# sub-makes, which makes it incredibly hard to understand.  There must be at
# least an introductory explanation about what _should_ happen here.
# The variable names are terrible, as well as their values.
#
# This test produces different results if the large block with the condition
# "scope == SCOPE_GLOBAL" in Var_SetWithFlags is removed.  This test should
# be rewritten to make it clear why there is a difference and why this is
# actually intended.  Removing that large block of code makes only this test
# and vardebug.mk fail, which is not enough.
#
# See also:
#	var-scope-cmdline.mk
#	varname-makeflags.mk

FU=	fu
FOO?=	foo
.if !empty(.TARGETS)
TAG=	${.TARGETS}
.endif
TAG?=	default

all:	one

show:
	@echo "${TAG} FU=<v>${FU}</v> FOO=<v>${FOO}</v> VAR=<v>${VAR}</v>"

one:	show
	@${.MAKE} -f ${MAKEFILE} FU=bar FOO+=goo two

two:	show
	@${.MAKE} -f ${MAKEFILE} three

three:	show
	@${.MAKE} -f ${MAKEFILE} four


.ifmake two
# this should not work
FU+= oops
FOO+= oops
_FU:= ${FU}
_FOO:= ${FOO}
two: immutable
immutable:
	@echo "$@ FU='${_FU}'"
	@echo "$@ FOO='${_FOO}'"
.endif
.ifmake four
VAR=Internal
.MAKEOVERRIDES+= VAR
.endif

four:	show
	@${.MAKE} -f ${MAKEFILE} five

M=	x
V.y=	is y
V.x=	is x
V:=	${V.$M}
K:=	${V}

show-v:
	@echo '${TAG} v=${V} k=${K}'

five:	show show-v
	@${.MAKE} -f ${MAKEFILE} M=y six

six:	show-v
	@${.MAKE} -f ${MAKEFILE} V=override show-v
