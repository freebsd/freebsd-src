# $FreeBSD$

LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a

DPADD+=		${LIBUTIL}
LDADD+=		-lutil

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL) && \
    defined(LDADD) && ${LDADD:M-lfetch} != ""
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

# Inherit BINDIR from one level up.
.include "../Makefile.inc"
