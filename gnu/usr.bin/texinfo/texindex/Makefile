# $FreeBSD: src/gnu/usr.bin/texinfo/texindex/Makefile,v 1.5.2.1 1999/10/26 08:40:01 tg Exp $

PROG=	texindex
NOMAN=	noman

CFLAGS+= -DHAVE_CONFIG_H -DLOCALEDIR=\"/usr/share/locale\"
CFLAGS+= -I${TXIDIR} -I${TXIDIR}/lib -I../libintl

DPADD=	${LIBTXI} ${LIBINTL} -lz
LDADD=	${LIBTXI} ${LIBINTL} -lz

.include <bsd.prog.mk>

.PATH: ${TXIDIR}/util
