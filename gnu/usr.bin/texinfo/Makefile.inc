#	$Id: Makefile.inc,v 1.8 1999/01/14 20:00:26 markm Exp $

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
