#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/usr.sbin/IPXrouted/Makefile,v 1.8 2003/11/15 17:10:56 trhodes Exp $

PROG=	IPXrouted
MAN=	IPXrouted.8
SRCS=	af.c if.c input.c main.c output.c startup.c tables.c timer.c trace.c
SRCS+=  sap_input.c sap_tables.c sap_output.c

WARNS?= 2

DPADD=	${LIBCOMPAT} ${LIBIPX}
LDADD=	-lcompat -lipx

.include <bsd.prog.mk>
