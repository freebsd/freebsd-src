# $Id: Makefile,v 1.4 1997/02/22 19:54:49 peter Exp $

.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/dig
.PATH:	${BIND_DIR}/bin/nslookup
.PATH:	${BIND_DIR}/doc/man

PROG=	dig
SRCS=	dig.c list.c subr.c debug.c send.c

.include <bsd.prog.mk>
