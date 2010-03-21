# $Id: Makefile,v 1.5 2004/09/08 08:27:12 bright Exp $
# $FreeBSD: src/share/examples/autofs/driver/Makefile,v 1.5.24.1 2010/02/10 00:26:20 kensmith Exp $

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
