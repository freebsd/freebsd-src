# $FreeBSD: src/usr.bin/newgrp/Makefile,v 1.1 2002/05/28 05:05:28 tjr Exp $

PROG=	newgrp
DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil

.if defined(ENABLE_SUID_NEWGRP) && ${ENABLE_SUID_NEWGRP} == "true"
BINMODE=	4555
INSTALLFLAGS=	-fschg
.endif

.include <bsd.prog.mk>
