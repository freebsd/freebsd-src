#	$Id: Makefile.inc,v 1.5.2.1 1997/04/11 17:53:54 asami Exp $

TXIDIR=	${.CURDIR}/../../../../contrib/texinfo

.if exists(${.OBJDIR}/../libtxi)
LIBTXI=	${.OBJDIR}/../libtxi/libtxi.a
.else
LIBTXI=	${.CURDIR}/../libtxi/libtxi.a
.endif

.include "../../Makefile.inc"
