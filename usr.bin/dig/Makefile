# $FreeBSD: src/usr.bin/dig/Makefile,v 1.5.2.1 1999/08/29 15:26:46 peter Exp $

.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/dig
.PATH:	${BIND_DIR}/bin/nslookup
.PATH:	${BIND_DIR}/doc/man

PROG=	dig
SRCS=	dig.c list.c subr.c debug.c send.c

.include <bsd.prog.mk>
