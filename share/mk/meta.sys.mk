# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: meta.sys.mk,v 1.56 2024/11/22 23:51:48 sjg Exp $

#
#	@(#) Copyright (c) 2010-2023, Simon J. Gerraty
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

# include this if you want to enable meta mode
# for maximum benefit, requires filemon(4) driver.

# absolute path to what we are reading.
_PARSEDIR ?= ${.PARSEDIR:tA}

.-include <local.meta.sys.env.mk>

.if !defined(SYS_MK_DIR)
SYS_MK_DIR := ${_PARSEDIR}
.endif

.if !target(.ERROR)

META_MODE += meta
.if empty(.MAKEFLAGS:M-s)
META_MODE += verbose
.endif
.if ${MAKE_VERSION:U0} > 20130323 && empty(.MAKE.PATH_FILEMON)
# we do not support filemon
META_MODE += nofilemon
MKDEP_MK ?= auto.dep.mk
.endif

# META_MODE_XTRAS makes it easier to add things like 'env'
# from the command line when debugging
# :U avoids problems from := below
META_MODE += ${META_MODE_XTRAS:U}

.MAKE.MODE ?= ${META_MODE}

_filemon := ${.MAKE.PATH_FILEMON:U/dev/filemon}

.if empty(UPDATE_DEPENDFILE)
_make_mode := ${.MAKE.MODE} ${META_MODE}
.if ${_make_mode:M*read*} != "" || ${_make_mode:M*nofilemon*} != ""
# tell everyone we are not updating Makefile.depend*
UPDATE_DEPENDFILE = NO
.export UPDATE_DEPENDFILE
.endif
.if ${_filemon:T:Mfilemon} == "filemon"
.if ${UPDATE_DEPENDFILE:Uyes:tl} == "no" && !exists(${_filemon})
# we should not get upset
META_MODE += nofilemon
.export META_MODE
.endif
.endif
.endif

.if !defined(NO_SILENT)
.if ${MAKE_VERSION} > 20110818
# only be silent when we have a .meta file
META_MODE += silent=yes
.else
.SILENT:
.endif
.endif

.if ${MK_DIRDEPS_BUILD:Uno} == "yes"

.if !defined(META2DEPS)
.if defined(PYTHON) && exists(${PYTHON})
# we prefer the python version of this - it is much faster
META2DEPS ?= ${.PARSEDIR}/meta2deps.py
.else
META2DEPS ?= ${.PARSEDIR}/meta2deps.sh
.endif
META2DEPS := ${META2DEPS}
.export META2DEPS
.endif

MAKE_PRINT_VAR_ON_ERROR += \
	.ERROR_TARGET \
	.ERROR_EXIT \
	.ERROR_META_FILE \
	.MAKE.LEVEL \
	MAKEFILE \
	.MAKE.MODE

MK_META_ERROR_TARGET = yes
.endif

.if ${MK_META_ERROR_TARGET:Uno} == "yes"

.if !defined(SB) && defined(SRCTOP)
SB = ${SRCTOP:H}
.endif
ERROR_LOGDIR ?= ${SB}/error
meta_error_log = ${ERROR_LOGDIR}/meta-${.MAKE.PID}.log

.if ${.MAKE.LEVEL} == 0 && !empty(NEWLOG_SH) && exists(${ERROR_LOGDIR})
.BEGIN:	_rotateErrorLog
_rotateErrorLog: .NOMETA .NOTMAIN
	@${NEWLOG_SH} -d -S -n ${ERROR_LOG_GENS:U4} ${ERROR_LOGDIR}
.endif

.ERROR: _metaError
# We are interested here in the target(s) that caused the build to fail.
# We want to ignore targets that were "aborted" due to failure
# elsewhere per the message below or a sub-make may just exit 6.
_metaError: .NOMETA .NOTMAIN
	-@[ ${.ERROR_EXIT:U0} = 6 ] && exit 0; \
	[ "${.ERROR_META_FILE}" ] && { \
	grep -q 'failure has been detected in another branch' ${.ERROR_META_FILE} && exit 0; \
	mkdir -p ${meta_error_log:H}; \
	cp ${.ERROR_META_FILE} ${meta_error_log}; \
	echo "ERROR: log ${meta_error_log}" >&2; }; :

.endif
.endif

# Are we, after all, in meta mode?
.if ${.MAKE.MODE:Uno:Mmeta*} != ""
MKDEP_MK ?= meta.autodep.mk

# we can afford to use cookies to prevent some targets
# re-running needlessly
META_COOKIE_TOUCH?= touch ${COOKIE.${.TARGET}:U${.OBJDIR}/${.TARGET:T}}
META_NOPHONY=
META_NOECHO= :

# some targets involve old pre-built targets
# ignore mtime of shell
# and mtime of makefiles does not matter in meta mode
.MAKE.META.IGNORE_PATHS += \
	${MAKEFILE} \
	${MAKE_SHELL} \
	${SHELL} \
	${SYS_MK_DIR} \


.if ${UPDATE_DEPENDFILE:Uyes:tl} != "no"
.if ${.MAKEFLAGS:Uno:M-k} != ""
# make this more obvious
.warning Setting UPDATE_DEPENDFILE=NO due to -k
UPDATE_DEPENDFILE= NO
.export UPDATE_DEPENDFILE
.elif ${_filemon:T} == "filemon" && !exists(${_filemon})
.error ${.newline}ERROR: The filemon module (${_filemon}) is not loaded.
.endif
.endif

.else				# in meta mode?

META_COOKIE_TOUCH=
# some targets need to be .PHONY in non-meta mode
META_NOPHONY= .PHONY
META_NOECHO= echo

.endif				# in meta mode?

.-include <local.meta.sys.mk>
