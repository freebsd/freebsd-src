# $FreeBSD: src/share/examples/isdn/v21/Makefile,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=		v21modem
NO_MAN=
DPADD=		${LIBUTIL} ${LIBM}
LDADD=		-lutil -lm
CFLAGS+=	-g -Wall

.if !defined(BSDTYPE)
BSDTYPE!=uname
.endif

.include <bsd.prog.mk>
