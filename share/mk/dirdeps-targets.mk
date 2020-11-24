# $FreeBSD$
# RCSid:
#       $Id: dirdeps-targets.mk,v 1.22 2020/08/15 18:00:11 sjg Exp $
#
#       @(#) Copyright (c) 2019-2020 Simon J. Gerraty
#
#       This file is provided in the hope that it will
#       be of use.  There is absolutely NO WARRANTY.
#       Permission to copy, redistribute or otherwise
#       use this file is hereby granted provided that
#       the above copyright notice and this notice are
#       left intact.
#
#       Please send copies of changes and bug-fixes to:
#       sjg@crufty.net
#

##
# This makefile is used to set initial DIRDEPS for top-level build
# targets.
#
# The basic idea is that we have a list of directories in
# DIRDEPS_TARGETS_DIRS which are relative to SRCTOP.
# When asked to make 'foo' we look for any directory named 'foo'
# under DIRDEPS_TARGETS_DIRS.
# We then search those dirs for any Makefile.depend*
# Finally we select any that match conditions like REQUESTED_MACHINE
# or TARGET_SPEC and initialize DIRDEPS accordingly.
#
# We will check each of the initial DIRDEPS for Makefile.dirdeps.options
# and include any found.
# This makes it feasible to tweak options like MK_DIRDEPS_CACHE
# for a specific target.
#
# If MK_STATIC_DIRDEPS_CACHE is defined we will check if the
# initial DIRDEPS has a static cache (Makefile.dirdeps.cache).
# This only makes sense for seriously expensive targets.
#

.if ${.MAKE.LEVEL} == 0
# pickup customizations
.-include <local.dirdeps-targets.mk>

# for DIRDEPS_BUILD this is how we prime the pump
DIRDEPS_TARGETS_DIRS ?= targets targets/pseudo
# these prefixes can modify how we behave
# they need to be stripped when looking for target dirs
DIRDEPS_TARGETS_PREFIX_LIST ?= pkg- build-

# some .TARGETS need filtering
DIRDEPS_TARGETS_FILTER += Nall

# matching target dirs if any
tdirs := ${.TARGETS:${DIRDEPS_TARGETS_FILTER:ts:}:${DIRDEPS_TARGETS_PREFIX_LIST:@p@S,^$p,,@:ts:}:@t@${DIRDEPS_TARGETS_DIRS:@d@$d/$t@}@:@d@${exists(${SRCTOP}/$d):?$d:}@}

.if !empty(DEBUG_DIRDEPS_TARGETS)
.info tdirs=${tdirs}
.endif

.if !empty(tdirs)
# some things we know we want to ignore
DIRDEPS_TARGETS_SKIP_LIST += \
	*~ \
	*.bak \
	*.inc \
	*.old \
	*.options \
	*.orig \
	*.rej \

# the list of MACHINEs we consider
DIRDEPS_TARGETS_MACHINE_LIST += \
	${ALL_MACHINE_LIST:U} \
	${PSEUDO_MACHINE_LIST:Ucommon host host32} \
	${TARGET_MACHINE_LIST}

DIRDEPS_TARGETS_MACHINE_LIST := ${DIRDEPS_TARGETS_MACHINE_LIST:O:u}

# raw Makefile.depend* list
tdeps != 'cd' ${SRCTOP} && 'ls' -1 ${tdirs:O:u:@d@$d/${.MAKE.DEPENDFILE_PREFIX}*@} 2> /dev/null; echo
.if ${DEBUG_DIRDEPS_TARGETS:U:Mdep*} != ""
.info tdeps=${tdeps}
.endif
# remove things we know we don't want
tdeps := ${tdeps:${DIRDEPS_TARGETS_SKIP_LIST:${M_ListToSkip}}}
.if ${DEBUG_DIRDEPS_TARGETS:U:Mdep*} != ""
.info tdeps=${tdeps}
.endif

# plain entries (no qualifiers) these apply to any TARGET_SPEC
ptdeps := ${tdeps:M*${.MAKE.DEPENDFILE_PREFIX}:S,/${.MAKE.DEPENDFILE_PREFIX},,}

# MACHINE qualified entries
mqtdeps := ${DIRDEPS_TARGETS_MACHINE_LIST:@m@${tdeps:M*.$m}@:S,/${.MAKE.DEPENDFILE_PREFIX},,}

tqtdeps =
.if ${TARGET_SPEC_VARS:[#]} > 1
# TARGET_SPEC qualified entries
.if !empty(TARGET_SPEC_LIST)
# we have a list of valid TARGET_SPECS; use it
tqtdeps := ${TARGET_SPEC_LIST:U:O:u:@t@${tdeps:M*.$t}@:S,/${.MAKE.DEPENDFILE_PREFIX},,}
.else
# do we have a list of valid tuple members for at least
# the last tupple element? if so match on that
TARGET_SPEC_LAST_LIST ?= ${${TARGET_SPEC_VARS:[-1]}_LIST}
.if !empty(TARGET_SPEC_LAST_LIST)
tqtdeps := ${TARGET_SPEC_LAST_LIST:U:O:u:@t@${tdeps:M*,$t}@:S,/${.MAKE.DEPENDFILE_PREFIX},,}
.else
# this is sub-optimal match MACHINE,
tqtdeps := ${DIRDEPS_TARGETS_MACHINE_LIST:@m@${tdeps:M*.$m,*}@:S,/${.MAKE.DEPENDFILE_PREFIX},,}
.endif
.endif
.endif

# now work out what we want in DIRDEPS
.if empty(REQUESTED_MACHINE)
# we want them all just as found
DIRDEPS = ${ptdeps} ${mqtdeps} ${tqtdeps}
.else
# we only want those that match REQUESTED_MACHINE/REQUESTED_TARGET_SPEC
# or REQUESTED_TARGET_SPEC (TARGET_SPEC)
DIRDEPS = \
	${ptdeps:@d@$d.${REQUESTED_TARGET_SPEC:U${TARGET_SPEC:U${REQUESTED_MACHINE}}}@} \
	${mqtdeps:M*.${REQUESTED_MACHINE}} \
	${tqtdeps:M*.${REQUESTED_TARGET_SPEC:U${TARGET_SPEC}}}
.endif
# clean up
DIRDEPS := ${DIRDEPS:O:u}

.if !empty(DEBUG_DIRDEPS_TARGETS)
.for x in tdeps ptdeps mqtdeps tqtdeps DIRDEPS
.info $x=${$x}
.endfor
.endif
.endif
# if we got DIRDEPS get to work
.if !empty(DIRDEPS)
DIRDEPS.dirs := ${DIRDEPS:S,^,${SRCTOP}/,:@d@${exists($d):?$d:${d:R}}@}
# some targets what to tweak options we might want to process now
.for m in ${DIRDEPS.dirs:S,$,/Makefile.dirdeps.options,}
.-include <$m>
.endfor
.if defined(MK_STATIC_DIRDEPS_CACHE)
# some targets are very expensive to compute dirdeps for
# so we may have a static cache
.for c in ${DIRDEPS.dirs:S,$,/Makefile.dirdeps.cache,}
.if exists($c)
STATIC_DIRDEPS_CACHE ?= $c
.if ${MK_STATIC_DIRDEPS_CACHE} == "yes"
DIRDEPS_CACHE ?= $c
MK_DIRDEPS_CACHE = yes
.endif
.endif
.endfor
.if defined(STATIC_DIRDEPS_CACHE)
.export STATIC_DIRDEPS_CACHE
.endif
.endif

# allow a top-level makefile to do other stuff
# before including dirdeps.mk
.if ${MK_DIRDEPS_TARGETS_INCLUDE_DIRDEPS:Uyes} == "yes"
.include <dirdeps.mk>
.endif

DIRDEPS_TARGETS_SKIP += all clean* destroy*

.for t in ${.TARGETS:${DIRDEPS_TARGETS_SKIP:${M_ListToSkip}}}
$t: dirdeps
.endfor
.endif
.endif
