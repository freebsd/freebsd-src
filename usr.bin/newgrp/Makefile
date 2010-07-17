# $FreeBSD: src/usr.bin/newgrp/Makefile,v 1.3.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	newgrp
DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil

.if defined(ENABLE_SUID_NEWGRP)
BINMODE=	4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
