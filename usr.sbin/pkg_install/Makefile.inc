# $FreeBSD$

LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL) && \
    defined(LDADD) && ${LDADD:M-lfetch} != ""
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

# Inherit BINDIR from one level up.
.include "../Makefile.inc"
