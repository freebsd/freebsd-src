#	$Id: Makefile,v 1.8 1999/01/14 20:00:44 markm Exp $

PROG=	install-info

CFLAGS+= -DHAVE_CONFIG_H -DLOCALEDIR=\"/usr/share/locale\"
CFLAGS+= -I${TXIDIR} -I${TXIDIR}/lib -I../libintl

DPADD=	${LIBTXI} ${LIBINTL} -lz
LDADD=	${LIBTXI} ${LIBINTL} -lz

.include <bsd.prog.mk>

.PATH: ${TXIDIR}/util
