#	$Id$

TXIDIR=	${.CURDIR}/../../../../contrib/texinfo

.if exists(${.OBJDIR}/../libtxi)
LIBTXI=	${.OBJDIR}/../libtxi/libtxi.a
LIBINTL = ${.OBJDIR}/../libintl/libintl.a
.else
LIBTXI=	${.CURDIR}/../libtxi/libtxi.a
LIBINTL = ${.CURDIR}/../libintl/libintl.a
.endif

.include "../../Makefile.inc"
