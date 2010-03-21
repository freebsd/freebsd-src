# $FreeBSD: src/gnu/usr.bin/texinfo/Makefile.inc,v 1.12.24.1 2010/02/10 00:26:20 kensmith Exp $

TXIDIR=		${.CURDIR}/../../../../contrib/texinfo
LIBTXI=		${.OBJDIR}/../libtxi/libtxi.a

CFLAGS+=	-DHAVE_CONFIG_H -DLOCALEDIR=\"/usr/share/locale\" \
		-I${TXIDIR} -I${TXIDIR}/lib

.include "../Makefile.inc"
