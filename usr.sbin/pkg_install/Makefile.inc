# $FreeBSD: src/usr.sbin/pkg_install/Makefile.inc,v 1.11 2004/08/06 07:27:06 cperciva Exp $

.if exists(${.OBJDIR}/../lib)
LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a
.else
LIBINSTALL=	${.CURDIR}/../lib/libinstall.a
.endif

.if !defined(NOCRYPT) && !defined(NO_OPENSSL) && \
    defined(LDADD) && ${LDADD:M-lfetch} != ""
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

# Inherit BINDIR from one level up.
.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
