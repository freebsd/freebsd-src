#	$Id$

PROG=	makeinfo
NOMAN=	yes
SRCS=	makeinfo.c multi.c

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -DLOCALEDIR=\"/usr/share/local\"
CFLAGS+= -I${TXIDIR}/makeinfo -I${TXIDIR} -I${TXIDIR}/lib -I../libintl

DPADD=	${LIBTXI} ${LIBINTL}
LDADD=	${LIBTXI} ${LIBINTL}

.include <bsd.prog.mk>

.PATH: $(TXIDIR)/makeinfo
