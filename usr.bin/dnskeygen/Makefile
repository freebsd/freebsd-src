# $FreeBSD$

USE_LIBBIND=	true
.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/dnskeygen
.PATH:	${BIND_DIR}/doc/man

PROG=	dnskeygen
SRCS=	dnskeygen.c 
MAN1=	dnskeygen.1

.include <bsd.prog.mk>
