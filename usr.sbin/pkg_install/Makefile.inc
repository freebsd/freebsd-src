# $FreeBSD: src/usr.sbin/pkg_install/Makefile.inc,v 1.8 2002/02/27 19:03:15 sobomax Exp $

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
