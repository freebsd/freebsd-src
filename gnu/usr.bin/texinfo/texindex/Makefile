# $FreeBSD$

PROG=	texindex
NOMAN=	noman

CFLAGS+= -DHAVE_CONFIG_H -DLOCALEDIR=\"/usr/share/locale\"
CFLAGS+= -I${TXIDIR} -I${TXIDIR}/lib -I../libintl

DPADD=	${LIBTXI} ${LIBINTL} -lz
LDADD=	${LIBTXI} ${LIBINTL} -lz

.include <bsd.prog.mk>

.PATH: ${TXIDIR}/util
