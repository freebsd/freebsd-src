# $FreeBSD: src/share/examples/isdn/v21/Makefile,v 1.1 2001/05/25 13:04:58 hm Exp $

PROG=		v21modem
NOMAN=		1
LDADD=		-lutil -lm
CFLAGS+=	-g -Wall

.if !defined(BSDTYPE)
BSDTYPE!=uname
.endif

.include <bsd.prog.mk>
