# $FreeBSD: src/gnu/usr.bin/texinfo/install-info/Makefile,v 1.8.2.1 1999/08/29 14:35:25 peter Exp $

PROG=	install-info

CFLAGS+= -DHAVE_CONFIG_H -DLOCALEDIR=\"/usr/share/locale\"
CFLAGS+= -I${TXIDIR} -I${TXIDIR}/lib -I../libintl

DPADD=	${LIBTXI} ${LIBINTL} -lz
LDADD=	${LIBTXI} ${LIBINTL} -lz

.include <bsd.prog.mk>

.PATH: ${TXIDIR}/util
