# $FreeBSD: src/share/examples/isdn/v21/Makefile,v 1.2.2.1 2005/02/13 07:23:28 obrien Exp $

PROG=		v21modem
NOMAN=
DPADD=		${LIBUTIL} ${LIBM}
LDADD=		-lutil -lm
CFLAGS+=	-g -Wall

.if !defined(BSDTYPE)
BSDTYPE!=uname
.endif

.include <bsd.prog.mk>
