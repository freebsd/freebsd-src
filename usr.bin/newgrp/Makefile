# $FreeBSD$

PROG=	newgrp
DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil

.if defined(ENABLE_SUID_NEWGRP) && ${ENABLE_SUID_NEWGRP} == "true"
BINMODE=	4555
INSTALLFLAGS=	-fschg
.endif

.include <bsd.prog.mk>
