# $FreeBSD$

WARNS?=		0

.if ${MACHINE_ARCH:Mmips*} && ${MACHINE_ARCH:Mmips64*} == ""
CFLAGS+= -mxgot
CXXFLAGS+= -mxgot
.endif

.include "../Makefile.inc"
