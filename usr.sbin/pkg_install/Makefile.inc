# $FreeBSD: src/usr.sbin/pkg_install/Makefile.inc,v 1.14.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a

.if ${MK_OPENSSL} != "no" && \
    defined(LDADD) && ${LDADD:M-lfetch} != ""
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

# Inherit BINDIR from one level up.
.include "../Makefile.inc"
