# $FreeBSD: src/usr.bin/newgrp/Makefile,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	newgrp
DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil

.if defined(ENABLE_SUID_NEWGRP)
BINMODE=	4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
