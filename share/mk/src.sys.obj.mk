# $FreeBSD$

_default_makeobjdir=	$${.CURDIR:S,^$${SRCTOP},$${OBJTOP},}

.if empty(OBJROOT) || ${.MAKE.LEVEL} == 0
.if defined(MAKEOBJDIRPREFIX) && !empty(MAKEOBJDIRPREFIX)
# put things approximately where they want
OBJROOT:=	${MAKEOBJDIRPREFIX}${SRCTOP}/
MAKEOBJDIRPREFIX=
.export MAKEOBJDIRPREFIX
.endif
.if empty(MAKEOBJDIR)
# OBJTOP set below
MAKEOBJDIR=	${_default_makeobjdir}
# export but do not track
.export-env MAKEOBJDIR
# Expand for our own use
MAKEOBJDIR:=	${MAKEOBJDIR}
.endif
.if !empty(SB)
SB_OBJROOT?=	${SB}/obj/
# this is what we use below
OBJROOT?=	${SB_OBJROOT}
.endif
OBJROOT?=	/usr/obj${SRCTOP}/
.if ${OBJROOT:M*/} != ""
OBJROOT:=	${OBJROOT:H:tA}/
.else
OBJROOT:=	${OBJROOT:H:tA}/${OBJROOT:T}
.endif
.export OBJROOT SRCTOP
.endif

.if defined(MAKEOBJDIR)
.if ${MAKEOBJDIR:M/*} == ""
.error Cannot use MAKEOBJDIR=${MAKEOBJDIR}${.newline}Unset MAKEOBJDIR to get default:  MAKEOBJDIR='${_default_makeobjdir}'
.endif
.endif
