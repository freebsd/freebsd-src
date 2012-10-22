# $FreeBSD$ 

.include "bsd.own.mk"

.if defined(.PARSEDIR)
# make sure this is available to unit-tests/Makefile
.export SRCTOP
.endif

.if ${MK_BMAKE} != "no"
PROG= make
.endif

WARNS=3
