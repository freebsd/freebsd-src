# $FreeBSD: src/usr.bin/newgrp/Makefile,v 1.3.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	newgrp
DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil

.if defined(ENABLE_SUID_NEWGRP)
BINMODE=	4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
