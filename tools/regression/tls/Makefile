# $FreeBSD: src/tools/regression/tls/Makefile,v 1.2.26.1 2010/12/21 17:10:29 kensmith Exp $

SUBDIR=libxx libyy ttls1 ttls2 ttls4

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=ttls3
.endif

.include <bsd.subdir.mk>
