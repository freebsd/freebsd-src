# $Id: Makefile,v 1.2 1996/08/29 21:51:37 peter Exp $

.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/tools
.PATH:	${BIND_DIR}/tools/nslookup
.PATH:	${BIND_DIR}/man

PROG=	dig
SRCS=	dig.c list.c subr.c debug.c send.c

.include <bsd.prog.mk>
