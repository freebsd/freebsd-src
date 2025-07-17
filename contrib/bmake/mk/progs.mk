# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: progs.mk,v 1.19 2025/05/19 19:15:22 sjg Exp $
#
#	@(#) Copyright (c) 2006, Simon J. Gerraty
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

.MAIN: all

.if defined(PROGS)

# In meta mode, we can capture dependenices for _one_ of the progs.
# if makefile doesn't nominate one, we use the first.
.ifndef UPDATE_DEPENDFILE_PROG
UPDATE_DEPENDFILE_PROG = ${PROGS:[1]}
.export UPDATE_DEPENDFILE_PROG
.endif

.ifndef PROG
# They may have asked us to build just one
.for t in ${PROGS}
.if make($t)
PROG ?= $t
.endif
.endfor
.endif

.if defined(PROG)
# just one of many
PROG_VARS += \
	BINDIR \
	CXXFLAGS \
	DPADD \
	DPLIBS \
	LDADD \
	MAN \

.ifndef SYS_OS_MK
# assume we are not using init.mk, otherwise
# we need to avoid overlap with its
# QUALIFIED_VAR_LIST which includes these and its
# VAR_QUALIFIER_LIST includes .TARGET which
# would match PROG
PROG_VARS += \
	CFLAGS \
	COPTS \
	CPPFLAGS \
	LDFLAGS \
	SRCS \

.endif

.for v in ${PROG_VARS:O:u}
.if defined(${v}.${PROG}) || defined(${v}_${PROG})
$v += ${${v}_${PROG}:U${${v}.${PROG}}}
.endif
.endfor

# for meta mode, there can be only one!
.if ${PROG} == ${UPDATE_DEPENDFILE_PROG:Uno}
UPDATE_DEPENDFILE ?= ${MK_UPDATE_DEPENDFILE:Uyes}
.endif
UPDATE_DEPENDFILE ?= NO

# ensure that we don't clobber each other's dependencies
DEPENDFILE?= .depend.${PROG}
# prog.mk will do the rest
.else
all: ${PROGS}

# We cannot capture dependencies for meta mode here
UPDATE_DEPENDFILE = NO
# nor can we safely run in parallel.
.NOTPARALLEL:
.endif
.endif

# handle being called [bsd.]progs.mk
.include <${.PARSEFILE:S,progs,prog,}>

.ifndef PROG
# tell progs.mk we might want to install things
PROGS_TARGETS+= cleandepend cleandir cleanobj depend install

.for p in ${PROGS}
.if defined(PROGS_CXX) && !empty(PROGS_CXX:M$p)
# bsd.prog.mk may need to know this
x.$p= PROG_CXX=$p
.endif

$p ${p}_p: .PHONY .MAKE
	(cd ${.CURDIR} && ${.MAKE} -f ${MAKEFILE} PROG=$p ${x.$p} -DWITHOUT_META_STATS)

.for t in ${PROGS_TARGETS:O:u}
$p.$t: .PHONY .MAKE
	(cd ${.CURDIR} && ${.MAKE} -f ${MAKEFILE} PROG=$p ${x.$p} ${@:E} -DWITHOUT_META_STATS)
.endfor
.endfor

.for t in ${PROGS_TARGETS:O:u}
$t: ${PROGS:%=%.$t}
.endfor

.if !defined(WITHOUT_META_STATS) && ${.MAKE.LEVEL} > 0
.END: _reldir_finish
.ERROR: _reldir_failed
.endif

.endif
