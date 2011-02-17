# $Id: Makefile,v 1.5 2004/09/08 08:27:12 bright Exp $
# $FreeBSD$

PROG=		autodriver
NO_MAN=

SRCS=		autodriver.c
WARNS?=		4
CFLAGS+=	-g
BINDIR?=	/sbin

DPADD+=		${.OBJDIR}/../libautofs/libautofs.a
#LDADD+=		-lautofs
LDADD+=		${.OBJDIR}/../libautofs/libautofs.a
LDFLAGS+=	-L${.OBJDIR}/../libautofs
CFLAGS+=	-I${.CURDIR}/../libautofs

.include <bsd.prog.mk>
