# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: init.mk,v 1.41 2025/04/18 20:49:54 sjg Exp $
#
#	@(#) Copyright (c) 2002-2024, Simon J. Gerraty
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

# should be set properly in sys.mk
_this ?= ${.PARSEFILE:S,bsd.,,}

.if !target(__${_this}__)
__${_this}__: .NOTMAIN

.if ${MAKE_VERSION:U0} > 20100408
_this_mk_dir := ${.PARSEDIR:tA}
.else
_this_mk_dir := ${.PARSEDIR}
.endif

.-include <local.init.mk>
.-include <${.CURDIR:H}/Makefile.inc>
.include <own.mk>
.include <compiler.mk>

# should have been set by sys.mk
CXX_SUFFIXES ?= .cc .cpp .cxx .C
CCM_SUFFIXES ?= .ccm
PCM ?= .pcm
# ${PICO} is used for PIC object files.
PICO ?= .pico

# SRCS which do not end up in OBJS
NO_OBJS_SRCS_SUFFIXES ?= .h ${CCM_SUFFIXES} .sh
OBJS_SRCS_PRE_FILTER += ${NO_OBJS_SRCS_SUFFIXES:@x@N*$x@}
# makefiles that actually *want* .o's in subdirs
# (it can be useful if multiple SRCS have same basename)
# can just set OBJS_SRCS_FILTER =
# we apply this as ${OBJS_SRCS_FILTER:ts:}
OBJS_SRCS_FILTER ?= T
OBJS_SRCS_FILTER += ${OBJS_SRCS_PRE_FILTER}
OBJS_SRCS_FILTER += R

.if defined(PROG_CXX) || ${SRCS:Uno:${CXX_SUFFIXES:S,^,N*,:ts:}} != ${SRCS:Uno:N/}
_CCLINK ?=	${CXX}
.endif
_CCLINK ?=	${CC}

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
	SRCS \

# a final :U avoids errors if someone uses :=
.for V in ${QUALIFIED_VAR_LIST:O:u:@q@$q $q_LAST@}
.for Q in ${VAR_QUALIFIER_LIST:u}
$V += ${$V_$Q:U${$V.$Q:U}} ${V_$Q_${COMPILER_TYPE}:U${$V.$Q.${COMPILER_TYPE}:U}}
.endfor
.endfor

CC_PG?= -pg
CXX_PG?= ${CC_PG}
CC_PIC?= -DPIC
CXX_PIC?= ${CC_PIC}
PROFFLAGS?= -DGPROF -DPROF

.if ${.MAKE.LEVEL:U1} == 0 && ${MK_DIRDEPS_BUILD:Uno} == "yes"
.if ${RELDIR} == "."
# top-level targets that are ok at level 0
DIRDEPS_BUILD_LEVEL0_TARGETS += clean* destroy*
M_ListToSkip?= O:u:S,^,N,:ts:
.if ${.TARGETS:Uall:${DIRDEPS_BUILD_LEVEL0_TARGETS:${M_ListToSkip}}} != ""
# this tells lib.mk and prog.mk to not actually build anything
_SKIP_BUILD = not building at level 0
.endif
.elif ${.TARGETS:U:Nall} == ""
_SKIP_BUILD = not building at level 0
# first .MAIN is what counts
.MAIN: dirdeps
.endif
.endif

.MAIN:		all

.if !defined(.PARSEDIR)
# no-op is the best we can do if not bmake.
.WAIT:
.endif

# allow makefiles to set ONLY_*_LIST and NOT_*_LIST
# to control _SKIP_BUILD
SKIP_BUILD_VAR_LIST += TARGET_SPEC ${TARGET_SPEC_VARS:UMACHINE}
.for v in ${SKIP_BUILD_VAR_LIST}
.if !empty(ONLY_$v_LIST) && ${ONLY_$v_LIST:Uno:M${$v}} == ""
_SKIP_BUILD ?= ${$v} not in ONLY_$v_LIST (${ONLY_$v_LIST})
.if ${MAKE_VERSION} > 20220924
.break
.endif
.elif !empty(NOT_$v_LIST) && ${NOT_$v_LIST:U:M${$v}} != ""
_SKIP_BUILD ?= ${$v} in NOT_$v_LIST (${NOT_$v_LIST})
.if ${MAKE_VERSION} > 20220924
.break
.endif
.endif
.endfor

# define this once for consistency
.if !defined(_SKIP_BUILD)
# beforebuild is a hook for things that must be done early
all: beforebuild .WAIT realbuild
.else
all: .PHONY
.if !empty(_SKIP_BUILD) && ${.MAKEFLAGS:M-V} == ""
.warning Skipping ${RELDIR} ${_SKIP_BUILD}
.endif
.endif
beforebuild:
realbuild:

.endif
