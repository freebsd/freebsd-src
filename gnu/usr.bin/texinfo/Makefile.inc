#	$Id: Makefile.inc,v 1.7 1997/04/07 17:21:19 bde Exp $

TXIDIR=	${.CURDIR}/../../../../contrib/texinfo

.if exists(${.OBJDIR}/../libtxi)
LIBTXI=	${.OBJDIR}/../libtxi/libtxi.a
.else
LIBTXI=	${.CURDIR}/../libtxi/libtxi.a
.endif

.include "../../Makefile.inc"
