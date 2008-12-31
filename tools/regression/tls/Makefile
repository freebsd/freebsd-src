# $FreeBSD: src/tools/regression/tls/Makefile,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

SUBDIR=libxx libyy ttls1 ttls2 ttls4

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=ttls3
.endif

.include <bsd.subdir.mk>
