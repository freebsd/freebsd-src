# $FreeBSD$ 

.include "bsd.own.mk"

.if defined(.PARSEDIR)
# make sure this is available to unit-tests/Makefile
.export SRCTOP
.endif

.if defined(MK_BMAKE)
PROG= make
.endif

WARNS=3
