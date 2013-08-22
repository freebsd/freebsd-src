# $Id: dirdeps.mk,v 1.23 2012/11/06 05:44:03 sjg Exp $

# Copyright (c) 2010-2012, Juniper Networks, Inc.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions 
# are met: 
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer. 
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.  
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

# Much of the complexity here is for supporting cross-building.
# If a tree does not support that, simply using plain Makefile.depend
# should provide sufficient clue.
# Otherwise the recommendation is to use Makefile.depend.${MACHINE}
# as expected below.

# Note: this file gets multiply included.
# This is what we do with DIRDEPS

# DIRDEPS:
#	This is a list of directories - relative to SRCTOP, it is only
#	of interest to .MAKE.LEVEL 0.
#	In some cases the entry may be qualified with a .<machine>
#	suffix, for example to force building something for the pseudo
#	machines "host" or "common" regardless of current ${MACHINE}.
#	All unqualified entries end up being qualified with .${MACHINE}
#	and _DIRDEPS_USE below, uses the suffix to set MACHINE
#	correctly when visiting each entry.
#
#	Each entry is also converted into a set of paths to look for
#	Makefile.depend.<machine> to learn the dependencies of each.
#	Each Makefile.depend.<machine> sets DEP_RELDIR to be the
#	the RELDIR (path relative to SRCTOP) for its directory, and
#	DEP_MACHINE to its suffix (<machine>), further since
#	each Makefile.depend.<machine> includes dirdeps.mk, this
#	processing is recursive and results in .MAKE.LEVEL 0 learning the
#	dependencies of the tree wrt the initial directory (_DEP_RELDIR).
#
# BUILD_AT_LEVEL0
#	Indicates whether .MAKE.LEVEL 0 builds anything:
#	if "no" sub-makes are used to build everything,
#	if "yes" sub-makes are only used to build for other machines.
#
# TARGET_SPEC_VARS
#	All the description above (and below) assumes <machine> is the
#	only data needed to control the build.
#	This is not always the case.  So in addition to setting
#	MACHINE in the build environment we set TARGET_SPEC which is
#	composed of the values of TARGET_SPEC_VARS separated by
#	commas.  The default is just MACHINE.
#
#	If more that MACHINE is needed then sys.mk needs to decompose
#	TARGET_SPEC and set the relevant variables accordingly.
#	It is important that MACHINE be included in TARGET_SPEC_VARS
#	since if there is more the value passed as MACHINE will infact
#	be the TARGET_SPEC.
#	Note: TARGET_SPEC cannot contain any '.'s so the target
#	tripple used by compiler folk won't work (directly anyway).
#
#	For example:
#
#		# variables other than MACHINE might be optional
#		TARGET_SPEC_VARS = MACHINE TARGET_OS
#		.if ${TARGET_SPEC:Uno:M*,*} != ""
#		_tspec := ${TARGET_SPEC:S/,/ /g}
#		MACHINE := ${_tspec:[1]}
#		TARGET_OS := ${_tspec:[2]}
#		# etc.
#		.for v in ${TARGET_SPEC_VARS:O:u}
#		.if empty($v)
#		.undef $v
#		.endif
#		.endfor
#		.endif
#	

.if ${.MAKE.LEVEL} == 0
# only the first instance is interested in all this

# First off, we want to know what ${MACHINE} to build for.
# This can be complicated if we are using a mixture of ${MACHINE} specific
# and non-specific Makefile.depend*

.if !target(_DIRDEP_USE)
# do some setup we only need once
_CURDIR ?= ${.CURDIR}

# If TARGET_SPEC_VARS is other than just MACHINE
# it should be set by sys.mk or similar by now.
# TARGET_SPEC must not contain any '.'s.
TARGET_SPEC_VARS ?= MACHINE
TARGET_SPEC = ${TARGET_SPEC_VARS:@v@${$v:U}@:ts,}

.if !defined(.MAKE.DEPENDFILE_PREFERENCE)
# this makes the logic below neater?
.MAKE.DEPENDFILE_PREFERENCE = ${_CURDIR}/${.MAKE.DEPENDFILE:T}
.if ${.MAKE.DEPENDFILE:E} == "${TARGET_SPEC}"
.if ${TARGET_SPEC} != ${MACHINE}
.MAKE.DEPENDFILE_PREFERENCE += ${_CURDIR}/${.MAKE.DEPENDFILE:T:R}.$${MACHINE}
.endif
.MAKE.DEPENDFILE_PREFERENCE += ${_CURDIR}/${.MAKE.DEPENDFILE:T:R}
.endif
.endif

_default_dependfile := ${.MAKE.DEPENDFILE_PREFERENCE:[1]:T}
_machine_dependfiles := ${.MAKE.DEPENDFILE_PREFERENCE:M*.${TARGET_SPEC}} \
	${.MAKE.DEPENDFILE_PREFERENCE:M*.${MACHINE}}

# for machine specific dependfiles we require ${MACHINE} to be at the end
# also for the sake of sanity we require a common prefix
.if !defined(.MAKE.DEPENDFILE_PREFIX)
.if !empty(_machine_dependfiles)
.MAKE.DEPENDFILE_PREFIX := ${_machine_dependfiles:[1]:T:R}
.else
.MAKE.DEPENDFILE_PREFIX := ${_default_dependfile:T}
.endif
.endif


# this is how we identify non-machine specific dependfiles
N_notmachine := ${.MAKE.DEPENDFILE_PREFERENCE:E:N${TARGET_SPEC}:N${MACHINE}:${M_ListToSkip}}

.endif				# !target(_DIRDEP_USE)

_last_dependfile := ${.MAKE.MAKEFILES:M*/${.MAKE.DEPENDFILE_PREFIX}*:[-1]}

# Note: if a makefile is read many times, the above
# will not work, so we also test for DEP_MACHINE==depend below.
.if empty(_last_dependfile)
# we haven't included one yet
DEP_MACHINE ?= ${TARGET_MACHINE:U${TARGET_SPEC}}
# else it should be correctly set by ${.MAKE.DEPENDFILE}
.elif ${_last_dependfile:E:${N_notmachine}} == "" || ${DEP_MACHINE:Uno:${N_notmachine}} == ""
# don't rely on manually maintained files to be correct
DEP_MACHINE := ${_DEP_MACHINE:U${TARGET_SPEC}}
.else
# just in case
DEP_MACHINE ?= ${_last_dependfile:E}
.endif

# pickup customizations
# as below you can use !target(_DIRDEP_USE) to protect things
# which should only be done once.
.-include "local.dirdeps.mk"

# the first time we are included the _DIRDEP_USE target will not be defined
# we can use this as a clue to do initialization and other one time things.
.if !target(_DIRDEP_USE)
# make sure this target exists
dirdeps:

# We normally expect to be included by Makefile.depend.*
# which sets the DEP_* macros below.
DEP_RELDIR ?= ${RELDIR}

# this can cause lots of output!
# set to a set of glob expressions that might match RELDIR
DEBUG_DIRDEPS ?= no

# remember the initial value of DEP_RELDIR - we test for it below.
_DEP_RELDIR := ${DEP_RELDIR}

# things we skip for host tools
SKIP_HOSTDIR ?=

NSkipHostDir = ${SKIP_HOSTDIR:N*.host:S,$,.host,:N.host:${M_ListToSkip}}
NSkipHostDep = ${SKIP_HOSTDIR:R:@d@*/$d*.host@:${M_ListToSkip}}

# things we always skip
# SKIP_DIRDEPS allows for adding entries on command line.
SKIP_DIR += .host *.WAIT ${SKIP_DIRDEPS}

.ifdef HOSTPROG
SKIP_DIR += ${SKIP_HOSTDIR}
.endif

NSkipDir = ${SKIP_DIR:${M_ListToSkip}}

.if defined(NO_DIRDEPS) || defined(NODIRDEPS)
# confine ourselves to the original dir
DIRDEPS_FILTER += M${_DEP_RELDIR}*
.endif

# we supress SUBDIR when visiting the leaves
# we assume sys.mk will set MACHINE_ARCH
_DIRDEP_USE:	.USE .MAKE
	@for m in ${.MAKE.MAKEFILE_PREFERENCE}; do \
		test -s ${.TARGET:R}/$$m || continue; \
		echo "${TRACER}Checking ${.TARGET:R} for ${.TARGET:E} ..."; \
		TARGET_SPEC=${.TARGET:E} \
		MACHINE=${.TARGET:E} MACHINE_ARCH= NO_SUBDIR=1 \
		${.MAKE} -C ${.TARGET:R} || exit 1; \
		break; \
	done

.ifdef ALL_MACHINES
# this is how you limit it to only the machines we have been built for
# previously.
.if empty(ONLY_MACHINE_LIST)
.if !empty(ALL_MACHINE_LIST)
# ALL_MACHINE_LIST is the list of all legal machines - ignore anything else
_machine_list != cd ${_CURDIR} && 'ls' -1 ${ALL_MACHINE_LIST:O:u:@m@${.MAKE.DEPENDFILE:T:R}.$m@} 2> /dev/null; echo
.else
_machine_list != 'ls' -1 ${_CURDIR}/${.MAKE.DEPENDFILE_PREFIX}.* 2> /dev/null; echo
.endif
_only_machines := ${_machine_list:${NIgnoreFiles:UN*.bak}:E:O:u}
.else
_only_machines := ${ONLY_MACHINE_LIST}
.endif

.if empty(_only_machines)
# we must be boot-strapping
_only_machines := ${TARGET_MACHINE:U${ALL_MACHINE_LIST:U${DEP_MACHINE}}}
.endif

.else				# ! ALL_MACHINES
# if ONLY_MACHINE_LIST is set, we are limited to that
# if TARGET_MACHINE is set - it is really the same as ONLY_MACHINE_LIST
# otherwise DEP_MACHINE is it - so DEP_MACHINE will match.
_only_machines := ${ONLY_MACHINE_LIST:U${TARGET_MACHINE:U${DEP_MACHINE}}:M${DEP_MACHINE}}
.endif

.if !empty(NOT_MACHINE_LIST)
_only_machines := ${_only_machines:${NOT_MACHINE_LIST:${M_ListToSkip}}}
.endif

# make sure we have a starting place?
DIRDEPS ?= ${RELDIR}
.endif				# target 

_debug_reldir := ${DEBUG_DIRDEPS:@x@${DEP_RELDIR:M$x}${${DEP_RELDIR}.${DEP_MACHINE}:L:M$x}@}
_debug_search := ${DEBUG_DIRDEPS:@x@${DEP_RELDIR:M$x}${${DEP_RELDIR}.depend:L:M$x}@}

# the rest is done repeatedly for every Makefile.depend we read.
# if we are anything but the original dir we care only about the
# machine type we were included for..

.if ${DEP_RELDIR} == "."
_this_dir := ${SRCTOP}
.else
_this_dir := ${SRCTOP}/${DEP_RELDIR}
.endif

# on rare occasions, there can be a need for extra help
_dep_hack := ${_this_dir}/${.MAKE.DEPENDFILE_PREFIX}.inc
.-include "${_dep_hack}"

.if ${DEP_RELDIR} != ${_DEP_RELDIR} || ${DEP_MACHINE} != ${TARGET_SPEC}
# this should be all
_machines := ${DEP_MACHINE}
.else
# this is the machine list we actually use below
_machines := ${_only_machines}

.if defined(HOSTPROG) || ${DEP_MACHINE} == "host"
# we need to build this guy's dependencies for host as well.
_machines += host
.endif

_machines := ${_machines:O:u}
.endif

# reset these each time through
_build_dirs =
_depdir_files =

.if ${DEP_RELDIR} == ${_DEP_RELDIR}
# pickup other machines for this dir if necessary
.if ${BUILD_AT_LEVEL0:Uyes} == "no"
_build_dirs += ${_machines:@m@${_CURDIR}.$m@}
.else
_build_dirs += ${_machines:N${DEP_MACHINE}:@m@${_CURDIR}.$m@}
.if ${DEP_MACHINE} == ${TARGET_SPEC}
# pickup local dependencies now
.-include <.depend>
.endif
.endif
.endif

.if !empty(_debug_reldir)
.info ${DEP_RELDIR}.${DEP_MACHINE}: _last_dependfile='${_last_dependfile}'
.info ${DEP_RELDIR}.${DEP_MACHINE}: DIRDEPS='${DIRDEPS}'
.info ${DEP_RELDIR}.${DEP_MACHINE}: _machines='${_machines}' 
.endif

.if !empty(DIRDEPS)

# this is what we start with
__depdirs := ${DIRDEPS:${NSkipDir}:${DIRDEPS_FILTER:ts:}:O:u:@d@${SRCTOP}/$d@}

# some entries may be qualified with .<machine> 
# the :M*/*/*.* just tries to limit the dirs we check to likely ones.
# the ${d:E:M*/*} ensures we don't consider junos/usr.sbin/mgd
__qual_depdirs := ${__depdirs:M*/*/*.*:@d@${exists($d):?:${"${d:E:M*/*}":?:${exists(${d:R}):?$d:}}}@}
__unqual_depdirs := ${__depdirs:${__qual_depdirs:Uno:${M_ListToSkip}}}

.if ${DEP_RELDIR} == ${_DEP_RELDIR}
# if it was called out - we likely need it.
__hostdpadd := ${DPADD:U.:M${HOST_OBJTOP}/*:S,${HOST_OBJTOP}/,,:H:${NSkipDir}:${DIRDEPS_FILTER:ts:}:S,$,.host,:N.*:@d@${SRCTOP}/$d@}
__qual_depdirs += ${__hostdpadd}
.endif

.if !empty(_debug_reldir)
.info depdirs=${__depdirs}
.info qualified=${__qual_depdirs}
.info unqualified=${__unqual_depdirs}
.endif

# _build_dirs is what we will feed to _DIRDEP_USE
_build_dirs += \
	${__qual_depdirs:M*.host:${NSkipHostDir}:N.host} \
	${__qual_depdirs:N*.host} \
	${_machines:@m@${__unqual_depdirs:@d@$d.$m@}@}

_build_dirs := ${_build_dirs:O:u}

# this is where we will pick up more dependencies from
# the inner inline loops look complex, but save a significant
# amount of memory compared to a .for loop.
_depdir_files =
.for d in ${_build_dirs}
.if exists($d)
# easy, we're building for ${MACHINE}
_depdir_files += ${.MAKE.DEPENDFILE_PREFERENCE:T:@m@${exists($d/$m):?$d/$m:}@:[1]}
.elif exists(${d:R}) && ${d:R:T} == ${d:T:R}
# a little more complex - building for another machine
# we will ensure the file is qualified with a machine
# so that if necessary _DEP_MACHINE can be set below
_depdir_files += ${.MAKE.DEPENDFILE_PREFERENCE:T:S,.${TARGET_SPEC}$,.${d:E},:S,.${MACHINE}$,.${d:E},:@m@${exists(${d:R}/$m):?${d:R}/$m:}@:[1]:@m@${"${m:M*.${d:E}}":?$m:$m.${d:E}}@}
.endif
.endfor

# clean up
_depdir_files := ${_depdir_files:O:u}

.endif				# empty DIRDEPS

# Normally if doing make -V something,
# we do not want to waste time chasing DIRDEPS
# but if we want to count the number of Makefile.depend* read, we do.
.if ${.MAKEFLAGS:M-V${_V_READ_DIRDEPS}} == ""
.if !empty(_build_dirs)
# this makes it all happen
dirdeps: ${_build_dirs}
${_build_dirs}:	_DIRDEP_USE

.if !empty(_debug_reldir)
.info ${DEP_RELDIR}.${DEP_MACHINE}: ${_build_dirs}
.endif

.for m in ${_machines}
# it would be nice to do :N${.TARGET}
.if !empty(__qual_depdirs)
.for q in ${__qual_depdirs:E:O:u:N$m}
.if !empty(_debug_reldir) || ${DEBUG_DIRDEPS:@x@${${DEP_RELDIR}.$m:L:M$x}${${DEP_RELDIR}.$q:L:M$x}@} != ""
.info ${DEP_RELDIR}.$m: ${_build_dirs:M*.$q}
.endif
${_this_dir}.$m: ${_build_dirs:M*.$q}
.endfor
.endif
.if !empty(_debug_reldir)
.info ${DEP_RELDIR}.$m: ${_build_dirs:M*.$m:N${_this_dir}.$m}
.endif
${_this_dir}.$m: ${_build_dirs:M*.$m:N${_this_dir}.$m}
.endfor

.endif

.for d in ${_depdir_files}
.if ${.MAKE.MAKEFILES:M${d}} == ""
.if !empty(_debug_search)
.info Looking for $d
.endif
.if exists($d)
.include <$d>
.elif exists(${d:R})
# an unqualified file exists, we qualified it above so we can set _DEP_MACHINE
# it might be manually maintained and shared by all machine types
# tell it the machine we are interested in.
_DEP_MACHINE := ${d:E}
.if !empty(_debug_reldir)
.info loading ${d:R} for ${_DEP_MACHINE}
.endif
# pretend we read $d, so we don't come by here again.
.MAKE.MAKEFILES += $d
.include <${d:R}>
.endif
.endif
.endfor
.endif				# -V

.elif ${.MAKE.LEVEL} > 42
.error You should have stopped recursing by now.
.else
_DEP_RELDIR := ${DEP_RELDIR}
# pickup local dependencies
.-include <.depend>
.endif

