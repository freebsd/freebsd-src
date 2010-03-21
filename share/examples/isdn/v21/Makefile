# $FreeBSD: src/share/examples/isdn/v21/Makefile,v 1.4.22.1 2010/02/10 00:26:20 kensmith Exp $

PROG=		v21modem
NO_MAN=
DPADD=		${LIBUTIL} ${LIBM}
LDADD=		-lutil -lm
CFLAGS+=	-g -Wall

.if !defined(BSDTYPE)
BSDTYPE!=uname
.endif

.include <bsd.prog.mk>
