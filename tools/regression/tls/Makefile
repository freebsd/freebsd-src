# $FreeBSD: src/tools/regression/tls/Makefile,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

SUBDIR=libxx libyy ttls1 ttls2 ttls4

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=ttls3
.endif

.include <bsd.subdir.mk>
