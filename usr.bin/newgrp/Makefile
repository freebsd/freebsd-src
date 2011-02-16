# $FreeBSD: src/usr.bin/newgrp/Makefile,v 1.3.26.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	newgrp
DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil

.if defined(ENABLE_SUID_NEWGRP)
BINMODE=	4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
