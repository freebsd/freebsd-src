# $FreeBSD$ 

.include "bsd.own.mk"

.if defined(.PARSEDIR)
# make sure this is available to unit-tests/Makefile
.export SRCTOP
.endif

.if defined(MK_BMAKE) && ${MK_BMAKE} != "no"
PROG= make
.endif

.if ${MK_SHARED_TOOLCHAIN} == "no"
NO_SHARED?=     YES
.endif

WARNS=3
