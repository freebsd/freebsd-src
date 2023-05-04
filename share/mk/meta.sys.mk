# $Id: meta.sys.mk,v 1.48 2023/05/04 16:41:10 sjg Exp $

#
#	@(#) Copyright (c) 2010-2021, Simon J. Gerraty
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

.-include <local.meta.sys.env.mk>

# If TARGET_SPEC_VARS is other than just MACHINE
# it should be set by now.
# TARGET_SPEC must not contain any '.'s.
TARGET_SPEC_VARS ?= MACHINE

.if !target(_meta_tspec_env_done_)
_meta_tspec_env_done_: .NOTMAIN
# Allow for local.meta.sys.env.mk to have done this

.if ${TARGET_SPEC:Uno:M*,*} != ""
# deal with TARGET_SPEC from env
_tspec := ${TARGET_SPEC:S/,/ /g}
.for i in ${TARGET_SPEC_VARS:${M_RANGE:Urange}}
${TARGET_SPEC_VARS:[$i]} := ${_tspec:[$i]}
.endfor
# We need to stop that TARGET_SPEC affecting any submakes
TARGET_SPEC=
# so export but do not track
.export-env TARGET_SPEC
.export ${TARGET_SPEC_VARS}
.for v in ${TARGET_SPEC_VARS:O:u}
.if empty($v)
.undef $v
.endif
.endfor
.endif
.endif

# Now make sure we know what TARGET_SPEC is
# as we may need it to find Makefile.depend*
.if ${MACHINE:Mhost*} != ""
# host is special
TARGET_SPEC = ${MACHINE}
.else
TARGET_SPEC = ${TARGET_SPEC_VARS:@v@${$v:U}@:ts,}
.endif

# absolute path to what we are reading.
_PARSEDIR = ${.PARSEDIR:tA}

.if !defined(SYS_MK_DIR)
SYS_MK_DIR := ${_PARSEDIR}
.endif

META_MODE += meta verbose
.if ${MAKE_VERSION:U0} > 20130323 && empty(.MAKE.PATH_FILEMON)
# we do not support filemon
META_MODE += nofilemon
MKDEP_MK ?= auto.dep.mk
.endif

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

# we use the pseudo machine "host" for the build host.
# this should be taken care of before we get here
.if ${OBJTOP:Ua} == ${HOST_OBJTOP:Ub}
MACHINE = host
.endif

.if !defined(MACHINE0)
# it can be handy to know which MACHINE kicked off the build
# for example, if using Makefild.depend for multiple machines,
# allowing only MACHINE0 to update can keep things simple.
MACHINE0 := ${MACHINE}
.export MACHINE0
.endif

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
MKDEP_MK ?= meta.autodep.mk

.if ${.MAKE.MAKEFILES:M*sys.dependfile.mk} == ""
# this does all the smarts of setting .MAKE.DEPENDFILE
.-include <sys.dependfile.mk>
# check if we got anything sane
.if ${.MAKE.DEPENDFILE} == ".depend"
.undef .MAKE.DEPENDFILE
.endif
.MAKE.DEPENDFILE ?= Makefile.depend
.endif

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

.if ${.MAKE.LEVEL} == 0
.if ${MK_DIRDEPS_BUILD:Uyes} == "yes"
# make sure dirdeps target exists and do it first
all: dirdeps .WAIT
dirdeps:
.NOPATH: dirdeps

.if defined(ALL_MACHINES)
# the first .MAIN: is what counts
# by default dirdeps is all we want at level0
.MAIN: dirdeps
.endif
.endif

.else	# level > 0

# Makefile.depend* get read at level 1+
# and often refer to DEP_MACHINE etc,
# so ensure DEP_* (for TARGET_SPEC_VARS anyway) are set
.for V in ${TARGET_SPEC_VARS}
DEP_$V = ${$V}
.endfor

.endif
.else
META_COOKIE_TOUCH=
# some targets need to be .PHONY in non-meta mode
META_NOPHONY= .PHONY
META_NOECHO= echo
.endif
.endif

.-include <local.meta.sys.mk>
