# $Id: Makefile,v 1.2 2004/07/17 11:26:51 bright Exp $
# $FreeBSD$

PROG=autodriver

SRCS=		autodriver.c
NOMAN=		YES
WERROR=		YES
WARNS=		4
CFLAGS+=	-g

DPADD+=		${.OBJDIR}/../libautofs/libautofs.a
#LDADD+=		-lautofs
LDADD+=		${.OBJDIR}/../libautofs/libautofs.a
LDFLAGS+=	-L${.OBJDIR}/../libautofs
CFLAGS+=	-I${.CURDIR}/../libautofs

.include <bsd.prog.mk>
