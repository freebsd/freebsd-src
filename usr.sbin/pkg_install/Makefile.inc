# $FreeBSD$

.include <bsd.own.mk>

LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a

DPADD+=		${LIBUTIL}
LDADD+=		-lutil

.if ${MK_OPENSSL} != "no" && \
    defined(LDADD) && ${LDADD:M-lfetch} != ""
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

# Inherit BINDIR from one level up.
.include "../Makefile.inc"
