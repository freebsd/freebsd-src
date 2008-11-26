# $FreeBSD: src/usr.bin/newgrp/Makefile,v 1.3.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	newgrp
DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil

.if defined(ENABLE_SUID_NEWGRP)
BINMODE=	4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
