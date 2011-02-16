# $FreeBSD: src/usr.sbin/pkg_install/Makefile.inc,v 1.17.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

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
