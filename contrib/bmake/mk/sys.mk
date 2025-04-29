# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: sys.mk,v 1.61 2024/10/30 23:46:26 sjg Exp $
#
#	@(#) Copyright (c) 2003-2023, Simon J. Gerraty
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

# Avoid putting anything platform specific in here.

# just in case we are an older bmake
.MAKE.OS ?= ${HOST_OS}

# _DEBUG_MAKE_FLAGS etc.
.include <sys.debug.mk>

.if !empty(_DEBUG_MAKE_FLAGS)
.if ${_DEBUG_MAKE_SYS_DIRS:Uno:@x@${.CURDIR:M$x}@} != ""
.MAKEFLAGS: ${_DEBUG_MAKE_FLAGS}
.endif
.endif

# useful modifiers
.include <sys.vars.mk>

# we expect a recent bmake
.if !defined(_TARGETS)
# some things we do only once
_TARGETS := ${.TARGETS}
.-include <sys.env.mk>
.endif

# we need HOST_TARGET etc below.
.include <host-target.mk>

# early customizations
.-include <local.sys.env.mk>

# Popular suffixes for C++
CXX_SUFFIXES += .cc .cpp .cxx .C
CXX_SUFFIXES := ${CXX_SUFFIXES:O:u}
# and C++ Modules
CCM_SUFFIXES += .ccm
CCM_SUFFIXES := ${CCM_SUFFIXES:O:u}
# precompiled modules
PCM ?= .pcm

SYS_MK ?= ${.PARSEDIR:tA}/${.PARSEFILE}
SYS_MK := ${SYS_MK}

# for systems that have an incompatible install
INSTALL_SH ?= ${SYS_MK:H}/install-sh

# find the OS specifics
.if defined(SYS_OS_MK)
.include <${SYS_OS_MK}>
.else
_sys_mk =
.for x in ${HOST_TARGET} ${.MAKE.OS} ${.MAKE.OS:S,64,,} ${HOST_OSTYPE} ${MACHINE} Generic
.if empty(_sys_mk)
.-include <sys/$x.mk>
_sys_mk := ${.MAKE.MAKEFILES:M*/$x.mk}
.if !empty(_sys_mk)
_sys_mk := sys/${_sys_mk:T}
.endif
.endif
.if empty(_sys_mk)
# might be an old style
.-include <$x.sys.mk>
_sys_mk := ${.MAKE.MAKEFILES:M*/$x.sys.mk:T}
.endif
.if !empty(_sys_mk) && ${MAKE_VERSION} >= 20220924
.break
.endif
.endfor

SYS_OS_MK := ${_sys_mk}
.export SYS_OS_MK
.endif

# some sys/ may have set this to grep -E
EGREP ?= egrep

# some options we need to know early
OPTIONS_DEFAULT_NO += \
	DIRDEPS_BUILD \
	DIRDEPS_CACHE

OPTIONS_DEFAULT_DEPENDENT += \
	AUTO_OBJ/DIRDEPS_BUILD \
	META_ERROR_TARGET/DIRDEPS_BUILD \
	META_MODE/DIRDEPS_BUILD \
	STAGING/DIRDEPS_BUILD \
	STATIC_DIRDEPS_CACHE/DIRDEPS_CACHE \

.-include <options.mk>

# :Uno incase options.mk not installed
.if ${MK_DIRDEPS_BUILD:Uno} == "yes"
.-include <sys.dirdeps.mk>
.endif
.if ${MK_META_MODE:Uno} == "yes"
.-include <meta.sys.mk>
.MAKE.MODE ?= meta verbose {META_MODE}
.endif
# make sure we have a harmless value
.MAKE.MODE ?= normal

# if you want objdirs make them automatic
# and do it early before we compute .PATH
.if ${MK_AUTO_OBJ:Uno} == "yes" || ${MKOBJDIRS:Uno} == "auto"
.include <auto.obj.mk>
.endif

.if !empty(SRCTOP)
.if ${.CURDIR} == ${SRCTOP}
RELDIR = .
.elif ${.CURDIR:M${SRCTOP}/*}
RELDIR := ${.CURDIR:S,${SRCTOP}/,,}
.endif
.endif

MACHINE_ARCH.host ?= ${_HOST_ARCH}
MACHINE_ARCH.${MACHINE} ?= ${MACHINE}
.if empty(MACHINE_ARCH)
MACHINE_ARCH = ${MACHINE_ARCH.${MACHINE}}
.endif

.ifndef ROOT_GROUP
ROOT_GROUP != sed -n '/:0:/{s/:.*//p;q;}' /etc/group
.export ROOT_GROUP
.endif

unix ?= We run ${_HOST_OSNAME}.

# We need a Bourne/POSIX shell
MAKE_SHELL ?= ${.SHELL:Ush}
SHELL := ${MAKE_SHELL}

# A race condition in mkdir, means that it can bail if another
# process made a dir that mkdir expected to.
# We repeat the mkdir -p a number of times to try and work around this.
# We stop looping as soon as the dir exists.
# If we get to the end of the loop, a plain mkdir will issue an error.
Mkdirs= Mkdirs() { \
	for d in $$*; do \
		for i in 1 2 3 4 5 6; do \
			mkdir -p $$d; \
			test -d $$d && return 0; \
		done; \
		mkdir $$d || exit $$?; \
	done; }

# pick up generic suffix rules
.include <suffixes.mk>

# late customizations
.-include <local.sys.mk>

# if .CURDIR is matched by any entry in DEBUG_MAKE_DIRS we
# will apply DEBUG_MAKE_FLAGS, now.
.if !empty(_DEBUG_MAKE_FLAGS)
.if ${_DEBUG_MAKE_DIRS:Uno:@x@${.CURDIR:M$x}@} != ""
.MAKEFLAGS: ${_DEBUG_MAKE_FLAGS}
.endif
.endif
