# $FreeBSD: src/usr.bin/dig/Makefile,v 1.5.2.2 1999/12/13 15:08:16 peter Exp $

USE_LIBBIND=	yes
.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/dig
.PATH:	${BIND_DIR}/bin/nslookup
.PATH:	${BIND_DIR}/doc/man

PROG=	dig
SRCS=	dig.c list.c subr.c debug.c send.c

.include <bsd.prog.mk>
