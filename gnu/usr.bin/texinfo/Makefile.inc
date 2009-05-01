# $FreeBSD: src/gnu/usr.bin/texinfo/Makefile.inc,v 1.12.20.1 2009/04/15 03:14:26 kensmith Exp $

TXIDIR=		${.CURDIR}/../../../../contrib/texinfo
LIBTXI=		${.OBJDIR}/../libtxi/libtxi.a

CFLAGS+=	-DHAVE_CONFIG_H -DLOCALEDIR=\"/usr/share/locale\" \
		-I${TXIDIR} -I${TXIDIR}/lib

.include "../Makefile.inc"
