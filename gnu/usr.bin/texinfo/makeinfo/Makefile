#	$Id: Makefile,v 1.9 1999/01/15 12:27:02 peter Exp $

PROG=	makeinfo
NOMAN=	yes
SRCS=	makeinfo.c multi.c

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -DLOCALEDIR=\"/usr/share/locale\"
CFLAGS+= -I${TXIDIR}/makeinfo -I${TXIDIR} -I${TXIDIR}/lib -I../libintl

DPADD=	${LIBTXI} ${LIBINTL}
LDADD=	${LIBTXI} ${LIBINTL}

.include <bsd.prog.mk>

.PATH: $(TXIDIR)/makeinfo
