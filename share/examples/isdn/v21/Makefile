# $FreeBSD: src/share/examples/isdn/v21/Makefile,v 1.2 2004/02/05 22:01:28 ru Exp $

PROG=		v21modem
NOMAN=		1
DPADD=		${LIBUTIL} ${LIBM}
LDADD=		-lutil -lm
CFLAGS+=	-g -Wall

.if !defined(BSDTYPE)
BSDTYPE!=uname
.endif

.include <bsd.prog.mk>
