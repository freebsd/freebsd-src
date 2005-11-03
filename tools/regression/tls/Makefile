# $FreeBSD: src/tools/regression/tls/Makefile,v 1.2 2005/04/23 23:47:58 davidxu Exp $

SUBDIR=libxx libyy ttls1 ttls2 ttls4

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=ttls3
.endif

.include <bsd.subdir.mk>
