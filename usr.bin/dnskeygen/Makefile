# $FreeBSD: src/usr.bin/dnskeygen/Makefile,v 1.1 1999/11/30 06:26:14 peter Exp $

USE_LIBBIND=	true
.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/dnskeygen
.PATH:	${BIND_DIR}/doc/man

PROG=	dnskeygen
SRCS=	dnskeygen.c 
MAN1=	dnskeygen.1

.include <bsd.prog.mk>
