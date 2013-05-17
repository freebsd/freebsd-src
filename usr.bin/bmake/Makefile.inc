# $FreeBSD$ 

.include "bsd.own.mk"

.if defined(.PARSEDIR)
# make sure this is available to unit-tests/Makefile
.export SRCTOP
.endif

# Sadly, we cannot assume bsd.own.mk did its job,
# nor can we safely include the one we want
.if !defined(WITHOUT_BMAKE)
MK_BMAKE= yes
.endif

.if defined(MK_BMAKE) && ${MK_BMAKE} != "no"
PROG= make
.endif

.if !defined(MK_SHARED_TOOLCHAIN) || ${MK_SHARED_TOOLCHAIN} == "no"
NO_SHARED?=     YES
.endif

WARNS=3
CFLAGS+= -DNO_PWD_OVERRIDE
