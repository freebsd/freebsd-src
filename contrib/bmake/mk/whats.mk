# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: whats.mk,v 1.12 2024/02/17 17:26:57 sjg Exp $
#
#	@(#) Copyright (c) 2014-2020, Simon J. Gerraty
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

.if ${MK_WHATSTRING:Uno} == "yes"
# it can be useful to embed a what(1) string in binaries
# so that the build location can be seen from a core file.
.if defined(KMOD)
what_thing ?= ${KMOD}
.elif defined(LIB)
what_thing ?= lib${LIB}
.elif defined(PROG)
what_thing ?= ${PROG}
SRCS ?= ${PROG}.c
.elif defined(SHLIB)
what_thing ?= lib${SHLIB}
.endif

.if !empty(what_thing)
# a unique name that won't conflict with anything
what_uuid = what_${what_thing}_${.CURDIR:T:hash}
what_var = what_${.CURDIR:T:hash}

SRCS += ${what_uuid}.c
CLEANFILES += ${what_uuid}.c
# we do not need to capture this
SUPPRESS_DEPEND += *${what_uuid}.c

SB ?= ${SRCTOP:H}
SB_LOCATION ?= ${HOST}:${SB}
# make customization easy
WHAT_LOCATION ?= ${.OBJDIR:S,${SB},${SB_LOCATION},}
WHAT_1 ?= ${what_thing:tu} built ${%Y%m%d:L:localtime} by ${USER}
WHAT_2 ?= ${what_location}
WHAT_LINE_IDS ?= 1 2
WHAT_NOCMP_LINE_IDS ?= 1
# you can add other WHAT_* just be sure to set WHAT_LINE_IDS
# and WHAT_NOCMP_LINE_IDS accordingly

# this works with clang and gcc
what_t = const char __attribute__ ((section(".data")))
what_location := ${WHAT_LOCATION}

# this script is done in multiple lines so we can
# use the token ${.OODATE:MNO_META_CMP}
# to prevent the variable parts making this constantly out-of-date
${what_uuid}.c:	.NOTMAIN
	echo 'extern const char ${WHAT_LINE_IDS:@i@${what_var}_$i[]@:ts,};' > $@
.for i in ${WHAT_LINE_IDS}
.if ${WHAT_NOCMP_LINE_IDS:M$i} != ""
	echo '${what_t} ${what_var}_$i[] = "@(#)${WHAT_$i}";' >> $@ ${.OODATE:MNO_META_CMP}
.else
	echo '${what_t} ${what_var}_$i[] = "@(#)${WHAT_$i}";' >> $@
.endif
.endfor

.endif
.endif
