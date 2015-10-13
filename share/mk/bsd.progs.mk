# $FreeBSD$
# $Id: progs.mk,v 1.11 2012/11/06 17:18:54 sjg Exp $
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

.if defined(PROGS) || defined(PROGS_CXX)
# we really only use PROGS below...
PROGS += ${PROGS_CXX}

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
PROG_OVERRIDE_VARS +=	BINDIR BINGRP BINOWN BINMODE DPSRCS MAN PROGNAME \
			SRCS
PROG_VARS +=	CFLAGS CPPFLAGS CXXFLAGS DPADD DPLIBS LDADD LIBADD LINKS \
		LDFLAGS MLINKS ${PROG_OVERRIDE_VARS}
.for v in ${PROG_VARS:O:u}
.if empty(${PROG_OVERRIDE_VARS:M$v})
.if defined(${v}.${PROG})
$v += ${${v}.${PROG}}
.elif defined(${v}_${PROG})
$v += ${${v}_${PROG}}
.endif
.else
$v ?=
.endif
.endfor

# for meta mode, there can be only one!
.if ${PROG} == ${UPDATE_DEPENDFILE_PROG}
UPDATE_DEPENDFILE ?= yes
.endif
UPDATE_DEPENDFILE ?= NO

# prog.mk will do the rest
.else
all: ${PROGS}

# We cannot capture dependencies for meta mode here
UPDATE_DEPENDFILE = NO
.endif
.endif	# PROGS || PROGS_CXX

# These are handled by the main make process.
.ifdef _RECURSING_PROGS
_PROGS_GLOBAL_VARS= CLEANFILES CLEANDIRS FILESGROUPS SCRIPTS CONFGROUPS
.for v in ${_PROGS_GLOBAL_VARS}
$v =
.endfor
.endif

# handle being called [bsd.]progs.mk
.include <bsd.prog.mk>

.if !empty(PROGS) && !defined(_RECURSING_PROGS)
# tell progs.mk we might want to install things
PROGS_TARGETS+= checkdpadd clean cleandepend cleandir depend install

.for p in ${PROGS}
.if defined(PROGS_CXX) && !empty(PROGS_CXX:M$p)
# bsd.prog.mk may need to know this
x.$p= PROG_CXX=$p
.endif

# Main PROG target
$p ${p}_p: .PHONY .MAKE
	(cd ${.CURDIR} && \
	    DEPENDFILE=.depend.$p \
	    ${MAKE} -f ${MAKEFILE} _RECURSING_PROGS= \
	    SUBDIR= PROG=$p ${x.$p})

# Pseudo targets for PROG, such as 'install'.
.for t in ${PROGS_TARGETS:O:u}
$p.$t: .PHONY .MAKE
	(cd ${.CURDIR} && \
	    DEPENDFILE=.depend.$p \
	    ${MAKE} -f ${MAKEFILE} _RECURSING_PROGS= \
	    SUBDIR= PROG=$p ${x.$p} ${@:E})
.endfor
.endfor

# Depend main pseudo targets on all PROG.pseudo targets too.
.for t in ${PROGS_TARGETS:O:u}
$t: ${PROGS:%=%.$t}
.endfor
.endif
