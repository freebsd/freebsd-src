#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/usr.sbin/IPXrouted/Makefile,v 1.4 1999/08/28 01:15:01 peter Exp $

PROG=	IPXrouted
MAN8=	IPXrouted.8
SRCS=	af.c if.c input.c main.c output.c startup.c tables.c timer.c trace.c
SRCS+=  sap_input.c sap_tables.c sap_output.c
DPADD=	${LIBCOMPAT} ${LIBIPX}
LDADD=	-lcompat -lipx

.include <bsd.prog.mk>
