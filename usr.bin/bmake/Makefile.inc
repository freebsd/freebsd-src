# $FreeBSD$ 

# we need the up to date bsd.own.mk
.if ${.CURDIR:M*make} == ""
.sinclude "share/mk/bsd.own.mk"
.else
.sinclude "../../share/mk/bsd.own.mk"
.endif
# the above should have found it...
.include "bsd.own.mk"

.if defined(.PARSEDIR)
# make sure this is available to unit-tests/Makefile
.export SRCTOP
.endif

.if defined(MK_BMAKE) && ${MK_BMAKE} != "no"
PROG= make
.endif

.if !defined(MK_SHARED_TOOLCHAIN) || ${MK_SHARED_TOOLCHAIN} == "no"
NO_SHARED?=     YES
.endif

WARNS=3
CFLAGS+= -DNO_PWD_OVERRIDE
