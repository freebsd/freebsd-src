# $FreeBSD$

.if exists(${.OBJDIR}/../lib)
LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a
.else
LIBINSTALL=	${.CURDIR}/../lib/libinstall.a
.endif

# Inherit BINDIR from one level up.
.if !target(__foobar__)
__foobar__:
.if exists(../Makefile.inc)
.include "../Makefile.inc"
.endif
.endif
