#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/usr.sbin/IPXrouted/Makefile,v 1.7 2001/07/20 06:19:33 obrien Exp $

PROG=	IPXrouted
MAN=	IPXrouted.8
SRCS=	af.c if.c input.c main.c output.c startup.c tables.c timer.c trace.c
SRCS+=  sap_input.c sap_tables.c sap_output.c

DPADD=	${LIBCOMPAT} ${LIBIPX}
LDADD=	-lcompat -lipx

.include <bsd.prog.mk>
