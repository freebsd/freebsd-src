# $FreeBSD: src/gnu/usr.bin/texinfo/makeinfo/Makefile,v 1.9.2.1 1999/08/29 14:35:30 peter Exp $

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
