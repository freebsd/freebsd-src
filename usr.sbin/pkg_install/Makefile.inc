# $FreeBSD: src/usr.sbin/pkg_install/Makefile.inc,v 1.5 1999/08/28 01:17:53 peter Exp $

.if exists(${.OBJDIR}/../lib)
LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a
.else
LIBINSTALL=	${.CURDIR}/../lib/libinstall.a
.endif

# Inherit BINDIR from one level up.
.include "../Makefile.inc"
