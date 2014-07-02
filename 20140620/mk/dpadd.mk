# $Id: dpadd.mk,v 1.19 2014/04/05 22:56:54 sjg Exp $
#
#	@(#) Copyright (c) 2004, Simon J. Gerraty
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

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__:

# sometimes we play games with .CURDIR etc
# _* hold the original values of .*
_OBJDIR?= ${.OBJDIR}
_CURDIR?= ${.CURDIR}

# DPLIBS helps us ensure we keep DPADD and LDADD in sync
DPLIBS+= ${DPLIBS_LAST}
DPADD+= ${DPLIBS}
.for __lib in ${DPLIBS:T:R}
LDADD+= ${LDADD_${__lib}:U${__lib:T:R:S/lib/-l/:C/\.so.*//}}
.endfor

# DPADD can contain things other than libs
__dpadd_libs = ${DPADD:M*/lib*}

# some libs have dependencies...
# DPLIBS_* allows bsd.libnames.mk to flag libs which must be included
# in DPADD for a given library.
.for __lib in ${__dpadd_libs:@d@${DPLIBS_${d:T:R}}@}
.if "${DPADD:M${__lib}}" == ""
DPADD+= ${__lib}
LDADD+= ${LDADD_${__lib}:U${__lib:T:R:S/lib/-l/:C/\.so.*//}}
.endif
.endfor
# Last of all... for libc and libgcc
DPADD+= ${DPADD_LAST}

# Convert DPADD into -I and -L options and add them to CPPFLAGS and LDADD
# For the -I's convert the path to a relative one.  For separate objdirs
# the DPADD paths will be to the obj tree so we need to subst anyway.

# If USE_PROFILE is yes, then check for profiled versions of libs
# and use them.

USE_PROFILE?=no
.if defined(LIBDL) && exists(${LIBDL})
.if defined(PROG) && (make(${PROG}_p) || ${USE_PROFILE} == yes) && \
	defined(LDFLAGS) && ${LDFLAGS:M-export-dynamic}
# building profiled version of a prog that needs dlopen to work
DPLIBS+= ${LIBDL}
.endif
.endif

.if defined(LIBDMALLOC) && exists(${LIBDMALLOC})
.if defined(USE_DMALLOC) && ${USE_DMALLOC} != no
.if !defined(NO_DMALLOC)
CPPFLAGS+= -DUSE_DMALLOC
.endif
DPLIBS+= ${LIBDMALLOC}
.endif
.endif

# Order -L's to search ours first.
# Avoids picking up old versions already installed.
__dpadd_libdirs := ${__dpadd_libs:R:H:S/^/-L/g:O:u:N-L}
LDADD += ${__dpadd_libdirs:M-L${OBJTOP}/*}
LDADD += ${__dpadd_libdirs:N-L${OBJTOP}/*}

.if ${.CURDIR} == ${SRCTOP}
RELDIR=.
RELTOP=.
.else
RELDIR?= ${.CURDIR:S,${SRCTOP}/,,}
.if ${RELDIR} == ${.CURDIR}
RELDIR?= ${.OBJDIR:S,${OBJTOP}/,,}
.endif
RELTOP?= ${RELDIR:C,[^/]+,..,g}
.endif
RELOBJTOP?= ${OBJTOP}
RELSRCTOP?= ${SRCTOP}

.if !make(dpadd)
.ifdef LIB
# Each lib is its own src_lib, we want to include it in SRC_LIBS
# so that the correct INCLUDES_* will be picked up automatically.
SRC_LIBS+= ${_OBJDIR}/lib${LIB}.a
.endif
.endif

# 
# This little bit of magic, assumes that SRC_libfoo will be
# set if it cannot be correctly derrived from ${LIBFOO}
# Note that SRC_libfoo and INCLUDES_libfoo should be named for the
# actual library name not the variable name that might refer to it.
# 99% of the time the two are the same, but the DPADD logic
# only has the library name available, so stick to that.
# 

SRC_LIBS?=
__dpadd_libs += ${SRC_LIBS}
DPMAGIC_LIBS += ${__dpadd_libs} \
	${__dpadd_libs:@d@${DPMAGIC_LIBS_${d:T:R}}@}

.for __lib in ${DPMAGIC_LIBS:O:u}
# 
# if SRC_libfoo is not set, then we assume that the srcdir corresponding
# to where we found the library is correct.
#
SRC_${__lib:T:R} ?= ${__lib:H:S,${OBJTOP},${RELSRCTOP},}
#
# This is a no-brainer but just to be complete...
#
OBJ_${__lib:T:R} ?= ${__lib:H:S,${OBJTOP},${RELOBJTOP},}
#
# If INCLUDES_libfoo is not set, then we'll use ${SRC_libfoo}/h if it exists,
# else just ${SRC_libfoo}.
#
INCLUDES_${__lib:T:R}?= -I${exists(${SRC_${__lib:T:R}}/h):?${SRC_${__lib:T:R}}/h:${SRC_${__lib:T:R}}}

.endfor

# Now for the bits we actually need
__dpadd_incs=
.for __lib in ${__dpadd_libs:u}
.if (make(${PROG}_p) || defined(NEED_GPROF)) && exists(${__lib:R}_p.a)
__ldadd=-l${__lib:T:R:S,lib,,}
LDADD := ${LDADD:S,^${__ldadd}$,${__ldadd}_p,g}
.endif

#
# Some libs generate headers, so we potentially need both
# the src dir and the obj dir.
# If ${INCLUDES_libfoo} contains a word ending in /h, we assume that either
# 1. it does not generate headers or
# 2. INCLUDES_libfoo will have been set correctly
# XXX it gets ugly avoiding duplicates... 
# use :? to ensure .for does not prevent correct evaluation
#
# We take care of duplicate suppression later.
__dpadd_incs += ${"${INCLUDES_${__lib:T:R}:M*/h}":? :-I${OBJ_${__lib:T:R}}}
__dpadd_incs += ${INCLUDES_${__lib:T:R}}
.endfor

#
# eliminate any duplicates - but don't mess with the order
# force evaluation now - to avoid giving make a headache
#
.for t in CFLAGS CXXFLAGS
# avoid duplicates
__$t_incs:=${$t:M-I*:O:u}
.for i in ${__dpadd_incs}
.if "${__$t_incs:M$i}" == ""
$t+= $i
__$t_incs+= $i
.endif
.endfor
.endfor

# This target is used to gather a list of
# dir: ${DPADD}
# entries
.if make(*dpadd*)
# allow overrides
.-include "dpadd++.mk"

.if !target(dpadd)
dpadd:	.NOTMAIN
.if defined(DPADD) && ${DPADD} != ""
	@echo "${RELDIR}: ${DPADD:S,${OBJTOP}/,,}"
.endif
.endif
.endif

.ifdef SRC_PATHADD
# We don't want to assume that we need to .PATH every element of 
# SRC_LIBS, but the Makefile cannot do
# .PATH: ${SRC_libfoo}
# since the value of SRC_libfoo must be available at the time .PATH:
# is read - and we only just worked it out.  
# Further, they can't wait until after include of {lib,prog}.mk as 
# the .PATH is needed before then.
# So we let the Makefile do
# SRC_PATHADD+= ${SRC_libfoo}
# and we defer the .PATH: until now so that SRC_libfoo will be available.
.PATH: ${SRC_PATHADD}
.endif

.endif
