# $FreeBSD: src/tools/regression/tls/Makefile,v 1.2.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

SUBDIR=libxx libyy ttls1 ttls2 ttls4

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=ttls3
.endif

.include <bsd.subdir.mk>
