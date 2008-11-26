# $FreeBSD: src/share/examples/isdn/v21/Makefile,v 1.4.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=		v21modem
NO_MAN=
DPADD=		${LIBUTIL} ${LIBM}
LDADD=		-lutil -lm
CFLAGS+=	-g -Wall

.if !defined(BSDTYPE)
BSDTYPE!=uname
.endif

.include <bsd.prog.mk>
