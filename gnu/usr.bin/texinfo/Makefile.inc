#	$Id$

TXIDIR=	${.CURDIR}/../../../../contrib/texinfo

.if exists(${.OBJDIR}/../libtxi)
LIBTXI=	${.OBJDIR}/../libtxi/libtxi.a
.else
LIBTXI=	${.CURDIR}/../libtxi/libtxi.a
.endif

.include "../../Makefile.inc"
