# $FreeBSD: src/usr.sbin/pkg_install/Makefile.inc,v 1.4.2.1 1999/08/29 15:45:06 peter Exp $

.if exists(${.OBJDIR}/../lib)
LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a
.else
LIBINSTALL=	${.CURDIR}/../lib/libinstall.a
.endif

# Inherit BINDIR from one level up.
.include "../Makefile.inc"
