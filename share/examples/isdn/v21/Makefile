# $FreeBSD: src/share/examples/isdn/v21/Makefile,v 1.4.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=		v21modem
NO_MAN=
DPADD=		${LIBUTIL} ${LIBM}
LDADD=		-lutil -lm
CFLAGS+=	-g -Wall

.if !defined(BSDTYPE)
BSDTYPE!=uname
.endif

.include <bsd.prog.mk>
