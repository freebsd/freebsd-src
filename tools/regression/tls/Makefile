# $FreeBSD: src/tools/regression/tls/Makefile,v 1.2.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

SUBDIR=libxx libyy ttls1 ttls2 ttls4

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=ttls3
.endif

.include <bsd.subdir.mk>
