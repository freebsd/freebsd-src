# $FreeBSD: src/gnu/usr.bin/texinfo/Makefile.inc,v 1.9.2.1 1999/08/29 14:35:22 peter Exp $

TXIDIR=		${.CURDIR}/../../../../contrib/texinfo

.if exists(${.OBJDIR}/../libintl)
LIBINTL=	${.OBJDIR}/../libintl/libintl.a
.else
LIBINTL=	${.CURDIR}/../libintl/libintl.a
.endif

.if exists(${.OBJDIR}/../libtxi)
LIBTXI=		${.OBJDIR}/../libtxi/libtxi.a
.else
LIBTXI=		${.CURDIR}/../libtxi/libtxi.a
.endif

.include "../Makefile.inc"
