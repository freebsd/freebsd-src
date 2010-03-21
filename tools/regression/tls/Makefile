# $FreeBSD: src/tools/regression/tls/Makefile,v 1.2.24.1 2010/02/10 00:26:20 kensmith Exp $

SUBDIR=libxx libyy ttls1 ttls2 ttls4

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=ttls3
.endif

.include <bsd.subdir.mk>
