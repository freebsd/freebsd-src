# $Id: Makefile,v 1.5 2004/09/08 08:27:12 bright Exp $
# $FreeBSD: src/share/examples/autofs/driver/Makefile,v 1.5.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=autodriver

SRCS=		autodriver.c
NO_MAN=
WARNS=		4
CFLAGS+=	-g
BINDIR?=	/sbin

DPADD+=		${.OBJDIR}/../libautofs/libautofs.a
#LDADD+=		-lautofs
LDADD+=		${.OBJDIR}/../libautofs/libautofs.a
LDFLAGS+=	-L${.OBJDIR}/../libautofs
CFLAGS+=	-I${.CURDIR}/../libautofs

.include <bsd.prog.mk>
