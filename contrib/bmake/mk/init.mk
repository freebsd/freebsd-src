# $Id: init.mk,v 1.17 2020/05/25 20:15:07 sjg Exp $
#
#	@(#) Copyright (c) 2002, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that 
#	the above copyright notice and this notice are
#	left intact. 
#      
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__:

.if ${MAKE_VERSION:U0} > 20100408
_this_mk_dir := ${.PARSEDIR:tA}
.else
_this_mk_dir := ${.PARSEDIR}
.endif

.-include <local.init.mk>
.-include <${.CURDIR:H}/Makefile.inc>
.include <own.mk>
.include <compiler.mk>

.MAIN:		all

# should have been set by sys.mk
CXX_SUFFIXES?= .cc .cpp .cxx .C

.if !empty(WARNINGS_SET) || !empty(WARNINGS_SET_${MACHINE_ARCH})
.include <warnings.mk>
.endif

# these are applied in order, least specific to most
VAR_QUALIFIER_LIST += \
	${TARGET_SPEC_VARS:UMACHINE:@v@${$v}@} \
	${COMPILER_TYPE} \
	${.TARGET:T:R} \
	${.TARGET:T} \
	${.IMPSRC:T} \
	${VAR_QUALIFIER_XTRA_LIST}

QUALIFIED_VAR_LIST += \
	CFLAGS \
	COPTS \
	CPPFLAGS \
	CPUFLAGS \
	LDFLAGS \

# a final :U avoids errors if someone uses :=
.for V in ${QUALIFIED_VAR_LIST:O:u:@q@$q $q_LAST@}
.for Q in ${VAR_QUALIFIER_LIST:u}
$V += ${$V.$Q:U} ${$V.$Q.${COMPILER_TYPE}:U}
.endfor
.endfor

CC_PG?= -pg
CXX_PG?= ${CC_PG}
CC_PIC?= -DPIC
CXX_PIC?= ${CC_PIC}
PROFFLAGS?= -DGPROF -DPROF

.if ${.MAKE.LEVEL:U1} == 0 && ${BUILD_AT_LEVEL0:Uyes:tl} == "no"
# this tells lib.mk and prog.mk to not actually build anything
_SKIP_BUILD = not building at level 0
.endif

.if !defined(.PARSEDIR)
# no-op is the best we can do if not bmake.
.WAIT:
.endif

# define this once for consistency
.if empty(_SKIP_BUILD)
# beforebuild is a hook for things that must be done early
all: beforebuild .WAIT realbuild
.else
all: .PHONY
.warning ${_SKIP_BUILD}
.endif
beforebuild:
realbuild:

.endif
