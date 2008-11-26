# $FreeBSD: src/gnu/usr.bin/texinfo/Makefile.inc,v 1.12.16.1 2008/10/02 02:57:24 kensmith Exp $

TXIDIR=		${.CURDIR}/../../../../contrib/texinfo
LIBTXI=		${.OBJDIR}/../libtxi/libtxi.a

CFLAGS+=	-DHAVE_CONFIG_H -DLOCALEDIR=\"/usr/share/locale\" \
		-I${TXIDIR} -I${TXIDIR}/lib

.include "../Makefile.inc"
