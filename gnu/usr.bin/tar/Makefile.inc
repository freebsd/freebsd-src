# $FreeBSD$

TARDIR=	${.CURDIR}/../../../../contrib/tar
TARCFLAGS= -DHAVE_CONFIG_H -I${.CURDIR}/.. -I${TARDIR}/lib

# Inherit BINDIR from one level up.
.if !target(__foobar__)
__foobar__:
.if exists(../Makefile.inc)
.include "../Makefile.inc"
.endif
.endif
