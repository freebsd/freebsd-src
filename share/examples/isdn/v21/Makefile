# $FreeBSD: src/share/examples/isdn/v21/Makefile,v 1.4 2004/12/21 08:47:09 ru Exp $

PROG=		v21modem
NO_MAN=
DPADD=		${LIBUTIL} ${LIBM}
LDADD=		-lutil -lm
CFLAGS+=	-g -Wall

.if !defined(BSDTYPE)
BSDTYPE!=uname
.endif

.include <bsd.prog.mk>
