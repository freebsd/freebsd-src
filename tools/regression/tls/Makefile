# $FreeBSD: src/tools/regression/tls/Makefile,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $

SUBDIR=libxx libyy ttls1 ttls2 ttls4

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=ttls3
.endif

.include <bsd.subdir.mk>
