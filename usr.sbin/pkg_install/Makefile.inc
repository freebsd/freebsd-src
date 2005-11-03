# $FreeBSD: src/usr.sbin/pkg_install/Makefile.inc,v 1.13 2005/06/10 06:12:53 des Exp $

LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL) && \
    defined(LDADD) && ${LDADD:M-lfetch} != ""
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

# Inherit BINDIR from one level up.
.include "../Makefile.inc"
