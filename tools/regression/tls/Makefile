# $FreeBSD: src/tools/regression/tls/Makefile,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $

SUBDIR=libxx libyy ttls1 ttls2 ttls4

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=ttls3
.endif

.include <bsd.subdir.mk>
