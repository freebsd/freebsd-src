# $FreeBSD: src/usr.bin/dnskeygen/Makefile,v 1.1.2.1 1999/12/13 15:08:17 peter Exp $

USE_LIBBIND=	true
.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/dnskeygen
.PATH:	${BIND_DIR}/doc/man

PROG=	dnskeygen
SRCS=	dnskeygen.c 
MAN1=	dnskeygen.1

.include <bsd.prog.mk>
