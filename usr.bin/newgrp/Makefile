# $FreeBSD$

PROG=	newgrp
DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil

.if defined(ENABLE_SUID_NEWGRP)
BINMODE=	4555
INSTALLFLAGS=	-fschg
.endif

.include <bsd.prog.mk>
