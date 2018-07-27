# $FreeBSD$
# $Id: meta.sys.mk,v 1.19 2014/08/02 23:16:02 sjg Exp $

#
#	@(#) Copyright (c) 2010, Simon J. Gerraty
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

.if ${MAKE_VERSION:U0} > 20100901
.if !target(.ERROR)

.-include "local.meta.sys.mk"

# absolute path to what we are reading.
_PARSEDIR = ${.PARSEDIR:tA}

.if !defined(SYS_MK_DIR)
SYS_MK_DIR := ${_PARSEDIR}
.endif

META_MODE += meta verbose
.MAKE.MODE ?= ${META_MODE}

.if ${.MAKE.LEVEL} == 0
_make_mode := ${.MAKE.MODE} ${META_MODE}
.if ${_make_mode:M*read*} != "" || ${_make_mode:M*nofilemon*} != ""
# tell everyone we are not updating Makefile.depend*
UPDATE_DEPENDFILE = NO
.export UPDATE_DEPENDFILE
.endif
.if ${UPDATE_DEPENDFILE:Uyes:tl} == "no" && !exists(/dev/filemon)
# we should not get upset
META_MODE += nofilemon
.export META_MODE
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

# make defaults .MAKE.DEPENDFILE to .depend
# that won't work for us.
.if ${.MAKE.DEPENDFILE} == ".depend"
.undef .MAKE.DEPENDFILE
.endif

# if you don't cross build for multiple MACHINEs concurrently, then
# .MAKE.DEPENDFILE = Makefile.depend
# probably makes sense - you can set that in local.sys.mk 
.MAKE.DEPENDFILE ?= Makefile.depend.${MACHINE}

# we use the pseudo machine "host" for the build host.
# this should be taken care of before we get here
.if ${OBJTOP:Ua} == ${HOST_OBJTOP:Ub}
MACHINE = host
.endif

.if ${.MAKE.LEVEL} == 0
# it can be handy to know which MACHINE kicked off the build
# for example, if using Makefild.depend for multiple machines,
# allowing only MACHINE0 to update can keep things simple.
MACHINE0 := ${MACHINE}
.export MACHINE0

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
	.ERROR_META_FILE \
	.MAKE.LEVEL \
	MAKEFILE \
	.MAKE.MODE

.if !defined(SB) && defined(SRCTOP)
SB = ${SRCTOP:H}
.endif
ERROR_LOGDIR ?= ${SB}/error
meta_error_log = ${ERROR_LOGDIR}/meta-${.MAKE.PID}.log

# we are not interested in make telling us a failure happened elsewhere
.ERROR: _metaError
_metaError: .NOMETA .NOTMAIN
	-@[ "${.ERROR_META_FILE}" ] && { \
	grep -q 'failure has been detected in another branch' ${.ERROR_META_FILE} && exit 0; \
	mkdir -p ${meta_error_log:H}; \
	cp ${.ERROR_META_FILE} ${meta_error_log}; \
	echo "ERROR: log ${meta_error_log}" >&2; }; :

.endif

# Are we, after all, in meta mode?
.if ${.MAKE.MODE:Uno:Mmeta*} != ""
MKDEP_MK = meta.autodep.mk

# we can afford to use cookies to prevent some targets
# re-running needlessly
META_COOKIE_TOUCH?= touch ${COOKIE.${.TARGET}:U${.OBJDIR}/${.TARGET:T}}
META_NOPHONY=

# some targets involve old pre-built targets
# ignore mtime of shell
# and mtime of makefiles does not matter in meta mode
.MAKE.META.IGNORE_PATHS += \
        ${MAKEFILE} \
        ${SHELL} \
        ${SYS_MK_DIR}

# if we think we are updating dependencies, 
# then filemon had better be present
.if ${UPDATE_DEPENDFILE:Uyes:tl} != "no" && !exists(/dev/filemon)
.error ${.newline}ERROR: The filemon module (/dev/filemon) is not loaded.
.endif

.if ${.MAKE.LEVEL} == 0
# make sure dirdeps target exists and do it first
all: dirdeps .WAIT
dirdeps:
.NOPATH: dirdeps

.if defined(ALL_MACHINES)
# the first .MAIN: is what counts
# by default dirdeps is all we want at level0
.MAIN: dirdeps
# tell dirdeps.mk what we want
BUILD_AT_LEVEL0 = no
.endif
.if ${.TARGETS:Nall} == "" 
# it works best if we do everything via sub-makes
BUILD_AT_LEVEL0 ?= no
.endif

.endif
.else
META_COOKIE_TOUCH=
# some targets need to be .PHONY in non-meta mode
META_NOPHONY= .PHONY
.endif
.endif
