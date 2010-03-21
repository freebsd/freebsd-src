# $FreeBSD: src/usr.sbin/pkg_install/Makefile.inc,v 1.14.2.2.4.1 2010/02/10 00:26:20 kensmith Exp $

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
