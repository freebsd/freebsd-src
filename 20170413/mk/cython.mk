# RCSid:
#	$Id: cython.mk,v 1.6 2014/10/15 06:23:51 sjg Exp $
#
#	@(#) Copyright (c) 2014, Simon J. Gerraty
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

# this is what we build
CYTHON_MODULE = ${CYTHON_MODULE_NAME}${CYTHON_PYVERSION}.so

CYTHON_MODULE_NAME?= it
CYTHON_SRCS?= ${CYTHON_MODULE_NAME}.pyx

# this is where we save generated src
CYTHON_SAVEGENDIR?= ${.CURDIR}/gen

# pyprefix is where python bits are
# which may not be where we want to put ours (prefix)
.if exists(/usr/pkg/include)
pyprefix?= /usr/pkg
.endif
pyprefix?= /usr/local

PYTHON_VERSION?= 2.7
PYTHON_H?= ${pyprefix}/include/python${PYTHON_VERSION}/Python.h
PYVERSION:= ${PYTHON_VERSION:C,\..*,,}

# set this empty if you don't want to handle multiple versions
.if !defined(CYTHON_PYVERSION)
CYTHON_PYVERSION:= ${PYVERSION}
.endif

CFLAGS+= -I${PYTHON_H:H}

CYTHON_GENSRCS= ${CYTHON_SRCS:R:S,$,${CYTHON_PYVERSION}.c,}
SRCS+= ${CYTHON_GENSRCS}

.SUFFIXES: .pyx .c .So

CYTHON?= ${pyprefix}/bin/cython

# if we don't have cython we can use pre-generated srcs
.if ${type ${CYTHON} 2> /dev/null || echo:L:sh:M/*} == ""
.PATH: ${CYTHON_SAVEGENDIR}
.else

.if !empty(CYTHON_PYVERSION)
.for c in ${CYTHON_SRCS}
${c:R}${CYTHON_PYVERSION}.${c:E}: $c
	ln -sf ${.ALLSRC:M*pyx} ${.TARGET}
.endfor
.endif

.pyx.c:
	${CYTHON} ${CYTHON_FLAGS} -${PYVERSION} -o ${.TARGET} ${.IMPSRC}


save-gen: ${CYTHON_GENSRCS}
	mkdir -p ${CYTHON_SAVEGENDIR}
	cp -p ${.ALLSRC} ${CYTHON_SAVEGENDIR}

.endif

COMPILE.c?= ${CC} -c ${CFLAGS}

.c.So:
	${COMPILE.c} ${PICFLAG} ${CC_PIC} ${.IMPSRC} -o ${.TARGET}

${CYTHON_MODULE}: ${SRCS:S,.c,.So,}
	${CC} ${CC_SHARED:U-shared} -o ${.TARGET} ${.ALLSRC:M*.So} ${LDADD}

# conf.host_target() is limited to uname -m rather than uname -p
_HOST_MACHINE!= uname -m
.if ${HOST_TARGET:M*${_HOST_MACHINE}} == ""
PY_HOST_TARGET:= ${HOST_TARGET:S,${_HOST_ARCH:U${uname -p:L:sh}}$,${_HOST_MACHINE},}
.endif

MODULE_BINDIR?= ${.CURDIR:H}/${PY_HOST_TARGET:U${HOST_TARGET}}

build-cython-module: ${CYTHON_MODULE}

install-cython-module: ${CYTHON_MODULE}
	test -d ${DESTDIR}${MODULE_BINDIR} || \
	${INSTALL} -d ${DESTDIR}${MODULE_BINDIR}
	${INSTALL} -m 755 ${.ALLSRC} ${DESTDIR}${MODULE_BINDIR}

CLEANFILES+= *.So ${CYTHON_MODULE}
