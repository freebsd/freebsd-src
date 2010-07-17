# $FreeBSD: src/gnu/usr.bin/texinfo/Makefile.inc,v 1.12.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

TXIDIR=		${.CURDIR}/../../../../contrib/texinfo
LIBTXI=		${.OBJDIR}/../libtxi/libtxi.a

CFLAGS+=	-DHAVE_CONFIG_H -DLOCALEDIR=\"/usr/share/locale\" \
		-I${TXIDIR} -I${TXIDIR}/lib

.include "../Makefile.inc"
