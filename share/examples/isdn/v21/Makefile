# $FreeBSD: src/share/examples/isdn/v21/Makefile,v 1.4.24.1 2010/12/21 17:10:29 kensmith Exp $

PROG=		v21modem
NO_MAN=
DPADD=		${LIBUTIL} ${LIBM}
LDADD=		-lutil -lm
CFLAGS+=	-g -Wall

.if !defined(BSDTYPE)
BSDTYPE!=uname
.endif

.include <bsd.prog.mk>
